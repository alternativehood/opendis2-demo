#include <d2adventure_rules/AdventureNavigationMapBuilder.hpp>
#include <d2adventure_rules/AdventurePathfinder.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <queue>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr std::array<d2runtime::MapCellCoord, 8> kDeltas = {
    d2runtime::MapCellCoord{0, -1}, d2runtime::MapCellCoord{1, -1},  d2runtime::MapCellCoord{1, 0},
    d2runtime::MapCellCoord{1, 1},  d2runtime::MapCellCoord{0, 1},   d2runtime::MapCellCoord{-1, 1},
    d2runtime::MapCellCoord{-1, 0}, d2runtime::MapCellCoord{-1, -1},
};

d2runtime::AdventureWorldState world(int width, int height, std::uint32_t raw = 1) {
    d2runtime::AdventureWorldState result;
    result.map_width = width;
    result.map_height = height;
    result.terrain.width = width;
    result.terrain.height = height;
    result.terrain.tiles.assign(static_cast<std::size_t>(width * height), {raw});
    return result;
}

d2runtime::AdventureMapObject blocking(std::string                          id,
                                       std::vector<d2runtime::MapCellCoord> footprint) {
    d2runtime::AdventureMapObject result;
    result.id = std::move(id);
    result.blocking = true;
    result.footprint = std::move(footprint);
    return result;
}

d2runtime::AdventureStack stack(std::string id, d2runtime::MapCellCoord position) {
    d2runtime::AdventureStack result;
    result.id = std::move(id);
    result.position = position;
    return result;
}

d2adventure::AdventureNavigationMap build(d2runtime::AdventureWorldState input) {
    const auto result = d2adventure::AdventureNavigationMapBuilder().build(input);
    EXPECT_TRUE(result.success());
    EXPECT_TRUE(result.map.has_value());
    return *result.map;
}

std::size_t index_for(d2runtime::MapCellCoord cell, int width) {
    return static_cast<std::size_t>(cell.y * width + cell.x);
}

std::optional<std::int64_t> reference_dijkstra(const d2adventure::AdventureNavigationMap& map,
                                               d2runtime::MapCellCoord                    start,
                                               d2runtime::MapCellCoord               destination,
                                               d2adventure::AdventureMovementProfile profile,
                                               std::string_view moving_stack_id) {
    if (!map.contains(start) || !map.contains(destination))
        return std::nullopt;
    const auto start_cell = map.traversal_cell_at(start, moving_stack_id);
    const auto destination_cell = map.traversal_cell_at(destination, moving_stack_id);
    if (!start_cell || !destination_cell ||
        !d2adventure::evaluate_adventure_movement_cell(profile, *start_cell).passable ||
        !d2adventure::evaluate_adventure_movement_cell(profile, *destination_cell).passable)
        return std::nullopt;
    if (start == destination)
        return 0;

    const auto                infinity = std::numeric_limits<std::int64_t>::max();
    std::vector<std::int64_t> distances(map.size(), infinity);
    std::vector<bool>         visited(map.size(), false);
    distances[index_for(start, map.width())] = 0;
    for (std::size_t iteration = 0; iteration < map.size(); ++iteration) {
        std::size_t current = map.size();
        for (std::size_t candidate = 0; candidate < map.size(); ++candidate) {
            if (!visited[candidate] && distances[candidate] != infinity &&
                (current == map.size() || distances[candidate] < distances[current]))
                current = candidate;
        }
        if (current == map.size())
            return std::nullopt;
        visited[current] = true;
        const auto                    map_width = static_cast<std::size_t>(map.width());
        const d2runtime::MapCellCoord cell{static_cast<int>(current % map_width),
                                           static_cast<int>(current / map_width)};
        if (cell == destination)
            return distances[current];
        for (const auto delta : kDeltas) {
            const d2runtime::MapCellCoord neighbor{cell.x + delta.x, cell.y + delta.y};
            if (!map.contains(neighbor))
                continue;
            const auto traversal = map.traversal_cell_at(neighbor, moving_stack_id);
            const auto decision =
                d2adventure::evaluate_adventure_movement_cell(profile, *traversal);
            if (!decision.passable)
                continue;
            const auto neighbor_index = index_for(neighbor, map.width());
            const auto candidate = distances[current] + decision.movement_cost;
            if (candidate < distances[neighbor_index])
                distances[neighbor_index] = candidate;
        }
    }
    return std::nullopt;
}

