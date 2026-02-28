#include "model_checker.h"
#include "cnf_generator.h"
#include "proof_parser.h"
#include "interpolant.h"
#include <iostream>
#include <cstdlib>
#include <set>
#include <string>

static std::string getWorkdir() {
    const char* wd = getenv("BMC_WORKDIR");
    return wd ? std::string(wd) : std::string(".");
}

ModelChecker::ModelChecker(const AIG& aig) : aig(aig) {}

bool ModelChecker::runBMC(int k, bool& foundCex, int skip) {
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
        bool unsat = runBMC(k, foundCex, skip);

        if (foundCex) {
            std::cout << "Counterexample found at bound " << k << std::endl;
            return false;
        }

        if (unsat) {
            ProofParser proof;
            if (!proof.parse(proof_path)) continue;

            std::set<int> sharedVars;
            for (unsigned i = 1; i <= aig.numLatches; i++) {
                sharedVars.insert(i);
            }

            int splitPoint = proof.getNodes().size() / 3;

            Interpolator interp(proof, splitPoint, sharedVars);
            auto interpolant = interp.computeInterpolant();

            std::cout << "  Safe at bound " << k << ", interpolant: "
                      << interpolant.size() << " clauses" << std::endl;

            if (interpolant.empty() || interpolant == reachable) {
                std::cout << "Fixpoint reached!" << std::endl;
                return true;
            }

            for (const auto& clause : interpolant) {
                reachable.push_back(clause);
            }
        }
    }

    std::cout << "Safe up to bound " << maxBound << std::endl;
    return true;
}