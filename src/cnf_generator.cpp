#include "cnf_generator.h"
#include <fstream>
#include <iostream>

CNFGenerator::CNFGenerator(const AIG& aig) : aig(aig), nextVar(1), aPartClauses(0) {}

int CNFGenerator::getCNFVar(unsigned aigLit, int time) {
    unsigned var = AIG::lit2var(aigLit);
    
    // Expand varMap if needed
    while ((int)varMap.size() <= time) {
        varMap.push_back(std::vector<int>(aig.maxVar + 1, 0));
    }
    
    if (varMap[time][var] == 0) {
        varMap[time][var] = nextVar++;
        // AIG variable 0 is constant FALSE — force it
        if (var == 0) {
            addClause({-varMap[time][var]});
        }
    }
    
    int cnfVar = varMap[time][var];
    return AIG::isNegated(aigLit) ? -cnfVar : cnfVar;
}

std::vector<int> CNFGenerator::getLatchCNFVars(int t) const {
    std::vector<int> vars;
    for (const auto& latch : aig.latches) {
        unsigned v = AIG::lit2var(latch.var);
        if ((int)varMap.size() > t && varMap[t][v] != 0)
            vars.push_back(varMap[t][v]);
    }
    return vars;
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

void CNFGenerator::generateBMC(int k, int skip) {
    clauses.clear();
    varMap.clear();
    nextVar = 1;
    aPartClauses = 0;

    // Initial state
    encodeInit();
    if (k >= 1) encodeTransition(0);
    aPartClauses = (int)clauses.size();  // A = init + T(s0,s1)

    // Transitions AND gates for timeframes 1..k-1
    for (int t = 1; t < k; t++) {
        encodeTransition(t);  // encodes ANDs + latch next-state for time t
    }

    // AND gates at final timeframe k
    for (const auto& gate : aig.ands) {
        encodeAnd(gate, k);
    }

    // Bad state only from skip+1 onwards
    std::vector<int> badClause;
    for (int t = skip + 1; t <= k; t++) {
        for (const auto& out : aig.outputs) {
            badClause.push_back(getCNFVar(out, t));
        }
    }
    if (!badClause.empty())
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