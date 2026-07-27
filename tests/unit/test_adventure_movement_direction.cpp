#include <gtest/gtest.h>

#include <d2adventure_render/map_geometry.hpp>
#include <d2runtime/AdventureMovementDirection.hpp>

#include <array>
#include <stdexcept>
#include <tuple>
#include <utility>

using d2runtime::AdventureIsoDirection;

TEST(AdventureMovementDirection, CanonicalEightDirectionMapping) {
    const std::array<std::tuple<int, int, AdventureIsoDirection>, 8> cases = {{
        {1, 0, AdventureIsoDirection::D6},
        {1, -1, AdventureIsoDirection::D5},
        {0, -1, AdventureIsoDirection::D4},
        {-1, -1, AdventureIsoDirection::D3},
        {-1, 0, AdventureIsoDirection::D2},
        {-1, 1, AdventureIsoDirection::D1},
        {0, 1, AdventureIsoDirection::D0},
        {1, 1, AdventureIsoDirection::D7},
    }};
    for (const auto [x, y, expected] : cases)
        EXPECT_EQ(d2runtime::adventure_direction_for_delta(x, y), expected);
}

TEST(AdventureMovementDirection, NorthAndSouthRemainCanonical) {
    EXPECT_EQ(d2runtime::adventure_direction_for_delta(0, -1), AdventureIsoDirection::D4);
    EXPECT_EQ(d2runtime::adventure_direction_for_delta(0, 1), AdventureIsoDirection::D0);
}

TEST(AdventureMovementDirection, MappingMatchesScreenProjection) {
    const auto geometry = d2engine::adventure_render::AdventureMapGeometry::from_source(3, 3);
    const std::array<std::tuple<int, int, int, int, AdventureIsoDirection>, 8> cases = {{
        {1, 0, 1, 1, AdventureIsoDirection::D6},
        {1, -1, 1, 0, AdventureIsoDirection::D5},
        {0, -1, 1, -1, AdventureIsoDirection::D4},
        {-1, -1, 0, -1, AdventureIsoDirection::D3},
        {-1, 0, -1, -1, AdventureIsoDirection::D2},
        {-1, 1, -1, 0, AdventureIsoDirection::D1},
        {0, 1, -1, 1, AdventureIsoDirection::D0},
        {1, 1, 0, 1, AdventureIsoDirection::D7},
    }};
    const auto origin = geometry.project_cell({0, 0});
    for (const auto [x, y, expected_screen_x, expected_screen_y, expected_direction] : cases) {
        const auto projected = geometry.project_cell({x, y});
        const int  screen_x = (projected.x > origin.x) - (projected.x < origin.x);
        const int  screen_y = (projected.y > origin.y) - (projected.y < origin.y);
        EXPECT_EQ(screen_x, expected_screen_x);
        EXPECT_EQ(screen_y, expected_screen_y);
        EXPECT_EQ(d2runtime::adventure_direction_for_delta(x, y), expected_direction);
    }
}

TEST(AdventureMovementDirection, InvalidDeltasAreRejected) {
    for (const auto [x, y] :
         {std::pair{0, 0}, std::pair{2, 0}, std::pair{-2, 0}, std::pair{0, 2}, std::pair{0, -2}})
        EXPECT_THROW(static_cast<void>(d2runtime::adventure_direction_for_delta(x, y)),
                     std::invalid_argument);
}
