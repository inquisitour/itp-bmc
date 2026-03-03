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

static std::string getWorkdir() {
    const char* wd = getenv("BMC_WORKDIR");
    return wd ? std::string(wd) : std::string(".");
}

ModelChecker::ModelChecker(const AIG& aig) : aig(aig) {}

bool ModelChecker::runBMC(int k, bool& foundCex, int& aPartSize, int skip) {
    // Nothing to check before skip
    if (k <= skip) {
        foundCex = false;
        return true;  // trivially UNSAT
    }

    std::string workdir = getWorkdir();
    std::string cnf     = workdir + "/out.cnf";
    std::string result  = workdir + "/result.txt";
    std::string proof   = workdir + "/proof.txt";
    std::string minisat = workdir + "/minisatp/minisat";

    CNFGenerator cnf_gen(aig);
    cnf_gen.generateBMC(k);
    cnf_gen.writeDIMACS(cnf);
    aPartSize = cnf_gen.getAPartSize();


    // Delete stale proof before running
    std::remove(proof.c_str());

    std::string cmd = minisat + " " + cnf + " -r " + result + " -p " + proof + " > /dev/null 2>&1";
    (void)system(cmd.c_str());

    FILE* f = fopen(result.c_str(), "r");
    if (!f) return false;

    char line[16];
    bool unsat = false;
    if (fgets(line, sizeof(line), f)) {
        unsat = (line[0] == 'U');
        foundCex = (line[0] == 'S');
    }
    fclose(f);
    return unsat;
}

bool ModelChecker::check(int maxBound, int skip) {
    std::string workdir = getWorkdir();
    std::string proof_path = workdir + "/proof.txt";

    std::vector<std::vector<int>> reachable;

    for (int k = 1; k <= maxBound; k++) {
        std::cout << "Checking bound " << k << "..." << std::endl;

        bool foundCex = false;
        int  aPartSize = 0;
        bool unsat = runBMC(k, foundCex, skip);

        if (foundCex) {
            std::cout << "Counterexample found at bound " << k << std::endl;
            return false;
        }

        if (aig.latches.empty()) {
            bool foundCex = false;
            int aPartSize = 0;
            bool unsat = runBMC(1, foundCex, aPartSize);
            if (foundCex) {
                std::cout << "Counterexample found at bound 1" << std::endl;
                return false;
            }
            if (unsat) {
                std::cout << "Fixpoint reached!" << std::endl;
                return true;
            }
            return true;
        }

        if (unsat) {
            ProofParser proof;
            if (!proof.parse(proof_path)) continue;

            // Use actual CNF variable IDs of latches at boundary t=1
            CNFGenerator cnf_gen(aig);
            cnf_gen.generateBMC(k);
            auto latchVars = cnf_gen.getLatchCNFVars(1);
            std::set<int> sharedVars(latchVars.begin(), latchVars.end());

            // Use actual A-part clause count as split point
            int splitPoint = aPartSize;

            Interpolator interp(proof, splitPoint, sharedVars);
            auto interpolant = interp.computeInterpolant();

            std::cout << "  Safe at bound " << k << ", interpolant: "
                      << interpolant.size() << " clauses" << std::endl;

            // Semantic subsumption check instead of syntactic equality
            auto isSubsumed = [](const std::vector<int>& newClause,
                                 const std::vector<std::vector<int>>& existing) -> bool {
                for (const auto& ec : existing) {
                    bool subsumed = true;
                    for (int lit : ec) {
                        if (std::find(newClause.begin(), newClause.end(), lit)
                                == newClause.end()) {
                            subsumed = false;
                            break;
                        }
                    }
                    if (subsumed) return true;
                }
                return false;
            };

            bool fixpoint = !interpolant.empty();
            for (const auto& clause : interpolant) {
                if (!isSubsumed(clause, reachable)) {
                    fixpoint = false;
                    break;
                }
            }

            if (fixpoint) {
                std::cout << "Fixpoint reached!" << std::endl;
                return true;
            }

            for (const auto& clause : interpolant)
                reachable.push_back(clause);
        }
    }

    std::cout << "Safe up to bound " << maxBound << std::endl;
    return true;
}