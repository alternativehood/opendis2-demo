#include <d2adventure_rules/AdventureRoutePrefix.hpp>

#include <gtest/gtest.h>

#include <stdexcept>

namespace {
d2adventure::AdventureRoute route() {
    d2adventure::AdventureRoute result;
    result.start = {0, 0};
    result.destination = {3, 0};
    result.steps = {
        {.cell = {1, 0}, .delta_x = 1, .delta_y = 0, .step_cost = 2, .cumulative_cost = 2},
        {.cell = {2, 0}, .delta_x = 1, .delta_y = 0, .step_cost = 3, .cumulative_cost = 5},
        {.cell = {3, 0}, .delta_x = 1, .delta_y = 0, .step_cost = 4, .cumulative_cost = 9}};
    result.total_cost = 9;
    return result;
}
} // namespace

TEST(AdventureRoutePrefix, SlicesWithoutChangingSource) {
    const auto source = route();
    const auto prefix = d2adventure::adventure_route_prefix(source, 2);
    EXPECT_EQ(prefix.start, (d2runtime::MapCellCoord{0, 0}));
    EXPECT_EQ(prefix.destination, (d2runtime::MapCellCoord{2, 0}));
    EXPECT_EQ(prefix.total_cost, 5);
    ASSERT_EQ(prefix.steps.size(), 2U);
    EXPECT_EQ(prefix.steps[1], source.steps[1]);
    EXPECT_EQ(source, route());
}

TEST(AdventureRoutePrefix, ZeroAndFullPrefixes) {
    const auto source = route();
    const auto empty = d2adventure::adventure_route_prefix(source, 0);
    EXPECT_TRUE(empty.steps.empty());
    EXPECT_EQ(empty.destination, source.start);
    EXPECT_EQ(empty.total_cost, 0);
    EXPECT_EQ(d2adventure::adventure_route_prefix(source, source.steps.size()), source);
}

TEST(AdventureRoutePrefix, RejectsMalformedRoutes) {
    auto malformed = route();
    malformed.steps[1].cumulative_cost = 4;
    EXPECT_THROW(static_cast<void>(d2adventure::adventure_route_prefix(malformed, 1)),
                 std::invalid_argument);
    EXPECT_THROW(static_cast<void>(d2adventure::adventure_route_prefix(route(), 4)),
                 std::invalid_argument);
    malformed = route();
    malformed.destination = {4, 0};
    EXPECT_THROW(static_cast<void>(d2adventure::adventure_route_prefix(malformed, 1)),
                 std::invalid_argument);
}
