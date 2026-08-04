#include <iostream>
#include <set>
#include <vector>
#include <string>
#include <cstdlib>
#include "aiger_parser.h"
#include "model_checker.h"
#include "proof_parser.h"
#include "interpolant.h"

int main(int argc, char* argv[]) {

    // TEMPORARY TEST
    if (argc == 2 && std::string(argv[1]) == "--test-interp") {
        std::string cnf_path = "/tmp/test_interp.cnf";
        std::string proof_path = "/tmp/adiag/proof.txt";
        std::string result_path = "/tmp/adiag/result.txt";
        std::string minisat = "/tmp/adiag/minisatp/minisat";
        
        std::string cmd = minisat + " " + cnf_path + " -r " + result_path + " -p " + proof_path + " > /dev/null 2>&1";
        system(cmd.c_str());
        
        ProofParser proof;
        proof.parse(proof_path);
        
        std::vector<std::vector<int>> aPartClauses = {{-1,2},{-1,3},{-2}};
        std::vector<std::vector<int>> bPartClauses = {{2,3},{2,4},{-4}};
        std::set<int> sharedVars = {2,3};
        
        Interpolator interp(proof, aPartClauses, bPartClauses, sharedVars);
        auto result = interp.computeInterpolant();
        
        std::cout << "Interpolant clauses: " << result.size() << std::endl;
        for (const auto& clause : result) {
            std::cout << "Clause: ";
            for (int lit : clause) std::cout << lit << " ";
            std::cout << std::endl;
        }
        return 0;
    }
    // END TEMPORARY TEST

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

