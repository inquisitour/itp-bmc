#include "aiger_parser.h"
#include <fstream>
#include <sstream>
#include <iostream>

bool parseAiger(const std::string& filename, AIG& aig) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Cannot open file: " << filename << std::endl;
        return false;
    }
    
    std::string header;
    file >> header;
    if (header != "aag") {
        std::cerr << "Not an ASCII AIGER file" << std::endl;
        return false;
    }
    
    file >> aig.maxVar >> aig.numInputs >> aig.numLatches 
         >> aig.numOutputs >> aig.numAnds;
    
    // Read inputs
    aig.inputs.resize(aig.numInputs);
    for (unsigned i = 0; i < aig.numInputs; i++) {
        file >> aig.inputs[i];
    }
    
    // Read latches
    aig.latches.resize(aig.numLatches);
    for (unsigned i = 0; i < aig.numLatches; i++) {
        file >> aig.latches[i].var >> aig.latches[i].next;
    }
    
    // Read outputs (bad state literals)
    aig.outputs.resize(aig.numOutputs);
    for (unsigned i = 0; i < aig.numOutputs; i++) {
        file >> aig.outputs[i];
    }
    
    // Read AND gates
    aig.ands.resize(aig.numAnds);
    for (unsigned i = 0; i < aig.numAnds; i++) {
        file >> aig.ands[i].out >> aig.ands[i].in0 >> aig.ands[i].in1;
    }
    
    return true;
}