#include "proof_parser.h"
#include <cstdio>
#include <ostream>
#include <iostream>

uint64_t ProofParser::getUInt(FILE* f) {
    int byte0 = fgetc(f);
    if (byte0 == EOF) return 0;
    if (!(byte0 & 0x80))
        return (uint64_t)byte0;

    unsigned b0 = (unsigned)byte0;
    switch ((b0 & 0x60) >> 5) {
        case 0: {
            unsigned b1 = (unsigned)fgetc(f);
            return ((b0 & 0x1F) << 8) | b1;
        }
        case 1: {
            unsigned b1 = (unsigned)fgetc(f);
            unsigned b2 = (unsigned)fgetc(f);
            return ((b0 & 0x1F) << 16) | (b1 << 8) | b2;
        }
        case 2: {
            unsigned b1 = (unsigned)fgetc(f);
            unsigned b2 = (unsigned)fgetc(f);
            unsigned b3 = (unsigned)fgetc(f);
            return ((b0 & 0x1F) << 24) | (b1 << 16) | (b2 << 8) | b3;
        }
        default: {
            unsigned c0 = (unsigned)fgetc(f);
            unsigned c1 = (unsigned)fgetc(f);
            unsigned c2 = (unsigned)fgetc(f);
            unsigned c3 = (unsigned)fgetc(f);
            unsigned c4 = (unsigned)fgetc(f);
            unsigned c5 = (unsigned)fgetc(f);
            unsigned c6 = (unsigned)fgetc(f);
            uint64_t hi = ((uint64_t)c0 << 24) | (c1 << 16) | (c2 << 8) | c3;
            uint64_t lo = ((uint64_t)c4 << 24) | (c5 << 16) | (c6 << 8) | (unsigned)fgetc(f);
            return (hi << 32) | lo;
        }
    }
}

bool ProofParser::parse(const std::string& filename) {
    FILE* f = fopen(filename.c_str(), "rb");
    if (!f) return false;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::cerr << "DEBUG proof file size=" << fsize << " bytes" << std::endl;

    nodes.clear();
    int id = 0;
    int clauseId = 0;

    while (true) {
        int firstByte = fgetc(f);
        if (firstByte == EOF) {
            std::cerr << "DEBUG parse ended: natural EOF at ftell=" << ftell(f)
                       << " id=" << id << " nodes=" << nodes.size() << std::endl;
            break;
        }
        ungetc(firstByte, f);

        uint64_t tmp = getUInt(f);

        ProofNode node;

        if ((tmp & 1) == 0) {
            node.isRoot = true;
            node.clauseIdx = clauseId++;
            int idx = (int)(tmp >> 1);
            int lit = (idx >> 1) + 1;
            if (idx & 1) lit = -lit;
            node.clause.push_back(lit);

            while (true) {
                uint64_t delta = getUInt(f);
                if (delta == 0) break;
                idx += (int)delta;
                lit = (idx >> 1) + 1;
                if (idx & 1) lit = -lit;
                node.clause.push_back(lit);
            }
        } else {
            node.isRoot = false;
            int idDelta = (int)(tmp >> 1);
            if (idDelta > id) {
                std::cerr << "DEBUG parse ended: sanity-check break"
                           << " id=" << id << " idDelta=" << idDelta
                           << " tmp=" << tmp << " ftell=" << ftell(f)
                           << " fsize=" << fsize
                           << " nodes=" << nodes.size() << std::endl;
                break;
            }
            node.chainIds.push_back(id - idDelta);

            while (true) {
                uint64_t v = getUInt(f);
                if (v == 0) break;
                node.chainVars.push_back((int)(v - 1));
                uint64_t delta = getUInt(f);
                if ((int)delta > id) {
                    std::cerr << "DEBUG inner-loop break (chain truncated)"
                               << " id=" << id << " delta=" << delta
                               << " ftell=" << ftell(f) << std::endl;
                    break;
                }
                node.chainIds.push_back(id - (int)delta);
            }

            if (node.chainVars.empty()) {
                continue;
            }
        }

        if (!node.isRoot && node.chainVars.size() + 1 != node.chainIds.size()) {
            std::cerr << "DEBUG MISMATCH id=" << id
                    << " chainIds=" << node.chainIds.size()
                    << " chainVars=" << node.chainVars.size() << std::endl;
        }

        nodes.push_back(node);
        id++;
    }

    fclose(f);
    return true;
}