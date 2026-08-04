#include "cnf_generator.h"
#include <fstream>
#include <iostream>
#include <map>

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

std::map<int,int> CNFGenerator::getLatchIdxToCNF0() const {
    std::map<int,int> m;
    for (size_t i = 0; i < aig.latches.size(); i++) {
        unsigned v = AIG::lit2var(aig.latches[i].var);
        if ((int)varMap.size() > 0 && varMap[0][v] != 0)
            m[(int)i] = varMap[0][v];
    }
    return m;
}

std::map<int,int> CNFGenerator::getCNFToLatchIdx() const {
    std::map<int,int> m;
    for (size_t i = 0; i < aig.latches.size(); i++) {
        unsigned v = AIG::lit2var(aig.latches[i].var);
        if ((int)varMap.size() > 1 && varMap[1][v] != 0)
            m[varMap[1][v]] = (int)i;
    }
    return m;
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
    for (const auto& latch : aig.latches) {
        int var = getCNFVar(latch.var, 0);
        if (latch.reset == 0)
            addClause({-var});          // starts low
        else if (latch.reset == 1)
            addClause({var});           // starts high
        // otherwise: unconstrained, emit nothing
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

    encodeInit();
    if (k >= 1) encodeTransition(0);
    // constraints at t=0 and t=1 → part of A
    for (unsigned c : aig.constraints) {
        addClause({getCNFVar(c, 0)});
        if (k >= 1) addClause({getCNFVar(c, 1)});
    }
    aPartClauses = (int)clauses.size();

    // Transitions AND gates for timeframes 1..k-1
    for (int t = 1; t < k; t++) {
        encodeTransition(t);  // encodes ANDs + latch next-state for time t
    }

    // AND gates at final timeframe k
    for (const auto& gate : aig.ands) {
        encodeAnd(gate, k);
    }

    // constraints at t=2..k → part of B
    for (int t = 2; t <= k; t++)
        for (unsigned c : aig.constraints)
            addClause({getCNFVar(c, t)});

    // Bad state only at final timeframe k
    if (k > skip) {
        std::vector<int> badClause;
        for (const auto& out : aig.outputs) {
            badClause.push_back(getCNFVar(out, k));
        }
        if (!badClause.empty())
            addClause(badClause);
    }
}

void CNFGenerator::generateIMC(int k,
    const std::vector<std::vector<std::pair<int,bool>>>& prevApprox)
{
    clauses.clear();
    varMap.clear();
    nextVar = 1;
    aPartClauses = 0;

    // Allocate every latch's t=0 var FIRST, before sel and before the
    // transition, so init / prevApprox / T all refer to the same variables.
    for (const auto& latch : aig.latches)
        getCNFVar(latch.var, 0);

    // Encode transition T(s0,s1) — establishes t=0 and t=1 CNF var IDs
    encodeTransition(0);

    // Auxiliary selector variable: sel=0 means init, sel=1 means prevApprox
    int sel = nextVar++;

    // Init branch: ¬sel → latch_i = 0 for all latches
    for (const auto& latch : aig.latches) {
        int var = getCNFVar(latch.var, 0);
        // sel ∨ ¬latch_i  (if ¬sel, then latch_i must be 0)
        addClause({sel, -var});
    }

    // prevApprox branch: sel → each clause of prevApprox
    auto latchToCNF0 = getLatchIdxToCNF0();
    for (const auto& latchClause : prevApprox) {
        std::vector<int> cnfClause;
        cnfClause.push_back(-sel); // ¬sel → skip this clause
        bool valid = true;
        for (auto [idx, neg] : latchClause) {
            auto it = latchToCNF0.find(idx);
            if (it == latchToCNF0.end()) { valid = false; break; }
            cnfClause.push_back(neg ? -(it->second) : it->second);
        }
        if (valid && cnfClause.size() > 1)
            addClause(cnfClause);
    }

    // constraints at t=0 and t=1 → part of A
    for (unsigned c : aig.constraints) {
        addClause({getCNFVar(c, 0)});
        addClause({getCNFVar(c, 1)});
    }

    aPartClauses = (int)clauses.size(); // A = (init ∨ prevApprox) ∧ T(s0,s1)

    // B: remaining transitions t=1..k-1
    for (int t = 1; t < k; t++)
        encodeTransition(t);

    // AND gates at final timeframe k
    for (const auto& gate : aig.ands)
        encodeAnd(gate, k);

    // constraints at t=2..k → part of B
    for (int t = 2; t <= k; t++)
        for (unsigned c : aig.constraints)
            addClause({getCNFVar(c, t)});

    // Bad at final timeframe k
    std::vector<int> badClause;
    for (const auto& out : aig.outputs)
        badClause.push_back(getCNFVar(out, k));
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