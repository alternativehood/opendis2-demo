#include <d2adventure_render/adventure_render_types.hpp>
#include <d2adventure_render/map_geometry.hpp>
#include <d2adventure_render/map_preparer.hpp>
#include <d2adventure_render/render_graph.hpp>
#include <d2adventure_render/terrain/road_asset_catalog.hpp>

#include <d2engine/assets/image_asset_key.hpp>
#include <d2engine/assets/render_graph_asset_collector.hpp>

#include <d2runtime/AdventureWorldState.hpp>

#include <gtest/gtest.h>

#include <set>
#include <string>
#include <vector>

namespace d2ar = d2engine::adventure_render;
namespace {
const auto kTestGeo = d2ar::AdventureMapGeometry::from_source(10, 10);
} // namespace

// ========================================================================
// 1. Logical-name parser tests
// ========================================================================

TEST(RoadRendering, ParseRoadLogicalNameAcceptsValid) {
    EXPECT_EQ(d2ar::parse_road_logical_name("ROAD0000"), 0);
    EXPECT_EQ(d2ar::parse_road_logical_name("ROAD0100"), 1);
    EXPECT_EQ(d2ar::parse_road_logical_name("ROAD0900"), 9);
    EXPECT_EQ(d2ar::parse_road_logical_name("ROAD1000"), 10);
    EXPECT_EQ(d2ar::parse_road_logical_name("ROAD1500"), 15);
}

TEST(RoadRendering, ParseRoadLogicalNameRejectsOutOfRange) {
    EXPECT_EQ(d2ar::parse_road_logical_name("ROAD1600"), -1);
    EXPECT_EQ(d2ar::parse_road_logical_name("ROAD9900"), -1);
}

TEST(RoadRendering, ParseRoadLogicalNameRejectsInvalidFormat) {
    EXPECT_EQ(d2ar::parse_road_logical_name("ROAD0101"), -1);
    EXPECT_EQ(d2ar::parse_road_logical_name("ROAD0010"), -1);
    EXPECT_EQ(d2ar::parse_road_logical_name("ROAD000"), -1);
    EXPECT_EQ(d2ar::parse_road_logical_name("ROAD00000"), -1);
    EXPECT_EQ(d2ar::parse_road_logical_name("ROADAA00"), -1);
    EXPECT_EQ(d2ar::parse_road_logical_name("XROAD0100"), -1);
    EXPECT_EQ(d2ar::parse_road_logical_name("ROAD1"), -1);
    EXPECT_EQ(d2ar::parse_road_logical_name("road0000"), -1);
}

// ========================================================================
// 2. Catalog lookup tests
// ========================================================================

TEST(RoadRendering, CatalogFindResolvesIndependently) {
    d2ar::RoadAssetCatalog catalog;

    d2ar::RoadAssetVisual v0;
    v0.logical_sprite = "ROAD0000";
    v0.width = 62;
    v0.height = 32;
    catalog.visuals[0] = v0;

    d2ar::RoadAssetVisual v15;
    v15.logical_sprite = "ROAD1500";
    v15.width = 62;
    v15.height = 32;
    catalog.visuals[15] = v15;

    EXPECT_NE(catalog.find(0), nullptr);
    EXPECT_EQ(catalog.find(0)->logical_sprite, "ROAD0000");

    EXPECT_NE(catalog.find(15), nullptr);
    EXPECT_EQ(catalog.find(15)->logical_sprite, "ROAD1500");

    EXPECT_EQ(catalog.find(16), nullptr);
    EXPECT_EQ(catalog.find(-1), nullptr);
}

// ========================================================================
// 3. VAR independence regression
// ========================================================================

TEST(RoadRendering, SameIndexDifferentVarResolveToSameVisual) {
    d2ar::RoadAssetCatalog catalog;

    d2ar::RoadAssetVisual v3;
    v3.logical_sprite = "ROAD0300";
    v3.width = 62;
    v3.height = 32;
    catalog.visuals[3] = v3;

    const auto* a = catalog.find(3);
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->logical_sprite, "ROAD0300");

    const auto* b = catalog.find(3);
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->logical_sprite, "ROAD0300");
    EXPECT_EQ(a, b);
}

