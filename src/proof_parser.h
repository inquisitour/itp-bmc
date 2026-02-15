#ifndef PROOF_PARSER_H
#define PROOF_PARSER_H

#include <vector>
#include <string>
#include <cstdint>

struct ProofNode {
    bool isRoot;
    std::vector<int> clause;      // For root nodes
    std::vector<int> chainIds;    // For chain nodes: clause IDs
    std::vector<int> chainVars;   // For chain nodes: pivot variables
};

class ProofParser {
public:
    bool parse(const std::string& filename);
    const std::vector<ProofNode>& getNodes() const { return nodes; }
    
private:
    std::vector<ProofNode> nodes;
    uint64_t getUInt(FILE* f);
};

#endif