d2adventure::AdventurePathResult path(const d2adventure::AdventureNavigationMap& map,
                                      d2runtime::MapCellCoord                    start,
                                      d2runtime::MapCellCoord                    destination,
                                      d2adventure::AdventureMovementProfile      profile,
                                      std::string_view                           stack_id = {}) {
    return d2adventure::AdventurePathfinder().find_path(map, start, destination, profile, stack_id);
}

std::size_t compare_all_pairs_with_dijkstra(const d2adventure::AdventureNavigationMap& map,
                                            std::string_view moving_stack_id) {
    std::size_t      comparisons = 0;
    const std::array profiles = {d2adventure::AdventureMovementProfile::Walking,
                                 d2adventure::AdventureMovementProfile::Flying,
                                 d2adventure::AdventureMovementProfile::Swimming};
    for (const auto profile : profiles) {
        for (int start_y = 0; start_y < map.height(); ++start_y) {
            for (int start_x = 0; start_x < map.width(); ++start_x) {
                const d2runtime::MapCellCoord start{start_x, start_y};
                for (int destination_y = 0; destination_y < map.height(); ++destination_y) {
                    for (int destination_x = 0; destination_x < map.width(); ++destination_x) {
                        const d2runtime::MapCellCoord destination{destination_x, destination_y};
                        const auto                    oracle =
                            reference_dijkstra(map, start, destination, profile, moving_stack_id);
                        const auto production =
                            path(map, start, destination, profile, moving_stack_id);
                        EXPECT_EQ(production.route.has_value(), oracle.has_value())
                            << "profile=" << static_cast<int>(profile) << " start=" << start_x
                            << "," << start_y << " destination=" << destination_x << ","
                            << destination_y;
                        if (oracle) {
                            EXPECT_TRUE(production.success());
                            EXPECT_TRUE(production.route);
                            if (production.route)
                                EXPECT_EQ(production.route->total_cost, *oracle);
                        } else {
                            EXPECT_FALSE(production.success());
                            EXPECT_FALSE(production.route);
                        }
                        ++comparisons;
                    }
                }
            }
        }
    }
    return comparisons;
}

} // namespace

TEST(AdventurePathfinder, ValidatesEmptyAndOutOfBoundsRequests) {
    const d2adventure::AdventureNavigationMap empty;
    EXPECT_EQ(path(empty, {0, 0}, {0, 0}, d2adventure::AdventureMovementProfile::Walking).status,
              d2adventure::AdventurePathStatus::EmptyNavigationMap);
    const auto map = build(world(2, 2));
    EXPECT_EQ(path(map, {-1, 0}, {1, 1}, d2adventure::AdventureMovementProfile::Walking).status,
              d2adventure::AdventurePathStatus::StartOutOfBounds);
    EXPECT_EQ(path(map, {0, 0}, {2, 1}, d2adventure::AdventureMovementProfile::Walking).status,
              d2adventure::AdventurePathStatus::DestinationOutOfBounds);
}

TEST(AdventurePathfinder, ValidatesBlockedEndpointsAndOccupancyIdentity) {
    auto input = world(3, 1);
    input.map_objects.push_back(blocking("rock", {{0, 0}}));
    input.stacks.push_back(stack("other", {1, 0}));
    const auto map = build(std::move(input));
    const auto start_blocked =
        path(map, {0, 0}, {2, 0}, d2adventure::AdventureMovementProfile::Walking);
    EXPECT_EQ(start_blocked.status, d2adventure::AdventurePathStatus::StartBlocked);
    EXPECT_EQ(start_blocked.block_reason,
              d2adventure::AdventureMovementBlockReason::BlockingObject);
    const auto occupied = path(map, {1, 0}, {2, 0}, d2adventure::AdventureMovementProfile::Walking);
    EXPECT_EQ(occupied.status, d2adventure::AdventurePathStatus::StartBlocked);
    EXPECT_EQ(occupied.block_reason, d2adventure::AdventureMovementBlockReason::OccupiedByStack);
    const auto ignored =
        path(map, {1, 0}, {2, 0}, d2adventure::AdventureMovementProfile::Walking, "other");
    EXPECT_TRUE(ignored.success());
    const auto occupied_destination =
        path(map, {2, 0}, {1, 0}, d2adventure::AdventureMovementProfile::Walking);
    EXPECT_EQ(occupied_destination.status, d2adventure::AdventurePathStatus::DestinationBlocked);
    EXPECT_EQ(occupied_destination.block_reason,
              d2adventure::AdventureMovementBlockReason::OccupiedByStack);
    EXPECT_FALSE(occupied_destination.route);
}

