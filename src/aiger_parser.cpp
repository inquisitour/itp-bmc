#include "aiger_parser.h"
extern "C" {
#include "../aiger-1.9.9/aiger.h"
}
#include <iostream>

bool parseAiger(const std::string& filename, AIG& aig) {
    aiger* a = aiger_init();

    const char* err = aiger_open_and_read_from_file(a, filename.c_str());
    if (err) {
        std::cerr << "AIGER parse error: " << err << std::endl;
        aiger_reset(a);
        return false;
    }

    aig.maxVar    = a->maxvar;
    aig.numInputs = a->num_inputs;
    aig.numLatches = a->num_latches;
    aig.numOutputs = a->num_outputs;
    aig.numAnds   = a->num_ands;

    // Inputs
    for (unsigned i = 0; i < a->num_inputs; i++)
        aig.inputs.push_back(a->inputs[i].lit);

    // Latches
    for (unsigned i = 0; i < a->num_latches; i++) {
        Latch l;
        l.var  = a->latches[i].lit;
        l.next = a->latches[i].next;
        aig.latches.push_back(l);
    }

    // Standard outputs
    for (unsigned i = 0; i < a->num_outputs; i++)
        aig.outputs.push_back(a->outputs[i].lit);

    // Bad properties — treat as additional outputs for safety checking
    for (unsigned i = 0; i < a->num_bad; i++)
        aig.outputs.push_back(a->bad[i].lit);

    // AND gates
    for (unsigned i = 0; i < a->num_ands; i++) {
        AndGate g;
        g.out = a->ands[i].lhs;
        g.in0 = a->ands[i].rhs0;
        g.in1 = a->ands[i].rhs1;
        aig.ands.push_back(g);
    }

    aiger_reset(a);
    return true;
}