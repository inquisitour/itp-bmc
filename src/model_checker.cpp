#include "model_checker.h"
#include "cnf_generator.h"
#include "proof_parser.h"
#include "interpolant.h"
#include <algorithm>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <map>
#include <fstream>
#include <string>
#include <unistd.h>
#include <climits>

static std::string getWorkdir() {
    const char* wd = getenv("BMC_WORKDIR");
    return wd ? std::string(wd) : std::string(".");
}

static std::string getBinaryDir() {
    char path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path)-1);
    if (len != -1) {
        path[len] = '\0';
        std::string p(path);
        return p.substr(0, p.rfind('/'));
    }
    return ".";
}

ModelChecker::ModelChecker(const AIG& aig) : aig(aig) {}

bool ModelChecker::check(int maxBound, int skip) {
    std::string workdir    = getWorkdir();
    std::string minisat    = getBinaryDir() + "/minisatp/minisat";
    std::string proof_path = workdir + "/proof.txt";
    std::string cnf_path   = workdir + "/out.cnf";
    std::string result_path = workdir + "/result.txt";

    // accumulated: interpolants renamed to s0, as (latch_idx, neg) pairs
    std::vector<std::vector<std::pair<int,bool>>> accumulated;

    if (aig.latches.empty()) {
        std::cout << "Fixpoint reached!" << std::endl;
        return true;
    }

    for (int k = 1; k <= maxBound; k++) {
        std::cout << "Checking bound " << k << "..." << std::endl;

        // Generate CNF
        CNFGenerator cnf_gen(aig);
        if (accumulated.empty()) {
            cnf_gen.generateBMC(k, skip);
        } else {
            cnf_gen.generateIMC(k, accumulated);
        }
        cnf_gen.writeDIMACS(cnf_path);

        int aPartSize = cnf_gen.getAPartSize();
        std::cerr << "DEBUG aPartSize=" << aPartSize << std::endl;
        auto latchVars1 = cnf_gen.getLatchCNFVars(1);
        std::cerr << "DEBUG latchVars1: ";
        for (int v : latchVars1) std::cerr << v << " ";
        std::cerr << std::endl;
        auto cnfToLatchIdx = cnf_gen.getCNFToLatchIdx();
        auto latchIdxToCNF0 = cnf_gen.getLatchIdxToCNF0();

        // Run MiniSAT
        std::remove(proof_path.c_str());
        std::string cmd = minisat + " " + cnf_path + " -r " + result_path +
                          " -p " + proof_path + " > /dev/null 2>&1";
        (void)system(cmd.c_str());

        FILE* f = fopen(result_path.c_str(), "r");
        if (!f) continue;
        char line[16];
        bool unsat = false, foundCex = false;
        if (fgets(line, sizeof(line), f)) {
            unsat    = (line[0] == 'U');
            foundCex = (line[0] == 'S');
        }
        fclose(f);

        if (foundCex && k > skip) {
            if (!accumulated.empty()) {
                // May be spurious — verify with pure BMC from real init
                CNFGenerator bmc_gen(aig);
                bmc_gen.generateBMC(k, skip);
                bmc_gen.writeDIMACS(cnf_path);
                std::remove(proof_path.c_str());
                (void)system((minisat + " " + cnf_path + " -r " + result_path + " > /dev/null 2>&1").c_str());
                FILE* f2 = fopen(result_path.c_str(), "r");
                bool realCex = false;
                if (f2) {
                    char l2[16];
                    if (fgets(l2, sizeof(l2), f2)) realCex = (l2[0] == 'S');
                    fclose(f2);
                }
                if (!realCex) {
                    std::cout << "  Spurious counterexample, continuing..." << std::endl;
                    continue;
                }
            }
            std::cout << "Counterexample found at bound " << k << std::endl;
            return false;
        }

        // Bounds at or below skip assert no bad clause — the formula encodes
        // no question, so neither SAT nor UNSAT carries information here.
        if (k <= skip) continue;

        if (!unsat) continue;

        // Extract interpolant
        ProofParser proof;
        if (!proof.parse(proof_path)) continue;

        std::set<int> sharedVars(latchVars1.begin(), latchVars1.end());
        auto aPartClauses = cnf_gen.getAPartClauses();
        auto bPartClauses = cnf_gen.getBPartClauses();
        Interpolator interp(proof, aPartClauses, bPartClauses, sharedVars);
        auto interpolant = interp.computeInterpolant();

        // Filter to shared-only clauses
        {
            std::vector<std::vector<int>> minimized;
            for (const auto& clause : interpolant) {
                bool allShared = true;
                for (int lit : clause)
                    if (!sharedVars.count(std::abs(lit))) { allShared = false; break; }
                if (allShared) minimized.push_back(clause);
            }
            interpolant = minimized;
        }

        std::cout << "  Interpolant: " << interpolant.size() << " clauses" << std::endl;

        if (interpolant.empty()) continue;

        // Rename interpolant: s1 CNF vars → latch indices
        std::vector<std::vector<std::pair<int,bool>>> interpByLatch;
        for (const auto& clause : interpolant) {
            std::vector<std::pair<int,bool>> latchClause;
            bool valid = true;
            for (int lit : clause) {
                int var = std::abs(lit);
                bool neg = (lit < 0);
                auto it = cnfToLatchIdx.find(var);
                if (it == cnfToLatchIdx.end()) { valid = false; break; }
                latchClause.push_back({it->second, neg});
            }
            if (valid && !latchClause.empty())
                interpByLatch.push_back(latchClause);
        }

        std::cerr << "DEBUG cnfToLatchIdx size=" << cnfToLatchIdx.size() << std::endl;
        std::cerr << "DEBUG latchVars1 size=" << latchVars1.size() << std::endl;
        std::cerr << "DEBUG interpolant clauses=" << interpolant.size() << std::endl;
        for (const auto& clause : interpolant) {
            std::cerr << "DEBUG clause: ";
            for (int lit : clause) std::cerr << lit << " ";
            std::cerr << std::endl;
        }
        std::cerr << "DEBUG interpByLatch size=" << interpByLatch.size() << std::endl;

        if (interpByLatch.empty()) continue;

        // Fixpoint check: does interpByLatch → accumulated?
        // Write CNF: interpByLatch (over s0) AND ¬(accumulated)
        // If UNSAT → fixpoint
        // Translate interpByLatch to CNF using latchIdxToCNF0
        std::vector<std::vector<int>> fixClauses;
        for (const auto& lc : interpByLatch) {
            std::vector<int> c;
            for (auto [idx, neg] : lc) {
                auto it = latchIdxToCNF0.find(idx);
                if (it == latchIdxToCNF0.end()) continue;
                c.push_back(neg ? -(it->second) : it->second);
            }
            if (!c.empty()) fixClauses.push_back(c);
        }

        // Negate accumulated: ¬(c1 ∧ c2 ∧ ... ∧ cn)
        // = for each clause ci, add selector sel_i
        // sel_i true means ci is violated
        // Need at least one sel_i true
        int numVars = (int)latchIdxToCNF0.size();
        int selBase = numVars + 1;

        if (!accumulated.empty()) {
            std::vector<int> atLeastOne;
            for (size_t i = 0; i < accumulated.size(); i++) {
                int sel = selBase + (int)i;
                atLeastOne.push_back(sel);
                for (auto [idx, neg] : accumulated[i]) {
                    auto it = latchIdxToCNF0.find(idx);
                    if (it == latchIdxToCNF0.end()) continue;
                    int l = neg ? -(it->second) : it->second;
                    fixClauses.push_back({-(sel), -l});
                }
            }
            fixClauses.push_back(atLeastOne);
        } else {
            // No accumulated yet — fixpoint if interpolant implies init
            // Init = all latches false at t=0
            // Negation of init = at least one latch is true
            std::vector<int> negInit;
            for (auto [idx, cnfVar] : latchIdxToCNF0)
                negInit.push_back(cnfVar);
            if (!negInit.empty())
                fixClauses.push_back(negInit);
        }

        // Write fixpoint check CNF
        int totalVars = numVars + (int)accumulated.size();
        std::string fp_cnf = workdir + "/fixpoint.cnf";
        std::string fp_res = workdir + "/fixpoint_result.txt";
        std::ofstream fp(fp_cnf);
        fp << "p cnf " << totalVars << " " << fixClauses.size() << "\n";
        for (const auto& c : fixClauses) {
            for (int l : c) fp << l << " ";
            fp << "0\n";
        }
        fp.close();

        (void)system((minisat + " " + fp_cnf + " -r " + fp_res + " > /dev/null 2>&1").c_str());
        FILE* fp_f = fopen(fp_res.c_str(), "r");
        bool fixpointReached = false;
        if (fp_f) {
            char fl[16];
            if (fgets(fl, sizeof(fl), fp_f))
                fixpointReached = (fl[0] == 'U');
            fclose(fp_f);
        }

        if (fixpointReached) {
            std::cout << "Fixpoint reached!" << std::endl;
            return true;
        }

        // Add to accumulated
        for (const auto& c : interpByLatch)
            accumulated.push_back(c);
    }

    std::cout << "Safe up to bound " << maxBound << std::endl;
    return true;
}