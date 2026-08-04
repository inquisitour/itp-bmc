#include <iostream>
#include <set>
#include <vector>
#include <string>
#include <cstdlib>
#include "aiger_parser.h"
#include "model_checker.h"
#include "proof_parser.h"
#include "interpolant.h"
#include "cnf_generator.h"

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

    // TEMPORARY TEST 2 — non-trivial interpolant expected
    if (argc == 2 && std::string(argv[1]) == "--test-interp2") {
        std::string cnf_path    = "/tmp/test_interp2.cnf";
        std::string proof_path  = "/tmp/adiag/proof.txt";
        std::string result_path = "/tmp/adiag/result.txt";
        std::string minisat     = "/tmp/adiag/minisatp/minisat";

        std::string cmd = minisat + " " + cnf_path + " -r " + result_path
                        + " -p " + proof_path + " > /dev/null 2>&1";
        system(cmd.c_str());

        ProofParser proof;
        proof.parse(proof_path);

        std::vector<std::vector<int>> aPartClauses = {{-1,2},{1,3}};
        std::vector<std::vector<int>> bPartClauses = {{-2,4},{-3,4},{-4}};
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
    // END TEMPORARY TEST 2

    // TEMPORARY TEST 3 — exercise generateIMC
    if (argc == 3 && std::string(argv[1]) == "--test-imc") {
        AIG aig;
        if (!parseAiger(argv[2], aig)) return 1;

        std::cout << "latches=" << aig.numLatches << std::endl;

        // Pretend the previous iteration gave us: (latch0 ∨ ¬latch1)
        std::vector<std::vector<std::pair<int,bool>>> accumulated = {
            { {0, false}, {1, true} }
        };

        CNFGenerator g(aig);
        g.generateIMC(1, accumulated);

        int aSize = g.getAPartSize();
        const auto& cls = g.getClauses();
        auto l2c0 = g.getLatchIdxToCNF0();
        auto c2l  = g.getCNFToLatchIdx();

        std::cout << "aPartSize=" << aSize
                  << " total=" << cls.size()
                  << " numVars=" << g.getNumVars() << std::endl;

        std::cout << "latch0 -> cnf0 var " << l2c0[0]
                  << " ; latch1 -> cnf0 var " << l2c0[1] << std::endl;
        std::cout << "cnfToLatchIdx size=" << c2l.size() << std::endl;

        // The approximation clause should be the LAST clause of the A-part
        std::cout << "last 3 A-part clauses:" << std::endl;
        for (int i = std::max(0, aSize - 3); i < aSize; i++) {
            std::cout << "  [" << i << "] ";
            for (int lit : cls[i]) std::cout << lit << " ";
            std::cout << std::endl;
        }
        return 0;
    }
    // END TEMPORARY TEST 3

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