TEST(RoadRendering, DifferentVarRoadsProduceSeparatePrimitives) {
    d2ar::RoadAssetCatalog catalog;

    d2ar::RoadAssetVisual v3;
    v3.logical_sprite = "ROAD0300";
    v3.width = 60;
    v3.height = 30;
    catalog.visuals[3] = v3;

    d2ar::AdventureMapPreparer preparer(kTestGeo);
    preparer.add_contributor(d2ar::make_road_contributor(catalog));

    d2runtime::AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;

    d2runtime::AdventureRoad road_a;
    road_a.id = "road_a";
    road_a.index = 3;
    road_a.variant = 0;
    road_a.position = {2, 3};
    world.roads.push_back(road_a);

    d2runtime::AdventureRoad road_b;
    road_b.id = "road_b";
    road_b.index = 3;
    road_b.variant = 1;
    road_b.position = {5, 6};
    world.roads.push_back(road_b);

    const auto result = preparer.prepare(world);

    ASSERT_EQ(result.graph.ground_overlay.size(), 2U);
    EXPECT_EQ(result.diagnostics.size(), 2U);
    for (const auto& d : result.diagnostics)
        EXPECT_EQ(d.kind, d2ar::PrepareDiagnosticKind::Resolved);

    EXPECT_NE(result.graph.ground_overlay[0].stable_id, result.graph.ground_overlay[1].stable_id);

    EXPECT_EQ(result.graph.ground_overlay[0].record_name, "ROAD0300");
    EXPECT_EQ(result.graph.ground_overlay[1].record_name, "ROAD0300");
}

// ========================================================================
// 4. Tile-centered placement — canonical anchor regression
// ========================================================================

TEST(RoadRendering, DrawOriginIsTileCentered) {
    const int kTileWidth = 64;
    const int kTileHeight = 32;
    auto      geo = d2ar::AdventureMapGeometry::from_source(10, 10, kTileWidth, kTileHeight);

    d2ar::RoadAssetCatalog catalog;

    d2ar::RoadAssetVisual v5;
    v5.logical_sprite = "ROAD0500";
    v5.width = 62;
    v5.height = 32;
    catalog.visuals[5] = v5;

    d2ar::AdventureMapPreparer preparer(geo);
    preparer.add_contributor(d2ar::make_road_contributor(catalog));

    d2runtime::AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.roads.push_back({
        .id = "r1",
        .index = 5,
        .variant = 0,
        .position = {4, 4},
    });

    const auto result = preparer.prepare(world);
    ASSERT_EQ(result.graph.ground_overlay.size(), 1U);
    const auto& prim = result.graph.ground_overlay[0];

    const auto tile_origin = geo.cell_canvas_origin({4, 4});

    const int expected_offset_x = (kTileWidth - v5.width) / 2;   // (64-62)/2 = 1
    const int expected_offset_y = (kTileHeight - v5.height) / 2; // (32-32)/2 = 0

    const int expected_draw_x = tile_origin.x + expected_offset_x;
    const int expected_draw_y = tile_origin.y + expected_offset_y;

    EXPECT_EQ(prim.draw_origin.x, expected_draw_x);
    EXPECT_EQ(prim.draw_origin.y, expected_draw_y);

    EXPECT_EQ(prim.visual_bounds.min_x, expected_draw_x);
    EXPECT_EQ(prim.visual_bounds.min_y, expected_draw_y);
    EXPECT_EQ(prim.visual_bounds.max_x, expected_draw_x + v5.width);
    EXPECT_EQ(prim.visual_bounds.max_y, expected_draw_y + v5.height);

    EXPECT_EQ(prim.src_width, v5.width);
    EXPECT_EQ(prim.src_height, v5.height);

    // Relationship to foot anchor
    const auto foot = geo.cell_foot_anchor({4, 4});
    EXPECT_EQ(foot.x, tile_origin.x + kTileWidth / 2);
    EXPECT_EQ(foot.y, tile_origin.y + kTileHeight);
}

