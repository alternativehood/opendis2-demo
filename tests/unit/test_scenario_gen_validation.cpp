#include <gtest/gtest.h>

#include "opendis2_battle_scenario_gen/scenario_gen_validation.hpp"

#include <stdexcept>

using d2gen::validate_scenario_gen_counts;

namespace {

TEST(ScenarioGenValidation, ACount5ALarge1IsValid) {
    // footprint: 5 + 1 = 6 <= 6 ✓
    EXPECT_NO_THROW(validate_scenario_gen_counts({5, 6, 1, 0}));
}

TEST(ScenarioGenValidation, ACount6ALarge1IsInvalid) {
    // footprint: 6 + 1 = 7 > 6
    EXPECT_THROW(validate_scenario_gen_counts({6, 6, 1, 0}), std::invalid_argument);
}

TEST(ScenarioGenValidation, ACount3ALarge3IsValid) {
    // footprint: 3 + 3 = 6 <= 6 ✓, and a_large_count <= a_count (3 <= 3) ✓
    EXPECT_NO_THROW(validate_scenario_gen_counts({3, 6, 3, 0}));
}

TEST(ScenarioGenValidation, ACount2ALarge3IsInvalid) {
    // a_large_count (3) > a_count (2)
    EXPECT_THROW(validate_scenario_gen_counts({2, 6, 3, 0}), std::invalid_argument);
}

TEST(ScenarioGenValidation, NegativeLargeCountIsInvalid) {
    EXPECT_THROW(validate_scenario_gen_counts({6, 6, -1, 0}), std::invalid_argument);
    EXPECT_THROW(validate_scenario_gen_counts({6, 6, 0, -1}), std::invalid_argument);
}

TEST(ScenarioGenValidation, LargeCountGreaterThan3IsInvalid) {
    EXPECT_THROW(validate_scenario_gen_counts({6, 6, 4, 0}), std::invalid_argument);
    EXPECT_THROW(validate_scenario_gen_counts({6, 6, 0, 4}), std::invalid_argument);
}

TEST(ScenarioGenValidation, BothSidesValidSimultaneously) {
    // a: 4 + 2 = 6 ✓, d: 3 + 3 = 6 ✓
    EXPECT_NO_THROW(validate_scenario_gen_counts({4, 3, 2, 3}));
}

} // namespace
