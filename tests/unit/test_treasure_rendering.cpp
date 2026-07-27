#include <d2adventure_render/map_geometry.hpp>
#include <d2adventure_render/map_preparer.hpp>
#include <d2adventure_render/treasure_contributor.hpp>
#include <d2adventure_render/terrain/treasure_asset_catalog.hpp>
#include <d2runtime/AdventureWorldState.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace d2ar = d2engine::adventure_render;
namespace d2rt = d2runtime;

namespace {

struct TreasureMapping {
    d2rt::AdventureTreasurePlacement placement;
    int                              image;
    const char*                      logical_sprite;
};

constexpr TreasureMapping kLandMappings[] = {
    {d2rt::AdventureTreasurePlacement::Land, 0, "G000BG0000100"},
    {d2rt::AdventureTreasurePlacement::Land, 3, "G000BG0000103"},
    {d2rt::AdventureTreasurePlacement::Land, 7, "G000BG0000107"},
};

constexpr TreasureMapping kWaterMappings[] = {
    {d2rt::AdventureTreasurePlacement::Water, 0, "G000BG0000000"},
    {d2rt::AdventureTreasurePlacement::Water, 3, "G000BG0000003"},
};

d2ar::TreasureAssetCatalog make_catalog() {
    d2ar::TreasureAssetCatalog catalog;
    for (const auto& mapping : kLandMappings) {
        d2ar::StaticTreasureVisual visual;
        visual.container_path = "Imgs/IsoCmon.ff";
        visual.logical_sprite = mapping.logical_sprite;
        visual.canvas_foot_x = 100;
        visual.canvas_foot_y = 100;
        visual.canvas_width = 100;
        visual.canvas_height = 100;
        catalog.land_visuals[static_cast<std::size_t>(mapping.image)] = std::move(visual);
    }
    for (const auto& mapping : kWaterMappings) {
        d2ar::StaticTreasureVisual visual;
        visual.container_path = "Imgs/IsoCmon.ff";
        visual.logical_sprite = mapping.logical_sprite;
        visual.canvas_foot_x = 320;
        visual.canvas_foot_y = 320;
        visual.canvas_width = 320;
        visual.canvas_height = 320;
        catalog.water_visuals[static_cast<std::size_t>(mapping.image)] = std::move(visual);
    }
    return catalog;
}

} // namespace

