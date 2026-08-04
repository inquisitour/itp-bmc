#include "interpolant.h"
#include <algorithm>
#include <iostream>
#include <ostream>

// We represent interpolants as CNF (vector of clauses).
// Convention:
//   TRUE  = empty clause list: {}
//   FALSE = list containing one empty clause: {{}}
//
// This matches the Huang/Krajicek/Pudlak system from the lecture:
//   A-clause base: I = FALSE = {{}}
//   B-clause base: I = TRUE  = {}
//   Pivot shared:  I = I1 OR  I2  (resolvent of two CNFs on the pivot)
//   Pivot local:   I = I1 AND I2  (concatenation of two CNFs)

Interpolator::Interpolator(const ProofParser& proof,
                           const std::vector<std::vector<int>>& aPartClauses,
                           const std::vector<std::vector<int>>& bPartClauses,
                           const std::set<int>& sharedVars)
    : proof(proof), aPartClauses(aPartClauses), bPartClauses(bPartClauses), sharedVars(sharedVars) {
    
    std::set<int> aVars, bVars;
    for (const auto& c : aPartClauses)
        for (int lit : c) aVars.insert(std::abs(lit));
    for (const auto& c : bPartClauses)
        for (int lit : c) bVars.insert(std::abs(lit));
    
    // A-local = appears in A but NOT in B
    for (int v : aVars)
        if (!bVars.count(v))
            aLocalVars.insert(v);
}

bool Interpolator::isAClause(int nodeId) {
    const auto& node = proof.getNodes()[nodeId];
    if (!node.isRoot) return false;
    
    // Match root clause literals against A-part clauses
    // Sort both for comparison
    std::vector<int> nodeLits = node.clause;
    std::sort(nodeLits.begin(), nodeLits.end());
    
    for (const auto& aClause : aPartClauses) {
        std::vector<int> ac = aClause;
        std::sort(ac.begin(), ac.end());
        if (ac == nodeLits) return true;
    }
    return false;
}

bool Interpolator::isSharedVar(int var) {
    return sharedVars.count(var) > 0;
}

bool Interpolator::isALocal(int var) {
    return !isSharedVar(var);
}

// CNF OR: (I1 ∨ I2) — resolvent of two CNFs on a shared pivot variable x
// For each pair of clauses (c1 from I1, c2 from I2), produce c1 ∪ c2
// Special cases: TRUE ∨ anything = TRUE, FALSE ∨ I = I
static std::vector<std::vector<int>> cnfOr(
    const std::vector<std::vector<int>>& i1,
    const std::vector<std::vector<int>>& i2)
{
    // TRUE OR anything = TRUE
    if (i1.empty()) return {};
    if (i2.empty()) return {};

    // FALSE = {{}} — FALSE OR I = I
    if (i1.size() == 1 && i1[0].empty()) return i2;
    if (i2.size() == 1 && i2[0].empty()) return i1;

    std::vector<std::vector<int>> result;
    for (const auto& c1 : i1) {
        for (const auto& c2 : i2) {
            std::vector<int> merged = c1;
            for (int lit : c2) {
                if (std::find(merged.begin(), merged.end(), lit) == merged.end())
                    merged.push_back(lit);
            }
            result.push_back(merged);
        }
    }
    return result;
}

// CNF AND: (I1 ∧ I2) — concatenation of two CNFs
static std::vector<std::vector<int>> cnfAnd(
    const std::vector<std::vector<int>>& i1,
    const std::vector<std::vector<int>>& i2)
{
    // FALSE AND anything = FALSE
    if (i1.size() == 1 && i1[0].empty()) return i1;
    if (i2.size() == 1 && i2[0].empty()) return i2;

    std::vector<std::vector<int>> result = i1;
    result.insert(result.end(), i2.begin(), i2.end());
    return result;
}

