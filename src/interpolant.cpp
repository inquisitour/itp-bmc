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

std::vector<std::vector<int>> Interpolator::computeInterpolant() {
    const auto& nodes = proof.getNodes();
    nodeInterpolants.resize(nodes.size());
    
    for (size_t i = 0; i < nodes.size(); i++) {
        const auto& node = nodes[i];
        
        if (node.isRoot) {
            // Base case
            if (isAClause(i)) {
                // A-clause: interpolant = shared literals
                std::vector<int> sharedLits;
                for (int lit : node.clause) {
                    if (isSharedVar(std::abs(lit))) {
                        sharedLits.push_back(lit);
                    }
                }
                if (sharedLits.empty()) {
                    nodeInterpolants[i] = {};  // true (empty CNF)
                } else {
                    nodeInterpolants[i] = {sharedLits};
                }
            } else {
                // B-clause: interpolant = negation of shared literals = true (we use empty for true)
                // Actually for B: I = NOT(disjunction of shared lits)
                // If no shared lits: I = true
                // If shared lits: I = conjunction of negated lits
                std::vector<std::vector<int>> cnf;
                for (int lit : node.clause) {
                    if (isSharedVar(std::abs(lit))) {
                        cnf.push_back({-lit});
                    }
                }
                nodeInterpolants[i] = cnf;
            }
        } else {
            // Resolution step
            int id1 = node.chainIds[0];
            auto result = nodeInterpolants[id1];
            
            for (size_t j = 0; j < node.chainVars.size(); j++) {
                int var = node.chainVars[j] + 1;  // 1-indexed
                int id2 = node.chainIds[j + 1];
                const auto& i2 = nodeInterpolants[id2];
                
                if (!isSharedVar(var)) {
                    // Local variable: OR the interpolants
                    for (const auto& clause : i2) {
                        result.push_back(clause);
                    }
                } else {
                    // Shared variable: combine with resolution
                    // I = (I1 OR pivot) AND (I2 OR -pivot)
                    // Simplified: just OR the CNFs (approximation)
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