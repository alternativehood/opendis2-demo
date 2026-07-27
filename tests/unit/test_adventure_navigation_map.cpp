#include <d2adventure_rules/AdventureNavigationMapBuilder.hpp>
#include <d2adventure_rules/AdventureMovementPolicy.hpp>

#include <gtest/gtest.h>

namespace {

d2runtime::AdventureWorldState world(int width = 2, int height = 2) {
    d2runtime::AdventureWorldState result;
    result.map_width = width;
    result.map_height = height;
    result.terrain.width = width;
    result.terrain.height = height;
    if (width > 0 && height > 0)
        result.terrain.tiles.assign(static_cast<std::size_t>(width * height), {1});
    return result;
}

d2runtime::AdventureMapObject blocking(std::string                          id,
                                       std::vector<d2runtime::MapCellCoord> footprint) {
    d2runtime::AdventureMapObject result;
    result.id = std::move(id);
    result.footprint = std::move(footprint);
    result.blocking = true;
    return result;
}

d2runtime::AdventureStack stack(std::string id, d2runtime::MapCellCoord position,
                                std::string inside = {}) {
    d2runtime::AdventureStack result;
    result.id = std::move(id);
    result.position = position;
    result.inside = std::move(inside);
    return result;
}

const d2adventure::AdventureNavigationDiagnostic*
find(const d2adventure::AdventureNavigationMapBuildResult& result,
     d2adventure::AdventureNavigationDiagnosticKind        kind) {
    for (const auto& diagnostic : result.diagnostics) {
        if (diagnostic.kind == kind)
            return &diagnostic;
    }
    return nullptr;
}

} // namespace

TEST(AdventureNavigationMapBuilder, InvalidDimensionsFailClosed) {
    auto       input = world(0, 2);
    const auto result = d2adventure::AdventureNavigationMapBuilder().build(input);
    EXPECT_FALSE(result.map.has_value());
    const auto* diagnostic =
        find(result, d2adventure::AdventureNavigationDiagnosticKind::InvalidMapDimensions);
    ASSERT_NE(diagnostic, nullptr);
    EXPECT_NE(diagnostic->message.find("width=0"), std::string::npos);
    EXPECT_NE(diagnostic->message.find("height=2"), std::string::npos);

    input = world(2, -1);
    EXPECT_FALSE(d2adventure::AdventureNavigationMapBuilder().build(input).map.has_value());
}

TEST(AdventureNavigationMapBuilder, TerrainStructureMustMatch) {
    auto input = world();
    input.terrain.width = 1;
    auto result = d2adventure::AdventureNavigationMapBuilder().build(input);
    EXPECT_FALSE(result.map.has_value());
    const auto* dimensions_diagnostic =
        find(result, d2adventure::AdventureNavigationDiagnosticKind::TerrainDimensionsMismatch);
    ASSERT_NE(dimensions_diagnostic, nullptr);
    EXPECT_NE(dimensions_diagnostic->message.find("world=2x2"), std::string::npos);
    EXPECT_NE(dimensions_diagnostic->message.find("terrain=1x2"), std::string::npos);

    input = world();
    input.terrain.tiles.pop_back();
    result = d2adventure::AdventureNavigationMapBuilder().build(input);
    EXPECT_FALSE(result.map.has_value());
    const auto* tile_count_diagnostic =
        find(result, d2adventure::AdventureNavigationDiagnosticKind::TerrainTileCountMismatch);
    ASSERT_NE(tile_count_diagnostic, nullptr);
    EXPECT_NE(tile_count_diagnostic->message.find("actual=3"), std::string::npos);
    EXPECT_NE(tile_count_diagnostic->message.find("expected=4"), std::string::npos);
}

TEST(AdventureNavigationMapBuilder, CanonicalTerrainIsRowMajor) {
    auto input = world();
    input.terrain.tiles = {{1}, {9}, {7}, {255}};
    const auto result = d2adventure::AdventureNavigationMapBuilder().build(input);
    ASSERT_TRUE(result.success());
    ASSERT_TRUE(result.map);
    EXPECT_EQ(result.map->width(), 2);
    EXPECT_EQ(result.map->height(), 2);
    EXPECT_EQ(result.map->size(), 4u);
    EXPECT_EQ(result.map->cell_at({0, 0})->ground, d2runtime::AdventureGroundType::Plain);
    EXPECT_EQ(result.map->cell_at({1, 0})->ground, d2runtime::AdventureGroundType::Forest);
    EXPECT_EQ(result.map->cell_at({0, 1})->ground, d2runtime::AdventureGroundType::Water);
    EXPECT_EQ(result.map->cell_at({1, 1})->ground, d2runtime::AdventureGroundType::Unknown);
}