// ========================================================================
// 5. Topology-independent origin regression
// ========================================================================

TEST(RoadRendering, TopologyIndependentDrawOrigin) {
    // Multiple ROAD topologies with identical canvas dimensions must
    // produce identical draw_origin when placed at the same cell.
    d2ar::RoadAssetCatalog catalog;

    constexpr int kW = 62;
    constexpr int kH = 32;

    for (int idx : {2, 8, 9, 10, 11}) {
        d2ar::RoadAssetVisual v;
        v.logical_sprite = "ROAD" + std::string(idx < 10 ? "0" : "") + std::to_string(idx) + "00";
        v.width = kW;
        v.height = kH;
        catalog.visuals[idx] = v;
    }

    const d2runtime::MapCellCoord cell{4, 4};

    for (int idx : {2, 8, 9, 10, 11}) {
        d2ar::AdventureMapPreparer preparer(kTestGeo);
        preparer.add_contributor(d2ar::make_road_contributor(catalog));

        d2runtime::AdventureWorldState world;
        world.map_width = 10;
        world.map_height = 10;
        world.roads.push_back({
            .id = "r" + std::to_string(idx),
            .index = idx,
            .variant = 0,
            .position = cell,
        });

        const auto result = preparer.prepare(world);
        ASSERT_EQ(result.graph.ground_overlay.size(), 1U);
        const auto& prim = result.graph.ground_overlay[0];

        const auto tile_origin = kTestGeo.cell_canvas_origin(cell);
        const int  expected_x = tile_origin.x + (kTestGeo.tile_width - kW) / 2;
        const int  expected_y = tile_origin.y + (kTestGeo.tile_height - kH) / 2;

        EXPECT_EQ(prim.draw_origin.x, expected_x) << "topology " << idx << " draw_origin.x differs";
        EXPECT_EQ(prim.draw_origin.y, expected_y) << "topology " << idx << " draw_origin.y differs";
    }
}

// ========================================================================
// 6. Adjacent-cell seam invariant
// ========================================================================

TEST(RoadRendering, AdjacentCellPlacementDeltaDependsOnlyOnCellGeometry) {
    d2ar::RoadAssetCatalog catalog;

    constexpr int kW = 62;
    constexpr int kH = 32;

    for (int idx : {0, 3, 5, 8}) {
        d2ar::RoadAssetVisual v;
        v.logical_sprite = "ROAD" + std::string(idx < 10 ? "0" : "") + std::to_string(idx) + "00";
        v.width = kW;
        v.height = kH;
        catalog.visuals[idx] = v;
    }

    // Horizontal neighbor: (3,3) → (4,3)
    {
        const d2runtime::MapCellCoord cell_a{3, 3};
        const d2runtime::MapCellCoord cell_b{4, 3};

        d2ar::AdventureMapPreparer preparer(kTestGeo);
        preparer.add_contributor(d2ar::make_road_contributor(catalog));

        d2runtime::AdventureWorldState world;
        world.map_width = 10;
        world.map_height = 10;
        world.roads.push_back({.id = "ra", .index = 0, .variant = 0, .position = cell_a});
        world.roads.push_back({.id = "rb", .index = 8, .variant = 0, .position = cell_b});

        const auto result = preparer.prepare(world);
        ASSERT_EQ(result.graph.ground_overlay.size(), 2U);

        const int dx_placement = result.graph.ground_overlay[1].draw_origin.x -
                                 result.graph.ground_overlay[0].draw_origin.x;
        const int dy_placement = result.graph.ground_overlay[1].draw_origin.y -
                                 result.graph.ground_overlay[0].draw_origin.y;

        const auto origin_a = kTestGeo.cell_canvas_origin(cell_a);
        const auto origin_b = kTestGeo.cell_canvas_origin(cell_b);

        const int dx_geometry = origin_b.x - origin_a.x;
        const int dy_geometry = origin_b.y - origin_a.y;

        EXPECT_EQ(dx_placement, dx_geometry);
        EXPECT_EQ(dy_placement, dy_geometry);
    }

    // Vertical neighbor: (3,3) → (3,4)
    {
        const d2runtime::MapCellCoord cell_a{3, 3};
        const d2runtime::MapCellCoord cell_b{3, 4};

        d2ar::AdventureMapPreparer preparer(kTestGeo);
        preparer.add_contributor(d2ar::make_road_contributor(catalog));

        d2runtime::AdventureWorldState world;
        world.map_width = 10;
        world.map_height = 10;
        world.roads.push_back({.id = "ra", .index = 3, .variant = 0, .position = cell_a});
        world.roads.push_back({.id = "rb", .index = 5, .variant = 0, .position = cell_b});

        const auto result = preparer.prepare(world);
        ASSERT_EQ(result.graph.ground_overlay.size(), 2U);

        const int dx_placement = result.graph.ground_overlay[1].draw_origin.x -
                                 result.graph.ground_overlay[0].draw_origin.x;
        const int dy_placement = result.graph.ground_overlay[1].draw_origin.y -
                                 result.graph.ground_overlay[0].draw_origin.y;

        const auto origin_a = kTestGeo.cell_canvas_origin(cell_a);
        const auto origin_b = kTestGeo.cell_canvas_origin(cell_b);

        const int dx_geometry = origin_b.x - origin_a.x;
        const int dy_geometry = origin_b.y - origin_a.y;

        EXPECT_EQ(dx_placement, dx_geometry);
        EXPECT_EQ(dy_placement, dy_geometry);
    }
}