TEST(AdventurePathfinder, BlocksObjectOccupiedDestinationWithExactReason) {
    auto input = world(3, 1);
    input.map_objects.push_back(blocking("rock", {{2, 0}}));
    const auto result = path(build(std::move(input)), {0, 0}, {2, 0},
                             d2adventure::AdventureMovementProfile::Walking);
    EXPECT_EQ(result.status, d2adventure::AdventurePathStatus::DestinationBlocked);
    EXPECT_EQ(result.block_reason, d2adventure::AdventureMovementBlockReason::BlockingObject);
    EXPECT_FALSE(result.route);
}

TEST(AdventurePathfinder, HandlesAlreadyAtDestinationAndRouteStructure) {
    const auto map = build(world(3, 3));
    const auto same = path(map, {1, 1}, {1, 1}, d2adventure::AdventureMovementProfile::Walking);
    ASSERT_EQ(same.status, d2adventure::AdventurePathStatus::AlreadyAtDestination);
    ASSERT_TRUE(same.route);
    EXPECT_TRUE(same.route->empty());
    EXPECT_EQ(same.route->total_cost, 0);

    const auto result = path(map, {0, 0}, {1, 0}, d2adventure::AdventureMovementProfile::Walking);
    ASSERT_EQ(result.status, d2adventure::AdventurePathStatus::Found);
    ASSERT_TRUE(result.route);
    ASSERT_EQ(result.route->size(), 1u);
    EXPECT_EQ(result.route->steps.front().cell, (d2runtime::MapCellCoord{1, 0}));
    EXPECT_EQ(result.route->steps.front().delta_x, 1);
    EXPECT_EQ(result.route->steps.front().delta_y, 0);
    EXPECT_EQ(result.route->steps.front().step_cost, 3);
    EXPECT_EQ(result.route->total_cost, 3);
}

TEST(AdventurePathfinder, SupportsDiagonalMovementWithoutCornerChecks) {
    auto input = world(3, 3);
    input.map_objects.push_back(blocking("north", {{1, 0}}));
    input.map_objects.push_back(blocking("east", {{0, 1}}));
    const auto map = build(std::move(input));
    const auto result = path(map, {0, 0}, {1, 1}, d2adventure::AdventureMovementProfile::Walking);
    ASSERT_EQ(result.status, d2adventure::AdventurePathStatus::Found);
    ASSERT_TRUE(result.route);
    ASSERT_EQ(result.route->size(), 1u);
    EXPECT_EQ(result.route->steps.front().delta_x, 1);
    EXPECT_EQ(result.route->steps.front().delta_y, 1);
}

TEST(AdventurePathfinder, AppliesDestinationCellMovementCosts) {
    auto input = world(4, 1);
    input.terrain.tiles[1].raw_value = 9;
    input.terrain.tiles[2].raw_value = 7;
    input.roads.push_back({"road", 0, 0, {3, 0}});
    const auto map = build(std::move(input));
    const auto walking = path(map, {0, 0}, {3, 0}, d2adventure::AdventureMovementProfile::Walking);
    ASSERT_TRUE(walking.route);
    ASSERT_EQ(walking.route->size(), 3u);
    EXPECT_EQ(walking.route->steps[0].step_cost, 4);
    EXPECT_EQ(walking.route->steps[1].step_cost, 6);
    EXPECT_EQ(walking.route->steps[2].step_cost, 1);
    EXPECT_EQ(walking.route->total_cost, 11);
    const auto flying = path(map, {0, 0}, {3, 0}, d2adventure::AdventureMovementProfile::Flying);
    ASSERT_TRUE(flying.route);
    EXPECT_EQ(flying.route->total_cost, 6);
}