std::vector<std::vector<int>> Interpolator::computeInterpolant() {
    const auto& nodes = proof.getNodes();
    if (nodes.empty()) return {};

    nodeInterpolants.resize(nodes.size());

    const std::vector<std::vector<int>> FALSE_CNF = {{}};
    const std::vector<std::vector<int>> TRUE_CNF  = {};

    // DEBUG
    int rootCount = 0, aCount = 0;
    for (size_t i = 0; i < nodes.size(); i++) {
        if (nodes[i].isRoot) {
            rootCount++;
            if (isAClause(i)) aCount++;
            if (rootCount <= 100)
                std::cerr << "DEBUG root " << i << " clauseIdx=" << nodes[i].clauseIdx
                          << " isA=" << isAClause(i) << std::endl;
        }
    }
    std::cerr << "DEBUG total roots=" << rootCount << " A-clauses=" << aCount << std::endl;
    int chainCount = 0;
    for (size_t i = 0; i < nodes.size(); i++)
        if (!nodes[i].isRoot) chainCount++;
    std::cerr << "DEBUG chains=" << chainCount << " roots=" << rootCount << std::endl;
    // END DEBUG

    for (size_t i = 0; i < nodes.size(); i++) {
        const auto& node = nodes[i];

        if (node.isRoot) {
            if (isAClause((int)i)) {
                nodeInterpolants[i] = FALSE_CNF;
            } else {
                // B-clause: restrict to shared variables
                std::vector<int> sharedLits;
                for (int lit : node.clause)
                    if (sharedVars.count(std::abs(lit)))
                        sharedLits.push_back(lit);
                // Empty sharedLits means no shared vars in this clause → TRUE
                if (sharedLits.empty())
                    nodeInterpolants[i] = TRUE_CNF;  // {}
                else
                    nodeInterpolants[i] = {sharedLits};  // one clause
                if (i == 41) { std::cerr << "DEBUG node41 clause: "; for (int lit : node.clause) std::cerr << lit << " "; std::cerr << " sharedLits=" << sharedLits.size() << std::endl; }
                if (i == 105) { std::cerr << "DEBUG node105 clause: "; for (int lit : node.clause) std::cerr << lit << " "; std::cerr << " sharedLits=" << sharedLits.size() << std::endl; }
            }
            // DEBUG node 25
            if (i == 25) {
                std::cerr << "DEBUG node25 clause: ";
                for (int lit : nodes[i].clause) std::cerr << lit << " ";
                std::cerr << std::endl;
            }
            // END DEBUG
        } else {
            if (node.chainIds.empty()) {
                nodeInterpolants[i] = TRUE_CNF;
                continue;
            }

            int id1 = node.chainIds[0];

            // DEBUG chain start
            std::cerr << "DEBUG chain " << i << " starts_from=" << id1
                      << " id1_interp_size=" << nodeInterpolants[id1].size();
            if (!nodeInterpolants[id1].empty())
                std::cerr << " id1_clause0_size=" << nodeInterpolants[id1][0].size();
            if (!node.chainVars.empty())
                std::cerr << " pivot0=" << (node.chainVars[0]+1)
                          << " aLocal=" << aLocalVars.count(node.chainVars[0]+1);
            std::cerr << std::endl;
            // END DEBUG chain start

            if (id1 < 0 || id1 >= (int)nodeInterpolants.size()) {
                nodeInterpolants[i] = TRUE_CNF;
                continue;
            }

            auto result = nodeInterpolants[id1];

            for (size_t j = 0; j < node.chainVars.size(); j++) {
                if (j + 1 >= node.chainIds.size()) break;

                int var = node.chainVars[j] + 1;
                int id2 = node.chainIds[j + 1];
                if (i == 26)
                    std::cerr << "DEBUG chain26 step " << j << " var=" << var 
                            << " aLocal=" << aLocalVars.count(var)
                            << " id2=" << id2 
                            << " id2_isRoot=" << nodes[id2].isRoot
                            << " id2_isA=" << (nodes[id2].isRoot ? isAClause(id2) : -1)
                            << " id2_interp_size=" << nodeInterpolants[id2].size()
                            << std::endl;

                if (id2 < 0 || id2 >= (int)nodeInterpolants.size()) continue;

                const auto& i2 = nodeInterpolants[id2];

                if (i == 40 || i == 42 || i == 78 || i == 106)
                    std::cerr << "DEBUG chain42 step j=" << j
                            << " var=" << var
                            << " aLocal=" << aLocalVars.count(var)
                            << " id2=" << id2
                            << " id2_interp_size=" << i2.size()
                            << (i2.size()==1 && i2[0].empty() ? " (FALSE)" : (i2.empty() ? " (TRUE)" : " (other)"))
                            << std::endl;

                // McMillan system: always OR
                result = cnfOr(result, i2);
            }

            nodeInterpolants[i] = result;

            if (i == 6) {
                std::cerr << "DEBUG chain6 result: ";
                for (const auto& c : nodeInterpolants[i])
                    for (int lit : c) std::cerr << lit << " ";
                std::cerr << std::endl;
            }

            // DEBUG chain result
            std::cerr << "DEBUG chain " << i
                      << " result_size=" << nodeInterpolants[i].size();
            if (!nodeInterpolants[i].empty())
                std::cerr << " clause0_size=" << nodeInterpolants[i][0].size();
            std::cerr << std::endl;
            // END DEBUG chain result
        }
    }

    std::cerr << "DEBUG final interpolant size=" << nodeInterpolants.back().size() << std::endl;
    if (!nodeInterpolants.back().empty())
        std::cerr << "DEBUG final clause[0] size=" << nodeInterpolants.back()[0].size() << std::endl;

    return nodeInterpolants.back();
}