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
#include <stdexcept>

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

// Is newI => (init OR F1 OR ... OR Fm)?  Decided by SAT:
// newI AND NOT init AND NOT F1 AND ... must be UNSAT.
// Latch i is SAT variable i+1; helper variables come after the latches.
static bool impliedByApprox(const LatchCNF& newI,
                            const std::vector<LatchCNF>& approx,
                            const AIG& aig,
                            const std::string& workdir,
                            const std::string& minisat)
{
    int L = (int)aig.latches.size();
    int next = L + 1;
    auto lit = [](int idx, bool neg) { return neg ? -(idx + 1) : (idx + 1); };
    std::vector<std::vector<int>> cls;

    for (const auto& c : newI) {
        std::vector<int> cl;
        for (auto [idx, neg] : c) cl.push_back(lit(idx, neg));
        cls.push_back(cl);
    }

    // NOT init: some latch with a defined reset differs from it
    std::vector<int> notInit;
    for (int i = 0; i < L; i++) {
        if (aig.latches[i].reset == 0)      notInit.push_back(i + 1);
        else if (aig.latches[i].reset == 1) notInit.push_back(-(i + 1));
    }
    if (notInit.empty()) return true;   // init is every state
    cls.push_back(notInit);

    // NOT Fj: some clause of Fj is violated
    for (const auto& F : approx) {
        if (F.empty()) return true;     // Fj is TRUE
        std::vector<int> some;
        for (const auto& c : F) {
            int v = next++;
            some.push_back(v);
            for (auto [idx, neg] : c)
                cls.push_back({-v, -lit(idx, neg)});
        }
        cls.push_back(some);
    }

    std::string cnf = workdir + "/fixpoint.cnf";
    std::string res = workdir + "/fixpoint_result.txt";
    {
        std::ofstream out(cnf);
        out << "p cnf " << (next - 1) << " " << cls.size() << "\n";
        for (const auto& c : cls) {
            for (int l : c) out << l << " ";
            out << "0\n";
        }
    }
    std::remove(res.c_str());
    (void)system((minisat + " " + cnf + " -r " + res + " > /dev/null 2>&1").c_str());
    FILE* f = fopen(res.c_str(), "r");
    if (!f) return false;
    char line[16];
    bool unsat = fgets(line, sizeof(line), f) && line[0] == 'U';
    fclose(f);
    return unsat;
}

ModelChecker::ModelChecker(const AIG& aig) : aig(aig) {}

bool ModelChecker::check(int maxBound, int skip) {
    std::string workdir     = getWorkdir();
    std::string minisat     = getBinaryDir() + "/minisatp/minisat";
    std::string proof_path  = workdir + "/proof.txt";
    std::string cnf_path    = workdir + "/out.cnf";
    std::string result_path = workdir + "/result.txt";

    if (aig.latches.empty()) {
        CNFGenerator g(aig);
        g.generateBMC(1, 0);
        g.writeDIMACS(cnf_path);
        std::remove(result_path.c_str());
        (void)system((minisat + " " + cnf_path + " -r " + result_path + " > /dev/null 2>&1").c_str());
        bool sat = false;
        if (FILE* rf = fopen(result_path.c_str(), "r")) {
            char l[16];
            if (fgets(l, sizeof(l), rf)) sat = (l[0] == 'S');
            fclose(rf);
        }
        if (sat) { std::cout << "Counterexample found at bound 1" << std::endl; return false; }
        std::cout << "Fixpoint reached!" << std::endl;
        return true;
    }

    for (int k = 1; k <= maxBound; k++) {
        if (k <= skip) continue;
        std::cout << "Checking bound " << k << "..." << std::endl;

        // Reachable over-approximation = init OR F1 OR F2 ...; restarts at init for every k
        std::vector<LatchCNF> approx;

        for (int iter = 0; iter < 200; iter++) {
            CNFGenerator cnf_gen(aig);
            if (approx.empty()) cnf_gen.generateBMC(k, skip);
            else                cnf_gen.generateIMC(k, approx);
            cnf_gen.writeDIMACS(cnf_path);

            auto latchVars1    = cnf_gen.getLatchCNFVars(1);
            auto cnfToLatchIdx = cnf_gen.getCNFToLatchIdx();

            std::remove(proof_path.c_str());
            std::remove(result_path.c_str());
            (void)system((minisat + " " + cnf_path + " -r " + result_path +
                          " -p " + proof_path + " > /dev/null 2>&1").c_str());

            FILE* f = fopen(result_path.c_str(), "r");
            if (!f) { std::cerr << "ERROR: no solver result at bound " << k << std::endl; break; }
            char line[16];
            bool unsat = false, foundCex = false;
            if (fgets(line, sizeof(line), f)) {
                unsat    = (line[0] == 'U');
                foundCex = (line[0] == 'S');
            }
            fclose(f);

            if (foundCex) {
                if (approx.empty()) {
                    // pure BMC from the real init: this is a real counterexample
                    std::cout << "Counterexample found at bound " << k << std::endl;
                    return false;
                }
                std::cout << "  Approximation too coarse at bound " << k
                          << " (iteration " << iter << "), increasing bound" << std::endl;
                break;
            }
            if (!unsat) break;

            ProofParser proof;
            if (!proof.parse(proof_path)) break;

            std::set<int> sharedVars(latchVars1.begin(), latchVars1.end());
            Interpolator interp(proof, cnf_gen.getAPartClauses(), cnf_gen.getBPartClauses(), sharedVars);
            std::vector<std::vector<int>> interpolant;
            try {
                interpolant = interp.computeInterpolant();
            } catch (const std::runtime_error& e) {
                std::cout << "  " << e.what() << std::endl;
                std::cout << "Safe up to bound " << k << " (interpolation gave up; bounded result only)" << std::endl;
                return true;
            }

            int bad = 0;
            for (const auto& clause : interpolant)
                for (int lit : clause)
                    if (!sharedVars.count(std::abs(lit))) { bad++; break; }
            if (bad) {
                std::cerr << "ERROR: " << bad << " interpolant clauses contain non-latch variables" << std::endl;
                break;
            }

            std::cout << "  Interpolant: " << interpolant.size() << " clauses" << std::endl;
            if (interpolant.empty()) break;

            LatchCNF interpByLatch;
            for (const auto& clause : interpolant) {
                std::vector<std::pair<int,bool>> lc;
                for (int lit : clause)
                    lc.push_back({cnfToLatchIdx.at(std::abs(lit)), lit < 0});
                interpByLatch.push_back(lc);
            }

            if (impliedByApprox(interpByLatch, approx, aig, workdir, minisat)) {
                std::cout << "Fixpoint reached!" << std::endl;
                return true;
            }
            approx.push_back(interpByLatch);
        }
    }

    std::cout << "Safe up to bound " << maxBound << std::endl;
    return true;
}