TEST(AdventureNavigationMap, BoundsAndOutsideAccess) {
    auto result = d2adventure::AdventureNavigationMapBuilder().build(world());
    ASSERT_TRUE(result.map);
    EXPECT_TRUE(result.map->contains({0, 0}));
    EXPECT_TRUE(result.map->contains({1, 1}));
    EXPECT_FALSE(result.map->contains({-1, 0}));
    EXPECT_FALSE(result.map->contains({0, 2}));
    EXPECT_EQ(result.map->cell_at({-1, 0}), nullptr);
    EXPECT_FALSE(result.map->traversal_cell_at({2, 0}).has_value());
}

TEST(AdventureNavigationMapBuilder, RoadsUseTypedCollectionAndWarnings) {
    auto input = world();
    input.roads.push_back({"r1", 99, 88, {1, 0}});
    input.roads.push_back({"r2", 99, 88, {1, 0}});
    input.roads.push_back({"bad", 0, 0, {5, 5}});
    const auto result = d2adventure::AdventureNavigationMapBuilder().build(input);
    ASSERT_TRUE(result.success());
    ASSERT_TRUE(result.map);
    EXPECT_TRUE(result.map->cell_at({1, 0})->has_road);
    EXPECT_FALSE(result.map->cell_at({0, 0})->has_road);
    EXPECT_EQ(result.warning_count(), 1u);
    EXPECT_NE(find(result, d2adventure::AdventureNavigationDiagnosticKind::RoadOutOfBounds),
              nullptr);
}

TEST(AdventureNavigationMapBuilder, BlockingFootprintsAndNonBlockingObjects) {
    auto input = world();
    input.map_objects.push_back(blocking("mountain", {{0, 0}, {1, 0}}));
    auto non_blocking = blocking("decor", {{0, 1}});
    non_blocking.blocking = false;
    input.map_objects.push_back(non_blocking);
    const auto result = d2adventure::AdventureNavigationMapBuilder().build(input);
    ASSERT_TRUE(result.success());
    EXPECT_EQ(result.map->cell_at({0, 0})->blocking_object_id, "mountain");
    EXPECT_EQ(result.map->cell_at({1, 0})->blocking_object_id, "mountain");
    EXPECT_FALSE(result.map->cell_at({0, 1})->blocking_object_id.has_value());
}

TEST(AdventureNavigationMapBuilder, StackMapObjectsAreIgnored) {
    auto input = world();
    auto object = blocking("generic-stack", {{0, 0}});
    object.kind = d2runtime::AdventureMapObjectKind::Stack;
    input.map_objects.push_back(object);
    const auto result = d2adventure::AdventureNavigationMapBuilder().build(input);
    ASSERT_TRUE(result.success());
    EXPECT_FALSE(result.map->cell_at({0, 0})->blocking_object_id.has_value());
}

TEST(AdventureNavigationMapBuilder, MalformedBlockingFootprintsFailClosed) {
    auto input = world();
    input.map_objects.push_back(blocking("empty", {}));
    input.map_objects.push_back(blocking("outside", {{4, 4}}));
    const auto result = d2adventure::AdventureNavigationMapBuilder().build(input);
    EXPECT_FALSE(result.success());
    EXPECT_FALSE(result.map.has_value());
    EXPECT_NE(find(result,
                   d2adventure::AdventureNavigationDiagnosticKind::BlockingObjectMissingFootprint),
              nullptr);
    EXPECT_NE(
        find(result,
             d2adventure::AdventureNavigationDiagnosticKind::BlockingObjectFootprintOutOfBounds),
        nullptr);
}

TEST(AdventureNavigationMapBuilder, BlockingOverlapKeepsFirstAndWarns) {
    auto input = world();
    input.map_objects.push_back(blocking("first", {{0, 0}}));
    input.map_objects.push_back(blocking("second", {{0, 0}}));
    const auto result = d2adventure::AdventureNavigationMapBuilder().build(input);
    ASSERT_TRUE(result.success());
    EXPECT_EQ(result.map->cell_at({0, 0})->blocking_object_id, "first");
    const auto* diagnostic =
        find(result, d2adventure::AdventureNavigationDiagnosticKind::OverlappingBlockingObjects);
    ASSERT_NE(diagnostic, nullptr);
    EXPECT_EQ(diagnostic->object_id, "second");
    EXPECT_EQ(diagnostic->conflicting_object_id, "first");
}

TEST(AdventureNavigationMapBuilder, VisibleAndContainedStacks) {
    auto input = world();
    input.stacks.push_back(stack("visible", {1, 1}));
    input.stacks.push_back(stack("contained", {0, 0}, "CITY"));
    const auto result = d2adventure::AdventureNavigationMapBuilder().build(input);
    ASSERT_TRUE(result.success());
    EXPECT_EQ(result.map->cell_at({1, 1})->occupying_stack_id, "visible");
    EXPECT_FALSE(result.map->cell_at({0, 0})->occupying_stack_id.has_value());
}

