#pragma once

#include <stdexcept>

namespace d2gen {

struct ScenarioGenCounts {
    int a_count;
    int d_count;
    int a_large_count;
    int d_large_count;
};

// Throws std::invalid_argument with a diagnostic message if counts violate any constraint:
//   0 <= a/d_large_count <= 3
//   a/d_large_count <= a/d_count
//   a/d_count + a/d_large_count <= 6  (physical footprint: large=2 cells, small=1, capacity=6)
inline void validate_scenario_gen_counts(const ScenarioGenCounts& c) {
    auto check = [](bool cond, const char* msg) {
        if (!cond)
            throw std::invalid_argument(msg);
    };
    check(c.a_large_count >= 0 && c.a_large_count <= 3, "--a-large-count must be 0..3");
    check(c.d_large_count >= 0 && c.d_large_count <= 3, "--d-large-count must be 0..3");
    check(c.a_large_count <= c.a_count, "--a-large-count must be <= --a-count");
    check(c.d_large_count <= c.d_count, "--d-large-count must be <= --d-count");
    check(c.a_count + c.a_large_count <= 6,
          "A-side physical footprint exceeds 6 cells (a_count + a_large_count <= 6)");
    check(c.d_count + c.d_large_count <= 6,
          "D-side physical footprint exceeds 6 cells (d_count + d_large_count <= 6)");
}

} // namespace d2gen
