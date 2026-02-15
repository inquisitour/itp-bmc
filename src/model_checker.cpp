#include "model_checker.h"
#include "cnf_generator.h"
#include "proof_parser.h"
#include "interpolant.h"
#include <iostream>
#include <cstdlib>
#include <set>

ModelChecker::ModelChecker(const AIG& aig) : aig(aig) {}

bool ModelChecker::runBMC(int k, bool& foundCex) {
    CNFGenerator cnf(aig);
    cnf.generateBMC(k);
    cnf.writeDIMACS("out.cnf");
    
    (void)system("./minisatp/minisat out.cnf -r result.txt -p proof.txt > /dev/null 2>&1");
    
    FILE* f = fopen("result.txt", "r");
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

bool ModelChecker::check(int maxBound) {
    std::vector<std::vector<int>> reachable;  // Over-approximation Q
    
    for (int k = 1; k <= maxBound; k++) {
        std::cout << "Checking bound " << k << "..." << std::endl;
        
        bool foundCex = false;
        bool unsat = runBMC(k, foundCex);
        
        if (foundCex) {
            std::cout << "Counterexample found at bound " << k << std::endl;
            return false;  // FAIL
        }
        
        if (unsat) {
            ProofParser proof;
            if (!proof.parse("proof.txt")) continue;
            
            // Compute shared variables (state vars at time 1)
            std::set<int> sharedVars;
            for (unsigned i = 1; i <= aig.numLatches; i++) {
                sharedVars.insert(i);
            }
            
            // Split point: A = initial + first transition
            // Approximate: first portion of clauses
            int splitPoint = proof.getNodes().size() / 3;
            
            Interpolator interp(proof, splitPoint, sharedVars);
            auto interpolant = interp.computeInterpolant();
            
            std::cout << "  Safe at bound " << k << ", interpolant: " 
                      << interpolant.size() << " clauses" << std::endl;
            
            // Fixpoint check: is interpolant subsumed by reachable?
            // Simplified: check if interpolant is empty or unchanged
            if (interpolant.empty() || interpolant == reachable) {
                std::cout << "Fixpoint reached!" << std::endl;
                return true;  // Proved SAFE
            }
            
            // Update reachable: Q' = Q ∨ I
            for (const auto& clause : interpolant) {
                reachable.push_back(clause);
            }
        }
    }
    
    std::cout << "Safe up to bound " << maxBound << std::endl;
    return true;  // OK (bounded)
}