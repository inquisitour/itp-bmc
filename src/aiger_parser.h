#ifndef AIGER_PARSER_H
#define AIGER_PARSER_H

#include <vector>
#include <string>
#include <cstdint>

struct Latch {
    unsigned var;       // latch variable (even number)
    unsigned next;      // next state literal
};

struct AndGate {
    unsigned out;       // output literal (even number)
    unsigned in0;       // input literal 0
    unsigned in1;       // input literal 1
};

struct AIG {
    unsigned maxVar;
    unsigned numInputs;
    unsigned numLatches;
    unsigned numOutputs;
    unsigned numAnds;
    
    std::vector<unsigned> inputs;
    std::vector<Latch> latches;
    std::vector<unsigned> outputs;  // bad state detectors (outputs + bad props)
    std::vector<AndGate> ands;
    
    // Helper: literal to variable
    static unsigned lit2var(unsigned lit) { return lit >> 1; }
    // Helper: is literal negated?
    static bool isNegated(unsigned lit) { return lit & 1; }
    // Helper: negate literal
    static unsigned negateLit(unsigned lit) { return lit ^ 1; }
};

bool parseAiger(const std::string& filename, AIG& aig);

#endif