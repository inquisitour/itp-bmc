#ifndef MODEL_CHECKER_H
#define MODEL_CHECKER_H

#include "aiger_parser.h"
#include <vector>

class ModelChecker {
public:
    ModelChecker(const AIG& aig);
    
    // Returns true if SAFE, false if FAIL
    bool check(int maxBound, int skip = 0);
    
private:
    const AIG& aig;
    std::vector<std::vector<int>> reachable;  // over-approximation of reachable states
    
    bool runBMC(int k, bool& foundCex, int skip = 0);
};

#endif