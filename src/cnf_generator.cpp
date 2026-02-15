#include "cnf_generator.h"
#include <fstream>
#include <iostream>

CNFGenerator::CNFGenerator(const AIG& aig) : aig(aig), nextVar(1) {}

int CNFGenerator::getCNFVar(unsigned aigLit, int time) {
    unsigned var = AIG::lit2var(aigLit);
    
    // Expand varMap if needed
    while ((int)varMap.size() <= time) {
        varMap.push_back(std::vector<int>(aig.maxVar + 1, 0));
    }
    
    if (varMap[time][var] == 0) {
        varMap[time][var] = nextVar++;
    }
    
    int cnfVar = varMap[time][var];
    return AIG::isNegated(aigLit) ? -cnfVar : cnfVar;
}

void CNFGenerator::addClause(const std::vector<int>& clause) {
    clauses.push_back(clause);
}

void CNFGenerator::encodeAnd(const AndGate& gate, int t) {
    int out = getCNFVar(gate.out, t);
    int in0 = getCNFVar(gate.in0, t);
    int in1 = getCNFVar(gate.in1, t);
    
    // out <-> (in0 AND in1)
    // (out -> in0): -out OR in0
    addClause({-out, in0});
    // (out -> in1): -out OR in1
    addClause({-out, in1});
    // (in0 AND in1 -> out): -in0 OR -in1 OR out
    addClause({-in0, -in1, out});
}

void CNFGenerator::encodeInit() {
    // All latches start at 0
    for (const auto& latch : aig.latches) {
        int var = getCNFVar(latch.var, 0);
        addClause({-var});  // latch = false at time 0
    }
}

void CNFGenerator::encodeTransition(int t) {
    // Encode all AND gates at time t
    for (const auto& gate : aig.ands) {
        encodeAnd(gate, t);
    }
    
    // Latch next state: latch[t+1] = next[t]
    for (const auto& latch : aig.latches) {
        int curr = getCNFVar(latch.var, t + 1);
        int next = getCNFVar(latch.next, t);
        
        // curr <-> next
        addClause({-curr, next});
        addClause({curr, -next});
    }
}

void CNFGenerator::encodeBad(int t) {
    // Output literal is "bad state" detector
    // We want to check if bad is reachable
    int bad = getCNFVar(aig.outputs[0], t);
    addClause({bad});  // Assert bad state at time t
}

void CNFGenerator::generateBMC(int k) {
    clauses.clear();
    varMap.clear();
    nextVar = 1;
    
    // Initial state
    encodeInit();
    
    // Transitions for timeframes 0..k-1
    for (int t = 0; t < k; t++) {
        encodeTransition(t);
    }
    
    // Encode AND gates at final timeframe
    for (const auto& gate : aig.ands) {
        encodeAnd(gate, k);
    }
    
    // Bad state reachable at ANY timeframe 0..k
    // (bad_0 OR bad_1 OR ... OR bad_k)
    std::vector<int> badClause;
    for (int t = 0; t <= k; t++) {
        // Encode ANDs needed for output at time t
        if (t > 0) {
            for (const auto& gate : aig.ands) {
                encodeAnd(gate, t);
            }
        }
        badClause.push_back(getCNFVar(aig.outputs[0], t));
    }
    addClause(badClause);
}

void CNFGenerator::writeDIMACS(const std::string& filename) {
    std::ofstream file(filename);
    file << "p cnf " << getNumVars() << " " << clauses.size() << "\n";
    for (const auto& clause : clauses) {
        for (int lit : clause) {
            file << lit << " ";
        }
        file << "0\n";
    }
}