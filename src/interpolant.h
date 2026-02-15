#ifndef INTERPOLANT_H
#define INTERPOLANT_H

#include "proof_parser.h"
#include <vector>
#include <set>

class Interpolator {
public:
    // A-part: clauses 0..splitPoint-1, B-part: splitPoint..end
    // sharedVars: variables appearing in both A and B
    Interpolator(const ProofParser& proof, int splitPoint, 
                 const std::set<int>& sharedVars);
    
    // Returns interpolant as CNF (vector of clauses)
    std::vector<std::vector<int>> computeInterpolant();

private:
    const ProofParser& proof;
    int splitPoint;
    std::set<int> sharedVars;
    
    // Interpolant for each proof node (as CNF clause indices)
    std::vector<std::vector<std::vector<int>>> nodeInterpolants;
    
    bool isAClause(int nodeId);
    bool isSharedVar(int var);
};

#endif