#include "model_checker.h"
#include "cnf_generator.h"
#include "proof_parser.h"
#include "interpolant.h"
#include <algorithm>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <set>
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

bool ModelChecker::runBMC(int k, bool& foundCex, int& aPartSize, std::vector<int>& latchVars, int skip) {
    if (k <= skip) {
        foundCex = false;
        return true;
    }

    std::string workdir  = getWorkdir();
    std::string cnf_path = workdir + "/out.cnf";
    std::string result   = workdir + "/result.txt";
    std::string proof    = workdir + "/proof.txt";
    std::string minisat  = getBinaryDir() + "/minisatp/minisat";

    CNFGenerator cnf_gen(aig);
    cnf_gen.generateBMC(k, skip);
    cnf_gen.writeDIMACS(cnf_path);
    aPartSize = cnf_gen.getAPartSize();
    latchVars = cnf_gen.getLatchCNFVars(1);

    std::remove(proof.c_str());

    std::string cmd = minisat + " " + cnf_path + " -r " + result + " -p " + proof + " > /dev/null 2>&1";
    (void)system(cmd.c_str());

    FILE* f = fopen(result.c_str(), "r");
    if (!f) return false;

    char line[16];
    bool unsat = false;
    if (fgets(line, sizeof(line), f)) {
        unsat    = (line[0] == 'U');
        foundCex = (line[0] == 'S');
    }
    fclose(f);
    return unsat;
}

bool ModelChecker::check(int maxBound, int skip) {
    std::string workdir    = getWorkdir();
    std::string proof_path = workdir + "/proof.txt";
    std::vector<std::vector<int>> reachable;

    if (aig.latches.empty()) {
        bool foundCex = false;
        int aPartSize = 0;
        std::vector<int> latchVars;
        bool unsat = runBMC(1, foundCex, aPartSize, latchVars, skip);
        if (foundCex) { std::cout << "Counterexample found at bound 1" << std::endl; return false; }
        if (unsat)    { std::cout << "Fixpoint reached!" << std::endl; return true; }
        return true;
    }

    for (int k = 1; k <= maxBound; k++) {
        std::cout << "Checking bound " << k << "..." << std::endl;

        bool foundCex = false;
        int  aPartSize = 0;
        std::vector<int> latchVars;
        bool unsat = runBMC(k, foundCex, aPartSize, latchVars, skip);

        if (foundCex) {
            std::cout << "Counterexample found at bound " << k << std::endl;
            return false;
        }

        if (unsat) {
            ProofParser proof;
            if (!proof.parse(proof_path)) continue;

            std::set<int> sharedVars(latchVars.begin(), latchVars.end());
            int splitPoint = aPartSize;

            Interpolator interp(proof, splitPoint, sharedVars);
            auto interpolant = interp.computeInterpolant();

            std::cout << "  Raw interpolant: " << interpolant.size() << " clauses" << std::endl;

            {
                std::vector<std::vector<int>> minimized;
                for (const auto& clause : interpolant) {
                    bool allShared = true;
                    for (int lit : clause)
                        if (sharedVars.count(std::abs(lit)) == 0) { allShared = false; break; }
                    if (allShared) minimized.push_back(clause);
                }
                interpolant = minimized;
            }

            std::cout << "  Safe at bound " << k << ", interpolant: "
                      << interpolant.size() << " clauses" << std::endl;

            auto isSubsumed = [](const std::vector<int>& newClause,
                                 const std::vector<std::vector<int>>& existing) -> bool {
                for (const auto& ec : existing) {
                    bool subsumed = true;
                    for (int lit : ec)
                        if (std::find(newClause.begin(), newClause.end(), lit) == newClause.end())
                            { subsumed = false; break; }
                    if (subsumed) return true;
                }
                return false;
            };

            bool fixpoint = !interpolant.empty();
            for (const auto& clause : interpolant)
                if (!isSubsumed(clause, reachable)) { fixpoint = false; break; }

            if (fixpoint) { std::cout << "Fixpoint reached!" << std::endl; return true; }

            for (const auto& clause : interpolant)
                reachable.push_back(clause);
        }
    }

    std::cout << "Safe up to bound " << maxBound << std::endl;
    return true;
}