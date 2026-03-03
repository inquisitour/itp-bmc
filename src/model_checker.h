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
    
    bool runBMC(int k, bool& foundCex, int& aPartSize, std::vector<int>& latchVars, int skip = 0);
};

#endif