TEST(AdventurePathfinder, RoutesAroundObjectsAndStacks) {
    auto input = world(5, 3);
    input.map_objects.push_back(blocking("wall", {{1, 1}, {2, 1}, {3, 1}}));
    input.stacks.push_back(stack("stack", {1, 0}));
    const auto map = build(std::move(input));
    const auto result = path(map, {0, 1}, {4, 1}, d2adventure::AdventureMovementProfile::Walking);
    ASSERT_EQ(result.status, d2adventure::AdventurePathStatus::Found);
    ASSERT_TRUE(result.route);
    for (const auto& step : result.route->steps) {
        EXPECT_NE(step.cell, (d2runtime::MapCellCoord{1, 1}));
        EXPECT_NE(step.cell, (d2runtime::MapCellCoord{2, 1}));
        EXPECT_NE(step.cell, (d2runtime::MapCellCoord{3, 1}));
        EXPECT_NE(step.cell, (d2runtime::MapCellCoord{1, 0}));
    }
}

TEST(AdventurePathfinder, ReturnsNoPathWhenDestinationIsEnclosed) {
    auto input = world(5, 5);
    input.map_objects.push_back(blocking("n", {{2, 1}}));
    input.map_objects.push_back(blocking("ne", {{3, 1}}));
    input.map_objects.push_back(blocking("e", {{3, 2}}));
    input.map_objects.push_back(blocking("se", {{3, 3}}));
    input.map_objects.push_back(blocking("s", {{2, 3}}));
    input.map_objects.push_back(blocking("sw", {{1, 3}}));
    input.map_objects.push_back(blocking("w", {{1, 2}}));
    input.map_objects.push_back(blocking("nw", {{1, 1}}));
    const auto map = build(std::move(input));
    const auto result = path(map, {0, 0}, {2, 2}, d2adventure::AdventureMovementProfile::Walking);
    EXPECT_EQ(result.status, d2adventure::AdventurePathStatus::NoPath);
    EXPECT_FALSE(result.route);
}

TEST(AdventurePathfinder, ProfilesRestrictGroundAndRespectWater) {
    auto input = world(3, 1);
    input.terrain.tiles[0].raw_value = 7;
    input.terrain.tiles[1].raw_value = 7;
    input.terrain.tiles[2].raw_value = 9;
    const auto map = build(std::move(input));
    const auto swimming =
        path(map, {0, 0}, {1, 0}, d2adventure::AdventureMovementProfile::Swimming);
    ASSERT_TRUE(swimming.route);
    EXPECT_EQ(swimming.route->total_cost, 2);
    const auto land = path(map, {0, 0}, {2, 0}, d2adventure::AdventureMovementProfile::Swimming);
    EXPECT_EQ(land.status, d2adventure::AdventurePathStatus::DestinationBlocked);
    EXPECT_EQ(land.block_reason, d2adventure::AdventureMovementBlockReason::ProfileRestriction);
    const auto flying = path(map, {0, 0}, {2, 0}, d2adventure::AdventureMovementProfile::Flying);
    ASSERT_TRUE(flying.route);
    EXPECT_EQ(flying.route->total_cost, 4);
}

TEST(AdventurePathfinder, SwimmingCannotTransitLandButWalkingCan) {
    auto input = world(3, 1);
    input.terrain.tiles[0].raw_value = 7;
    input.terrain.tiles[1].raw_value = 1;
    input.terrain.tiles[2].raw_value = 7;
    const auto map = build(std::move(input));
    const auto swimming =
        path(map, {0, 0}, {2, 0}, d2adventure::AdventureMovementProfile::Swimming);
    EXPECT_EQ(swimming.status, d2adventure::AdventurePathStatus::NoPath);
    EXPECT_FALSE(swimming.route);

    const auto walking = path(map, {0, 0}, {2, 0}, d2adventure::AdventureMovementProfile::Walking);
    ASSERT_EQ(walking.status, d2adventure::AdventurePathStatus::Found);
    ASSERT_TRUE(walking.route);
    ASSERT_EQ(walking.route->size(), 2u);
    EXPECT_EQ(walking.route->steps[0].step_cost, 3);
    EXPECT_EQ(walking.route->steps[1].step_cost, 6);
    EXPECT_EQ(walking.route->total_cost, 9);
}

