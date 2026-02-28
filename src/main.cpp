#include <iostream>
#include "aiger_parser.h"
#include "model_checker.h"

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <bound> <file.aag>" << std::endl;
        return 1;
    }
    
    int bound = std::stoi(argv[1]);
    std::string filename = argv[2];
    int skip = (argc >= 4) ? std::stoi(argv[3]) : 0;
    
    AIG aig;
    if (!parseAiger(filename, aig)) {
        return 1;
    }
    
    std::cout << "Model: " << aig.numInputs << " inputs, " 
              << aig.numLatches << " latches, "
              << aig.numAnds << " ANDs" << std::endl;
    
    ModelChecker mc(aig);
    bool safe = mc.check(bound, skip);
    
    std::cout << (safe ? "OK" : "FAIL") << std::endl;
    
    return safe ? 0 : 1;
}