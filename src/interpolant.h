#ifndef INTERPOLANT_H
#define INTERPOLANT_H

#include "proof_parser.h"
#include <vector>
#include <set>

class Interpolator {
public:
    Interpolator(const ProofParser& proof,
                 const std::vector<std::vector<int>>& aPartClauses,
                 const std::vector<std::vector<int>>& bPartClauses,
                 const std::set<int>& sharedVars);
    
    // Returns interpolant as CNF (vector of clauses)
    std::vector<std::vector<int>> computeInterpolant();

private:
    const ProofParser& proof;
    const std::vector<std::vector<int>>& aPartClauses;
    const std::vector<std::vector<int>>& bPartClauses;
    std::set<int> sharedVars;
    std::set<int> aVars;
    std::set<int> aLocalVars;
    
    // Interpolant for each proof node (as CNF clause indices)
    std::vector<std::vector<std::vector<int>>> nodeInterpolants;
    
    bool isAClause(int nodeId);
    bool isALocal(int var);
    bool isSharedVar(int var);
};

#endif