// ========================================================================
// 7. Cell canvas origin invariant
// ========================================================================

TEST(RoadRendering, CellCanvasOriginConsistency) {
    const d2runtime::MapCellCoord cell{5, 3};

    const auto origin = kTestGeo.cell_canvas_origin(cell);
    const auto foot = kTestGeo.cell_foot_anchor(cell);

    EXPECT_EQ(foot.x, origin.x + kTestGeo.half_tile_width);
    EXPECT_EQ(foot.y, origin.y + kTestGeo.tile_height);

    // Origin of (0,0) must be at normalized canvas top-left
    const auto origin00 = kTestGeo.cell_canvas_origin({0, 0});
    const auto tile00 = kTestGeo.project_cell({0, 0});
    EXPECT_EQ(origin00.x, tile00.x - kTestGeo.min_world_x);
    EXPECT_EQ(origin00.y, tile00.y - kTestGeo.min_world_y);
}

// ========================================================================
// 8. GroundOverlay regression
// ========================================================================

TEST(RoadRendering, ResolvedRoadGoesToGroundOverlayNotWorld) {
    d2ar::RoadAssetCatalog catalog;

    d2ar::RoadAssetVisual v0;
    v0.logical_sprite = "ROAD0000";
    v0.width = 62;
    v0.height = 32;
    catalog.visuals[0] = v0;

    d2ar::AdventureMapPreparer preparer(kTestGeo);
    preparer.add_contributor(d2ar::make_road_contributor(catalog));

    d2runtime::AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.roads.push_back({
        .id = "r1",
        .index = 0,
        .variant = 0,
        .position = {3, 3},
    });

    const auto result = preparer.prepare(world);

    EXPECT_EQ(result.graph.ground_overlay.size(), 1U);
    EXPECT_EQ(result.graph.world.size(), 0U);

    const auto& prim = result.graph.ground_overlay[0];
    EXPECT_EQ(prim.phase, d2ar::AdventureRenderPhase::GroundOverlay);
    ASSERT_EQ(prim.footprint.size(), 1U);
    EXPECT_EQ(prim.footprint[0].x, 3);
    EXPECT_EQ(prim.footprint[0].y, 3);
    EXPECT_EQ(prim.depth_anchor.x, 3);
    EXPECT_EQ(prim.depth_anchor.y, 3);

    EXPECT_TRUE(result.pick_entries.empty());
}

