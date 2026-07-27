#include "stack_pair_generator.hpp"

#include <vector>

namespace d2battle_sweep {

std::vector<StackPair> generate_directed_pairs(const std::vector<StackCatalogEntry>& catalog) {
    std::vector<StackPair> pairs;
    pairs.reserve(expected_pair_count(catalog.size()));

    for (const auto& a : catalog) {
        for (const auto& b : catalog) {
            if (a.stack_id == b.stack_id)
                continue;
            pairs.push_back({a.stack_id, b.stack_id});
        }
    }

    return pairs;
}

} // namespace d2battle_sweep
