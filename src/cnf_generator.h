#ifndef CNF_GENERATOR_H
#define CNF_GENERATOR_H

#include "aiger_parser.h"
#include <vector>
#include <map>

class CNFGenerator {
public:
    CNFGenerator(const AIG& aig);
    
    // Generate CNF for BMC up to bound k with optional skip for initial states
    void generateBMC(int k, int skip = 0);

    // IMC iteration: A = (init ∨ prevApprox) ∧ T(s0,s1), B = T(s1..sk) ∧ bad(sk)
    // prevApprox: clauses expressed as (latch_index, negated) pairs over s0
    void generateIMC(int k, const std::vector<std::vector<std::pair<int,bool>>>& prevApprox);
    
    // Get CNF in DIMACS format
    const std::vector<std::vector<int>>& getClauses() const { return clauses; }
    int getNumVars() const { return nextVar - 1; }
    int getAPartSize() const { return aPartClauses; }

    // CNF var IDs for latches at timeframe t
    std::vector<int> getLatchCNFVars(int t) const;

    // Latch index -> CNF var at t=0 (for interpolant renaming)
    std::map<int,int> getLatchIdxToCNF0() const;

    // CNF var at t=1 -> latch index (for interpolant extraction)
    std::map<int,int> getCNFToLatchIdx() const;
    
    // Write to DIMACS file
    void writeDIMACS(const std::string& filename);

    std::vector<std::vector<int>> getAPartClauses() const {
        return std::vector<std::vector<int>>(clauses.begin(), clauses.begin() + aPartClauses);
    }

    std::vector<std::vector<int>> getBPartClauses() const {
        return std::vector<std::vector<int>>(clauses.begin() + aPartClauses,
                                            clauses.end());
    }

private:
    const AIG& aig;
    std::vector<std::vector<int>> clauses;
    int nextVar;
    int aPartClauses;  // Number of clauses in A part (init + T(s0,s1))
    
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