// ========================================================================
// 9. Unresolved behavior
// ========================================================================

TEST(RoadRendering, UnresolvedRoadProducesNoPrimitives) {
    d2ar::RoadAssetCatalog catalog;

    d2ar::AdventureMapPreparer preparer(kTestGeo);
    preparer.add_contributor(d2ar::make_road_contributor(catalog));

    d2runtime::AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.roads.push_back({
        .id = "r_bad",
        .index = 7,
        .variant = 0,
        .position = {5, 5},
    });

    const auto result = preparer.prepare(world);

    EXPECT_EQ(result.graph.ground_overlay.size(), 0U);
    EXPECT_EQ(result.graph.world.size(), 0U);
    ASSERT_GE(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics[0].kind, d2ar::PrepareDiagnosticKind::UnresolvedNoSprite);
    EXPECT_TRUE(result.has_unresolved());
}

TEST(RoadRendering, OutOfRangeIndexProducesUnresolved) {
    d2ar::RoadAssetCatalog catalog;

    d2ar::RoadAssetVisual v0;
    v0.logical_sprite = "ROAD0000";
    v0.width = 62;
    v0.height = 32;
    catalog.visuals[0] = v0;

    d2ar::AdventureMapPreparer preparer(kTestGeo);
    preparer.add_contributor(d2ar::make_road_contributor(catalog));

    d2runtime::AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.roads.push_back({
        .id = "r_oob",
        .index = 99,
        .variant = 0,
        .position = {2, 2},
    });

    const auto result = preparer.prepare(world);

    EXPECT_EQ(result.graph.ground_overlay.size(), 0U);
    ASSERT_GE(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics[0].kind, d2ar::PrepareDiagnosticKind::UnresolvedNoSprite);
    EXPECT_EQ(result.diagnostics[0].object_id, "r_oob");
}

// ========================================================================
// 10. Road stable ID structure
// ========================================================================

TEST(RoadRendering, RoadStableIdUsesRoadPrefix) {
    d2ar::RoadAssetCatalog catalog;

    d2ar::RoadAssetVisual v0;
    v0.logical_sprite = "ROAD0000";
    v0.width = 62;
    v0.height = 32;
    catalog.visuals[0] = v0;

    d2ar::AdventureMapPreparer preparer(kTestGeo);
    preparer.add_contributor(d2ar::make_road_contributor(catalog));

    d2runtime::AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.roads.push_back({
        .id = "S143RD0001",
        .index = 0,
        .variant = 0,
        .position = {3, 3},
    });

    const auto result = preparer.prepare(world);
    ASSERT_EQ(result.graph.ground_overlay.size(), 1U);
    EXPECT_EQ(result.graph.ground_overlay[0].stable_id, d2ar::stable_render_id("Road:S143RD0001"));
}

// ========================================================================
// 11. Generic preload phase coverage
// ========================================================================

