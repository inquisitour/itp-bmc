#include "cnf_generator.h"
#include <fstream>
#include <iostream>
#include <map>
#include <algorithm>

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
    // constraints at t=0 → part of A
    for (unsigned c : aig.constraints)
        addClause({getCNFVar(c, 0)});
    aPartClauses = (int)clauses.size();

    // Transitions AND gates for timeframes 1..k-1
    for (int t = 1; t < k; t++) {
        encodeTransition(t);  // encodes ANDs + latch next-state for time t
    }

    // AND gates at final timeframe k
    for (const auto& gate : aig.ands) {
        encodeAnd(gate, k);
    }

    // constraints at t=1..k → part of B
    for (int t = 1; t <= k; t++)
        for (unsigned c : aig.constraints)
            addClause({getCNFVar(c, t)});

    // Bad at ANY frame t in (skip, k]
    if (k > skip) {
        std::vector<int> badClause;
        for (int t = std::max(1, skip + 1); t <= k; t++)
            for (const auto& out : aig.outputs)
                badClause.push_back(getCNFVar(out, t));
        if (!badClause.empty())
            addClause(badClause);
    }
}

void CNFGenerator::generateInitBad() {
    clauses.clear();
    varMap.clear();
    nextVar = 1;
    aPartClauses = 0;
    encodeInit();
    for (const auto& gate : aig.ands)
        encodeAnd(gate, 0);
    for (unsigned c : aig.constraints)
        addClause({getCNFVar(c, 0)});
    std::vector<int> bad;
    for (const auto& out : aig.outputs)
        bad.push_back(getCNFVar(out, 0));
    if (!bad.empty())
        addClause(bad);
}

void CNFGenerator::generateIMC(int k, const std::vector<LatchCNF>& disjuncts)
{
    clauses.clear();
    varMap.clear();
    nextVar = 1;
    aPartClauses = 0;

    // Every latch's t=0 var first, so init / approximation / T share variables
    for (const auto& latch : aig.latches)
        getCNFVar(latch.var, 0);

    encodeTransition(0);

    auto latchToCNF0 = getLatchIdxToCNF0();

    // One selector per disjunct: sel[0] = init, sel[j+1] = disjuncts[j]
    std::vector<int> sel;
    for (size_t j = 0; j <= disjuncts.size(); j++)
        sel.push_back(nextVar++);

    // init OR F1 OR ... OR Fm
    addClause(sel);

    // sel[0] -> init (only latches with a defined reset)
    for (const auto& latch : aig.latches) {
        int var = getCNFVar(latch.var, 0);
        if (latch.reset == 0)      addClause({-sel[0], -var});
        else if (latch.reset == 1) addClause({-sel[0], var});
    }

    // sel[j+1] -> every clause of disjuncts[j]
    for (size_t j = 0; j < disjuncts.size(); j++) {
        for (const auto& latchClause : disjuncts[j]) {
            std::vector<int> c = {-sel[j + 1]};
            for (auto [idx, neg] : latchClause) {
                int v = latchToCNF0.at(idx);
                c.push_back(neg ? -v : v);
            }
            addClause(c);
        }
    }

    // constraints at t=0 -> part of A
    for (unsigned c : aig.constraints)
        addClause({getCNFVar(c, 0)});

    aPartClauses = (int)clauses.size();

    for (int t = 1; t < k; t++)
        encodeTransition(t);

    for (const auto& gate : aig.ands)
        encodeAnd(gate, k);

    // constraints at t=1..k -> part of B
    for (int t = 1; t <= k; t++)
        for (unsigned c : aig.constraints)
            addClause({getCNFVar(c, t)});

    std::vector<int> badClause;
    for (int t = 1; t <= k; t++)
        for (const auto& out : aig.outputs)
            badClause.push_back(getCNFVar(out, t));
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