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

bool Interpolator::isALocal(int var) {
    return !isSharedVar(var);
}

std::vector<std::vector<int>> Interpolator::computeInterpolant() {
    const auto& nodes = proof.getNodes();
    
    if (nodes.empty()) return {};
    
    nodeInterpolants.resize(nodes.size());
    
    for (size_t i = 0; i < nodes.size(); i++) {
        const auto& node = nodes[i];
        
        if (node.isRoot) {
            if (isAClause(i)) {
                std::vector<int> sharedLits;
                for (int lit : node.clause) {
                    if (isSharedVar(std::abs(lit))) {
                        sharedLits.push_back(lit);
                    }
                }
                if (sharedLits.empty()) {
                    nodeInterpolants[i] = {};
                } else {
                    nodeInterpolants[i] = {sharedLits};
                }
            } else {
                std::vector<std::vector<int>> cnf;
                for (int lit : node.clause) {
                    if (isSharedVar(std::abs(lit))) {
                        cnf.push_back({-lit});
                    }
                }
                nodeInterpolants[i] = cnf;
            }
        } else {
            // Bounds check for chain IDs
            if (node.chainIds.empty()) {
                nodeInterpolants[i] = {};
                continue;
            }
            
            int id1 = node.chainIds[0];
            if (id1 < 0 || id1 >= (int)nodeInterpolants.size()) {
                nodeInterpolants[i] = {};
                continue;
            }
            
            auto result = nodeInterpolants[id1];
            
            for (size_t j = 0; j < node.chainVars.size(); j++) {
                if (j + 1 >= node.chainIds.size()) break;
                
                int var = node.chainVars[j] + 1;
                int id2 = node.chainIds[j + 1];
                
                if (id2 < 0 || id2 >= (int)nodeInterpolants.size()) continue;
                
                const auto& i2 = nodeInterpolants[id2];
                
                if (isSharedVar(var)) {
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
                        newResult.push_back({var});
                    }
                    if (i2.empty()) {
                        newResult.push_back({-var});
                    }
                    result = newResult;
                } else {
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