TEST(RoadRendering, RenderAssetCollectorIncludesAllPhases) {
    d2ar::PreparedAdventureRenderGraph graph;

    {
        d2ar::PreparedAdventureRenderPrimitive p;
        p.phase = d2ar::AdventureRenderPhase::GroundOverlay;
        p.container_path = "Imgs/IsoTerrn.ff";
        p.record_name = "ROAD0100";
        p.footprint.push_back({0, 0});
        p.depth_anchor = {0, 0};
        graph.ground_overlay.push_back(std::move(p));
    }

    {
        d2ar::PreparedAdventureRenderPrimitive p;
        p.phase = d2ar::AdventureRenderPhase::World;
        p.container_path = "Imgs/IsoTerrn.ff";
        p.record_name = "HUF0001";
        p.footprint.push_back({1, 0});
        p.depth_anchor = {1, 0};
        graph.world.push_back(std::move(p));
    }

    {
        d2ar::AdventureAnimationData anim;
        anim.animation_name = "G000MG0027";
        anim.frames = {
            {.record_name = "F1.PNG", .duration_ms = 100},
            {.record_name = "F2.PNG", .duration_ms = 100},
            {.record_name = "F3.PNG", .duration_ms = 100},
        };
        d2ar::PreparedAdventureRenderPrimitive p;
        p.phase = d2ar::AdventureRenderPhase::World;
        p.container_path = "Imgs/IsoCmon.ff";
        p.animation = anim;
        p.footprint.push_back({2, 0});
        p.depth_anchor = {2, 0};
        graph.world.push_back(std::move(p));
    }

    {
        d2ar::PreparedAdventureRenderPrimitive p;
        p.phase = d2ar::AdventureRenderPhase::WorldOverlay;
        p.container_path = "Imgs/IsoTerrn.ff";
        p.record_name = "OVERLAY001";
        p.footprint.push_back({3, 0});
        p.depth_anchor = {3, 0};
        graph.world_overlay.push_back(std::move(p));
    }

    const auto keys = d2engine::collect_adventure_render_asset_keys(graph);

    std::set<std::string> asset_ids;
    for (const auto& key : keys) {
        asset_ids.insert(key.container_path + "/" + key.image_name);
    }

    EXPECT_TRUE(asset_ids.contains("Imgs/IsoTerrn.ff/ROAD0100"));
    EXPECT_TRUE(asset_ids.contains("Imgs/IsoTerrn.ff/HUF0001"));
    EXPECT_TRUE(asset_ids.contains("Imgs/IsoCmon.ff/F1.PNG"));
    EXPECT_TRUE(asset_ids.contains("Imgs/IsoCmon.ff/F2.PNG"));
    EXPECT_TRUE(asset_ids.contains("Imgs/IsoCmon.ff/F3.PNG"));
    EXPECT_TRUE(asset_ids.contains("Imgs/IsoTerrn.ff/OVERLAY001"));

    EXPECT_EQ(asset_ids.size(), 6U);
    EXPECT_EQ(keys.size(), 6U);
}

TEST(RoadRendering, RenderAssetCollectorSkipsEmptyReferences) {
    d2ar::PreparedAdventureRenderGraph graph;

    {
        d2ar::PreparedAdventureRenderPrimitive p;
        p.phase = d2ar::AdventureRenderPhase::GroundOverlay;
        p.footprint.push_back({0, 0});
        p.depth_anchor = {0, 0};
        graph.ground_overlay.push_back(std::move(p));
    }

    const auto keys = d2engine::collect_adventure_render_asset_keys(graph);
    EXPECT_TRUE(keys.empty());
}

// ========================================================================
// 12. Interaction masks do not include roads
// ========================================================================

TEST(RoadRendering, StackInteractionMaskIgnoresRoads) {
    d2ar::RoadAssetCatalog catalog;

    d2ar::RoadAssetVisual v;
    v.logical_sprite = "ROAD0000";
    v.width = 62;
    v.height = 32;
    catalog.visuals[0] = v;

    d2ar::AdventureMapPreparer preparer(kTestGeo);
    preparer.add_contributor(d2ar::make_road_contributor(catalog));

    d2runtime::AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.roads.push_back({
        .id = "r1",
        .index = 0,
        .variant = 0,
        .position = {3, 3},
    });

    const auto result = preparer.prepare(world);

    EXPECT_TRUE(result.pick_entries.empty());
}

// ========================================================================
// 13. Empty test fixture regression (no roads)
// ========================================================================

TEST(RoadRendering, EmptyWorldNoRoads) {
    d2ar::RoadAssetCatalog     catalog;
    d2ar::AdventureMapPreparer preparer(kTestGeo);
    preparer.add_contributor(d2ar::make_road_contributor(catalog));

    d2runtime::AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;

    const auto result = preparer.prepare(world);
    EXPECT_TRUE(result.graph.ground_overlay.empty());
    EXPECT_TRUE(result.graph.world.empty());
    EXPECT_TRUE(result.diagnostics.empty());
    EXPECT_FALSE(result.has_unresolved());
}