TEST(AdventurePathfinder, ChoosesWeightedRoadRouteAndDoesNotUseRoadForFlying) {
    auto input = world(5, 3);
    for (int x = 0; x < 5; ++x)
        input.roads.push_back({"road" + std::to_string(x), 0, 0, {x, 2}});
    const auto map = build(std::move(input));
    const auto walking = path(map, {0, 1}, {4, 1}, d2adventure::AdventureMovementProfile::Walking);
    ASSERT_TRUE(walking.route);
    EXPECT_EQ(walking.route->total_cost, 6);
    const auto flying = path(map, {0, 1}, {4, 1}, d2adventure::AdventureMovementProfile::Flying);
    ASSERT_TRUE(flying.route);
    EXPECT_EQ(flying.route->total_cost, 8);
    EXPECT_EQ(flying.route->size(), 4u);
}

TEST(AdventurePathfinder, RejectsMoreExpensiveRoadDetour) {
    auto input = world(5, 2);
    input.roads.push_back({"road", 0, 0, {0, 0}});
    const auto result = path(build(std::move(input)), {0, 1}, {4, 1},
                             d2adventure::AdventureMovementProfile::Walking);
    ASSERT_EQ(result.status, d2adventure::AdventurePathStatus::Found);
    ASSERT_TRUE(result.route);
    EXPECT_EQ(result.route->total_cost, 12);
    ASSERT_EQ(result.route->size(), 4u);
    for (const auto& step : result.route->steps)
        EXPECT_NE(step.cell, (d2runtime::MapCellCoord{0, 0}));
}

TEST(AdventurePathfinder, FlyingRespectsOccupancyAndUnsupportedTransitCells) {
    {
        auto input = world(3, 1);
        input.map_objects.push_back(blocking("rock", {{1, 0}}));
        const auto result = path(build(std::move(input)), {0, 0}, {2, 0},
                                 d2adventure::AdventureMovementProfile::Flying);
        EXPECT_EQ(result.status, d2adventure::AdventurePathStatus::NoPath);
        EXPECT_FALSE(result.route);
    }
    {
        auto input = world(3, 1);
        input.stacks.push_back(stack("other", {1, 0}));
        const auto result = path(build(std::move(input)), {0, 0}, {2, 0},
                                 d2adventure::AdventureMovementProfile::Flying, "moving");
        EXPECT_EQ(result.status, d2adventure::AdventurePathStatus::NoPath);
        EXPECT_FALSE(result.route);
    }
    {
        auto input = world(3, 1);
        input.terrain.tiles[1].raw_value = 255;
        const auto result = path(build(std::move(input)), {0, 0}, {2, 0},
                                 d2adventure::AdventureMovementProfile::Flying);
        EXPECT_EQ(result.status, d2adventure::AdventurePathStatus::NoPath);
        EXPECT_FALSE(result.route);
    }
}

TEST(AdventurePathfinder, OrthogonalAndDiagonalEntriesHaveTheSameCost) {
    const auto map = build(world(2, 2));
    const auto orthogonal =
        path(map, {0, 0}, {1, 0}, d2adventure::AdventureMovementProfile::Walking);
    const auto diagonal = path(map, {0, 0}, {1, 1}, d2adventure::AdventureMovementProfile::Walking);
    ASSERT_TRUE(orthogonal.route);
    ASSERT_TRUE(diagonal.route);
    EXPECT_EQ(orthogonal.route->steps.front().step_cost, 3);
    EXPECT_EQ(diagonal.route->steps.front().step_cost, 3);
}