TEST(TreasureRendering, SingleTreasureCreatesOneGroundObjectPrimitive) {
    const auto                 geometry = d2ar::AdventureMapGeometry::from_source(10, 10);
    d2ar::AdventureMapPreparer preparer(geometry);
    preparer.add_contributor(d2ar::make_treasure_contributor(make_catalog()));

    d2runtime::AdventureWorldState world;
    world.treasures.push_back({.id = "S143BG0001",
                               .looter_id = "",
                               .item_ids = {},
                               .position = {3, 4},
                               .image = 7,
                               .placement = d2rt::AdventureTreasurePlacement::Land,
                               .footprint = {{3, 4}}});

    auto result = preparer.prepare(world);

    ASSERT_EQ(result.graph.world.size(), 1u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    const auto& prim = result.graph.world.front();
    EXPECT_EQ(prim.phase, d2ar::AdventureRenderPhase::World);
    EXPECT_EQ(prim.level, d2ar::WorldRenderLevel::GroundObject);
    EXPECT_EQ(prim.local_suborder, 0);
    EXPECT_EQ(prim.container_path, "Imgs/IsoCmon.ff");
    EXPECT_EQ(prim.record_name, "G000BG0000107");
    EXPECT_EQ(prim.src_width, 100);
    EXPECT_EQ(prim.src_height, 100);
    EXPECT_FLOAT_EQ(prim.alpha, 1.0f);

    const auto foot = geometry.cell_foot_anchor({3, 4});
    EXPECT_EQ(prim.draw_origin.x, foot.x - 100);
    EXPECT_EQ(prim.draw_origin.y, foot.y - 100);
}

TEST(TreasureRendering, LandAndWaterUsePlacementSpecificSprites) {
    const auto                 geometry = d2ar::AdventureMapGeometry::from_source(10, 10);
    d2ar::AdventureMapPreparer preparer(geometry);
    preparer.add_contributor(d2ar::make_treasure_contributor(make_catalog()));

    d2runtime::AdventureWorldState world;
    world.treasures.push_back({.id = "S143BG0000",
                               .position = {1, 1},
                               .image = 0,
                               .placement = d2rt::AdventureTreasurePlacement::Land,
                               .footprint = {{1, 1}}});
    world.treasures.push_back({.id = "S143BG0001",
                               .position = {2, 1},
                               .image = 0,
                               .placement = d2rt::AdventureTreasurePlacement::Water,
                               .footprint = {{2, 1}}});
    world.treasures.push_back({.id = "S143BG0003",
                               .position = {3, 1},
                               .image = 3,
                               .placement = d2rt::AdventureTreasurePlacement::Land,
                               .footprint = {{3, 1}}});
    world.treasures.push_back({.id = "S143BG0004",
                               .position = {4, 1},
                               .image = 3,
                               .placement = d2rt::AdventureTreasurePlacement::Water,
                               .footprint = {{4, 1}}});
    world.treasures.push_back({.id = "S143BG0007",
                               .position = {5, 1},
                               .image = 7,
                               .placement = d2rt::AdventureTreasurePlacement::Land,
                               .footprint = {{5, 1}}});
    world.treasures.push_back({.id = "S143BG0011",
                               .position = {6, 1},
                               .image = 7,
                               .placement = d2rt::AdventureTreasurePlacement::Water,
                               .footprint = {{6, 1}}});

    auto result = preparer.prepare(world);

    ASSERT_EQ(result.graph.world.size(), 5u);
    std::unordered_map<std::string, std::string> record_names;
    for (const auto& prim : result.graph.world) {
        record_names[prim.debug_label] = prim.record_name;
    }

    EXPECT_EQ(record_names.at("Treasure:S143BG0000"), "G000BG0000100");
    EXPECT_EQ(record_names.at("Treasure:S143BG0001"), "G000BG0000000");
    EXPECT_EQ(record_names.at("Treasure:S143BG0003"), "G000BG0000103");
    EXPECT_EQ(record_names.at("Treasure:S143BG0004"), "G000BG0000003");
    EXPECT_EQ(record_names.at("Treasure:S143BG0007"), "G000BG0000107");
    EXPECT_TRUE(std::any_of(result.diagnostics.begin(), result.diagnostics.end(),
                            [](const d2ar::PrepareDiagnostic& diag) {
                                return diag.kind ==
                                           d2ar::PrepareDiagnosticKind::UnresolvedNoSprite &&
                                       diag.message.find("placement=Water") != std::string::npos;
                            }));

    const auto land_foot = geometry.cell_foot_anchor({1, 1});
    const auto water_foot = geometry.cell_foot_anchor({2, 1});
    for (const auto& prim : result.graph.world) {
        if (prim.debug_label == "Treasure:S143BG0000") {
            EXPECT_EQ(prim.draw_origin.x, land_foot.x - 100);
            EXPECT_EQ(prim.draw_origin.y, land_foot.y - 100);
        }
        if (prim.debug_label == "Treasure:S143BG0001") {
            EXPECT_EQ(prim.draw_origin.x, water_foot.x - 320);
            EXPECT_EQ(prim.draw_origin.y, water_foot.y - 320);
        }
    }
}

TEST(TreasureRendering, MultiCellFootprintUsesDepthAnchorFromFootprint) {
    const auto                 geometry = d2ar::AdventureMapGeometry::from_source(10, 10);
    d2ar::AdventureMapPreparer preparer(geometry);
    preparer.add_contributor(d2ar::make_treasure_contributor(make_catalog()));

    d2runtime::AdventureWorldState world;
    world.treasures.push_back({.id = "S143BG0002",
                               .looter_id = "",
                               .item_ids = {},
                               .position = {1, 1},
                               .image = 3,
                               .placement = d2rt::AdventureTreasurePlacement::Land,
                               .footprint = {{1, 1}, {2, 1}, {2, 2}}});

    auto result = preparer.prepare(world);

    ASSERT_EQ(result.graph.world.size(), 1u);
    const auto& prim = result.graph.world.front();
    EXPECT_EQ(prim.depth_anchor, (d2runtime::MapCellCoord{2, 2}));
    EXPECT_EQ(prim.footprint.size(), 3u);
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics.front().kind, d2ar::PrepareDiagnosticKind::Resolved);
}

