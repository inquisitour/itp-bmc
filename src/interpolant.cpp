#include "interpolant.h"
#include <algorithm>
#include <iostream>
#include <ostream>
#include <stdexcept>
#include <string>

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

    for (const auto& c : aPartClauses) {
        std::vector<int> s = c;
        std::sort(s.begin(), s.end());
        aClauseSet.insert(s);
    }

    std::cerr << "DEBUG aVars=" << aVars.size()
            << " bVars=" << bVars.size()
            << " aLocal=" << aLocalVars.size()
            << " shared=" << (aVars.size() - aLocalVars.size()) << std::endl;

    int extra = 0;
    for (int v : aVars)
        if (bVars.count(v) && !sharedVars.count(v)) extra++;
    std::cerr << "DEBUG shared-but-not-latch=" << extra << std::endl;
}

bool Interpolator::isAClause(int nodeId) {
    const auto& node = proof.getNodes()[nodeId];
    if (!node.isRoot) return false;
    
    // Match root clause literals against A-part clauses
    // Sort both for comparison
    std::vector<int> nodeLits = node.clause;
    std::sort(nodeLits.begin(), nodeLits.end());
    
    return aClauseSet.count(nodeLits) > 0;
}

bool Interpolator::isSharedVar(int var) {
    return sharedVars.count(var) > 0;
}

bool Interpolator::isALocal(int var) {
    return !isSharedVar(var);
}

static const size_t kMaxCross = 2000000;

// Sort literals, drop duplicate literals, drop tautological clauses,
// drop duplicate clauses. An empty clause makes the whole CNF FALSE.
static void normalize(std::vector<std::vector<int>>& f) {
    std::vector<std::vector<int>> out;
    out.reserve(f.size());
    bool isFalse = false;
    for (auto& c : f) {
        std::sort(c.begin(), c.end());
        c.erase(std::unique(c.begin(), c.end()), c.end());
        if (c.empty()) { isFalse = true; break; }
        bool taut = false;
        for (int l : c)
            if (l < 0 && std::binary_search(c.begin(), c.end(), -l)) { taut = true; break; }
        if (!taut) out.push_back(std::move(c));
    }
    if (isFalse) { f.assign(1, std::vector<int>()); return; }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    f = std::move(out);
}

static std::vector<std::vector<int>> cnfOr(
    const std::vector<std::vector<int>>& i1,
    const std::vector<std::vector<int>>& i2)
{
    if (i1.empty() || i2.empty()) return {};                 // TRUE
    if (i1.size() == 1 && i1[0].empty()) return i2;          // FALSE or I
    if (i2.size() == 1 && i2[0].empty()) return i1;
    if (i1.size() * i2.size() > kMaxCross)
        throw std::runtime_error("interpolant too large: " + std::to_string(i1.size()) +
                                 " x " + std::to_string(i2.size()) + " clauses in an OR");
    std::vector<std::vector<int>> result;
    result.reserve(i1.size() * i2.size());
    for (const auto& c1 : i1)
        for (const auto& c2 : i2) {
            std::vector<int> m = c1;
            m.insert(m.end(), c2.begin(), c2.end());
            result.push_back(std::move(m));
        }
    normalize(result);
    return result;
}

static std::vector<std::vector<int>> cnfAnd(
    const std::vector<std::vector<int>>& i1,
    const std::vector<std::vector<int>>& i2)
{
    if (i1.size() == 1 && i1[0].empty()) return i1;          // FALSE
    if (i2.size() == 1 && i2[0].empty()) return i2;
    std::vector<std::vector<int>> result = i1;
    result.insert(result.end(), i2.begin(), i2.end());
    normalize(result);
    return result;
}

std::vector<std::vector<int>> Interpolator::computeInterpolant() {
    const auto& nodes = proof.getNodes();
    if (nodes.empty()) return {};

    const std::vector<std::vector<int>> FALSE_CNF = {{}};
    const std::vector<std::vector<int>> TRUE_CNF  = {};

    // Only the cone of the final node matters.
    std::vector<char> visited(nodes.size(), 0);
    std::vector<int> stack = { (int)nodes.size() - 1 };
    while (!stack.empty()) {
        int cur = stack.back(); stack.pop_back();
        if (cur < 0 || cur >= (int)nodes.size() || visited[cur]) continue;
        visited[cur] = 1;
        if (!nodes[cur].isRoot)
            for (int id : nodes[cur].chainIds) stack.push_back(id);
    }

    nodeInterpolants.assign(nodes.size(), std::vector<std::vector<int>>());
    size_t cone = 0, coneA = 0;

    for (size_t i = 0; i < nodes.size(); i++) {
        if (!visited[i]) continue;
        cone++;
        const auto& node = nodes[i];

        if (node.isRoot) {
            if (isAClause((int)i)) {
                coneA++;
                std::vector<int> sharedLits;
                for (int lit : node.clause)
                    if (!aLocalVars.count(std::abs(lit))) sharedLits.push_back(lit);
                nodeInterpolants[i] = sharedLits.empty()
                    ? FALSE_CNF : std::vector<std::vector<int>>{sharedLits};
            } else {
                nodeInterpolants[i] = TRUE_CNF;
            }
            continue;
        }

        if (node.chainIds.empty()) { nodeInterpolants[i] = TRUE_CNF; continue; }

        auto result = nodeInterpolants[node.chainIds[0]];
        for (size_t j = 0; j < node.chainVars.size() && j + 1 < node.chainIds.size(); j++) {
            int var = node.chainVars[j] + 1;
            const auto& i2 = nodeInterpolants[node.chainIds[j + 1]];
            result = aLocalVars.count(var) ? cnfOr(result, i2) : cnfAnd(result, i2);
        }
        nodeInterpolants[i] = std::move(result);
    }

    std::cerr << "DEBUG cone=" << cone << "/" << nodes.size()
              << " A-roots-in-cone=" << coneA
              << " final=" << nodeInterpolants.back().size() << std::endl;
    return nodeInterpolants.back();
}