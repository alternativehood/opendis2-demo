#include <d2adventure_rules/AdventureRoutePreviewBuilder.hpp>

#include <gtest/gtest.h>

#include <optional>
#include <stdexcept>

namespace {

d2adventure::AdventureRoute route() {
    d2adventure::AdventureRoute result;
    result.start = {0, 0};
    result.destination = {3, 1};
    result.steps = {{{1, 0}, 1, 0, 3, 3}, {{2, 1}, 1, 1, 4, 7}, {{3, 1}, 1, 0, 3, 10}};
    result.total_cost = 10;
    return result;
}

} // namespace

TEST(AdventureRoutePreview, AffordableRoutePreservesAllNormalSteps) {
    const auto source = route();
    const auto preview =
        d2adventure::AdventureRoutePreviewBuilder{}.build(source, 10, std::nullopt);
    ASSERT_EQ(preview.start, source.start);
    ASSERT_EQ(preview.destination, source.destination);
    ASSERT_EQ(preview.steps.size(), 3u);
    for (std::size_t index = 0; index < preview.steps.size(); ++index) {
        EXPECT_EQ(preview.steps[index].route_step_index, index);
        EXPECT_EQ(preview.steps[index].cell, source.steps[index].cell);
        EXPECT_EQ(preview.steps[index].marker, d2adventure::AdventureRouteMarkerKind::Normal);
    }
    EXPECT_NE(preview.steps.front().cell, source.start);
}

TEST(AdventureRoutePreview, OverBudgetStopsAtActionLimitAndPreservesDestination) {
    const auto preview = d2adventure::AdventureRoutePreviewBuilder{}.build(route(), 1, 0u);
    ASSERT_EQ(preview.steps.size(), 3u);
    EXPECT_EQ(preview.steps[0].marker, d2adventure::AdventureRouteMarkerKind::ActionLimit);
    EXPECT_EQ(preview.steps[1].marker, d2adventure::AdventureRouteMarkerKind::ActionLimit);
    EXPECT_EQ(preview.steps[1].route_step_index, 1u);
    EXPECT_EQ(preview.steps[2].marker, d2adventure::AdventureRouteMarkerKind::ActionLimit);
    EXPECT_EQ(preview.destination, (d2runtime::MapCellCoord{3, 1}));
}

TEST(AdventureRoutePreview, FirstAndFinalUnaffordableStepsAreMarked) {
    const auto first = d2adventure::AdventureRoutePreviewBuilder{}.build(route(), 0, 0u);
    ASSERT_EQ(first.steps.size(), 3u);
    EXPECT_EQ(first.steps[0].marker, d2adventure::AdventureRouteMarkerKind::ActionLimit);

    const auto final = d2adventure::AdventureRoutePreviewBuilder{}.build(route(), 7, 2u);
    ASSERT_EQ(final.steps.size(), 3u);
    EXPECT_EQ(final.steps[0].marker, d2adventure::AdventureRouteMarkerKind::Normal);
    EXPECT_EQ(final.steps[1].marker, d2adventure::AdventureRouteMarkerKind::Normal);
    EXPECT_EQ(final.steps[2].marker, d2adventure::AdventureRouteMarkerKind::ActionLimit);
}

TEST(AdventureRoutePreview, EmptyRoutePreservesEndpoints) {
    d2adventure::AdventureRoute source;
    source.start = {2, 2};
    source.destination = source.start;
    const auto preview = d2adventure::AdventureRoutePreviewBuilder{}.build(source, 0, std::nullopt);
    EXPECT_TRUE(preview.steps.empty());
    EXPECT_EQ(preview.start, source.start);
    EXPECT_EQ(preview.destination, source.destination);
}

TEST(AdventureRoutePreview, RepeatedBuildsAreIdenticalAndDoNotMutateSource) {
    const auto source = route();
    const auto first = d2adventure::AdventureRoutePreviewBuilder{}.build(source, 1, 0u);
    const auto second = d2adventure::AdventureRoutePreviewBuilder{}.build(source, 1, 0u);
    EXPECT_EQ(first, second);
    EXPECT_EQ(source, route());
}

TEST(AdventureRoutePreview, RejectsInconsistentInputs) {
    auto source = route();
    EXPECT_THROW(static_cast<void>(d2adventure::AdventureRoutePreviewBuilder{}.build(
                     source, 10, source.steps.size())),
                 std::invalid_argument);
    EXPECT_THROW(static_cast<void>(d2adventure::AdventureRoutePreviewBuilder{}.build(
                     source, 10, source.steps.size() + 1)),
                 std::invalid_argument);
    source = route();
    source.destination = {9, 9};
    EXPECT_THROW(static_cast<void>(
                     d2adventure::AdventureRoutePreviewBuilder{}.build(source, 10, std::nullopt)),
                 std::invalid_argument);

    source = route();
    source.steps[0].cell = source.start;
    EXPECT_THROW(static_cast<void>(
                     d2adventure::AdventureRoutePreviewBuilder{}.build(source, 10, std::nullopt)),
                 std::invalid_argument);

    source = route();
    source.steps[1].cell = {3, 1};
    EXPECT_THROW(static_cast<void>(
                     d2adventure::AdventureRoutePreviewBuilder{}.build(source, 10, std::nullopt)),
                 std::invalid_argument);

    source = route();
    source.steps[1].delta_x = 0;
    EXPECT_THROW(static_cast<void>(
                     d2adventure::AdventureRoutePreviewBuilder{}.build(source, 10, std::nullopt)),
                 std::invalid_argument);

    source = route();
    source.steps[0].cell = source.start;
    source.steps[0].delta_x = 0;
    source.steps[0].delta_y = 0;
    EXPECT_THROW(static_cast<void>(
                     d2adventure::AdventureRoutePreviewBuilder{}.build(source, 10, std::nullopt)),
                 std::invalid_argument);
}

TEST(AdventureRoutePreview, EmptyRouteRejectsUnaffordableIndex) {
    d2adventure::AdventureRoute source;
    EXPECT_THROW(
        static_cast<void>(d2adventure::AdventureRoutePreviewBuilder{}.build(source, 0, 0u)),
        std::invalid_argument);
}
