#include "interpolant.h"
#include <algorithm>

Interpolator::Interpolator(const ProofParser& proof, int splitPoint,
                           const std::set<int>& sharedVars)
    : proof(proof), splitPoint(splitPoint), sharedVars(sharedVars) {}

bool Interpolator::isAClause(int nodeId) {
    return nodeId < splitPoint;
}

bool Interpolator::isSharedVar(int var) {
    return sharedVars.count(var) > 0;
}

// Helper to check if variable is A-local (only in A, not in B)
bool Interpolator::isALocal(int var) {
    return !isSharedVar(var);  // Simplified: non-shared = local
}

std::vector<std::vector<int>> Interpolator::computeInterpolant() {
    const auto& nodes = proof.getNodes();
    nodeInterpolants.resize(nodes.size());
    
    for (size_t i = 0; i < nodes.size(); i++) {
        const auto& node = nodes[i];
        
        if (node.isRoot) {
            if (isAClause(i)) {
                // A-clause: I = disjunction of shared literals
                std::vector<int> sharedLits;
                for (int lit : node.clause) {
                    if (isSharedVar(std::abs(lit))) {
                        sharedLits.push_back(lit);
                    }
                }
                if (sharedLits.empty()) {
                    nodeInterpolants[i] = {};  // represents FALSE
                } else {
                    nodeInterpolants[i] = {sharedLits};
                }
            } else {
                // B-clause: I = conjunction of negated shared literals
                std::vector<std::vector<int>> cnf;
                for (int lit : node.clause) {
                    if (isSharedVar(std::abs(lit))) {
                        cnf.push_back({-lit});
                    }
                }
                nodeInterpolants[i] = cnf;  // empty = TRUE
            }
        } else {
            // Resolution: Res(C1 ∨ x, C2 ∨ ¬x) with interpolants I1, I2
            int id1 = node.chainIds[0];
            auto result = nodeInterpolants[id1];
            
            for (size_t j = 0; j < node.chainVars.size(); j++) {
                int var = node.chainVars[j] + 1;
                int id2 = node.chainIds[j + 1];
                const auto& i2 = nodeInterpolants[id2];
                
                if (isSharedVar(var)) {
                    // Shared: I = (x ∨ I1) ∧ (¬x ∨ I2)
                    // In CNF: add x to each clause of I1, add ¬x to each clause of I2
                    std::vector<std::vector<int>> newResult;
                    for (auto clause : result) {
                        clause.push_back(var);
                        newResult.push_back(clause);
                    }
                    for (auto clause : i2) {
                        clause.push_back(-var);
                        newResult.push_back(clause);
                    }
                    if (result.empty()) {
                        newResult.push_back({var});  // FALSE ∨ x = x
                    }
                    if (i2.empty()) {
                        newResult.push_back({-var}); // TRUE needs ¬x clause
                    }
                    result = newResult;
                } else {
                    // A-local or B-local: I = I1 ∨ I2 or I = I1 ∧ I2
                    // For simplicity, use AND (conjunction = union of CNF)
                    for (const auto& clause : i2) {
                        result.push_back(clause);
                    }
                }
            }
            nodeInterpolants[i] = result;
        }
    }
    
    return nodeInterpolants.back();
}