TEST(AdventurePathfinder, UniformMapUsesChebyshevDiagonalRoute) {
    const auto result =
        path(build(world(4, 4)), {0, 0}, {3, 3}, d2adventure::AdventureMovementProfile::Walking);
    ASSERT_EQ(result.status, d2adventure::AdventurePathStatus::Found);
    ASSERT_TRUE(result.route);
    ASSERT_EQ(result.route->size(), 3u);
    for (std::size_t index = 0; index < result.route->steps.size(); ++index) {
        EXPECT_EQ(
            result.route->steps[index].cell,
            (d2runtime::MapCellCoord{static_cast<int>(index + 1), static_cast<int>(index + 1)}));
        EXPECT_EQ(result.route->steps[index].delta_x, 1);
        EXPECT_EQ(result.route->steps[index].delta_y, 1);
        EXPECT_EQ(result.route->steps[index].step_cost, 3);
    }
    EXPECT_EQ(result.route->total_cost, 9);
}

TEST(AdventurePathfinder, UsesExactDeterministicTieBreakingRoute) {
    auto input = world(3, 3);
    input.map_objects.push_back(blocking("rock", {{1, 1}}));
    const auto map = build(std::move(input));
    const auto expected = path(map, {1, 2}, {1, 0}, d2adventure::AdventureMovementProfile::Walking);
    ASSERT_TRUE(expected.route);
    ASSERT_EQ(expected.route->size(), 2u);
    EXPECT_EQ(expected.route->steps[0].cell, (d2runtime::MapCellCoord{2, 1}));
    EXPECT_EQ(expected.route->steps[1].cell, (d2runtime::MapCellCoord{1, 0}));
    for (int repeat = 0; repeat < 20; ++repeat)
        EXPECT_EQ(path(map, {1, 2}, {1, 0}, d2adventure::AdventureMovementProfile::Walking).route,
                  expected.route);
}

TEST(AdventurePathfinder, UnreachableCellsDoNotChangeTieBreakingRoute) {
    auto small_input = world(3, 3);
    small_input.map_objects.push_back(blocking("rock", {{1, 1}}));
    const auto small_map = build(std::move(small_input));
    const auto expected =
        path(small_map, {1, 2}, {1, 0}, d2adventure::AdventureMovementProfile::Walking);

    auto large_input = world(5, 3);
    large_input.map_objects.push_back(blocking("rock", {{1, 1}}));
    large_input.map_objects.push_back(blocking("wall", {{3, 0}, {3, 1}, {3, 2}}));
    const auto actual = path(build(std::move(large_input)), {1, 2}, {1, 0},
                             d2adventure::AdventureMovementProfile::Walking);
    ASSERT_TRUE(expected.route);
    ASSERT_TRUE(actual.route);
    EXPECT_EQ(actual.route, expected.route);
}

TEST(AdventurePathfinder, IsDeterministicAndMatchesDijkstraForAllProfiles) {
    auto input = world(5, 4);
    input.terrain.tiles[6].raw_value = 9;
    input.terrain.tiles[7].raw_value = 7;
    input.terrain.tiles[8].raw_value = 9;
    input.roads.push_back({"r1", 0, 0, {0, 3}});
    input.roads.push_back({"r2", 0, 0, {1, 3}});
    input.map_objects.push_back(blocking("rock", {{2, 1}}));
    input.stacks.push_back(stack("occupied", {3, 2}));
    const auto map = build(std::move(input));
    for (const auto profile : {d2adventure::AdventureMovementProfile::Walking,
                               d2adventure::AdventureMovementProfile::Flying,
                               d2adventure::AdventureMovementProfile::Swimming}) {
        const auto oracle = reference_dijkstra(map, {0, 0}, {4, 3}, profile, "moving");
        const auto first = path(map, {0, 0}, {4, 3}, profile, "moving");
        ASSERT_EQ(first.route.has_value(), oracle.has_value());
        if (oracle) {
            ASSERT_TRUE(first.route);
            EXPECT_EQ(first.route->total_cost, *oracle);
        }
        for (int repeat = 0; repeat < 20; ++repeat)
            EXPECT_EQ(path(map, {0, 0}, {4, 3}, profile, "moving").route, first.route);
    }
}

