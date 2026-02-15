#include "proof_parser.h"
#include <cstdio>

uint64_t ProofParser::getUInt(FILE* f) {
    uint64_t val = 0;
    int shift = 0;
    int byte;
    do {
        byte = fgetc(f);
        if (byte == EOF) return 0;
        val |= (uint64_t)(byte & 0x7F) << shift;
        shift += 7;
    } while (byte & 0x80);
    return val;
}

bool ProofParser::parse(const std::string& filename) {
    FILE* f = fopen(filename.c_str(), "rb");
    if (!f) return false;
    
    nodes.clear();
    int id = 0;
    
    while (true) {
        int firstByte = fgetc(f);
        if (firstByte == EOF) break;
        ungetc(firstByte, f);
        
        uint64_t tmp = getUInt(f);
        
        ProofNode node;
        
        if ((tmp & 1) == 0) {
            // Root clause
            node.isRoot = true;
            int idx = tmp >> 1;
            if (idx > 0) {
                int lit = (idx >> 1) + 1;
                if (idx & 1) lit = -lit;
                node.clause.push_back(lit);
                
                while (true) {
                    uint64_t delta = getUInt(f);
                    if (delta == 0) break;
                    idx += delta;
                    lit = (idx >> 1) + 1;
                    if (idx & 1) lit = -lit;
                    node.clause.push_back(lit);
                }
            } else {
                // Empty clause case
                getUInt(f); // consume terminator
            }
        } else {
            // Chain
            node.isRoot = false;
            int idDelta = tmp >> 1;
            if (idDelta > id) break;  // sanity check
            node.chainIds.push_back(id - idDelta);
            
            while (true) {
                uint64_t v = getUInt(f);
                if (v == 0) break;
                node.chainVars.push_back(v - 1);
                uint64_t delta = getUInt(f);
                if ((int)delta > id) break;
                node.chainIds.push_back(id - delta);
            }
            
            if (node.chainVars.empty()) {
                // Deletion - don't add node, don't increment id
                continue;
            }
        }
        
        nodes.push_back(node);
        id++;
    }
    
    fclose(f);
    return true;
}