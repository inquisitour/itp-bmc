#ifndef CNF_GENERATOR_H
#define CNF_GENERATOR_H

#include "aiger_parser.h"
#include <vector>

class CNFGenerator {
public:
    CNFGenerator(const AIG& aig);
    
    // Generate CNF for BMC up to bound k with optional skip for initial states
    void generateBMC(int k, int skip = 0);
    
    // Get CNF in DIMACS format
    const std::vector<std::vector<int>>& getClauses() const { return clauses; }
    int getNumVars() const { return nextVar - 1; }
    
    // Write to DIMACS file
    void writeDIMACS(const std::string& filename);

private:
    const AIG& aig;
    std::vector<std::vector<int>> clauses;
    int nextVar;
    
    // Map: (original_lit, timeframe) -> CNF variable
    int getCNFVar(unsigned aigLit, int time);
    
    // Add clause
    void addClause(const std::vector<int>& clause);
    
    // Tseitin encoding for AND gate at timeframe t
    void encodeAnd(const AndGate& gate, int t);
    
    // Encode initial state (latches = 0)
    void encodeInit();
    
    // Encode transition at timeframe t -> t+1
    void encodeTransition(int t);
    
    // Encode bad state at timeframe t
    void encodeBad(int t);
    
    // Variable mapping storage
    std::vector<std::vector<int>> varMap; // [time][var] -> cnf_var
};

#endif