TEST(TreasureRendering, EmptyFootprintEmitsDiagnosticAndNoPrimitive) {
    const auto                 geometry = d2ar::AdventureMapGeometry::from_source(10, 10);
    d2ar::AdventureMapPreparer preparer(geometry);
    preparer.add_contributor(d2ar::make_treasure_contributor(make_catalog()));

    d2runtime::AdventureWorldState world;
    world.treasures.push_back({.id = "S143BG0003",
                               .looter_id = "",
                               .item_ids = {},
                               .position = {5, 6},
                               .image = 7,
                               .placement = d2rt::AdventureTreasurePlacement::Land,
                               .footprint = {}});

    auto result = preparer.prepare(world);

    EXPECT_TRUE(result.graph.world.empty());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics.front().kind, d2ar::PrepareDiagnosticKind::UnresolvedNoSprite);
    EXPECT_NE(result.diagnostics.front().message.find("reason=empty_footprint"), std::string::npos);
}

TEST(TreasureRendering, UnsupportedImageEmitsDiagnosticAndNoPrimitive) {
    const auto                 geometry = d2ar::AdventureMapGeometry::from_source(10, 10);
    d2ar::AdventureMapPreparer preparer(geometry);
    preparer.add_contributor(d2ar::make_treasure_contributor(make_catalog()));

    d2runtime::AdventureWorldState world;
    world.treasures.push_back({.id = "S143BG0012",
                               .looter_id = "",
                               .item_ids = {},
                               .position = {5, 6},
                               .image = 12,
                               .placement = d2rt::AdventureTreasurePlacement::Land,
                               .footprint = {{5, 6}}});

    auto result = preparer.prepare(world);

    EXPECT_TRUE(result.graph.world.empty());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics.front().kind, d2ar::PrepareDiagnosticKind::UnresolvedNoSprite);
    EXPECT_NE(result.diagnostics.front().message.find("placement=Land"), std::string::npos);
}

TEST(TreasureRendering, WaterUnsupportedImageEmitsDiagnosticAndNoPrimitive) {
    const auto                 geometry = d2ar::AdventureMapGeometry::from_source(10, 10);
    d2ar::AdventureMapPreparer preparer(geometry);
    preparer.add_contributor(d2ar::make_treasure_contributor(make_catalog()));

    d2runtime::AdventureWorldState world;
    world.treasures.push_back({.id = "S143BG0013",
                               .looter_id = "",
                               .item_ids = {},
                               .position = {5, 6},
                               .image = 7,
                               .placement = d2rt::AdventureTreasurePlacement::Water,
                               .footprint = {{5, 6}}});

    auto result = preparer.prepare(world);

    EXPECT_TRUE(result.graph.world.empty());
    ASSERT_EQ(result.diagnostics.size(), 1u);
    EXPECT_EQ(result.diagnostics.front().kind, d2ar::PrepareDiagnosticKind::UnresolvedNoSprite);
    EXPECT_NE(result.diagnostics.front().message.find("placement=Water"), std::string::npos);
}

TEST(TreasureRendering, GenericBagOnlyWorldDoesNotRenderTreasurePrimitive) {
    const auto                 geometry = d2ar::AdventureMapGeometry::from_source(10, 10);
    d2ar::AdventureMapPreparer preparer(geometry);
    preparer.add_contributor(d2ar::make_treasure_contributor(make_catalog()));

    d2runtime::AdventureWorldState world;
    world.map_objects.push_back(
        {.id = "S143BG0004", .kind = d2runtime::AdventureMapObjectKind::Bag, .position = {2, 2}});

    auto result = preparer.prepare(world);

    EXPECT_TRUE(result.graph.world.empty());
    EXPECT_TRUE(result.diagnostics.empty());
}
