#include "interpolant.h"
#include <algorithm>

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

Interpolator::Interpolator(const ProofParser& proof, int splitPoint,
                           const std::set<int>& sharedVars)
    : proof(proof), splitPoint(splitPoint), sharedVars(sharedVars) {}

bool Interpolator::isAClause(int nodeId) {
    return nodeId < splitPoint;
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

    // FALSE sentinel
    const std::vector<std::vector<int>> FALSE_CNF = {{}};
    // TRUE sentinel
    const std::vector<std::vector<int>> TRUE_CNF  = {};

    for (size_t i = 0; i < nodes.size(); i++) {
        const auto& node = nodes[i];

        if (node.isRoot) {
            if (isAClause((int)i)) {
                // A-clause: base interpolant = FALSE
                nodeInterpolants[i] = FALSE_CNF;
            } else {
                // B-clause: base interpolant = TRUE
                nodeInterpolants[i] = TRUE_CNF;
            }
        } else {
            if (node.chainIds.empty()) {
                nodeInterpolants[i] = TRUE_CNF;
                continue;
            }

            int id1 = node.chainIds[0];
            if (id1 < 0 || id1 >= (int)nodeInterpolants.size()) {
                nodeInterpolants[i] = TRUE_CNF;
                continue;
            }

            auto result = nodeInterpolants[id1];

            for (size_t j = 0; j < node.chainVars.size(); j++) {
                if (j + 1 >= node.chainIds.size()) break;

                int var = node.chainVars[j] + 1;  // 1-based CNF variable
                int id2 = node.chainIds[j + 1];

                if (id2 < 0 || id2 >= (int)nodeInterpolants.size()) continue;

                const auto& i2 = nodeInterpolants[id2];

                if (isSharedVar(var)) {
                    // Shared pivot: I = I1 OR I2
                    result = cnfOr(result, i2);
                } else {
                    // Local pivot: I = I1 AND I2
                    result = cnfAnd(result, i2);
                }
            }

            nodeInterpolants[i] = result;
        }
    }

    return nodeInterpolants.back();
}