TEST(AdventureNavigationMapBuilder, StackDiagnosticsAndIdentity) {
    auto input = world();
    input.stacks.push_back(stack("first", {0, 0}));
    input.stacks.push_back(stack("second", {0, 0}));
    input.stacks.push_back(stack("outside", {9, 9}));
    const auto result = d2adventure::AdventureNavigationMapBuilder().build(input);
    EXPECT_FALSE(result.success());
    EXPECT_FALSE(result.map.has_value());
    EXPECT_NE(find(result, d2adventure::AdventureNavigationDiagnosticKind::OverlappingStacks),
              nullptr);
    EXPECT_NE(find(result, d2adventure::AdventureNavigationDiagnosticKind::StackOutOfBounds),
              nullptr);
}

TEST(AdventureNavigationMapBuilder, StackOnBlockingObjectIsError) {
    auto input = world();
    input.map_objects.push_back(blocking("mountain", {{0, 0}}));
    input.stacks.push_back(stack("stack", {0, 0}));
    const auto result = d2adventure::AdventureNavigationMapBuilder().build(input);
    EXPECT_FALSE(result.success());
    EXPECT_FALSE(result.map.has_value());
    const auto* diagnostic =
        find(result, d2adventure::AdventureNavigationDiagnosticKind::StackOnBlockingObject);
    ASSERT_NE(diagnostic, nullptr);
    EXPECT_EQ(diagnostic->object_id, "stack");
    EXPECT_EQ(diagnostic->conflicting_object_id, "mountain");
}

TEST(AdventureNavigationMap, TraversalOccupancyAndIgnoringOwnStack) {
    auto input = world();
    input.stacks.push_back(stack("stack", {0, 0}));
    input.map_objects.push_back(blocking("mountain", {{1, 1}}));
    auto result = d2adventure::AdventureNavigationMapBuilder().build(input);
    ASSERT_TRUE(result.map);
    ASSERT_EQ(result.map->traversal_cell_at({0, 0})->occupancy,
              d2adventure::AdventureCellOccupancy::OccupiedByStack);
    EXPECT_EQ(result.map->traversal_cell_at({0, 0}, "stack")->occupancy,
              d2adventure::AdventureCellOccupancy::Free);
    EXPECT_EQ(result.map->traversal_cell_at({0, 0}, "other")->occupancy,
              d2adventure::AdventureCellOccupancy::OccupiedByStack);
    EXPECT_EQ(result.map->traversal_cell_at({1, 1})->occupancy,
              d2adventure::AdventureCellOccupancy::BlockingObject);
}

TEST(AdventureNavigationMap, TraversalFactsFeedMovementPolicy) {
    auto input = world();
    input.roads.push_back({"road", 0, 0, {0, 0}});
    auto result = d2adventure::AdventureNavigationMapBuilder().build(input);
    ASSERT_TRUE(result.map);
    const auto traversal = result.map->traversal_cell_at({0, 0});
    ASSERT_TRUE(traversal);
    const auto decision = d2adventure::evaluate_adventure_movement_cell(
        d2adventure::AdventureMovementProfile::Walking, *traversal);
    EXPECT_TRUE(decision.passable);
    EXPECT_EQ(decision.movement_cost, 1);
}

TEST(AdventureNavigationDiagnostics, SeverityTableAndResultCounts) {
    const auto errors = {
        d2adventure::AdventureNavigationDiagnosticKind::InvalidMapDimensions,
        d2adventure::AdventureNavigationDiagnosticKind::TerrainDimensionsMismatch,
        d2adventure::AdventureNavigationDiagnosticKind::TerrainTileCountMismatch,
        d2adventure::AdventureNavigationDiagnosticKind::BlockingObjectMissingFootprint,
        d2adventure::AdventureNavigationDiagnosticKind::BlockingObjectFootprintOutOfBounds,
        d2adventure::AdventureNavigationDiagnosticKind::StackOutOfBounds,
        d2adventure::AdventureNavigationDiagnosticKind::OverlappingStacks,
        d2adventure::AdventureNavigationDiagnosticKind::StackOnBlockingObject,
    };
    for (const auto kind : errors)
        EXPECT_TRUE(d2adventure::is_adventure_navigation_error(kind));
    EXPECT_FALSE(d2adventure::is_adventure_navigation_error(
        d2adventure::AdventureNavigationDiagnosticKind::RoadOutOfBounds));
    EXPECT_FALSE(d2adventure::is_adventure_navigation_error(
        d2adventure::AdventureNavigationDiagnosticKind::OverlappingBlockingObjects));

    auto input = world();
    input.roads.push_back({"bad", 0, 0, {9, 9}});
    const auto result = d2adventure::AdventureNavigationMapBuilder().build(input);
    EXPECT_TRUE(result.success());
    EXPECT_EQ(result.error_count(), 0u);
    EXPECT_EQ(result.warning_count(), 1u);
}