TEST(AdventurePathfinder, ExpandedDijkstraOracleCoversFourFixedMapsAndAllPairs) {
    auto mixed = world(4, 3);
    mixed.terrain.tiles[1].raw_value = 9;
    mixed.terrain.tiles[2].raw_value = 7;
    mixed.terrain.tiles[5].raw_value = 9;
    mixed.roads.push_back({"r1", 0, 0, {0, 2}});
    mixed.roads.push_back({"r2", 0, 0, {1, 2}});
    mixed.roads.push_back({"r3", 0, 0, {2, 2}});

    auto footprints = world(5, 3);
    footprints.map_objects.push_back(blocking("wall", {{2, 0}, {2, 1}, {2, 2}}));
    footprints.map_objects.push_back(blocking("island", {{4, 1}, {4, 2}}));

    auto occupied = world(4, 3);
    occupied.stacks.push_back(stack("s1", {1, 1}));
    occupied.stacks.push_back(stack("s2", {2, 1}));

    auto restricted = world(4, 3);
    for (int y = 0; y < 3; ++y) {
        restricted.terrain.tiles[static_cast<std::size_t>(y * 4 + 0)].raw_value = 7;
        restricted.terrain.tiles[static_cast<std::size_t>(y * 4 + 1)].raw_value = 7;
        restricted.terrain.tiles[static_cast<std::size_t>(y * 4 + 2)].raw_value = 1;
        restricted.terrain.tiles[static_cast<std::size_t>(y * 4 + 3)].raw_value = 9;
    }

    const auto mixed_map = build(std::move(mixed));
    const auto footprint_map = build(std::move(footprints));
    const auto occupied_map = build(std::move(occupied));
    const auto restricted_map = build(std::move(restricted));
    EXPECT_EQ(compare_all_pairs_with_dijkstra(mixed_map, "moving"), 3u * 4u * 3u * 4u * 3u);
    EXPECT_EQ(compare_all_pairs_with_dijkstra(footprint_map, "moving"), 3u * 5u * 3u * 5u * 3u);
    EXPECT_EQ(compare_all_pairs_with_dijkstra(occupied_map, "moving"), 3u * 4u * 3u * 4u * 3u);
    EXPECT_EQ(compare_all_pairs_with_dijkstra(restricted_map, "moving"), 3u * 4u * 3u * 4u * 3u);
}

TEST(AdventurePathfinder, RejectsUnsupportedGroundAndPreservesRouteAccounting) {
    auto input = world(2, 1);
    input.terrain.tiles[1].raw_value = 255;
    const auto map = build(std::move(input));
    const auto blocked = path(map, {0, 0}, {1, 0}, d2adventure::AdventureMovementProfile::Walking);
    EXPECT_EQ(blocked.status, d2adventure::AdventurePathStatus::DestinationBlocked);
    EXPECT_EQ(blocked.block_reason, d2adventure::AdventureMovementBlockReason::UnsupportedGround);

    const auto route =
        path(build(world(4, 1)), {0, 0}, {3, 0}, d2adventure::AdventureMovementProfile::Walking);
    ASSERT_TRUE(route.route);
    std::int64_t            sum = 0;
    std::int64_t            previous = 0;
    d2runtime::MapCellCoord previous_cell = route.route->start;
    for (const auto& step : route.route->steps) {
        EXPECT_NE(step.cell, route.route->start);
        EXPECT_EQ(step.delta_x, step.cell.x - previous_cell.x);
        EXPECT_EQ(step.delta_y, step.cell.y - previous_cell.y);
        EXPECT_GE(step.delta_x, -1);
        EXPECT_LE(step.delta_x, 1);
        EXPECT_GE(step.delta_y, -1);
        EXPECT_LE(step.delta_y, 1);
        EXPECT_NE(step.delta_x == 0 && step.delta_y == 0, true);
        EXPECT_GT(step.step_cost, 0);
        sum += step.step_cost;
        EXPECT_EQ(step.cumulative_cost, sum);
        EXPECT_GT(step.cumulative_cost, previous);
        previous = step.cumulative_cost;
        previous_cell = step.cell;
    }
    EXPECT_EQ(sum, route.route->total_cost);
    EXPECT_EQ(route.route->steps.back().cell, route.route->destination);
}
