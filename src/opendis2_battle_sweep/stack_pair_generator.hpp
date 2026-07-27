#pragma once

#include "stack_catalog.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace d2battle_sweep {

struct StackPair {
    std::string party1_stack_id;
    std::string party2_stack_id;
};

[[nodiscard]] std::vector<StackPair>
generate_directed_pairs(const std::vector<StackCatalogEntry>& catalog);

[[nodiscard]] constexpr std::size_t expected_pair_count(std::size_t n) {
    return n * (n - 1);
}

} // namespace d2battle_sweep
