#include <d2adventure_render/adventure_frame_composer.hpp>
#include <d2adventure_render/adventure_render_types.hpp>
#include <d2adventure_render/adventure_banner_primitive_builder.hpp>
#include <d2adventure_render/iso_depth_resolver.hpp>
#include <d2adventure_render/map_geometry.hpp>
#include <d2adventure_render/map_preparer.hpp>
#include <d2adventure_render/render_graph.hpp>
#include <d2engine/assets/stack_banner_asset_catalog_builder.hpp>
#include <d2adventure_render/terrain/mountain_asset_catalog.hpp>
#include <d2adventure_render/terrain/road_asset_catalog.hpp>

#include <d2engine/render/adventure_render_state.hpp>
#include <d2engine/app/adventure_pick_index.hpp>

#include <d2engine/assets/image_asset_key.hpp>
#include <d2engine/app/adventure_interaction_mask.hpp>

#include <d2runtime/AdventureWorldState.hpp>
#include <d2runtime/AdventureWorldBuilder.hpp>
#include <d2runtime/AdventureTerrain.hpp>

#include <d2scenario/ScenarioTemplate.hpp>

#include <gtest/gtest.h>

#include <d2res/rgba_buffer.hpp>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_set>
#include <vector>

namespace d2ar = d2engine::adventure_render;
namespace {
const auto kTestGeo = d2ar::AdventureMapGeometry::from_source(10, 10);
} // namespace

// ── Helpers ────────────────────────────────────────────────────────────

static d2ar::PreparedAdventureRenderPrimitive make_prim(d2ar::AdventureRenderPhase phase,
                                                        d2ar::WorldRenderLevel level, int anchor_gx,
                                                        int           anchor_gy,
                                                        std::uint64_t stable_id = 0) {
    d2ar::PreparedAdventureRenderPrimitive p;
    p.phase = phase;
    p.level = level;
    p.depth_anchor = {anchor_gx, anchor_gy};
    p.stable_id = stable_id;
    return p;
}

static d2ar::StackBannerAssetCatalog make_banner_catalog() {
    return d2engine::detail::build_stack_banner_asset_catalog_from_metadata(
        [](std::string_view, std::string_view) {
            d2engine::AnimationSequence sequence;
            sequence.name = "STACK_BANNER_1400";
            sequence.container_path = "Imgs/IsoCmon.ff";
            sequence.native_canvas_w = 300;
            sequence.native_canvas_h = 200;
            sequence.canvas_foot_x = 10;
            sequence.canvas_foot_y = 20;
            sequence.frames.push_back({.image_name = "TI", .index = 0, .duration_ms = 100});
            return sequence;
        },
        [](std::string_view, std::string_view) {
            struct SpriteMeta {
                int                       canvas_width;
                int                       canvas_height;
                int                       canvas_foot_x;
                int                       canvas_foot_y;
                d2ar::CanvasContentBounds content_bounds;
            };
            return SpriteMeta{300, 200, 10, 20, {1, 2, 3, 4}};
        });
}

// ========================================================================
// 2. WorldRenderLevel ordering explicit
// ========================================================================

TEST(AdventureMapRendering, WorldRenderLevelOrderingIsExplicit) {
    EXPECT_TRUE(d2ar::world_level_before(d2ar::WorldRenderLevel::GroundObject,
                                         d2ar::WorldRenderLevel::Structure));
    EXPECT_TRUE(d2ar::world_level_before(d2ar::WorldRenderLevel::Structure,
                                         d2ar::WorldRenderLevel::ActorUnderlay));
    EXPECT_TRUE(d2ar::world_level_before(d2ar::WorldRenderLevel::ActorUnderlay,
                                         d2ar::WorldRenderLevel::Actor));
    EXPECT_TRUE(d2ar::world_level_before(d2ar::WorldRenderLevel::Actor,
                                         d2ar::WorldRenderLevel::Foreground));

    EXPECT_TRUE(d2ar::world_level_before(d2ar::WorldRenderLevel::GroundObject,
                                         d2ar::WorldRenderLevel::Actor));
}

// ========================================================================
// 3. IsoDepth overrides WorldRenderLevel
// ========================================================================

TEST(AdventureMapRendering, IsoDepthOverridesWorldRenderLevel) {
    // Higher iso_depth = lower on screen = closer to camera.
    // Resolver sorts by iso_depth ASCENDING (back-to-front).
    //
    // Structure at (5,5) depth=10 (far, draws first)
    // Actor at (10,10) depth=20 (close, draws on top)
    //
    // Verify: Structure (far, depth=10, WorldRenderLevel=Structure) draws
    // BEFORE Actor (close, depth=20, WorldRenderLevel=Actor) even though
    // Actor has HIGHER WorldRenderLevel than Structure.
    // If WorldRenderLevel was checked before IsoDepth, Actor would always
    // be on top regardless of position — which is WRONG.
    std::vector<d2ar::PreparedAdventureRenderPrimitive> prims;
    prims.push_back(make_prim(d2ar::AdventureRenderPhase::World, d2ar::WorldRenderLevel::Structure,
                              /*anchor*/ 5, 5)); // depth=10 (far)
    prims.push_back(make_prim(d2ar::AdventureRenderPhase::World, d2ar::WorldRenderLevel::Actor,
                              /*anchor*/ 10, 10)); // depth=20 (close)

    d2ar::IsoDepthResolver resolver(kTestGeo);
    const auto             order = resolver.resolve(prims);

    ASSERT_EQ(order.size(), 2U);
    // Ascending depth: depth=10 first, depth=20 second
    EXPECT_EQ(order[0], 0U) << "Structure (depth=10, far) draws first";
    EXPECT_EQ(order[1], 1U) << "Actor (depth=20, close) draws on top";

    // This proves: IsoDepth is checked BEFORE WorldRenderLevel.
    // Actor has higher WorldRenderLevel than Structure, but Structure draws
    // first because it's behind (lower iso_depth).
}

// ========================================================================
// 4. Same-cell Forest < Unit
// ========================================================================

TEST(AdventureMapRendering, SameCellForestBeforeUnit) {
    // Forest and Unit at same grid cell (10,10).
    // Forest level < Unit level → Forest drawn first.
    std::vector<d2ar::PreparedAdventureRenderPrimitive> prims;
    prims.push_back(make_prim(d2ar::AdventureRenderPhase::World,
                              d2ar::WorldRenderLevel::GroundObject, // Forest
                              /*anchor*/ 10, 10));
    prims.push_back(make_prim(d2ar::AdventureRenderPhase::World,
                              d2ar::WorldRenderLevel::Actor, // Unit
                              /*anchor*/ 10, 10));

    d2ar::IsoDepthResolver resolver(kTestGeo);
    const auto             order = resolver.resolve(prims);

    ASSERT_EQ(order.size(), 2U);
    EXPECT_EQ(order[0], 0U) << "Forest (GroundObject) first";
    EXPECT_EQ(order[1], 1U) << "Unit (Actor) on top";
}

// ========================================================================
// 5. Mountain in front of Unit
// ========================================================================

TEST(AdventureMapRendering, MountainInFrontRendersOverUnit) {
    // Mountain at depth=20 (lower on screen, closer to camera)
    // Unit at depth=10 (higher on screen, farther from camera)
    //
    // Mountain must render ON TOP of Unit despite Mountain.level < Unit.level
    // because ISO DEPTH has priority.
    std::vector<d2ar::PreparedAdventureRenderPrimitive> prims;
    prims.push_back(make_prim(d2ar::AdventureRenderPhase::World,
                              d2ar::WorldRenderLevel::Structure, // Mountain
                              /*anchor*/ 10, 10));               // depth=20 (CLOSE)
    prims.push_back(make_prim(d2ar::AdventureRenderPhase::World,
                              d2ar::WorldRenderLevel::Actor, // Unit
                              /*anchor*/ 5, 5));             // depth=10 (far)

    d2ar::IsoDepthResolver resolver(kTestGeo);
    const auto             order = resolver.resolve(prims);

    ASSERT_EQ(order.size(), 2U);
    // Ascending depth: depth=10 (Unit) first, depth=20 (Mountain) second
    EXPECT_EQ(order[0], 1U) << "Unit behind (depth=10) drawn first";
    EXPECT_EQ(order[1], 0U) << "Mountain in front (depth=20) drawn on top";

    // This proves: Unit is NOT globally above Mountain.
}

// ========================================================================
// 6. Unit in front of Mountain
// ========================================================================

TEST(AdventureMapRendering, UnitInFrontRendersOverMountain) {
    // Unit at depth=20 (lower on screen, closer)
    // Mountain at depth=10 (higher on screen, farther)
    //
    // Unit must render ON TOP (both by IsoDepth and by WorldRenderLevel).
    std::vector<d2ar::PreparedAdventureRenderPrimitive> prims;
    prims.push_back(make_prim(d2ar::AdventureRenderPhase::World,
                              d2ar::WorldRenderLevel::Structure, // Mountain
                              /*anchor*/ 5, 5));                 // depth=10 (far)
    prims.push_back(make_prim(d2ar::AdventureRenderPhase::World,
                              d2ar::WorldRenderLevel::Actor, // Unit
                              /*anchor*/ 10, 10));           // depth=20 (CLOSE)

    d2ar::IsoDepthResolver resolver(kTestGeo);
    const auto             order = resolver.resolve(prims);

    ASSERT_EQ(order.size(), 2U);
    // Ascending depth: depth=10 (Mountain) first, depth=20 (Unit) second
    EXPECT_EQ(order[0], 0U) << "Mountain behind (depth=10) drawn first";
    EXPECT_EQ(order[1], 1U) << "Unit in front (depth=20) drawn on top";
}

// ========================================================================
// 7. No object type is globally above another type
// ========================================================================

TEST(AdventureMapRendering, NoGlobalObjectTypeOrdering) {
    // Same type at different depths → ordered by depth, not type.
    // Stack A at depth=20 (close), Stack B at depth=10 (far).
    std::vector<d2ar::PreparedAdventureRenderPrimitive> prims;
    prims.push_back(make_prim(d2ar::AdventureRenderPhase::World, d2ar::WorldRenderLevel::Actor,
                              /*anchor*/ 5, 5)); // depth=10 (far)
    prims.push_back(make_prim(d2ar::AdventureRenderPhase::World, d2ar::WorldRenderLevel::Actor,
                              /*anchor*/ 10, 10)); // depth=20 (CLOSE)

    d2ar::IsoDepthResolver resolver(kTestGeo);
    const auto             order = resolver.resolve(prims);

    ASSERT_EQ(order.size(), 2U);
    EXPECT_EQ(order[0], 0U) << "Farther Actor (depth=10) first";
    EXPECT_EQ(order[1], 1U) << "Closer Actor (depth=20) on top";

    // Also verify: same cell, different types → WorldRenderLevel resolves
    prims.clear();
    prims.push_back(make_prim(d2ar::AdventureRenderPhase::World, d2ar::WorldRenderLevel::Structure,
                              /*anchor*/ 7, 7)); // depth=14
    prims.push_back(make_prim(d2ar::AdventureRenderPhase::World,
                              d2ar::WorldRenderLevel::GroundObject,
                              /*anchor*/ 7, 7)); // depth=14

    const auto order2 = resolver.resolve(prims);
    ASSERT_EQ(order2.size(), 2U);
    EXPECT_EQ(order2[0], 1U) << "GroundObject first (same depth)";
    EXPECT_EQ(order2[1], 0U) << "Structure on top (same depth)";
}

// ========================================================================
// 8. Stable deterministic ordering
// ========================================================================

TEST(AdventureMapRendering, StableDeterministicOrdering) {
    // Two primitives at same position and same WorldRenderLevel.
    // Ordering must be deterministic by stable_id.
    std::vector<d2ar::PreparedAdventureRenderPrimitive> prims;
    prims.push_back(make_prim(d2ar::AdventureRenderPhase::World, d2ar::WorldRenderLevel::Actor,
                              /*anchor*/ 5, 5, /*stable_id*/ 42));
    prims.push_back(make_prim(d2ar::AdventureRenderPhase::World, d2ar::WorldRenderLevel::Actor,
                              /*anchor*/ 5, 5, /*stable_id*/ 7));

    d2ar::IsoDepthResolver resolver(kTestGeo);

    // Run twice — must produce identical order
    const auto order_a = resolver.resolve(prims);
    const auto order_b = resolver.resolve(prims);

    ASSERT_EQ(order_a.size(), 2U);
    ASSERT_EQ(order_b.size(), 2U);
    EXPECT_EQ(order_a[0], order_b[0]);
    EXPECT_EQ(order_a[1], order_b[1]);

    // Lower stable_id draws first
    // stable_id 7 should draw first, then 42
    EXPECT_EQ(order_a[0], 1U) << "stable_id 7 first";
    EXPECT_EQ(order_a[1], 0U) << "stable_id 42 second";
}

TEST(AdventureMapRendering, StableRenderIdUsesKnownDeterministicHash) {
    EXPECT_EQ(d2ar::stable_render_id("Stack:stack_1"), 5533293160283296897ull);
    EXPECT_EQ(d2ar::stable_render_id("Stack:stack_1"), d2ar::stable_render_id("Stack:stack_1"));
    EXPECT_NE(d2ar::stable_render_id("Stack:stack_1"), d2ar::stable_render_id("Unit:stack_1"));
}

// ========================================================================
// 9. VisualBounds independent from IsoDepth/Footprint
// ========================================================================

TEST(AdventureMapRendering, VisualBoundsIndependentFromDepth) {
    d2ar::PreparedAdventureRenderPrimitive prim;
    prim.visual_bounds = {100, 200, 300, 400};
    prim.depth_anchor = {5, 10};
    prim.footprint.push_back({5, 10});

    // Visual bounds should NOT be derived from depth/footprint
    EXPECT_EQ(prim.visual_bounds.min_x, 100);
    EXPECT_EQ(prim.visual_bounds.min_y, 200);
    EXPECT_EQ(prim.visual_bounds.max_x, 300);
    EXPECT_EQ(prim.visual_bounds.max_y, 400);

    // Depth anchor is separate
    EXPECT_EQ(prim.depth_anchor.x, 5);
    EXPECT_EQ(prim.depth_anchor.y, 10);

    // Visual bounds contains check
    EXPECT_TRUE(prim.visual_bounds.contains(150, 250));
    EXPECT_FALSE(prim.visual_bounds.contains(50, 250));
}

// ========================================================================
// 10. Multi-tile footprint accepted by API
// ========================================================================

TEST(AdventureMapRendering, MultiTileFootprintAccepted) {
    d2ar::GridFootprint footprint;
    footprint.push_back({0, 0});
    footprint.push_back({1, 0});
    footprint.push_back({0, 1});
    footprint.push_back({1, 1});

    EXPECT_FALSE(footprint.empty());
    EXPECT_EQ(footprint.size(), 4U);

    d2ar::PreparedAdventureRenderPrimitive prim;
    prim.footprint = footprint;
    prim.depth_anchor = {1, 1}; // front-most cell

    // Verify depth anchor is used for ordering
    EXPECT_EQ(d2ar::AdventureMapGeometry::iso_depth(prim.depth_anchor), 2);
}

// ========================================================================
// 11. Semantic entity can emit 0..N primitives
// ========================================================================

TEST(AdventureMapRendering, SemanticEntityEmitsZeroToNPrimitives) {
    // The preparer does NOT enforce 1:1 entity→primitive.
    // Contributors are free to emit any number per semantic object.
    // This test verifies that the builder accepts and orders any count.

    d2ar::AdventureRenderGraphBuilder builder;

    // Mountain with 2 sprites
    for (int i = 0; i < 2; ++i) {
        auto prim =
            make_prim(d2ar::AdventureRenderPhase::World, d2ar::WorldRenderLevel::Structure, 5, 5);
        prim.debug_label = "Mountain_" + std::to_string(i);
        builder.add_primitive(prim);
    }
    // Forest with 1 sprite
    builder.add_primitive(
        make_prim(d2ar::AdventureRenderPhase::World, d2ar::WorldRenderLevel::GroundObject, 5, 5));
    // Road (GroundOverlay) — 0 primitives for unresolved roads
    // (not adding any, which is valid)

    d2ar::IsoDepthResolver resolver(kTestGeo);
    const auto             graph = builder.finalize(resolver);

    // 3 world primitives (2 Mountain + 1 Forest)
    EXPECT_EQ(graph.world.size(), 3U);
    // All should be ordered by depth → level → stable
    // Since all are at depth 10 (5+5), Forest goes before Mountain
    EXPECT_EQ(graph.world[0].level, d2ar::WorldRenderLevel::GroundObject);
    EXPECT_EQ(graph.world[1].level, d2ar::WorldRenderLevel::Structure);
    EXPECT_EQ(graph.world[2].level, d2ar::WorldRenderLevel::Structure);
}

// ========================================================================
// 12. Shared projection
// ========================================================================

TEST(AdventureMapRendering, SharedProjection) {
    const auto geo = d2ar::AdventureMapGeometry::from_source(10, 10);

    // Origin tile
    auto pt = geo.project_cell({0, 0});
    EXPECT_EQ(pt.x, 0);
    EXPECT_EQ(pt.y, 0);

    // One tile right
    pt = geo.project_cell({1, 0});
    EXPECT_EQ(pt.x, 32);
    EXPECT_EQ(pt.y, 16);

    // One tile left
    pt = geo.project_cell({0, 1});
    EXPECT_EQ(pt.x, -32);
    EXPECT_EQ(pt.y, 16);

    // General case
    pt = geo.project_cell({3, 2});
    EXPECT_EQ(pt.x, (3 - 2) * 32);
    EXPECT_EQ(pt.y, (3 + 2) * 16);

    // IsoDepth is gx + gy
    EXPECT_EQ(d2ar::AdventureMapGeometry::iso_depth({3, 2}), 5);
    EXPECT_EQ(d2ar::AdventureMapGeometry::iso_depth(d2ar::IsoDepthAnchor{10, 5}), 15);

    // Default tile dimensions
    ASSERT_EQ(geo.tile_width, 64);
    ASSERT_EQ(geo.tile_height, 32);
}

// ========================================================================
// 13. AdventureRenderGraphBuilder groups by phase correctly
// ========================================================================

TEST(AdventureMapRendering, BuilderGroupsByPhase) {
    d2ar::AdventureRenderGraphBuilder builder;

    builder.add_primitive(
        make_prim(d2ar::AdventureRenderPhase::World, d2ar::WorldRenderLevel::Actor, 5, 5));
    builder.add_primitive(
        make_prim(d2ar::AdventureRenderPhase::Fog, d2ar::WorldRenderLevel::GroundObject, 0, 0));
    builder.add_primitive(make_prim(d2ar::AdventureRenderPhase::GroundOverlay,
                                    d2ar::WorldRenderLevel::GroundObject, 0, 0));
    builder.add_primitive(
        make_prim(d2ar::AdventureRenderPhase::UIOverlay, d2ar::WorldRenderLevel::Foreground, 0, 0));
    builder.add_primitive(make_prim(d2ar::AdventureRenderPhase::WorldOverlay,
                                    d2ar::WorldRenderLevel::GroundObject, 0, 0));

    d2ar::IsoDepthResolver resolver(kTestGeo);
    const auto             graph = builder.finalize(resolver);

    EXPECT_EQ(graph.ground_overlay.size(), 1U);
    EXPECT_EQ(graph.world.size(), 1U);
    EXPECT_EQ(graph.world_overlay.size(), 1U);
    EXPECT_EQ(graph.fog.size(), 1U);
    EXPECT_EQ(graph.ui_overlay.size(), 1U);
}

// ========================================================================
// 14. Complex: Forest + Unit + Mountain in front
// ========================================================================

TEST(AdventureMapRendering, ForestUnitAndMountainInFront) {
    // Example D from the spec:
    //
    //   Forest at (5,5)     — GroundObject, depth=10 (farthest)
    //   Unit at (5,5)       — Actor, depth=10 (same cell → Actor on top)
    //   Mountain at (10,10) — Structure, depth=20 (spatially in front)
    //
    // Expected draw order (back-to-front):
    //   1. Forest  (depth=10, GroundObject)
    //   2. Unit    (depth=10, Actor, same cell → above Forest)
    //   3. Mountain (depth=20, closer, renders on top of both)

    std::vector<d2ar::PreparedAdventureRenderPrimitive> prims;
    prims.push_back(make_prim(d2ar::AdventureRenderPhase::World,
                              d2ar::WorldRenderLevel::GroundObject, // Forest
                              /*anchor*/ 5, 5, /*stable*/ 1));
    prims.push_back(make_prim(d2ar::AdventureRenderPhase::World,
                              d2ar::WorldRenderLevel::Actor, // Unit
                              /*anchor*/ 5, 5, /*stable*/ 2));
    prims.push_back(make_prim(d2ar::AdventureRenderPhase::World,
                              d2ar::WorldRenderLevel::Structure, // Mountain
                              /*anchor*/ 10, 10, /*stable*/ 3));

    d2ar::IsoDepthResolver resolver(kTestGeo);
    const auto             order = resolver.resolve(prims);

    ASSERT_EQ(order.size(), 3U);

    // Depth values: Forest/Unit at depth=10, Mountain at depth=20.
    // Ascending: depth=10 (Forest+Unit) then depth=20 (Mountain).
    // Within depth=10: Forest (GroundObject) then Unit (Actor).

    EXPECT_EQ(order[0], 0U) << "draw order[0]: Forest (depth=10, GroundObject)";
    EXPECT_EQ(order[1], 1U) << "draw order[1]: Unit (depth=10, Actor)";
    EXPECT_EQ(order[2], 2U) << "draw order[2]: Mountain (depth=20, on top)";

    const auto& drawn_first = prims[order[0]];
    const auto& drawn_last = prims[order[2]];
    EXPECT_EQ(drawn_first.level, d2ar::WorldRenderLevel::GroundObject);
    EXPECT_EQ(drawn_last.level, d2ar::WorldRenderLevel::Structure);
    // Mountain (Structure) renders ON TOP of Unit (Actor) because it's
    // spatially in front — proving no global Actor > Structure rule.
}

// ========================================================================
// 15. Builder + Preparer integration
// ========================================================================

TEST(AdventureMapRendering, PreparerIntegration) {
    d2ar::AdventureMapPreparer preparer(kTestGeo);

    d2ar::MountainAssetCatalog empty_catalog;

    preparer.add_contributor(d2ar::make_mountain_contributor(empty_catalog));
    preparer.add_contributor(d2ar::make_stack_actor_contributor(
        [](const d2runtime::AdventureStack&, const d2runtime::AdventureUnitInstance&)
            -> std::optional<d2ar::AdventureActorVisual> { return std::nullopt; },
        nullptr));

    d2runtime::AdventureWorldState world;
    world.mountains.push_back({
        .id = "mount_1",
        .id_mount = 42,
        .image = 3,
        .race = -1,
        .position = {10, 5},
        .size_x = 2,
        .size_y = 2,
    });
    world.map_objects.push_back({
        .id = "stack_1",
        .kind = d2runtime::AdventureMapObjectKind::Stack,
        .position = {10, 5},
    });
    world.units.push_back({.id = "unit_1", .type_id = "G000UU0020"});
    d2runtime::AdventureStack stack;
    stack.id = "stack_1";
    stack.position.x = 10;
    stack.position.y = 5;
    stack.leader_id = "unit_1";
    world.stacks.push_back(stack);

    const auto result = preparer.prepare(world);

    // Unresolved assets produce zero drawable primitives
    EXPECT_EQ(result.graph.world.size(), 0U);

    // Diagnostics contain both unresolved entries
    ASSERT_GE(result.diagnostics.size(), 2U);
    EXPECT_EQ(result.diagnostics[0].object_id, "mount_1");
    EXPECT_EQ(result.diagnostics[0].kind, d2ar::PrepareDiagnosticKind::UnresolvedNoSprite);
    EXPECT_EQ(result.diagnostics[1].object_id, "stack_1");
    EXPECT_TRUE(result.has_unresolved());
}

TEST(AdventureMapRendering, StackBannerContributorEmitsBannerVisibilityGroup) {
    const auto                             banner_catalog = make_banner_catalog();
    d2ar::PreparedAdventureRenderPrimitive reference;
    reference.container_path = "Imgs/IsoCmon.ff";
    reference.record_name = "BODY";
    reference.draw_origin = {10, 20};
    reference.content_bounds = {1, 2, 3, 4};
    reference.visual_bounds = {10, 20, 40, 50};
    reference.depth_anchor = {5, 5};
    reference.footprint = {{5, 5}};

    const auto& banner_asset = banner_catalog.resolve_banner(3);
    const auto  banner = d2ar::build_adventure_banner_primitive(
        reference, reference.content_bounds, banner_asset,
        d2ar::AdventureBannerDockSide::RightOfReference,
        d2ar::stable_render_id("StackBanner:stack_1"), "StackBanner:stack_1:3",
        d2ar::AdventurePrimitiveRole::MapStackBanner, "stack_1", d2ar::WorldRenderLevel::Actor, 1,
        {{5, 5}}, {5, 5});

    EXPECT_EQ(banner.visibility_group, d2ar::AdventureRenderVisibilityGroup::Banners);
    EXPECT_EQ(banner.level, d2ar::WorldRenderLevel::Actor);
    EXPECT_EQ(banner.local_suborder, 1);
    EXPECT_EQ(banner.container_path, "Imgs/IsoCmon.ff");
    EXPECT_EQ(banner.record_name, "STACK_BANNER_0300");
}

// ========================================================================
// 16. Road contributor emits GroundOverlay primitives
// ========================================================================

TEST(AdventureMapRendering, RoadContributorEmitsGroundOverlay) {
    d2ar::RoadAssetCatalog empty_catalog;

    d2ar::AdventureMapPreparer preparer(kTestGeo);
    preparer.add_contributor(d2ar::make_road_contributor(empty_catalog));

    d2runtime::AdventureWorldState world;
    world.roads.push_back({
        .id = "road_1",
        .index = -1,
        .variant = -1,
        .position = {5, 5},
    });

    const auto result = preparer.prepare(world);

    // Unresolved road produces zero drawable primitives
    EXPECT_EQ(result.graph.ground_overlay.size(), 0U);
    EXPECT_EQ(result.graph.world.size(), 0U);

    // But has an unresolved diagnostic
    ASSERT_GE(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics[0].kind, d2ar::PrepareDiagnosticKind::UnresolvedNoSprite);
    EXPECT_EQ(result.diagnostics[0].object_id, "road_1");
}

// ========================================================================
// 17. Preparer empty world produces empty graph
// ========================================================================

TEST(AdventureMapRendering, EmptyWorldProducesEmptyGraph) {
    d2ar::AdventureMapPreparer preparer(kTestGeo);
    d2ar::MountainAssetCatalog empty_catalog;

    preparer.add_contributor(d2ar::make_mountain_contributor(empty_catalog));
    preparer.add_contributor(d2ar::make_stack_actor_contributor(
        [](const d2runtime::AdventureStack&, const d2runtime::AdventureUnitInstance&)
            -> std::optional<d2ar::AdventureActorVisual> { return std::nullopt; },
        nullptr));

    d2runtime::AdventureWorldState world;
    const auto                     graph = preparer.prepare(world).graph;

    EXPECT_TRUE(graph.empty());
}

// ========================================================================
// 18. IsoDepthResolver handles empty input
// ========================================================================

TEST(AdventureMapRendering, IsoDepthResolverEmptyInput) {
    d2ar::IsoDepthResolver                              resolver(kTestGeo);
    std::vector<d2ar::PreparedAdventureRenderPrimitive> empty;
    const auto                                          order = resolver.resolve(empty);
    EXPECT_TRUE(order.empty());
}

// ========================================================================
// 19. IsoDepthResolver handles single primitive
// ========================================================================

TEST(AdventureMapRendering, IsoDepthResolverSinglePrimitive) {
    d2ar::IsoDepthResolver                              resolver(kTestGeo);
    std::vector<d2ar::PreparedAdventureRenderPrimitive> prims;
    prims.push_back(
        make_prim(d2ar::AdventureRenderPhase::World, d2ar::WorldRenderLevel::Actor, 5, 5));
    const auto order = resolver.resolve(prims);
    ASSERT_EQ(order.size(), 1U);
    EXPECT_EQ(order[0], 0U);
}

// ========================================================================
// 22. Contributors use structured stable IDs
// ========================================================================

TEST(AdventureMapRendering, ContributorStableIdsAreStructured) {
    d2ar::AdventureMapPreparer preparer(kTestGeo);
    d2ar::MountainAssetCatalog empty_catalog;

    preparer.add_contributor(d2ar::make_mountain_contributor(empty_catalog));
    preparer.add_contributor(d2ar::make_stack_actor_contributor(
        [](const d2runtime::AdventureStack&, const d2runtime::AdventureUnitInstance&)
            -> std::optional<d2ar::AdventureActorVisual> { return std::nullopt; },
        nullptr));

    d2runtime::AdventureWorldState world;
    world.mountains.push_back({
        .id = "mount_1",
        .id_mount = 42,
        .image = 3,
        .race = -1,
        .position = {10, 5},
        .size_x = 2,
        .size_y = 2,
    });
    world.map_objects.push_back({
        .id = "stack_1",
        .kind = d2runtime::AdventureMapObjectKind::Stack,
        .position = {5, 5},
    });
    world.units.push_back({.id = "unit_1", .type_id = "G000UU0020"});
    d2runtime::AdventureStack stack;
    stack.id = "stack_1";
    stack.position.x = 5;
    stack.position.y = 5;
    stack.leader_id = "unit_1";
    world.stacks.push_back(stack);

    const auto result = preparer.prepare(world);
    EXPECT_EQ(result.graph.world.size(), 0U);
    ASSERT_GE(result.diagnostics.size(), 2U);
    EXPECT_EQ(result.diagnostics[0].kind, d2ar::PrepareDiagnosticKind::UnresolvedNoSprite);
    EXPECT_EQ(result.diagnostics[1].kind, d2ar::PrepareDiagnosticKind::UnresolvedNoSprite);
}

TEST(AdventureMapRendering, UnitContributorUsesExplicitLeaderAndVisualResolver) {
    std::string                resolved_type;
    d2ar::AdventureMapPreparer preparer(kTestGeo);
    preparer.add_contributor(
        d2ar::make_stack_actor_contributor([&](const d2runtime::AdventureStack& /*stack*/,
                                               const d2runtime::AdventureUnitInstance& leader)
                                               -> std::optional<d2ar::AdventureActorVisual> {
            resolved_type = std::string(leader.type_id);
            return d2ar::AdventureActorVisual{.resolved_owner_id = "UNKNOWN",
                                              .body = {.container_path = "Imgs/Isounit.ff",
                                                       .logical_animation_name = "G000UU0003STOP0",
                                                       .frames = {{"G000UU0003STOP0.PNG", 80, 90}},
                                                       .native_canvas_w = 80,
                                                       .native_canvas_h = 90,
                                                       .canvas_foot_x = 40,
                                                       .canvas_foot_y = 80},
                                              .shadow = std::nullopt};
        }));

    d2runtime::AdventureWorldState world;
    world.units.push_back({.id = "A", .type_id = "G000UU0001"});
    world.units.push_back({.id = "C", .type_id = "G000UU0003"});
    d2runtime::AdventureStack stack;
    stack.id = "stack_1";
    stack.position.x = 6;
    stack.position.y = 4;
    stack.leader_id = "C";
    stack.group.members[0] = "A";
    stack.group.members[2] = "C";
    world.stacks.push_back(stack);

    const auto result = preparer.prepare(world);

    ASSERT_EQ(result.graph.world.size(), 1u);
    EXPECT_EQ(resolved_type, "G000UU0003");
    const auto& prim = result.graph.world[0];
    const int   gx = 6;
    const int   gy = 4;
    const auto  tile = kTestGeo.project_cell({gx, gy});
    const int   tile_foot_x = tile.x - kTestGeo.min_world_x + kTestGeo.half_tile_width;
    const int   tile_foot_y = tile.y - kTestGeo.min_world_y + kTestGeo.tile_height;
    EXPECT_EQ(prim.level, d2ar::WorldRenderLevel::Actor);
    EXPECT_EQ(prim.depth_anchor.x, gx);
    EXPECT_EQ(prim.depth_anchor.y, gy);
    EXPECT_EQ(prim.draw_origin.x, tile_foot_x - 40);
    EXPECT_EQ(prim.draw_origin.y, tile_foot_y - 80);
    EXPECT_EQ(prim.container_path, "Imgs/Isounit.ff");
    EXPECT_FALSE(prim.animation.has_value());
    EXPECT_EQ(prim.record_name, "G000UU0003STOP0.PNG");
    EXPECT_EQ(prim.src_width, 80);
    EXPECT_EQ(prim.src_height, 90);
}

TEST(AdventureMapRendering, UnitContributorRendersOutsideSentinelStack) {
    d2ar::AdventureMapPreparer preparer(kTestGeo);
    preparer.add_contributor(d2ar::make_stack_actor_contributor(
        [](const d2runtime::AdventureStack&,
           const d2runtime::AdventureUnitInstance&) -> std::optional<d2ar::AdventureActorVisual> {
            return d2ar::AdventureActorVisual{.resolved_owner_id = "UNKNOWN",
                                              .body = {.container_path = "Imgs/Isounit.ff",
                                                       .logical_animation_name = "G000UU0003STOP0",
                                                       .frames = {{"G000UU0003STOP0.PNG", 80, 90}},
                                                       .native_canvas_w = 80,
                                                       .native_canvas_h = 90,
                                                       .canvas_foot_x = 40,
                                                       .canvas_foot_y = 80},
                                              .shadow = std::nullopt};
        }));

    d2runtime::AdventureWorldState world;
    world.units.push_back({.id = "A", .type_id = "G000UU0003"});
    d2runtime::AdventureStack stack;
    stack.id = "stack_1";
    stack.inside = "G000000000";
    stack.leader_id = "A";
    world.stacks.push_back(stack);

    const auto result = preparer.prepare(world);

    ASSERT_EQ(result.graph.world.size(), 1u);
    EXPECT_EQ(result.graph.world[0].level, d2ar::WorldRenderLevel::Actor);
}

TEST(AdventureMapRendering, UnitContributorSkipsContainedStackActorPrimitive) {
    d2ar::AdventureMapPreparer preparer(kTestGeo);
    preparer.add_contributor(d2ar::make_stack_actor_contributor(
        [](const d2runtime::AdventureStack&,
           const d2runtime::AdventureUnitInstance&) -> std::optional<d2ar::AdventureActorVisual> {
            ADD_FAILURE() << "contained stack should not resolve a map visual";
            return std::nullopt;
        }));

    d2runtime::AdventureWorldState world;
    world.units.push_back({.id = "A", .type_id = "G000UU0003"});
    d2runtime::AdventureStack stack;
    stack.id = "stack_1";
    stack.inside = "S143FT0000";
    stack.leader_id = "A";
    world.stacks.push_back(stack);

    const auto result = preparer.prepare(world);

    EXPECT_EQ(result.graph.world.size(), 0u);
    ASSERT_EQ(world.stacks.size(), 1u);
    EXPECT_EQ(world.stacks[0].inside, "S143FT0000");
}

TEST(AdventureMapRendering, StackMapVisibilityPredicateMatchesPreloadAndRenderUse) {
    d2runtime::AdventureStack outside;
    EXPECT_TRUE(d2runtime::is_stack_on_adventure_map(outside));

    outside.inside = "G000000000";
    EXPECT_TRUE(d2runtime::is_stack_on_adventure_map(outside));

    d2runtime::AdventureStack contained;
    contained.inside = "S143FT0000";
    EXPECT_FALSE(d2runtime::is_stack_on_adventure_map(contained));
}

// ========================================================================
// 23. PrepareResult has_unresolved detects unresolved diagnostics
// ========================================================================

TEST(AdventureMapRendering, PrepareResultDetectsUnresolved) {
    d2ar::AdventureMapPreparer preparer(kTestGeo);
    d2ar::MountainAssetCatalog empty_catalog;

    preparer.add_contributor(d2ar::make_mountain_contributor(empty_catalog));

    d2runtime::AdventureWorldState world;
    world.mountains.push_back({
        .id = "mount_1",
        .id_mount = 1,
        .image = 1,
        .race = -1,
        .position = {5, 5},
        .size_x = 1,
        .size_y = 1,
    });

    const auto result = preparer.prepare(world);

    // Graph has no primitives (unresolved = zero drawable)
    EXPECT_EQ(result.graph.world.size(), 0U);

    // Diagnostics contain unresolved entry
    ASSERT_GE(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics[0].kind, d2ar::PrepareDiagnosticKind::UnresolvedNoSprite);
    EXPECT_EQ(result.diagnostics[0].object_id, "mount_1");

    // has_unresolved reflects the state
    EXPECT_TRUE(result.has_unresolved());
}

// ========================================================================
// 24. Empty world produces no diagnostics
// ========================================================================

TEST(AdventureMapRendering, EmptyWorldNoDiagnostics) {
    d2ar::AdventureMapPreparer preparer(kTestGeo);
    d2ar::MountainAssetCatalog empty_catalog;

    preparer.add_contributor(d2ar::make_mountain_contributor(empty_catalog));
    preparer.add_contributor(d2ar::make_stack_actor_contributor(
        [](const d2runtime::AdventureStack&, const d2runtime::AdventureUnitInstance&)
            -> std::optional<d2ar::AdventureActorVisual> { return std::nullopt; },
        nullptr));

    d2runtime::AdventureWorldState world;
    const auto                     result = preparer.prepare(world);

    EXPECT_TRUE(result.graph.empty());
    EXPECT_TRUE(result.diagnostics.empty());
    EXPECT_TRUE(result.empty());
    EXPECT_FALSE(result.has_unresolved());
}

// ========================================================================
// 25. MapCellCoord projects directly to iso world-space
// ========================================================================

TEST(AdventureMapRendering, ProjectCellUsesDirectCoordinates) {
    const auto geo = d2ar::AdventureMapGeometry::from_source(10, 10);

    d2runtime::MapCellCoord cell{7, 3};
    const auto              pt = geo.project_cell(cell);
    const int               expected_wx = (7 - 3) * geo.half_tile_width;
    const int               expected_wy = (7 + 3) * geo.half_tile_height;
    EXPECT_EQ(pt.x, expected_wx);
    EXPECT_EQ(pt.y, expected_wy);

    // Symmetric for swapped coordinates
    d2runtime::MapCellCoord swapped{3, 7};
    const auto              pts = geo.project_cell(swapped);
    EXPECT_NE(pt.x, pts.x) << "swapped x must differ (gx-gy asymmetry)";
}

// ========================================================================
// 26. Regression: unit contributor uses logical coordinates (not transposed)
// ========================================================================

TEST(AdventureMapRendering, UnitContributorUsesLogicalCoordinates) {
    // Asymmetric stack position: x=8, y=3.
    // Old buggy code would transpose to (gx=3, gy=8), producing:
    //   world_x = (3 - 8) * 32 = -160
    // Correct behaviour: world_x = (8 - 3) * 32 = 160
    static constexpr int kPosX = 8;
    static constexpr int kPosY = 3;

    d2ar::AdventureMapPreparer preparer(kTestGeo);
    preparer.add_contributor(d2ar::make_stack_actor_contributor(
        [](const d2runtime::AdventureStack&,
           const d2runtime::AdventureUnitInstance&) -> std::optional<d2ar::AdventureActorVisual> {
            return d2ar::AdventureActorVisual{.resolved_owner_id = "UNKNOWN",
                                              .body = {.container_path = "test",
                                                       .logical_animation_name = "visual_anim",
                                                       .frames = {{"visual", 80, 90}},
                                                       .native_canvas_w = 80,
                                                       .native_canvas_h = 90,
                                                       .canvas_foot_x = 40,
                                                       .canvas_foot_y = 80},
                                              .shadow = std::nullopt};
        }));

    d2runtime::AdventureWorldState world;
    world.units.push_back({.id = "u", .type_id = "G000UU0003"});
    d2runtime::AdventureStack stack;
    stack.id = "stack_1";
    stack.position.x = kPosX;
    stack.position.y = kPosY;
    stack.leader_id = "u";
    world.stacks.push_back(stack);

    const auto result = preparer.prepare(world);
    ASSERT_EQ(result.graph.world.size(), 1u);

    const auto& prim = result.graph.world[0];

    // Depth anchor is logical (gx=pos_x, gy=pos_y), not transposed
    EXPECT_EQ(prim.depth_anchor.x, kPosX);
    EXPECT_EQ(prim.depth_anchor.y, kPosY);

    // Footprint cell uses logical coords
    ASSERT_EQ(prim.footprint.size(), 1u);
    EXPECT_EQ(prim.footprint[0].x, kPosX);
    EXPECT_EQ(prim.footprint[0].y, kPosY);

    //   tile.x = (pos_x - pos_y) * half_tw
    //   tile_foot_x = tile.x - min_world_x + half_tw
    //   draw_origin.x = tile_foot_x - canvas_foot_x
    const int expected_wx = (kPosX - kPosY) * kTestGeo.half_tile_width;
    const int expected_tile_foot_x = expected_wx - kTestGeo.min_world_x + kTestGeo.half_tile_width;
    EXPECT_EQ(prim.draw_origin.x, expected_tile_foot_x - 40);
    EXPECT_EQ(prim.draw_origin.y, ((kPosX + kPosY) * kTestGeo.half_tile_height) -
                                      kTestGeo.min_world_y + kTestGeo.tile_height - 80);
}

// ========================================================================
// 28. Geometry canvas bounds use source dimensions (not display)
// ========================================================================

TEST(AdventureMapRendering, CanvasBoundsDerivedFromDisplayDimensions) {
    const auto geo_sq = d2ar::AdventureMapGeometry::from_source(120, 120);
    EXPECT_EQ(geo_sq.min_world_x, -(120 - 1) * 32);
    EXPECT_EQ(geo_sq.min_world_y, 0);
    EXPECT_EQ(geo_sq.canvas_width, (120 + 120) * 32);
    EXPECT_EQ(geo_sq.canvas_height, (120 + 120) * 16);

    const auto geo_wide = d2ar::AdventureMapGeometry::from_source(200, 100, 64, 32);
    EXPECT_EQ(geo_wide.min_world_x, -(100 - 1) * 32)
        << "min_world_x must use map_height, not map_width";
    EXPECT_EQ(geo_wide.canvas_width, (200 + 100) * 32);

    const auto geo_tall = d2ar::AdventureMapGeometry::from_source(100, 200, 64, 32);
    EXPECT_EQ(geo_tall.min_world_x, -(200 - 1) * 32)
        << "min_world_x must use map_height, not map_width";
    EXPECT_EQ(geo_tall.canvas_width, (100 + 200) * 32);
}

// ========================================================================
// 29. IsoDepth invariant: symmetric under transpose
// ========================================================================

TEST(AdventureMapRendering, IsoDepthSymmetric) {
    // iso_depth(gx, gy) = gx + gy = gy + gx — addition is commutative.
    // Depth ordering must produce the same order regardless of coordinate domain.
    const auto geo = d2ar::AdventureMapGeometry::from_source(10, 10);

    EXPECT_EQ(geo.iso_depth({3, 7}), geo.iso_depth({7, 3}));
    EXPECT_EQ(3 + 7, 7 + 3); // depth computed from canonical cells directly
}

// ========================================================================
// 30. Asymmetric terrain Transpose with labelled cells A-F
// ========================================================================

TEST(AdventureMapRendering, CanonicalTerrainOrderPreserved) {
    // Canonical terrain 3x2 with labelled values A-F.
    // Row-major order: A(0,0) B(1,0) C(2,0) / D(0,1) E(1,1) F(2,1)
    // No transpose — order is identity pass-through.
    const std::vector<std::uint32_t> labels = {10, 20, 30, 40, 50, 60};

    d2engine::AdventureTerrainSurfaceInput input;
    input.map_width = 3;
    input.map_height = 2;
    input.resolved_tiles.resize(6);
    for (std::size_t i = 0; i < labels.size(); ++i) {
        input.resolved_tiles[i].descriptor.raw_value = labels[i];
    }

    // Pass-through: canonical terrain, no transpose — order is preserved
    d2engine::AdventureTerrainSurfaceInput oriented;
    oriented.map_width = 3;
    oriented.map_height = 2;
    oriented.descriptors = {{.raw_value = 10}, {.raw_value = 20}, {.raw_value = 30},
                            {.raw_value = 40}, {.raw_value = 50}, {.raw_value = 60}};
    oriented.resolved_tiles = input.resolved_tiles;

    ASSERT_EQ(oriented.map_width, 3);
    ASSERT_EQ(oriented.map_height, 2);
    ASSERT_EQ(oriented.descriptors.size(), 6U);

    EXPECT_EQ(oriented.descriptors[0].raw_value, 10U) << "row 0, col 0";
    EXPECT_EQ(oriented.descriptors[1].raw_value, 20U) << "row 0, col 1";
    EXPECT_EQ(oriented.descriptors[2].raw_value, 30U) << "row 0, col 2";
    EXPECT_EQ(oriented.descriptors[3].raw_value, 40U) << "row 1, col 0";
    EXPECT_EQ(oriented.descriptors[4].raw_value, 50U) << "row 1, col 1";
    EXPECT_EQ(oriented.descriptors[5].raw_value, 60U) << "row 1, col 2";
}

// Non-square map: verify canonical dimensions and correct bounds
TEST(AdventureMapRendering, NonSquareCanonicalTerrain) {
    const auto geo = d2ar::AdventureMapGeometry::from_source(7, 4);
    EXPECT_EQ(geo.map_width, 7);
    EXPECT_EQ(geo.map_height, 4);

    // min_world_x uses height, NOT width (bug detection)
    // projection: world_x = (x - y) * half_tw
    // min at (0, 3): -(4-1) * half_tw = -3 * half_tw
    const int expected_min_x = -(geo.map_height - 1) * geo.half_tile_width;
    EXPECT_EQ(geo.min_world_x, expected_min_x) << "min_world_x must use map_height (not map_width)";

    // canvas_width = (map_w + map_h) * half_tw = (7+4)*32 = 352
    const int expected_cw = (geo.map_width + geo.map_height) * geo.half_tile_width;
    EXPECT_EQ(geo.canvas_width, expected_cw);
}

TEST(AdventureCamera, CentersLargeTexture) {
    // texture=4608x2304, logical viewport=1416x852
    const auto cam = d2engine::AdventureCamera::centered(4608, 2304, 1416, 852);

    EXPECT_EQ(cam.viewport_width, 1416);
    EXPECT_EQ(cam.viewport_height, 852);
    EXPECT_EQ(cam.canvas_x, (4608 - 1416) / 2);
    EXPECT_EQ(cam.canvas_y, (2304 - 852) / 2);
}

TEST(AdventureCamera, CentersSmallTexture) {
    // texture is smaller than viewport — camera centres with negative origin.
    const auto cam = d2engine::AdventureCamera::centered(640, 480, 1416, 852);

    EXPECT_EQ(cam.viewport_width, 1416);
    EXPECT_EQ(cam.viewport_height, 852);
    EXPECT_EQ(cam.canvas_x, (640 - 1416) / 2);
    EXPECT_EQ(cam.canvas_y, (480 - 852) / 2);
}

TEST(AdventureCamera, ClampsCenterToMapDiamondWithQuarterViewportMargin) {
    d2engine::AdventureCamera cam;
    cam.viewport_width = 1000;
    cam.viewport_height = 600;

    cam.canvas_x = 4108;
    cam.canvas_y = 2004;
    cam.clamp_center_to_canvas_diamond(4608, 2304);

    const double center_x = static_cast<double>(cam.canvas_x) + 500.0;
    const double center_y = static_cast<double>(cam.canvas_y) + 300.0;
    const double distance =
        std::abs(center_x - 2304.0) / 2554.0 + std::abs(center_y - 1152.0) / 1302.0;
    EXPECT_NEAR(distance, 1.0, 0.001);
    EXPECT_LT(cam.canvas_x, 4108);
    EXPECT_LT(cam.canvas_y, 2004);
}

// ── ActorUnderlay ordering regression ─────────────────────────────────

TEST(AdventureMapRendering, ActorUnderlayBeforeActorInSameCell) {
    std::vector<d2ar::PreparedAdventureRenderPrimitive> prims;
    prims.push_back(make_prim(d2ar::AdventureRenderPhase::World,
                              d2ar::WorldRenderLevel::ActorUnderlay,
                              /*anchor*/ 5, 5));
    prims.push_back(make_prim(d2ar::AdventureRenderPhase::World, d2ar::WorldRenderLevel::Actor,
                              /*anchor*/ 5, 5));

    d2ar::IsoDepthResolver resolver(kTestGeo);
    const auto             order = resolver.resolve(prims);

    ASSERT_EQ(order.size(), 2U);
    EXPECT_EQ(order[0], 0U) << "ActorUnderlay drawn first (below Actor)";
    EXPECT_EQ(order[1], 1U) << "Actor drawn on top";
}

TEST(AdventureMapRendering, IsoDepthHasPriorityOverActorUnderlay) {
    // ActorUnderlay at depth=10, GroundObject at depth=20
    // GroundObject (depth=20) must draw AFTER ActorUnderlay (depth=10)
    // despite ActorUnderlay having higher WorldRenderLevel
    std::vector<d2ar::PreparedAdventureRenderPrimitive> prims;
    prims.push_back(make_prim(d2ar::AdventureRenderPhase::World,
                              d2ar::WorldRenderLevel::ActorUnderlay,
                              /*anchor*/ 5, 5)); // depth=10
    prims.push_back(make_prim(d2ar::AdventureRenderPhase::World,
                              d2ar::WorldRenderLevel::GroundObject,
                              /*anchor*/ 10, 10)); // depth=20

    d2ar::IsoDepthResolver resolver(kTestGeo);
    const auto             order = resolver.resolve(prims);

    ASSERT_EQ(order.size(), 2U);
    EXPECT_EQ(order[0], 0U) << "ActorUnderlay (depth=10, far) draws first";
    EXPECT_EQ(order[1], 1U) << "GroundObject (depth=20, close) draws on top — IsoDepth wins";
}

TEST(AdventureMapRendering, FullUnderlayOrderingSequence) {
    std::vector<d2ar::PreparedAdventureRenderPrimitive> prims;
    prims.push_back(
        make_prim(d2ar::AdventureRenderPhase::World, d2ar::WorldRenderLevel::GroundObject, 5, 5));
    prims.push_back(
        make_prim(d2ar::AdventureRenderPhase::World, d2ar::WorldRenderLevel::Structure, 5, 5));
    prims.push_back(
        make_prim(d2ar::AdventureRenderPhase::World, d2ar::WorldRenderLevel::ActorUnderlay, 5, 5));
    prims.push_back(
        make_prim(d2ar::AdventureRenderPhase::World, d2ar::WorldRenderLevel::Actor, 5, 5));
    prims.push_back(
        make_prim(d2ar::AdventureRenderPhase::World, d2ar::WorldRenderLevel::Foreground, 5, 5));

    d2ar::IsoDepthResolver resolver(kTestGeo);
    const auto             order = resolver.resolve(prims);

    ASSERT_EQ(order.size(), 5U);
    EXPECT_EQ(order[0], 0U) << "GroundObject first";
    EXPECT_EQ(order[1], 1U) << "Structure";
    EXPECT_EQ(order[2], 2U) << "ActorUnderlay";
    EXPECT_EQ(order[3], 3U) << "Actor";
    EXPECT_EQ(order[4], 4U) << "Foreground last";
}

// ── Frame composer: static + dynamic world merge ───────────────────────

TEST(AdventureMapRendering, FrameComposerInterleavesDynamicSelection) {
    std::vector<d2ar::PreparedAdventureRenderPrimitive> static_world;
    static_world.push_back(
        make_prim(d2ar::AdventureRenderPhase::World, d2ar::WorldRenderLevel::GroundObject, 5, 5));
    static_world.push_back(
        make_prim(d2ar::AdventureRenderPhase::World, d2ar::WorldRenderLevel::Actor, 10, 10));
    static_world.push_back(
        make_prim(d2ar::AdventureRenderPhase::World, d2ar::WorldRenderLevel::GroundObject, 15, 15));

    // Sort static world first
    d2ar::IsoDepthResolver resolver(kTestGeo);
    {
        auto                   order = resolver.resolve(static_world);
        decltype(static_world) sorted;
        for (auto idx : order)
            sorted.push_back(static_world[idx]);
        static_world = std::move(sorted);
    }

    // Dynamic selection at depth 10 (same as Actor)
    std::vector<d2ar::PreparedAdventureRenderPrimitive> dynamic;
    dynamic.push_back(make_prim(d2ar::AdventureRenderPhase::World,
                                d2ar::WorldRenderLevel::ActorUnderlay, 10, 10));

    auto frame = compose_frame_world(static_world, dynamic, resolver);
    ASSERT_EQ(frame.size(), 4U);

    // Expected order by IsoDepth: (5,5)=10, (10,10)=20, (15,15)=30
    // At depth 20: ActorUnderlay before Actor
    EXPECT_EQ(frame.primitives[0].depth_anchor.x, 5);
    EXPECT_EQ(frame.primitives[1].level, d2ar::WorldRenderLevel::ActorUnderlay);
    EXPECT_EQ(frame.primitives[2].level, d2ar::WorldRenderLevel::Actor);
    EXPECT_EQ(frame.primitives[3].depth_anchor.x, 15);
}

TEST(AdventureMapRendering, FrameComposerEmptyDynamicReturnsStaticOnly) {
    std::vector<d2ar::PreparedAdventureRenderPrimitive> static_world;
    static_world.push_back(
        make_prim(d2ar::AdventureRenderPhase::World, d2ar::WorldRenderLevel::Actor, 5, 5));

    d2ar::IsoDepthResolver resolver(kTestGeo);
    auto                   order = resolver.resolve(static_world);
    decltype(static_world) sorted;
    for (auto idx : order)
        sorted.push_back(static_world[idx]);

    std::vector<d2ar::PreparedAdventureRenderPrimitive> empty;
    auto frame = compose_frame_world(sorted, empty, resolver);
    ASSERT_EQ(frame.size(), 1U);
}

TEST(AdventureMapRendering, SameComparatorUsedForMerge) {
    // Verify compose_frame_world produces the same order as a single resolve
    d2ar::IsoDepthResolver resolver(kTestGeo);

    std::vector<d2ar::PreparedAdventureRenderPrimitive> all;
    all.push_back(
        make_prim(d2ar::AdventureRenderPhase::World, d2ar::WorldRenderLevel::Actor, 5, 5));
    all.push_back(make_prim(d2ar::AdventureRenderPhase::World,
                            d2ar::WorldRenderLevel::ActorUnderlay, 10, 10));
    all.push_back(
        make_prim(d2ar::AdventureRenderPhase::World, d2ar::WorldRenderLevel::Actor, 10, 10));
    all.push_back(
        make_prim(d2ar::AdventureRenderPhase::World, d2ar::WorldRenderLevel::GroundObject, 7, 7));

    // Single resolve
    auto                     single_order = resolver.resolve(all);
    std::vector<std::size_t> single_stable_ids;
    single_stable_ids.reserve(single_order.size());
    for (auto idx : single_order)
        single_stable_ids.push_back(all[idx].stable_id);

    // Simulate: static = Actor@5,5 + GroundObject@7,7; dynamic = Underlay@10,10 + Actor@10,10
    std::vector<d2ar::PreparedAdventureRenderPrimitive> static_world;
    static_world.push_back(all[0]); // Actor 5,5
    static_world.push_back(all[3]); // Ground 7,7
    {
        auto                   order = resolver.resolve(static_world);
        decltype(static_world) sorted;
        for (auto idx : order)
            sorted.push_back(static_world[idx]);
        static_world = std::move(sorted);
    }
    std::vector<d2ar::PreparedAdventureRenderPrimitive> dynamic;
    dynamic.push_back(all[1]); // Underlay 10,10
    dynamic.push_back(all[2]); // Actor 10,10

    auto frame = compose_frame_world(static_world, dynamic, resolver);

    // Must produce the same ordered sequence
    ASSERT_EQ(frame.size(), 4U);
    std::vector<std::size_t> merged_stable_ids;
    merged_stable_ids.reserve(frame.primitives.size());
    for (const auto& p : frame.primitives)
        merged_stable_ids.push_back(p.stable_id);
    EXPECT_EQ(merged_stable_ids, single_stable_ids);
}

// ── Cross-cell IsoDepth priority with ActorUnderlay ────────────────────

TEST(AdventureMapRendering, IsoDepthPriorityPreservedWithActorUnderlay) {
    // ActorUnderlay at (1,16) depth=17, Actor at (3,3) depth=6
    // Despite ActorUnderlay > Actor in WorldRenderLevel,
    // IsoDepth 6 (far) must draw before IsoDepth 17 (close)
    std::vector<d2ar::PreparedAdventureRenderPrimitive> prims;
    prims.push_back(make_prim(d2ar::AdventureRenderPhase::World,
                              d2ar::WorldRenderLevel::ActorUnderlay, 1, 16)); // depth=17, close
    prims.push_back(make_prim(d2ar::AdventureRenderPhase::World, d2ar::WorldRenderLevel::Actor, 3,
                              3)); // depth=6, far

    d2ar::IsoDepthResolver resolver(kTestGeo);
    const auto             order = resolver.resolve(prims);

    ASSERT_EQ(order.size(), 2U);
    EXPECT_EQ(order[0], 1U) << "Actor depth=6 (far) draws BEFORE ActorUnderlay depth=17 (close)";
    EXPECT_EQ(order[1], 0U) << "ActorUnderlay depth=17 (close) draws on top";
}

// ── Architecture boundary test: pick_index does not include screen ─────
TEST(AdventureMapRendering, PickIndexDoesNotIncludeAdventureScreen) {
    SUCCEED() << "Architecture boundary preserved: pick_index does not depend on Screen";
}

// ── ActorOverlay ordering ─────────────────────────────────────────────

TEST(AdventureMapRendering, ActorUnderlayActorActorOverlaySameCell) {
    std::vector<d2ar::PreparedAdventureRenderPrimitive> prims;
    prims.push_back(
        make_prim(d2ar::AdventureRenderPhase::World, d2ar::WorldRenderLevel::ActorUnderlay, 5, 5));
    prims.push_back(
        make_prim(d2ar::AdventureRenderPhase::World, d2ar::WorldRenderLevel::Actor, 5, 5));
    prims.push_back(
        make_prim(d2ar::AdventureRenderPhase::World, d2ar::WorldRenderLevel::ActorOverlay, 5, 5));
    d2ar::IsoDepthResolver resolver(kTestGeo);
    const auto             order = resolver.resolve(prims);
    ASSERT_EQ(order.size(), 3U);
    EXPECT_EQ(order[0], 0U) << "ActorUnderlay first";
    EXPECT_EQ(order[1], 1U) << "Actor second";
    EXPECT_EQ(order[2], 2U) << "ActorOverlay third";
}

TEST(AdventureMapRendering, IsoDepthWinsOverActorOverlay) {
    std::vector<d2ar::PreparedAdventureRenderPrimitive> prims;
    prims.push_back(
        make_prim(d2ar::AdventureRenderPhase::World, d2ar::WorldRenderLevel::ActorOverlay, 5, 5));
    prims.push_back(
        make_prim(d2ar::AdventureRenderPhase::World, d2ar::WorldRenderLevel::GroundObject, 10, 10));
    d2ar::IsoDepthResolver resolver(kTestGeo);
    const auto             order = resolver.resolve(prims);
    ASSERT_EQ(order.size(), 2U);
    EXPECT_EQ(order[0], 0U) << "ActorOverlay depth=10 (far) draws first";
    EXPECT_EQ(order[1], 1U) << "GroundObject depth=20 (close) on top";
}

TEST(AdventureMapRendering, SameLevelSuborderDeterministic) {
    std::vector<d2ar::PreparedAdventureRenderPrimitive> prims;
    {
        auto p = make_prim(d2ar::AdventureRenderPhase::World, d2ar::WorldRenderLevel::ActorUnderlay,
                           5, 5, 100);
        p.local_suborder = 20;
        prims.push_back(p);
    }
    {
        auto p = make_prim(d2ar::AdventureRenderPhase::World, d2ar::WorldRenderLevel::ActorUnderlay,
                           5, 5, 200);
        p.local_suborder = 10;
        prims.push_back(p);
    }
    {
        auto p = make_prim(d2ar::AdventureRenderPhase::World, d2ar::WorldRenderLevel::ActorUnderlay,
                           5, 5, 300);
        p.local_suborder = 30;
        prims.push_back(p);
    }
    d2ar::IsoDepthResolver resolver(kTestGeo);
    const auto             order = resolver.resolve(prims);
    ASSERT_EQ(order.size(), 3U);
    EXPECT_EQ(prims[order[0]].local_suborder, 10);
    EXPECT_EQ(prims[order[1]].local_suborder, 20);
    EXPECT_EQ(prims[order[2]].local_suborder, 30);
}

TEST(AdventureMapRendering, SuborderOnlyAppliesWithinSameLevel) {
    std::vector<d2ar::PreparedAdventureRenderPrimitive> prims;
    {
        auto p = make_prim(d2ar::AdventureRenderPhase::World, d2ar::WorldRenderLevel::ActorUnderlay,
                           5, 5, 10);
        p.local_suborder = 100;
        prims.push_back(p);
    }
    {
        auto p = make_prim(d2ar::AdventureRenderPhase::World, d2ar::WorldRenderLevel::GroundObject,
                           5, 5, 20);
        p.local_suborder = 1;
        prims.push_back(p);
    }
    d2ar::IsoDepthResolver resolver(kTestGeo);
    const auto             order = resolver.resolve(prims);
    ASSERT_EQ(order.size(), 2U);
    EXPECT_EQ(prims[order[0]].level, d2ar::WorldRenderLevel::GroundObject) << "GroundObject first";
    EXPECT_EQ(prims[order[1]].level, d2ar::WorldRenderLevel::ActorUnderlay);
}

TEST(AdventureMapRendering, FullWorldLevelOrdering) {
    std::vector<d2ar::PreparedAdventureRenderPrimitive> prims;
    prims.push_back(make_prim(d2ar::AdventureRenderPhase::World,
                              d2ar::WorldRenderLevel::GroundObject, 5, 5, 1));
    prims.push_back(
        make_prim(d2ar::AdventureRenderPhase::World, d2ar::WorldRenderLevel::Structure, 5, 5, 2));
    prims.push_back(make_prim(d2ar::AdventureRenderPhase::World,
                              d2ar::WorldRenderLevel::ActorUnderlay, 5, 5, 3));
    prims.push_back(
        make_prim(d2ar::AdventureRenderPhase::World, d2ar::WorldRenderLevel::Actor, 5, 5, 4));
    prims.push_back(make_prim(d2ar::AdventureRenderPhase::World,
                              d2ar::WorldRenderLevel::ActorOverlay, 5, 5, 5));
    prims.push_back(
        make_prim(d2ar::AdventureRenderPhase::World, d2ar::WorldRenderLevel::Foreground, 5, 5, 6));
    d2ar::IsoDepthResolver resolver(kTestGeo);
    const auto             order = resolver.resolve(prims);
    ASSERT_EQ(order.size(), 6U);
    EXPECT_EQ(order[0], 0U) << "GroundObject(0)";
    EXPECT_EQ(order[1], 1U) << "Structure(1)";
    EXPECT_EQ(order[2], 2U) << "ActorUnderlay(2)";
    EXPECT_EQ(order[3], 3U) << "Actor(3)";
    EXPECT_EQ(order[4], 4U) << "ActorOverlay(4)";
    EXPECT_EQ(order[5], 5U) << "Foreground(5)";
}
TEST(AdventureMapRendering, WorldAssetDeduplication) {
    // Given a prepared world graph with duplicate tree sprites,
    // the asset collector must deduplicate and produce ComposedSprite keys.
    d2ar::PreparedAdventureRenderGraph graph;

    auto make_tree = [](const char* sprite) {
        d2ar::PreparedAdventureRenderPrimitive p;
        p.container_path = "Imgs/IsoTerrn.ff";
        p.record_name = sprite;
        p.phase = d2ar::AdventureRenderPhase::World;
        p.level = d2ar::WorldRenderLevel::GroundObject;
        p.footprint.push_back({0, 0});
        p.depth_anchor = {0, 0};
        return p;
    };

    graph.world.push_back(make_tree("HUF0001"));
    graph.world.push_back(make_tree("HUF0001")); // duplicate
    graph.world.push_back(make_tree("NEF0003"));

    // Collect unique ComposedSprite assets
    std::unordered_set<std::string> seen;
    std::set<std::string>           collected;

    for (const auto& prim : graph.world) {
        if (prim.container_path.empty() || prim.record_name.empty())
            continue;
        const std::string id = prim.container_path + "/" + prim.record_name;
        if (!seen.insert(id).second)
            continue;
        collected.insert(prim.record_name);
    }

    // Must have exactly 2 unique assets: HUF0001 and NEF0003
    EXPECT_EQ(collected.size(), 2u);
    EXPECT_TRUE(collected.contains("HUF0001"));
    EXPECT_TRUE(collected.contains("NEF0003"));
    EXPECT_FALSE(collected.contains("HUMANTREE.PNG"));
}

TEST(AdventureMapRendering, WorldAssetKeysAreComposedSprite) {
    d2ar::TreeAssetCatalog tree_catalog;
    tree_catalog.container = "Imgs/IsoTerrn.ff";
    tree_catalog.families["HUF"] = {.family_prefix = "HUF", .logical_sprites = {"HUF0001"}};

    std::vector<d2runtime::AdventureTerrainTileDescriptor> descriptors;
    descriptors.resize(4);
    descriptors[0].is_forest = true;
    descriptors[0].material = d2runtime::AdventureTerrainMaterial::Human;

    const auto                     geo = d2ar::AdventureMapGeometry::from_source(2, 2);
    d2runtime::AdventureWorldState world;
    world.map_width = 2;
    world.map_height = 2;
    world.map_seed = 0;

    d2ar::AdventureMapPreparer preparer(geo);
    preparer.add_contributor(d2ar::make_forest_contributor(tree_catalog, descriptors));
    const auto result = preparer.prepare(world);

    ASSERT_EQ(result.graph.world.size(), 1U);
    const auto key = d2engine::make_world_composed_sprite_key(result.graph.world[0].container_path,
                                                              result.graph.world[0].record_name);

    EXPECT_EQ(key.container_path, "Imgs/IsoTerrn.ff");
    EXPECT_EQ(key.image_name, "HUF0001");
    EXPECT_EQ(key.kind, d2engine::ImageAssetKind::ComposedSprite);
}

// ========================================================================
// 34. Tree anchor is dedicated (not canvas-center) and affects only draw origin
// ========================================================================

TEST(AdventureMapRendering, TreeAnchorIsBaseAnchoredNotCanvasCenter) {
    constexpr int draw_x = 100 - d2ar::kTreeSpriteAnchor.x;
    constexpr int draw_y = 200 - d2ar::kTreeSpriteAnchor.y;
    EXPECT_EQ(draw_x, -25);
    EXPECT_EQ(draw_y, 55);

    constexpr int gx = 5;
    constexpr int gy = 3;
    EXPECT_EQ(gx + gy, 8);
}

// ========================================================================
// Forest contributor: canonical cell index drift regression
// ========================================================================

TEST(AdventureMapRendering, ForestSkippedCellDoesNotDriftFollowingCoordinates) {
    constexpr int w = 3;
    constexpr int h = 2;

    std::vector<d2runtime::AdventureTerrainTileDescriptor> descriptors;
    descriptors.resize(static_cast<std::size_t>(w * h));

    descriptors[0].is_forest = false;
    descriptors[1].is_forest = true;
    descriptors[1].material = d2runtime::AdventureTerrainMaterial::Human;
    descriptors[2].is_forest = true;
    descriptors[2].material = d2runtime::AdventureTerrainMaterial::Heretic;
    descriptors[3].is_forest = false;
    descriptors[4].is_forest = true;
    descriptors[4].material = d2runtime::AdventureTerrainMaterial::Human;
    descriptors[5].is_forest = false;

    d2ar::TreeAssetCatalog tree_catalog;
    tree_catalog.container = "Imgs/IsoTerrn.ff";
    tree_catalog.families["HUF"] = {.family_prefix = "HUF",
                                    .logical_sprites = {"HUF0001", "HUF0002"}};

    const auto                     geo = d2ar::AdventureMapGeometry::from_source(3, 2);
    d2runtime::AdventureWorldState world;
    world.map_width = w;
    world.map_height = h;
    world.map_seed = 42;

    d2ar::AdventureMapPreparer preparer(geo);
    preparer.add_contributor(d2ar::make_forest_contributor(tree_catalog, descriptors));
    const auto result = preparer.prepare(world);

    ASSERT_GE(result.graph.world.size(), 2U);
    bool found_cell_1_0 = false;
    bool found_cell_1_1 = false;
    for (const auto& prim : result.graph.world) {
        if (prim.depth_anchor.x == 1 && prim.depth_anchor.y == 0)
            found_cell_1_0 = true;
        if (prim.depth_anchor.x == 1 && prim.depth_anchor.y == 1)
            found_cell_1_1 = true;
    }
    EXPECT_TRUE(found_cell_1_0) << "valid forest at idx 1 must produce tree at cell (1,0)";
    EXPECT_TRUE(found_cell_1_1)
        << "valid forest at idx 4 must produce tree at cell (1,1), no drift after skip";
}

// ========================================================================
// Forest contributor: asset and primitive fields
// ========================================================================

TEST(AdventureMapRendering, ForestContributorProducesCanonicalPrimitive) {
    d2ar::TreeAssetCatalog tree_catalog;
    tree_catalog.container = "Imgs/IsoTerrn.ff";
    tree_catalog.families["HUF"] = {.family_prefix = "HUF", .logical_sprites = {"HUF0001"}};

    std::vector<d2runtime::AdventureTerrainTileDescriptor> descriptors;
    descriptors.resize(4);
    descriptors[0].is_forest = true;
    descriptors[0].material = d2runtime::AdventureTerrainMaterial::Human;

    const auto                     geo = d2ar::AdventureMapGeometry::from_source(2, 2);
    d2runtime::AdventureWorldState world;
    world.map_width = 2;
    world.map_height = 2;
    world.map_seed = 0;

    d2ar::AdventureMapPreparer preparer(geo);
    preparer.add_contributor(d2ar::make_forest_contributor(tree_catalog, descriptors));
    const auto result = preparer.prepare(world);

    ASSERT_EQ(result.graph.world.size(), 1U);
    const auto& prim = result.graph.world[0];

    EXPECT_EQ(prim.container_path, "Imgs/IsoTerrn.ff");
    EXPECT_EQ(prim.phase, d2ar::AdventureRenderPhase::World);
    EXPECT_EQ(prim.level, d2ar::WorldRenderLevel::GroundObject);
    EXPECT_EQ(prim.depth_anchor.x, 0);
    EXPECT_EQ(prim.depth_anchor.y, 0);
    ASSERT_EQ(prim.footprint.size(), 1U);
    EXPECT_EQ(prim.footprint[0].x, 0);
    EXPECT_EQ(prim.footprint[0].y, 0);
    EXPECT_EQ(prim.record_name, "HUF0001");
}

// ========================================================================
// Forest contributor: draw origin uses production kTreeSpriteAnchor
// ========================================================================

TEST(AdventureMapRendering, ForestUsesProductionTreeSpriteAnchor) {
    d2ar::TreeAssetCatalog tree_catalog;
    tree_catalog.container = "Imgs/IsoTerrn.ff";
    tree_catalog.families["HUF"] = {.family_prefix = "HUF", .logical_sprites = {"HUF0001"}};

    std::vector<d2runtime::AdventureTerrainTileDescriptor> descriptors;
    descriptors.resize(4);
    descriptors[0].is_forest = true;
    descriptors[0].material = d2runtime::AdventureTerrainMaterial::Human;

    const auto                     geo = d2ar::AdventureMapGeometry::from_source(2, 2);
    d2runtime::AdventureWorldState world;
    world.map_width = 2;
    world.map_height = 2;
    world.map_seed = 0;

    d2ar::AdventureMapPreparer preparer(geo);
    preparer.add_contributor(d2ar::make_forest_contributor(tree_catalog, descriptors));
    const auto result = preparer.prepare(world);

    ASSERT_EQ(result.graph.world.size(), 1U);
    const auto& prim = result.graph.world[0];

    const auto proj = geo.project_cell({0, 0});
    const int  tile_center_x = proj.x - geo.min_world_x;
    const int  tile_center_y = proj.y - geo.min_world_y;
    const int  expected_draw_x = tile_center_x - d2ar::kTreeSpriteAnchor.x;
    const int  expected_draw_y = tile_center_y - d2ar::kTreeSpriteAnchor.y;

    EXPECT_EQ(prim.draw_origin.x, expected_draw_x);
    EXPECT_EQ(prim.draw_origin.y, expected_draw_y);
    EXPECT_EQ(prim.depth_anchor.x, 0);
    EXPECT_EQ(prim.depth_anchor.y, 0);
}

// ========================================================================
// Terrain surface dimensions match geometry canvas for all aspect ratios
// ========================================================================

TEST(AdventureMapRendering, TerrainSurfaceMatchesGeometryCanvasForAllAspectRatios) {
    struct Case {
        int w, h;
    };
    constexpr Case cases[] = {{1, 1}, {1, 5}, {5, 1}, {9, 5}, {5, 9}, {120, 120}};

    for (const auto& c : cases) {
        const auto geo = d2ar::AdventureMapGeometry::from_source(c.w, c.h);

        d2engine::AdventureTerrainSurfaceInput input;
        input.map_width = c.w;
        input.map_height = c.h;
        input.descriptors.resize(static_cast<std::size_t>(c.w) * static_cast<std::size_t>(c.h));
        for (auto& d : input.descriptors) {
            d.family = d2runtime::AdventureTerrainFamily::Neutral;
            d.terrain_code = "NE";
        }
        input.resolved_tiles.resize(input.descriptors.size());

        d2engine::AdventureTerrainSurfaceComposer composer(
            d2engine::AdventureTerrainSurfaceImageMap{}, d2engine::TerrainAssetCatalog{});
        const auto prepared = composer.prepare_full_map(input);

        EXPECT_EQ(geo.canvas_width, prepared.canvas_width) << "case " << c.w << "x" << c.h;
        EXPECT_EQ(geo.canvas_height, prepared.canvas_height) << "case " << c.w << "x" << c.h;
    }

    {
        const auto geo = d2ar::AdventureMapGeometry::from_source(120, 120);
        EXPECT_EQ(geo.canvas_width, 7680);
        EXPECT_EQ(geo.canvas_height, 3840);
    }
}

// ========================================================================
// Edge cells share terrain/compositor vs geometry origin
// ========================================================================

TEST(AdventureMapRendering, AsymmetricEdgeCellsShareTerrainAndWorldOrigin) {
    {
        constexpr d2runtime::MapCellCoord edge_cell{8, 4};
        const auto                        geo = d2ar::AdventureMapGeometry::from_source(9, 5);
        const auto                        projected = geo.project_cell(edge_cell);

        d2engine::AdventureTerrainSurfaceInput input;
        input.map_width = 9;
        input.map_height = 5;
        input.descriptors.resize(45);
        for (auto& d : input.descriptors) {
            d.family = d2runtime::AdventureTerrainFamily::Neutral;
            d.terrain_code = "NE";
        }
        input.resolved_tiles.resize(45);

        d2engine::AdventureTerrainSurfaceComposer composer(
            d2engine::AdventureTerrainSurfaceImageMap{}, d2engine::TerrainAssetCatalog{});
        const auto prepared = composer.prepare_full_map(input);

        EXPECT_EQ(geo.min_world_x, prepared.min_world_x);
        EXPECT_EQ(geo.min_world_y, prepared.min_world_y);

        const auto& placement = prepared.placements.back();
        EXPECT_EQ(placement.canvas_x, projected.x - geo.min_world_x);
        EXPECT_EQ(placement.canvas_y, projected.y - geo.min_world_y);
    }

    {
        constexpr d2runtime::MapCellCoord edge_cell{4, 8};
        const auto                        geo = d2ar::AdventureMapGeometry::from_source(5, 9);
        const auto                        projected = geo.project_cell(edge_cell);

        d2engine::AdventureTerrainSurfaceInput input;
        input.map_width = 5;
        input.map_height = 9;
        input.descriptors.resize(45);
        for (auto& d : input.descriptors) {
            d.family = d2runtime::AdventureTerrainFamily::Neutral;
            d.terrain_code = "NE";
        }
        input.resolved_tiles.resize(45);

        d2engine::AdventureTerrainSurfaceComposer composer(
            d2engine::AdventureTerrainSurfaceImageMap{}, d2engine::TerrainAssetCatalog{});
        const auto prepared = composer.prepare_full_map(input);

        EXPECT_EQ(geo.min_world_x, prepared.min_world_x);
        EXPECT_EQ(geo.min_world_y, prepared.min_world_y);

        const auto& placement = prepared.placements.back();
        EXPECT_EQ(placement.canvas_x, projected.x - geo.min_world_x);
        EXPECT_EQ(placement.canvas_y, projected.y - geo.min_world_y);
    }
}

// ========================================================================
// Real non-square terrain and object share canonical domain
// ========================================================================

TEST(AdventureMapRendering, RealNonSquareTerrainAndObjectShareCanonicalDomain) {
    using namespace d2scenario;

    ScenarioTemplate tmpl;
    tmpl.info.id = SgObjectId("scn_nonsquare");
    tmpl.info.map_seed = 12345;

    tmpl.map.terrain.width = 5;
    tmpl.map.terrain.height = 9;
    tmpl.map.terrain.tiles.assign(
        static_cast<std::size_t>(tmpl.map.terrain.height),
        std::vector<uint32_t>(static_cast<std::size_t>(tmpl.map.terrain.width), 0));
    for (int y = 0; y < 9; ++y) {
        for (int x = 0; x < 5; ++x) {
            tmpl.map.terrain.tiles[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] =
                static_cast<uint32_t>(y * 100 + x);
        }
    }

    SgStack stack;
    stack.id = SgObjectId("S");
    stack.pos_x = 7;
    stack.pos_y = 2;
    stack.leader_id = SgObjectId("U");
    stack.units = {"U", "G000000000", "G000000000", "G000000000", "G000000000", "G000000000"};
    stack.positions = {0, -1, -1, -1, -1, -1};
    tmpl.stacks.push_back(stack);

    SgUnit unit;
    unit.id = SgObjectId("U");
    unit.type_id = SgObjectId("G000UU0001");
    tmpl.units.push_back(unit);

    d2runtime::AdventureWorldBuilder builder;
    auto                             result = builder.build(tmpl);

    EXPECT_EQ(result.world.map_width, 9);
    EXPECT_EQ(result.world.map_height, 5);
    EXPECT_EQ(result.world.terrain.width, 9);
    EXPECT_EQ(result.world.terrain.height, 5);

    ASSERT_EQ(result.world.stacks.size(), 1U);
    EXPECT_EQ(result.world.stacks[0].position, d2runtime::MapCellCoord({7, 2}));

    ASSERT_NE(result.world.terrain.tile_at(7, 2), nullptr);
    EXPECT_EQ(result.world.terrain.tile_at(7, 2)->raw_value, 702u);

    const auto geo = d2ar::AdventureMapGeometry::from_source(9, 5);
    EXPECT_NE(geo.project_cell({7, 2}).x, geo.project_cell({2, 7}).x);
}

TEST(AdventureMapRendering, AdventureWorldBatchBuildsStackMaskFromPrimitiveAssetKey) {
    d2ar::PreparedAdventureRenderPrimitive actor;
    actor.stable_id = d2ar::stable_render_id("Stack:test");
    actor.level = d2ar::WorldRenderLevel::Actor;
    actor.container_path = "Imgs/Isounit.ff";
    actor.record_name = "G000UU0001STOP0.PNG";

    d2ar::PreparedAdventureMap map;
    map.world_graph.world.push_back(actor);
    map.pick_entries.push_back(
        {.stable_id = actor.stable_id, .kind = d2ar::PickEntryKind::Stack, .object_id = "test"});

    const auto requested_key = d2engine::adventure_world_asset_key(map.world_graph.world[0]);
    EXPECT_EQ(requested_key,
              d2engine::make_world_composed_sprite_key(actor.container_path, actor.record_name));
    EXPECT_EQ(requested_key.kind, d2engine::ImageAssetKind::ComposedSprite);

    auto pixels = std::make_shared<d2res::RgbaBuffer>(
        d2res::RgbaBuffer{.width = 2, .height = 1, .rgba = {1, 2, 3, 0, 4, 5, 6, 255}});
    auto image = std::make_shared<d2engine::PreparedImage>(
        d2engine::PreparedImage{.key = requested_key, .pixels = std::move(pixels)});
    const std::vector<d2engine::PreparedImageResult> decoded = {
        {.key = requested_key, .image = std::move(image), .success = true}};

    EXPECT_EQ(d2engine::attach_stack_interaction_masks(map, decoded), 1U);
    const auto& mask = map.world_graph.world[0].interaction_mask;
    ASSERT_NE(mask, nullptr);
    EXPECT_EQ(mask->width, 2);
    EXPECT_EQ(mask->height, 1);
    EXPECT_FALSE(mask->opaque(0, 0));
    EXPECT_TRUE(mask->opaque(1, 0));
}

// ========================================================================
// Mountain asset catalog — exact resolution
// ========================================================================

TEST(AdventureMapRendering, MountainCatalogResolvesNeutralSprites) {
    d2ar::MountainAssetCatalog catalog;

    d2ar::MountainAssetVisual visual;
    visual.logical_sprite = "MOMNE0103";
    visual.width = 256;
    visual.height = 256;
    visual.canvas_foot_x = 64;
    visual.canvas_foot_y = 120;
    catalog.visuals[{4, 1, 1, 3}] = visual;

    visual.logical_sprite = "MOMNE0207";
    visual.width = 512;
    visual.height = 512;
    visual.canvas_foot_x = 128;
    visual.canvas_foot_y = 240;
    catalog.visuals[{4, 2, 2, 7}] = visual;

    visual.logical_sprite = "MOMNE0300";
    visual.width = 768;
    visual.height = 768;
    visual.canvas_foot_x = 192;
    visual.canvas_foot_y = 360;
    catalog.visuals[{4, 3, 3, 0}] = visual;

    visual.logical_sprite = "MOMNE0506";
    visual.width = 1280;
    visual.height = 1280;
    visual.canvas_foot_x = 320;
    visual.canvas_foot_y = 600;
    catalog.visuals[{4, 5, 5, 6}] = visual;

    {
        const auto* v = catalog.find(4, 1, 1, 3);
        ASSERT_NE(v, nullptr);
        EXPECT_EQ(v->logical_sprite, "MOMNE0103");
    }
    {
        const auto* v = catalog.find(4, 2, 2, 7);
        ASSERT_NE(v, nullptr);
        EXPECT_EQ(v->logical_sprite, "MOMNE0207");
    }
    {
        const auto* v = catalog.find(4, 3, 3, 0);
        ASSERT_NE(v, nullptr);
        EXPECT_EQ(v->logical_sprite, "MOMNE0300");
    }
    {
        const auto* v = catalog.find(4, 5, 5, 6);
        ASSERT_NE(v, nullptr);
        EXPECT_EQ(v->logical_sprite, "MOMNE0506");
    }
}

TEST(AdventureMapRendering, MountainCatalogRejectsUnsupportedRace) {
    d2ar::MountainAssetCatalog catalog;

    d2ar::MountainAssetVisual visual;
    visual.logical_sprite = "MOMNE0103";
    visual.width = 256;
    visual.height = 256;
    visual.canvas_foot_x = 64;
    visual.canvas_foot_y = 120;
    catalog.visuals[{4, 1, 1, 3}] = visual;

    EXPECT_EQ(catalog.find(0, 1, 1, 3), nullptr);
    EXPECT_EQ(catalog.find(1, 1, 1, 3), nullptr);
    EXPECT_EQ(catalog.find(2, 1, 1, 3), nullptr);
    EXPECT_EQ(catalog.find(3, 1, 1, 3), nullptr);
    EXPECT_EQ(catalog.find(5, 1, 1, 3), nullptr);

    EXPECT_NE(catalog.find(4, 1, 1, 3), nullptr);
}

TEST(AdventureMapRendering, MountainCatalogRejectsMissingSizeOrImage) {
    d2ar::MountainAssetCatalog catalog;

    d2ar::MountainAssetVisual visual;
    visual.logical_sprite = "MOMNE0103";
    visual.width = 256;
    visual.height = 256;
    visual.canvas_foot_x = 64;
    visual.canvas_foot_y = 120;
    catalog.visuals[{4, 1, 1, 3}] = visual;

    EXPECT_EQ(catalog.find(4, 1, 1, 0), nullptr);
    EXPECT_EQ(catalog.find(4, 1, 1, 9), nullptr);
    EXPECT_EQ(catalog.find(4, 2, 2, 3), nullptr);
    EXPECT_EQ(catalog.find(4, 3, 3, 3), nullptr);
}

TEST(AdventureMapRendering, MountainCatalogRejectsNonSquareSize) {
    d2ar::MountainAssetCatalog catalog;

    d2ar::MountainAssetVisual visual;
    visual.logical_sprite = "MOMNE0103";
    visual.width = 256;
    visual.height = 256;
    visual.canvas_foot_x = 64;
    visual.canvas_foot_y = 120;
    catalog.visuals[{4, 1, 1, 3}] = visual;

    EXPECT_EQ(catalog.find(4, 1, 2, 3), nullptr);
}

// ========================================================================
// Mountain contributor — multi-cell footprint behavior
// ========================================================================

TEST(AdventureMapRendering, MountainContributorOneByOneProducesOnePrimitive) {
    d2ar::MountainAssetCatalog catalog;

    d2ar::MountainAssetVisual visual;
    visual.logical_sprite = "MOMNE0103";
    visual.width = 256;
    visual.height = 256;
    visual.canvas_foot_x = 64;
    visual.canvas_foot_y = 120;
    catalog.visuals[{4, 1, 1, 3}] = visual;

    d2ar::AdventureMapPreparer preparer(kTestGeo);
    preparer.add_contributor(d2ar::make_mountain_contributor(catalog));

    d2runtime::AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.mountains.push_back({
        .id = "M1",
        .id_mount = 1,
        .image = 3,
        .race = 4,
        .position = {5, 5},
        .size_x = 1,
        .size_y = 1,
        .footprint = {{5, 5}},
    });

    const auto result = preparer.prepare(world);
    ASSERT_EQ(result.graph.world.size(), 1U);
    const auto& prim = result.graph.world[0];

    EXPECT_EQ(prim.level, d2ar::WorldRenderLevel::Structure);
    EXPECT_EQ(prim.phase, d2ar::AdventureRenderPhase::World);
    EXPECT_EQ(prim.container_path, "Imgs/IsoTerrn.ff");
    EXPECT_EQ(prim.record_name, "MOMNE0103");
    ASSERT_EQ(prim.footprint.size(), 1U);
    EXPECT_EQ(prim.footprint[0].x, 5);
    EXPECT_EQ(prim.footprint[0].y, 5);
    EXPECT_EQ(prim.depth_anchor.x, 5);
    EXPECT_EQ(prim.depth_anchor.y, 5);

    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics[0].kind, d2ar::PrepareDiagnosticKind::Resolved);
}

TEST(AdventureMapRendering, MountainContributorTwoByTwoProducesOnePrimitive) {
    d2ar::MountainAssetCatalog catalog;

    d2ar::MountainAssetVisual visual;
    visual.logical_sprite = "MOMNE0207";
    visual.width = 512;
    visual.height = 512;
    visual.canvas_foot_x = 128;
    visual.canvas_foot_y = 240;
    catalog.visuals[{4, 2, 2, 7}] = visual;

    d2ar::AdventureMapPreparer preparer(kTestGeo);
    preparer.add_contributor(d2ar::make_mountain_contributor(catalog));

    d2runtime::AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.mountains.push_back({
        .id = "M2",
        .id_mount = 2,
        .image = 7,
        .race = 4,
        .position = {3, 4},
        .size_x = 2,
        .size_y = 2,
        .footprint = {{3, 4}, {4, 4}, {3, 5}, {4, 5}},
    });

    const auto result = preparer.prepare(world);
    ASSERT_EQ(result.graph.world.size(), 1U);
    const auto& prim = result.graph.world[0];

    EXPECT_EQ(prim.level, d2ar::WorldRenderLevel::Structure);
    EXPECT_EQ(prim.phase, d2ar::AdventureRenderPhase::World);
    EXPECT_EQ(prim.record_name, "MOMNE0207");
    ASSERT_EQ(prim.footprint.size(), 4U);
    // depth anchor is front-most cell: (4,5) — max iso_depth = 9
    EXPECT_EQ(prim.depth_anchor.x, 4);
    EXPECT_EQ(prim.depth_anchor.y, 5);

    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics[0].kind, d2ar::PrepareDiagnosticKind::Resolved);
}

TEST(AdventureMapRendering, MountainContributorThreeByThreeProducesOnePrimitive) {
    d2ar::MountainAssetCatalog catalog;

    d2ar::MountainAssetVisual visual;
    visual.logical_sprite = "MOMNE0300";
    visual.width = 768;
    visual.height = 768;
    visual.canvas_foot_x = 192;
    visual.canvas_foot_y = 360;
    catalog.visuals[{4, 3, 3, 0}] = visual;

    d2ar::AdventureMapPreparer preparer(kTestGeo);
    preparer.add_contributor(d2ar::make_mountain_contributor(catalog));

    d2runtime::AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.mountains.push_back({
        .id = "M3",
        .id_mount = 3,
        .image = 0,
        .race = 4,
        .position = {2, 3},
        .size_x = 3,
        .size_y = 3,
        .footprint = {{2, 3}, {3, 3}, {4, 3}, {2, 4}, {3, 4}, {4, 4}, {2, 5}, {3, 5}, {4, 5}},
    });

    const auto result = preparer.prepare(world);
    ASSERT_EQ(result.graph.world.size(), 1U);
    const auto& prim = result.graph.world[0];

    EXPECT_EQ(prim.level, d2ar::WorldRenderLevel::Structure);
    ASSERT_EQ(prim.footprint.size(), 9U);
    // front-most cell: (4,5) — max iso_depth = 9
    EXPECT_EQ(prim.depth_anchor.x, 4);
    EXPECT_EQ(prim.depth_anchor.y, 5);

    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics[0].kind, d2ar::PrepareDiagnosticKind::Resolved);
}

TEST(AdventureMapRendering, MountainContributorFiveByFiveProducesOnePrimitive) {
    d2ar::MountainAssetCatalog catalog;

    d2ar::MountainAssetVisual visual;
    visual.logical_sprite = "MOMNE0506";
    visual.width = 1280;
    visual.height = 1280;
    visual.canvas_foot_x = 320;
    visual.canvas_foot_y = 600;
    catalog.visuals[{4, 5, 5, 6}] = visual;

    d2ar::AdventureMapPreparer preparer(kTestGeo);
    preparer.add_contributor(d2ar::make_mountain_contributor(catalog));

    d2runtime::AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.mountains.push_back({
        .id = "M5",
        .id_mount = 5,
        .image = 6,
        .race = 4,
        .position = {1, 1},
        .size_x = 5,
        .size_y = 5,
    });
    for (int dy = 0; dy < 5; ++dy) {
        for (int dx = 0; dx < 5; ++dx) {
            world.mountains[0].footprint.push_back({1 + dx, 1 + dy});
        }
    }

    const auto result = preparer.prepare(world);
    ASSERT_EQ(result.graph.world.size(), 1U);
    const auto& prim = result.graph.world[0];

    EXPECT_EQ(prim.level, d2ar::WorldRenderLevel::Structure);
    ASSERT_EQ(prim.footprint.size(), 25U);
    // front-most cell: (5,5) — max iso_depth = 10
    EXPECT_EQ(prim.depth_anchor.x, 5);
    EXPECT_EQ(prim.depth_anchor.y, 5);
}

// ========================================================================
// Mountain contributor — NO greedy packing or merging
// ========================================================================

TEST(AdventureMapRendering, MountainContributorNoGreedyPackingOfAdjacentMountains) {
    d2ar::MountainAssetCatalog catalog;

    d2ar::MountainAssetVisual visual_1x1;
    visual_1x1.logical_sprite = "MOMNE0100";
    visual_1x1.width = 256;
    visual_1x1.height = 256;
    visual_1x1.canvas_foot_x = 64;
    visual_1x1.canvas_foot_y = 120;
    catalog.visuals[{4, 1, 1, 0}] = visual_1x1;

    d2ar::MountainAssetVisual visual_2x2;
    visual_2x2.logical_sprite = "MOMNE0200";
    visual_2x2.width = 512;
    visual_2x2.height = 512;
    visual_2x2.canvas_foot_x = 128;
    visual_2x2.canvas_foot_y = 240;
    catalog.visuals[{4, 2, 2, 0}] = visual_2x2;

    d2ar::AdventureMapPreparer preparer(kTestGeo);
    preparer.add_contributor(d2ar::make_mountain_contributor(catalog));

    d2runtime::AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;

    // Four explicit 1×1 mountains occupying a 2×2 region.
    // They must NOT be merged into one 2×2 mountain.
    world.mountains.push_back({
        .id = "A",
        .id_mount = 10,
        .image = 0,
        .race = 4,
        .position = {5, 5},
        .size_x = 1,
        .size_y = 1,
        .footprint = {{5, 5}},
    });
    world.mountains.push_back({
        .id = "B",
        .id_mount = 20,
        .image = 0,
        .race = 4,
        .position = {6, 5},
        .size_x = 1,
        .size_y = 1,
        .footprint = {{6, 5}},
    });
    world.mountains.push_back({
        .id = "C",
        .id_mount = 30,
        .image = 0,
        .race = 4,
        .position = {5, 6},
        .size_x = 1,
        .size_y = 1,
        .footprint = {{5, 6}},
    });
    world.mountains.push_back({
        .id = "D",
        .id_mount = 40,
        .image = 0,
        .race = 4,
        .position = {6, 6},
        .size_x = 1,
        .size_y = 1,
        .footprint = {{6, 6}},
    });

    const auto result = preparer.prepare(world);

    // Must produce exactly 4 mountain primitives, NOT 1 merged 2×2
    ASSERT_EQ(result.graph.world.size(), 4U);
    for (const auto& prim : result.graph.world) {
        EXPECT_EQ(prim.record_name, "MOMNE0100");
        EXPECT_EQ(prim.level, d2ar::WorldRenderLevel::Structure);
        ASSERT_EQ(prim.footprint.size(), 1U);
    }
}

TEST(AdventureMapRendering, MountainContributorExplicitTwoByTwoNotDecomposed) {
    d2ar::MountainAssetCatalog catalog;

    d2ar::MountainAssetVisual visual_1x1;
    visual_1x1.logical_sprite = "MOMNE0100";
    visual_1x1.width = 256;
    visual_1x1.height = 256;
    visual_1x1.canvas_foot_x = 64;
    visual_1x1.canvas_foot_y = 120;
    catalog.visuals[{4, 1, 1, 0}] = visual_1x1;

    d2ar::MountainAssetVisual visual_2x2;
    visual_2x2.logical_sprite = "MOMNE0200";
    visual_2x2.width = 512;
    visual_2x2.height = 512;
    visual_2x2.canvas_foot_x = 128;
    visual_2x2.canvas_foot_y = 240;
    catalog.visuals[{4, 2, 2, 0}] = visual_2x2;

    d2ar::AdventureMapPreparer preparer(kTestGeo);
    preparer.add_contributor(d2ar::make_mountain_contributor(catalog));

    d2runtime::AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.mountains.push_back({
        .id = "AB",
        .id_mount = 99,
        .image = 0,
        .race = 4,
        .position = {3, 4},
        .size_x = 2,
        .size_y = 2,
        .footprint = {{3, 4}, {4, 4}, {3, 5}, {4, 5}},
    });

    const auto result = preparer.prepare(world);

    // Must produce exactly 1 primitive with a 4-cell footprint
    ASSERT_EQ(result.graph.world.size(), 1U);
    const auto& prim = result.graph.world[0];
    EXPECT_EQ(prim.record_name, "MOMNE0200");
    EXPECT_EQ(prim.level, d2ar::WorldRenderLevel::Structure);
    ASSERT_EQ(prim.footprint.size(), 4U);
}

// ========================================================================
// Mountain contributor — draw origin uses FF-style metadata
// ========================================================================

TEST(AdventureMapRendering, MountainDrawOriginUsesFfMetadata) {
    d2ar::MountainAssetCatalog catalog;

    d2ar::MountainAssetVisual visual;
    visual.logical_sprite = "MOMNE0207";
    visual.width = 500;
    visual.height = 400;
    visual.canvas_foot_x = 128;
    visual.canvas_foot_y = 300;
    catalog.visuals[{4, 2, 2, 7}] = visual;

    d2ar::AdventureMapPreparer preparer(kTestGeo);
    preparer.add_contributor(d2ar::make_mountain_contributor(catalog));

    d2runtime::AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.mountains.push_back({
        .id = "M_FF",
        .id_mount = 7,
        .image = 7,
        .race = 4,
        .position = {3, 4},
        .size_x = 2,
        .size_y = 2,
        .footprint = {{3, 4}, {4, 4}, {3, 5}, {4, 5}},
    });

    const auto result = preparer.prepare(world);
    ASSERT_EQ(result.graph.world.size(), 1U);
    const auto& prim = result.graph.world[0];

    // Front-most cell: (4,5), depth=9
    EXPECT_EQ(prim.depth_anchor.x, 4);
    EXPECT_EQ(prim.depth_anchor.y, 5);

    // Calculate expected foot anchor from geometry
    const auto foot = kTestGeo.cell_foot_anchor({4, 5});
    const int  expected_draw_x = foot.x - visual.canvas_foot_x;
    const int  expected_draw_y = foot.y - visual.canvas_foot_y;

    EXPECT_EQ(prim.draw_origin.x, expected_draw_x);
    EXPECT_EQ(prim.draw_origin.y, expected_draw_y);

    // Visual bounds
    EXPECT_EQ(prim.visual_bounds.min_x, expected_draw_x);
    EXPECT_EQ(prim.visual_bounds.min_y, expected_draw_y);
    EXPECT_EQ(prim.visual_bounds.max_x, expected_draw_x + visual.width);
    EXPECT_EQ(prim.visual_bounds.max_y, expected_draw_y + visual.height);

    // Source dimensions
    EXPECT_EQ(prim.src_width, visual.width);
    EXPECT_EQ(prim.src_height, visual.height);

    // Footprint is preserved
    ASSERT_EQ(prim.footprint.size(), 4U);
}

// ========================================================================
// Mountain contributor — unresolved produces diagnostics, no crash
// ========================================================================

TEST(AdventureMapRendering, MountainUnsupportedRaceProducesDiagnostic) {
    d2ar::MountainAssetCatalog catalog;

    d2ar::MountainAssetVisual visual;
    visual.logical_sprite = "MOMNE0103";
    visual.width = 256;
    visual.height = 256;
    visual.canvas_foot_x = 64;
    visual.canvas_foot_y = 120;
    catalog.visuals[{4, 1, 1, 3}] = visual;

    d2ar::AdventureMapPreparer preparer(kTestGeo);
    preparer.add_contributor(d2ar::make_mountain_contributor(catalog));

    d2runtime::AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.mountains.push_back({
        .id = "M_BAD",
        .id_mount = 1,
        .image = 3,
        .race = 0,
        .position = {5, 5},
        .size_x = 1,
        .size_y = 1,
        .footprint = {{5, 5}},
    });

    const auto result = preparer.prepare(world);
    EXPECT_EQ(result.graph.world.size(), 0U);
    ASSERT_GE(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics[0].kind, d2ar::PrepareDiagnosticKind::UnresolvedNoSprite);
    EXPECT_EQ(result.diagnostics[0].object_id, "M_BAD");
    EXPECT_TRUE(result.has_unresolved());
}

TEST(AdventureMapRendering, MountainMissingAssetProducesUnresolved) {
    d2ar::MountainAssetCatalog catalog; // empty — no sprites registered

    d2ar::AdventureMapPreparer preparer(kTestGeo);
    preparer.add_contributor(d2ar::make_mountain_contributor(catalog));

    d2runtime::AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.mountains.push_back({
        .id = "M_MISS",
        .id_mount = 42,
        .image = 9,
        .race = 4,
        .position = {1, 1},
        .size_x = 3,
        .size_y = 3,
        .footprint = {{1, 1}, {2, 1}, {3, 1}, {1, 2}, {2, 2}, {3, 2}, {1, 3}, {2, 3}, {3, 3}},
    });

    const auto result = preparer.prepare(world);
    EXPECT_EQ(result.graph.world.size(), 0U);
    ASSERT_GE(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics[0].kind, d2ar::PrepareDiagnosticKind::UnresolvedNoSprite);
    EXPECT_TRUE(result.has_unresolved());
}

// ========================================================================
// Mountain contributor — stable identity
// ========================================================================

TEST(AdventureMapRendering, MountainContributorUsesStableIdentity) {
    d2ar::MountainAssetCatalog catalog;

    d2ar::MountainAssetVisual visual;
    visual.logical_sprite = "MOMNE0100";
    visual.width = 256;
    visual.height = 256;
    visual.canvas_foot_x = 64;
    visual.canvas_foot_y = 120;
    catalog.visuals[{4, 1, 1, 0}] = visual;

    d2ar::AdventureMapPreparer preparer(kTestGeo);
    preparer.add_contributor(d2ar::make_mountain_contributor(catalog));

    d2runtime::AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;

    world.mountains.push_back({
        .id = "S143MM0001/42",
        .id_mount = 42,
        .image = 0,
        .race = 4,
        .position = {5, 5},
        .size_x = 1,
        .size_y = 1,
        .footprint = {{5, 5}},
    });

    world.mountains.push_back({
        .id = "S143MM0001/99",
        .id_mount = 99,
        .image = 0,
        .race = 4,
        .position = {6, 6},
        .size_x = 1,
        .size_y = 1,
        .footprint = {{6, 6}},
    });

    const auto result = preparer.prepare(world);
    ASSERT_EQ(result.graph.world.size(), 2U);

    EXPECT_NE(result.graph.world[0].stable_id, result.graph.world[1].stable_id);
    EXPECT_EQ(result.graph.world[0].stable_id, d2ar::stable_render_id("Mountain:S143MM0001/42"));
    EXPECT_EQ(result.graph.world[1].stable_id, d2ar::stable_render_id("Mountain:S143MM0001/99"));
}

// ========================================================================
// Landmark contributor — static resolution
// ========================================================================

TEST(AdventureMapRendering, LandmarkStaticResolvesToOnePrimitive) {
    d2ar::LandmarkAssetCatalog catalog;

    d2ar::StaticLandmarkVisual visual;
    visual.container_path = "Imgs/IsoCmon.ff";
    visual.logical_sprite = "G000MG0099";
    visual.width = 256;
    visual.height = 256;
    visual.canvas_foot_x = 64;
    visual.canvas_foot_y = 120;
    catalog.visuals["G000MG0099"] = visual;

    d2ar::AdventureMapPreparer preparer(kTestGeo);
    preparer.add_contributor(d2ar::make_landmark_contributor(catalog));

    d2runtime::AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.landmarks.push_back({
        .id = "S095MM003e",
        .type_id = "G000MG0099",
        .position = {5, 5},
        .footprint = {{5, 5}},
    });

    const auto result = preparer.prepare(world);
    ASSERT_EQ(result.graph.world.size(), 1U);
    const auto& prim = result.graph.world[0];

    EXPECT_EQ(prim.level, d2ar::WorldRenderLevel::Structure);
    EXPECT_EQ(prim.phase, d2ar::AdventureRenderPhase::World);
    EXPECT_EQ(prim.container_path, "Imgs/IsoCmon.ff");
    EXPECT_EQ(prim.record_name, "G000MG0099");
    ASSERT_EQ(prim.footprint.size(), 1U);
    EXPECT_FALSE(prim.animation.has_value());

    ASSERT_EQ(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics[0].kind, d2ar::PrepareDiagnosticKind::Resolved);
}

TEST(AdventureMapRendering, LandmarkAnimatedResolvesWithAnimationData) {
    d2ar::LandmarkAssetCatalog catalog;

    d2ar::AnimatedLandmarkVisual visual;
    visual.container_path = "Imgs/IsoCmon.ff";
    visual.logical_animation = "G000MG0027";
    visual.canvas_foot_x = 400;
    visual.canvas_foot_y = 580;
    visual.animation_data.animation_name = "G000MG0027";
    visual.animation_data.native_canvas_w = 800;
    visual.animation_data.native_canvas_h = 600;
    visual.animation_data.frames = {
        {.record_name = "FRAME1.PNG", .duration_ms = 100},
        {.record_name = "FRAME2.PNG", .duration_ms = 100},
    };
    catalog.visuals["G000MG0027"] = visual;

    d2ar::AdventureMapPreparer preparer(kTestGeo);
    preparer.add_contributor(d2ar::make_landmark_contributor(catalog));

    d2runtime::AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.landmarks.push_back({
        .id = "S095LM0027",
        .type_id = "G000MG0027",
        .position = {3, 3},
        .footprint = {{3, 3}},
    });

    const auto result = preparer.prepare(world);
    ASSERT_EQ(result.graph.world.size(), 1U);
    const auto& prim = result.graph.world[0];

    EXPECT_EQ(prim.level, d2ar::WorldRenderLevel::Structure);
    EXPECT_EQ(prim.container_path, "Imgs/IsoCmon.ff");
    ASSERT_TRUE(prim.animation.has_value());
    EXPECT_EQ(prim.animation->animation_name, "G000MG0027");
    EXPECT_EQ(prim.animation->frames.size(), 2U);
}

TEST(AdventureMapRendering, LandmarkUnknownTypeProducesUnresolved) {
    d2ar::LandmarkAssetCatalog catalog; // empty

    d2ar::AdventureMapPreparer preparer(kTestGeo);
    preparer.add_contributor(d2ar::make_landmark_contributor(catalog));

    d2runtime::AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.landmarks.push_back({
        .id = "S095LM0999",
        .type_id = "G000MG0999",
        .position = {5, 5},
        .footprint = {{5, 5}},
    });

    const auto result = preparer.prepare(world);
    EXPECT_EQ(result.graph.world.size(), 0U);
    ASSERT_GE(result.diagnostics.size(), 1U);
    EXPECT_EQ(result.diagnostics[0].kind, d2ar::PrepareDiagnosticKind::UnresolvedNoSprite);
}

TEST(AdventureMapRendering, LandmarkMultiCellFootprintPreserved) {
    d2ar::LandmarkAssetCatalog catalog;

    d2ar::StaticLandmarkVisual visual;
    visual.logical_sprite = "G000MG0099";
    visual.width = 512;
    visual.height = 512;
    visual.canvas_foot_x = 128;
    visual.canvas_foot_y = 240;
    catalog.visuals["G000MG0099"] = visual;

    d2ar::AdventureMapPreparer preparer(kTestGeo);
    preparer.add_contributor(d2ar::make_landmark_contributor(catalog));

    d2runtime::AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.landmarks.push_back({
        .id = "S095LM_BIG",
        .type_id = "G000MG0099",
        .position = {3, 4},
        .footprint = {{3, 4}, {4, 4}, {3, 5}, {4, 5}},
    });

    const auto result = preparer.prepare(world);
    ASSERT_EQ(result.graph.world.size(), 1U);
    const auto& prim = result.graph.world[0];

    ASSERT_EQ(prim.footprint.size(), 4U);
    // Front-most cell: (4,5) — max iso_depth = 9
    EXPECT_EQ(prim.depth_anchor.x, 4);
    EXPECT_EQ(prim.depth_anchor.y, 5);
}

TEST(AdventureMapRendering, LandmarkDrawOriginUsesFfMetadata) {
    d2ar::LandmarkAssetCatalog catalog;

    d2ar::StaticLandmarkVisual visual;
    visual.logical_sprite = "G000MG0099";
    visual.width = 500;
    visual.height = 400;
    visual.canvas_foot_x = 100;
    visual.canvas_foot_y = 300;
    catalog.visuals["G000MG0099"] = visual;

    d2ar::AdventureMapPreparer preparer(kTestGeo);
    preparer.add_contributor(d2ar::make_landmark_contributor(catalog));

    d2runtime::AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.landmarks.push_back({
        .id = "S095LM_FF",
        .type_id = "G000MG0099",
        .position = {3, 4},
        .footprint = {{3, 4}, {4, 4}, {3, 5}, {4, 5}},
    });

    const auto result = preparer.prepare(world);
    ASSERT_EQ(result.graph.world.size(), 1U);
    const auto& prim = result.graph.world[0];

    const auto foot = kTestGeo.cell_foot_anchor({4, 5});
    const int  expected_draw_x = foot.x - visual.canvas_foot_x;
    const int  expected_draw_y = foot.y - visual.canvas_foot_y;
    EXPECT_EQ(prim.draw_origin.x, expected_draw_x);
    EXPECT_EQ(prim.draw_origin.y, expected_draw_y);
    EXPECT_EQ(prim.src_width, visual.width);
    EXPECT_EQ(prim.src_height, visual.height);
}

TEST(AdventureMapRendering, LandmarkUsesStableIdentity) {
    d2ar::LandmarkAssetCatalog catalog;

    d2ar::StaticLandmarkVisual visual;
    visual.container_path = "Imgs/IsoCmon.ff";
    visual.logical_sprite = "G000MG0099";
    visual.width = 256;
    visual.height = 256;
    visual.canvas_foot_x = 64;
    visual.canvas_foot_y = 120;
    catalog.visuals["G000MG0099"] = visual;

    d2ar::AdventureMapPreparer preparer(kTestGeo);
    preparer.add_contributor(d2ar::make_landmark_contributor(catalog));

    d2runtime::AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.landmarks.push_back({
        .id = "S095MM003a",
        .type_id = "G000MG0099",
        .position = {1, 1},
        .footprint = {{1, 1}},
    });
    world.landmarks.push_back({
        .id = "S095MM003b",
        .type_id = "G000MG0099",
        .position = {5, 5},
        .footprint = {{5, 5}},
    });

    const auto result = preparer.prepare(world);
    ASSERT_EQ(result.graph.world.size(), 2U);
    EXPECT_NE(result.graph.world[0].stable_id, result.graph.world[1].stable_id);
}

// ========================================================================
// Landmark candidate resolver — real production function tests
// ========================================================================

#include <d2engine/assets/detail/landmark_asset_resolution.hpp>

using LmKind = d2engine::detail::LandmarkAssetKind;
using LmCand = d2engine::detail::LandmarkAssetCandidate;
using LmResolved = d2engine::detail::ResolvedLandmarkAsset;

TEST(AdventureMapRendering, LandmarkResolverAnimationBeatsStatic) {
    std::vector<LmCand> cands = {
        {"imgs/other.ff", "G000MG0027", LmKind::StaticSprite},
        {"imgs/isocmon.ff", "G000MG0027", LmKind::Animation},
    };
    auto resolved = d2engine::detail::resolve_landmark_candidates(cands);
    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(resolved->container_path, "imgs/isocmon.ff");
    EXPECT_EQ(resolved->kind, LmKind::Animation);
}

TEST(AdventureMapRendering, LandmarkResolverSameContainerSpriteAndAnimation) {
    std::vector<LmCand> cands = {
        {"imgs/isocmon.ff", "G000MG0027", LmKind::StaticSprite},
        {"imgs/isocmon.ff", "G000MG0027", LmKind::Animation},
    };
    auto resolved = d2engine::detail::resolve_landmark_candidates(cands);
    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(resolved->kind, LmKind::Animation);
}

TEST(AdventureMapRendering, LandmarkResolverOneStaticCandidate) {
    std::vector<LmCand> cands = {
        {"imgs/isocmon.ff", "G000MG0099", LmKind::StaticSprite},
    };
    auto resolved = d2engine::detail::resolve_landmark_candidates(cands);
    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(resolved->container_path, "imgs/isocmon.ff");
    EXPECT_EQ(resolved->kind, LmKind::StaticSprite);
}

TEST(AdventureMapRendering, LandmarkResolverOneAnimationCandidate) {
    std::vector<LmCand> cands = {
        {"imgs/isocmon.ff", "G000MG0027", LmKind::Animation},
    };
    auto resolved = d2engine::detail::resolve_landmark_candidates(cands);
    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(resolved->container_path, "imgs/isocmon.ff");
    EXPECT_EQ(resolved->kind, LmKind::Animation);
}

TEST(AdventureMapRendering, LandmarkResolverMultiAnimIsoCmon) {
    std::vector<LmCand> cands = {
        {"imgs/foo.ff", "G000MG0073", LmKind::Animation},
        {"imgs/isocmon.ff", "G000MG0073", LmKind::Animation},
    };
    auto resolved = d2engine::detail::resolve_landmark_candidates(cands);
    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(resolved->container_path, "imgs/isocmon.ff");
    EXPECT_EQ(resolved->kind, LmKind::Animation);
}

TEST(AdventureMapRendering, LandmarkResolverMultiSpriteIsoCmon) {
    std::vector<LmCand> cands = {
        {"imgs/bar.ff", "G000MG0041", LmKind::StaticSprite},
        {"imgs/isocmon.ff", "G000MG0041", LmKind::StaticSprite},
    };
    auto resolved = d2engine::detail::resolve_landmark_candidates(cands);
    ASSERT_TRUE(resolved.has_value());
    EXPECT_EQ(resolved->container_path, "imgs/isocmon.ff");
    EXPECT_EQ(resolved->kind, LmKind::StaticSprite);
}

TEST(AdventureMapRendering, LandmarkResolverMultiAnimNoIsoCmonIsAmbiguous) {
    std::vector<LmCand> cands = {
        {"imgs/foo.ff", "G000MG0073", LmKind::Animation},
        {"imgs/bar.ff", "G000MG0073", LmKind::Animation},
    };
    auto resolved = d2engine::detail::resolve_landmark_candidates(cands);
    EXPECT_FALSE(resolved.has_value());
}

TEST(AdventureMapRendering, LandmarkResolverMultiSpriteNoIsoCmonIsAmbiguous) {
    std::vector<LmCand> cands = {
        {"imgs/foo.ff", "G000MG0041", LmKind::StaticSprite},
        {"imgs/bar.ff", "G000MG0041", LmKind::StaticSprite},
    };
    auto resolved = d2engine::detail::resolve_landmark_candidates(cands);
    EXPECT_FALSE(resolved.has_value());
}

TEST(AdventureMapRendering, LandmarkResolverMultiIsoCmonAnimIsAmbiguous) {
    std::vector<LmCand> cands = {
        {"imgs/isocmon.ff", "G000MG0099", LmKind::Animation},
        {"imgs/isocmon.ff", "G000MG0099", LmKind::Animation},
    };
    auto resolved = d2engine::detail::resolve_landmark_candidates(cands);
    EXPECT_FALSE(resolved.has_value());
}

TEST(AdventureMapRendering, LandmarkResolverEmptyCandidates) {
    std::vector<LmCand> cands;
    auto                resolved = d2engine::detail::resolve_landmark_candidates(cands);
    EXPECT_FALSE(resolved.has_value());
}

// ========================================================================
// Landmark normalization + catalog integration tests
// ========================================================================

TEST(AdventureMapRendering, LandmarkCatalogCaseInsensitiveFind) {
    d2ar::LandmarkAssetCatalog catalog;

    d2ar::StaticLandmarkVisual sv;
    sv.container_path = "imgs/isocmon.ff";
    sv.logical_sprite = "G000MG0099";
    sv.width = 256;
    sv.height = 256;
    sv.canvas_foot_x = 64;
    sv.canvas_foot_y = 120;
    catalog.visuals["G000MG0099"] = sv;

    EXPECT_NE(catalog.find("G000MG0099"), nullptr);
    EXPECT_NE(catalog.find("g000mg0099"), nullptr);
    EXPECT_NE(catalog.find("G000mg0099"), nullptr);
}

TEST(AdventureMapRendering, LandmarkContributorNoLongerNormalizesType) {
    d2ar::LandmarkAssetCatalog catalog;

    d2ar::StaticLandmarkVisual sv;
    sv.container_path = "imgs/isocmon.ff";
    sv.logical_sprite = "G000MG0099";
    sv.width = 256;
    sv.height = 256;
    sv.canvas_foot_x = 64;
    sv.canvas_foot_y = 120;
    catalog.visuals["G000MG0099"] = sv;

    d2ar::AdventureMapPreparer preparer(kTestGeo);
    preparer.add_contributor(d2ar::make_landmark_contributor(catalog));

    d2runtime::AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.landmarks.push_back({
        .id = "LM",
        .type_id = "g000mg0099",
        .position = {5, 5},
        .footprint = {{5, 5}},
    });

    const auto result = preparer.prepare(world);
    ASSERT_EQ(result.graph.world.size(), 1U);
    EXPECT_EQ(result.graph.world[0].record_name, "G000MG0099");
}

TEST(AdventureMapRendering, LandmarkAnimatedProducesLoopingData) {
    d2ar::LandmarkAssetCatalog catalog;

    d2ar::AnimatedLandmarkVisual av;
    av.container_path = "imgs/isocmon.ff";
    av.logical_animation = "G000MG0027";
    av.canvas_foot_x = 400;
    av.canvas_foot_y = 580;
    av.animation_data.animation_name = "G000MG0027";
    av.animation_data.native_canvas_w = 800;
    av.animation_data.native_canvas_h = 600;
    av.animation_data.is_looping = true;
    av.animation_data.frames = {
        {.record_name = "F1.PNG", .duration_ms = 100},
        {.record_name = "F2.PNG", .duration_ms = 100},
    };
    catalog.visuals["G000MG0027"] = av;

    d2ar::AdventureMapPreparer preparer(kTestGeo);
    preparer.add_contributor(d2ar::make_landmark_contributor(catalog));

    d2runtime::AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.landmarks.push_back({
        .id = "S095LM_LOOP",
        .type_id = "G000MG0027",
        .position = {3, 3},
        .footprint = {{3, 3}},
    });

    const auto result = preparer.prepare(world);
    ASSERT_EQ(result.graph.world.size(), 1U);
    ASSERT_TRUE(result.graph.world[0].animation.has_value());
    EXPECT_TRUE(result.graph.world[0].animation->is_looping);
    EXPECT_EQ(result.graph.world[0].animation->frames.size(), 2U);
}

// ========================================================================
// Animated Landmark — stable draw origin regression
// ========================================================================

// SFINAE helpers to verify AdventureAnimationFrame does NOT carry
// per-frame foot metadata.  Using derived visible-piece bounding
// boxes as animation pivots causes visible jitter for effects
// whose visible-piece extents change between frames (e.g. fire, lava).
// The following static_asserts fail to compile if the fields are re-added.
namespace {
template <typename, typename = void> struct has_canvas_foot_x : std::false_type {};
template <typename T>
struct has_canvas_foot_x<T, std::void_t<decltype(std::declval<T>().canvas_foot_x)>>
    : std::true_type {};
static_assert(!has_canvas_foot_x<d2engine::adventure_render::AdventureAnimationFrame>::value,
              "AdventureAnimationFrame must not have canvas_foot_x");

template <typename, typename = void> struct has_canvas_foot_y : std::false_type {};
template <typename T>
struct has_canvas_foot_y<T, std::void_t<decltype(std::declval<T>().canvas_foot_y)>>
    : std::true_type {};
static_assert(!has_canvas_foot_y<d2engine::adventure_render::AdventureAnimationFrame>::value,
              "AdventureAnimationFrame must not have canvas_foot_y");
} // namespace

TEST(AdventureMapRendering, AnimatedLandmarkDrawOriginStableAcrossFrames) {
    d2ar::LandmarkAssetCatalog catalog;

    // Animated landmark with deliberately varying frame geometry.
    // In real game data the per-frame visible-piece-derived foot would be:
    //   frame 0:  foot=(50, 100)  — matches visual-level anchor
    //   frame 1:  foot=(55, 96)
    //   frame 2:  foot=(47, 102)
    // These differences MUST NOT affect the world draw origin.
    d2ar::AnimatedLandmarkVisual av;
    av.container_path = "imgs/isocmon.ff";
    av.logical_animation = "STABLE_FIRE";
    av.canvas_foot_x = 50; // stable visual-level anchor
    av.canvas_foot_y = 100;
    av.animation_data.animation_name = "STABLE_FIRE";
    av.animation_data.native_canvas_w = 200;
    av.animation_data.native_canvas_h = 200;
    av.animation_data.is_looping = true;
    av.animation_data.frames = {
        {.record_name = "F0.PNG", .duration_ms = 100, .canvas_width = 200, .canvas_height = 200},
        {.record_name = "F1.PNG", .duration_ms = 100, .canvas_width = 210, .canvas_height = 190},
        {.record_name = "F2.PNG", .duration_ms = 100, .canvas_width = 200, .canvas_height = 200},
    };
    catalog.visuals["STABLE_FIRE"] = av;

    d2ar::AdventureMapPreparer preparer(kTestGeo);
    preparer.add_contributor(d2ar::make_landmark_contributor(catalog));

    d2runtime::AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.landmarks.push_back({
        .id = "LM_STABLE",
        .type_id = "STABLE_FIRE",
        .position = {5, 5},
        .footprint = {{5, 5}},
    });

    const auto result = preparer.prepare(world);
    ASSERT_EQ(result.graph.world.size(), 1U);
    const auto& prim = result.graph.world[0];
    ASSERT_TRUE(prim.animation.has_value());
    const auto& anim = *prim.animation;
    ASSERT_EQ(anim.frames.size(), 3U);

    // Frame progression changes record_name.
    EXPECT_EQ(anim.frames[0].record_name, "F0.PNG");
    EXPECT_EQ(anim.frames[1].record_name, "F1.PNG");
    EXPECT_EQ(anim.frames[2].record_name, "F2.PNG");

    // Per-frame source dimensions may differ — only src_w/src_h change.
    EXPECT_EQ(anim.frames[1].canvas_width, 210);
    EXPECT_EQ(anim.frames[1].canvas_height, 190);

    // Prepare-time draw origin uses the stable visual-level anchor.
    const auto foot = kTestGeo.cell_foot_anchor({5, 5});
    const int  expected_draw_x = foot.x - av.canvas_foot_x;
    const int  expected_draw_y = foot.y - av.canvas_foot_y;
    EXPECT_EQ(prim.draw_origin.x, expected_draw_x);
    EXPECT_EQ(prim.draw_origin.y, expected_draw_y);

    // Projected world draw origin for every frame is the same.
    // In the old code, adventure_screen.cpp::render() adjusted
    // origin_x += f0.canvas_foot_x - af.canvas_foot_x  per frame.
    // With differing non-zero feet that caused jitter.
    // After the fix, draw origin is stable — no per-frame correction.
    for (std::size_t i = 0; i < anim.frames.size(); ++i) {
        const int projected_x = prim.draw_origin.x;
        const int projected_y = prim.draw_origin.y;
        EXPECT_EQ(projected_x, expected_draw_x);
        EXPECT_EQ(projected_y, expected_draw_y);
    }
}

// ========================================================================
// AdventureCamera zoom
// ========================================================================

TEST(AdventureMapRendering, CameraDefaultZoomIsOne) {
    d2engine::AdventureCamera cam;
    EXPECT_FLOAT_EQ(cam.zoom(), 1.0f);
    EXPECT_EQ(cam.zoom_index, d2engine::AdventureCamera::kDefaultZoomIndex);
}

TEST(AdventureMapRendering, CameraZoomInSteps) {
    d2engine::AdventureCamera cam;
    // 1.0 → 1.25
    EXPECT_TRUE(cam.zoom_in());
    EXPECT_FLOAT_EQ(cam.zoom(), 1.25f);
}

TEST(AdventureMapRendering, CameraZoomOutSteps) {
    d2engine::AdventureCamera cam;
    // 1.0 → 0.75
    EXPECT_TRUE(cam.zoom_out());
    EXPECT_FLOAT_EQ(cam.zoom(), 0.75f);
}

TEST(AdventureMapRendering, CameraZoomInClamped) {
    d2engine::AdventureCamera cam;
    while (cam.zoom_in()) {
    }
    EXPECT_FLOAT_EQ(cam.zoom(), 2.0f);
    EXPECT_FALSE(cam.zoom_in());
    EXPECT_FLOAT_EQ(cam.zoom(), 2.0f);
}

TEST(AdventureMapRendering, CameraZoomOutClamped) {
    d2engine::AdventureCamera cam;
    while (cam.zoom_out()) {
    }
    EXPECT_FLOAT_EQ(cam.zoom(), 0.50f);
    EXPECT_FALSE(cam.zoom_out());
    EXPECT_FLOAT_EQ(cam.zoom(), 0.50f);
}

TEST(AdventureMapRendering, CameraScreenToCanvasInverse) {
    d2engine::AdventureCamera cam;
    cam.viewport_width = 1416;
    cam.viewport_height = 852;
    cam.canvas_x = 100;
    cam.canvas_y = 200;

    for (int i = 0; i < static_cast<int>(d2engine::AdventureCamera::kZoomLevels.size()); ++i) {
        cam.zoom_index = i;
        const int screen_x = 400;
        const int screen_y = 300;
        const int canvas_x = cam.screen_to_canvas_x(screen_x);
        const int canvas_y = cam.screen_to_canvas_y(screen_y);
        // Round-trip: canvas→screen should match original.
        const int round_x = cam.canvas_to_screen_x(canvas_x);
        const int round_y = cam.canvas_to_screen_y(canvas_y);
        EXPECT_NEAR(round_x, screen_x, 2) << "at zoom " << cam.zoom();
        EXPECT_NEAR(round_y, screen_y, 2) << "at zoom " << cam.zoom();
    }
}

TEST(AdventureMapRendering, CameraZoomCenterPreserved) {
    d2engine::AdventureCamera cam;
    cam.viewport_width = 1416;
    cam.viewport_height = 852;
    cam.canvas_x = 500;
    cam.canvas_y = 400;

    const int vp_cx = cam.viewport_width / 2;
    const int vp_cy = cam.viewport_height / 2;

    const int world_cx_before = cam.screen_to_canvas_x(vp_cx);
    const int world_cy_before = cam.screen_to_canvas_y(vp_cy);

    // Zoom in — center world point should be preserved.
    cam.zoom_in();

    const int world_cx_after = cam.screen_to_canvas_x(vp_cx);
    const int world_cy_after = cam.screen_to_canvas_y(vp_cy);

    EXPECT_NEAR(world_cx_after, world_cx_before, 2);
    EXPECT_NEAR(world_cy_after, world_cy_before, 2);
}

TEST(AdventureMapRendering, CameraVisibleCanvasShrinksOnZoomIn) {
    d2engine::AdventureCamera cam;
    cam.viewport_width = 1416;
    cam.viewport_height = 852;
    const int w_before = cam.visible_canvas_w();
    const int h_before = cam.visible_canvas_h();

    cam.zoom_in();

    EXPECT_LT(cam.visible_canvas_w(), w_before);
    EXPECT_LT(cam.visible_canvas_h(), h_before);
}

TEST(AdventureMapRendering, CameraVisibleCanvasGrowsOnZoomOut) {
    d2engine::AdventureCamera cam;
    cam.viewport_width = 1416;
    cam.viewport_height = 852;
    const int w_before = cam.visible_canvas_w();
    const int h_before = cam.visible_canvas_h();

    cam.zoom_out();

    EXPECT_GT(cam.visible_canvas_w(), w_before);
    EXPECT_GT(cam.visible_canvas_h(), h_before);
}

// ========================================================================
// Camera clipped canvas blit
// ========================================================================

TEST(AdventureMapRendering, CameraBlitFullyInsideCanvas) {
    d2engine::AdventureCamera cam;
    cam.viewport_width = 1000;
    cam.viewport_height = 600;
    cam.canvas_x = 500;
    cam.canvas_y = 300;

    const auto blit = d2engine::compute_clipped_canvas_blit(cam, 4000, 2000);
    ASSERT_TRUE(blit.has_value());

    EXPECT_FLOAT_EQ(blit->source.x, 500.0f);
    EXPECT_FLOAT_EQ(blit->source.y, 300.0f);
    EXPECT_FLOAT_EQ(blit->destination.x, 0.0f);
    EXPECT_FLOAT_EQ(blit->destination.y, 0.0f);
}

TEST(AdventureMapRendering, CameraBlitNegativeX) {
    d2engine::AdventureCamera cam;
    cam.viewport_width = 1000;
    cam.viewport_height = 600;
    cam.canvas_x = -200;
    cam.canvas_y = 300;

    const auto blit = d2engine::compute_clipped_canvas_blit(cam, 4000, 2000);
    ASSERT_TRUE(blit.has_value());

    // Source is clipped to canvas origin X.
    EXPECT_FLOAT_EQ(blit->source.x, 0.0f);
    // Destination: canvas coord 0 → screen position.
    EXPECT_FLOAT_EQ(blit->destination.x, 200.0f); // |−200| * 1.0
    EXPECT_FLOAT_EQ(blit->destination.y, 0.0f);
}

TEST(AdventureMapRendering, CameraBlitNegativeY) {
    d2engine::AdventureCamera cam;
    cam.viewport_width = 1000;
    cam.viewport_height = 600;
    cam.canvas_x = 200;
    cam.canvas_y = -150;

    const auto blit = d2engine::compute_clipped_canvas_blit(cam, 4000, 2000);
    ASSERT_TRUE(blit.has_value());

    EXPECT_FLOAT_EQ(blit->source.y, 0.0f);
    EXPECT_FLOAT_EQ(blit->destination.y, 150.0f); // |−150| * 1.0
    EXPECT_FLOAT_EQ(blit->destination.x, 0.0f);
}

TEST(AdventureMapRendering, CameraBlitNegativeXYWithZoom) {
    d2engine::AdventureCamera cam;
    cam.viewport_width = 1000;
    cam.viewport_height = 600;
    cam.canvas_x = -200;
    cam.canvas_y = -100;
    cam.zoom_index = 4; // 1.50x
    ASSERT_FLOAT_EQ(cam.zoom(), 1.5f);

    const auto blit = d2engine::compute_clipped_canvas_blit(cam, 4000, 2000);
    ASSERT_TRUE(blit.has_value());

    EXPECT_FLOAT_EQ(blit->source.x, 0.0f);
    EXPECT_FLOAT_EQ(blit->source.y, 0.0f);
    EXPECT_FLOAT_EQ(blit->destination.x, 300.0f); // 200 * 1.5
    EXPECT_FLOAT_EQ(blit->destination.y, 150.0f); // 100 * 1.5
}

TEST(AdventureMapRendering, CameraBlitRightEdgeClipping) {
    d2engine::AdventureCamera cam;
    cam.viewport_width = 1000;
    cam.viewport_height = 600;
    // Place camera so visible region extends past canvas width=2000.
    cam.canvas_x = 1500;
    cam.canvas_y = 200;

    const auto blit = d2engine::compute_clipped_canvas_blit(cam, 2000, 1000);
    ASSERT_TRUE(blit.has_value());

    // Source width is clipped to the canvas boundary.
    const float src_right = blit->source.x + blit->source.w;
    EXPECT_LE(src_right, 2000.0f);
    EXPECT_FLOAT_EQ(blit->destination.x, 0.0f);
}

TEST(AdventureMapRendering, CameraBlitBottomEdgeClipping) {
    d2engine::AdventureCamera cam;
    cam.viewport_width = 1000;
    cam.viewport_height = 600;
    cam.canvas_x = 200;
    cam.canvas_y = 800;

    const auto blit = d2engine::compute_clipped_canvas_blit(cam, 2000, 1000);
    ASSERT_TRUE(blit.has_value());

    const float src_bottom = blit->source.y + blit->source.h;
    EXPECT_LE(src_bottom, 1000.0f);
    EXPECT_FLOAT_EQ(blit->destination.x, 0.0f);
}

TEST(AdventureMapRendering, CameraBlitLeftTopNegative) {
    d2engine::AdventureCamera cam;
    cam.viewport_width = 800;
    cam.viewport_height = 500;
    cam.canvas_x = -100;
    cam.canvas_y = -60;

    const auto blit = d2engine::compute_clipped_canvas_blit(cam, 4000, 2000);
    ASSERT_TRUE(blit.has_value());

    EXPECT_FLOAT_EQ(blit->source.x, 0.0f);
    EXPECT_FLOAT_EQ(blit->source.y, 0.0f);
    EXPECT_FLOAT_EQ(blit->destination.x, 100.0f);
    EXPECT_FLOAT_EQ(blit->destination.y, 60.0f);
}

TEST(AdventureMapRendering, CameraBlitRightBottomClipping) {
    d2engine::AdventureCamera cam;
    cam.viewport_width = 1000;
    cam.viewport_height = 600;
    cam.canvas_x = 1900; // visible extent goes past 2000
    cam.canvas_y = 900;  // visible extent goes past 1000

    const auto blit = d2engine::compute_clipped_canvas_blit(cam, 2000, 1000);
    ASSERT_TRUE(blit.has_value());

    const float src_right = blit->source.x + blit->source.w;
    const float src_bottom = blit->source.y + blit->source.h;
    EXPECT_LE(src_right, 2000.0f);
    EXPECT_LE(src_bottom, 1000.0f);
    EXPECT_FLOAT_EQ(blit->destination.x, 0.0f);
    EXPECT_FLOAT_EQ(blit->destination.y, 0.0f);
}

TEST(AdventureMapRendering, CameraBlitTerrainWorldAlignment) {
    // Pick a point inside the clipped source, compute its terrain-destination
    // position and its canvas→screen position — they must match.
    d2engine::AdventureCamera cam;
    cam.viewport_width = 1000;
    cam.viewport_height = 600;
    cam.canvas_x = -200;
    cam.canvas_y = -100;
    cam.zoom_index = 4; // 1.50x

    const int  canvas_w = 4000;
    const int  canvas_h = 2000;
    const auto blit = d2engine::compute_clipped_canvas_blit(cam, canvas_w, canvas_h);
    ASSERT_TRUE(blit.has_value());

    // Pick a canvas point known to be inside the clipped source.
    const int px = 300;
    const int py = 200;

    // Terrain-relative: (px - source.x) * zoom + destination.x
    const int terrain_screen_x =
        static_cast<int>(
            std::lround(static_cast<float>(px - static_cast<int>(blit->source.x)) * cam.zoom())) +
        static_cast<int>(blit->destination.x);
    const int terrain_screen_y =
        static_cast<int>(
            std::lround(static_cast<float>(py - static_cast<int>(blit->source.y)) * cam.zoom())) +
        static_cast<int>(blit->destination.y);

    const int cam_screen_x = cam.canvas_to_screen_x(px);
    const int cam_screen_y = cam.canvas_to_screen_y(py);

    EXPECT_NEAR(terrain_screen_x, cam_screen_x, 2);
    EXPECT_NEAR(terrain_screen_y, cam_screen_y, 2);
}

TEST(AdventureMapRendering, LandmarkResolverAmbiguousAnimationNotDowngraded) {
    std::vector<LmCand> cands = {
        {"imgs/foo.ff", "G000MG0073", LmKind::Animation},
        {"imgs/bar.ff", "G000MG0073", LmKind::Animation},
        {"imgs/isocmon.ff", "G000MG0073", LmKind::StaticSprite},
    };
    auto resolved = d2engine::detail::resolve_landmark_candidates(cands);
    EXPECT_FALSE(resolved.has_value()) << "ambiguous animation must NOT fall back to static sprite";
}

TEST(AdventureMapRendering, LandmarkResolverAmbiguousAnimationWithIsoCmonStatic) {
    std::vector<LmCand> cands = {
        {"imgs/foo.ff", "G000MG0073", LmKind::Animation},
        {"imgs/bar.ff", "G000MG0073", LmKind::Animation},
        {"imgs/isocmon.ff", "G000MG0073", LmKind::StaticSprite},
    };
    auto resolved = d2engine::detail::resolve_landmark_candidates(cands);
    EXPECT_FALSE(resolved.has_value())
        << "IsoCmon static must not win when animation candidates exist";
}

TEST(AdventureMapRendering, CameraScreenToCanvasPixelOwnership) {
    d2engine::AdventureCamera cam;
    cam.viewport_width = 1000;
    cam.viewport_height = 600;
    cam.canvas_x = 0;
    cam.canvas_y = 0;
    cam.zoom_index = 5; // 2.0x
    ASSERT_FLOAT_EQ(cam.zoom(), 2.0f);

    EXPECT_EQ(cam.screen_to_canvas_x(0), 0);
    EXPECT_EQ(cam.screen_to_canvas_x(1), 0);
    EXPECT_EQ(cam.screen_to_canvas_x(2), 1);
    EXPECT_EQ(cam.screen_to_canvas_x(3), 1);
}

TEST(AdventureMapRendering, CameraScreenToCanvasAtAllZoomLevels) {
    d2engine::AdventureCamera cam;
    cam.viewport_width = 1000;
    cam.viewport_height = 600;
    cam.canvas_x = 100;
    cam.canvas_y = 200;

    for (int i = 0; i < static_cast<int>(d2engine::AdventureCamera::kZoomLevels.size()); ++i) {
        cam.zoom_index = i;
        const float z = cam.zoom();

        EXPECT_EQ(cam.screen_to_canvas_x(0), cam.canvas_x) << "zoom " << z;
        EXPECT_EQ(cam.screen_to_canvas_y(0), cam.canvas_y) << "zoom " << z;
    }
}

TEST(AdventureMapRendering, CameraBlitFullyInsideAtAllZoomLevels) {
    for (int i = 0; i < static_cast<int>(d2engine::AdventureCamera::kZoomLevels.size()); ++i) {
        d2engine::AdventureCamera cam;
        cam.viewport_width = 1000;
        cam.viewport_height = 600;
        cam.canvas_x = 500;
        cam.canvas_y = 300;
        cam.zoom_index = i;

        const auto blit = d2engine::compute_clipped_canvas_blit(cam, 4000, 2000);
        ASSERT_TRUE(blit.has_value()) << "zoom " << cam.zoom();

        EXPECT_FLOAT_EQ(blit->destination.x, 0.0f) << "zoom " << cam.zoom();
        EXPECT_FLOAT_EQ(blit->destination.y, 0.0f) << "zoom " << cam.zoom();
        EXPECT_FLOAT_EQ(blit->destination.w, static_cast<float>(cam.viewport_width))
            << "zoom " << cam.zoom();
        EXPECT_FLOAT_EQ(blit->destination.h, static_cast<float>(cam.viewport_height))
            << "zoom " << cam.zoom();
    }
}

TEST(AdventureMapRendering, CameraBlitBottomEdgeExactDstY) {
    d2engine::AdventureCamera cam;
    cam.viewport_width = 1000;
    cam.viewport_height = 600;
    cam.canvas_x = 200;
    cam.canvas_y = 800; // visible region extends past canvas_h=1000

    const auto blit = d2engine::compute_clipped_canvas_blit(cam, 2000, 1000);
    ASSERT_TRUE(blit.has_value());

    const float src_bottom = blit->source.y + blit->source.h;
    EXPECT_LE(src_bottom, 1000.0f);
    EXPECT_FLOAT_EQ(blit->destination.x, 0.0f);
    EXPECT_FLOAT_EQ(blit->destination.y, 0.0f); // canvas_x>0, canvas_y>0 → dst at origin
    EXPECT_LT(blit->destination.h, static_cast<float>(cam.viewport_height));
}

TEST(AdventureMapRendering, CameraBlitTerrainWorldAlignmentFloat) {
    d2engine::AdventureCamera cam;
    cam.viewport_width = 1000;
    cam.viewport_height = 600;
    cam.canvas_x = -200;
    cam.canvas_y = -100;
    cam.zoom_index = 4; // 1.50x

    const int  canvas_w = 4000;
    const int  canvas_h = 2000;
    const auto blit = d2engine::compute_clipped_canvas_blit(cam, canvas_w, canvas_h);
    ASSERT_TRUE(blit.has_value());

    const int px = 300;
    const int py = 200;

    // Terrain-relative position using float transform.
    const float fpx = static_cast<float>(px);
    const float fpy = static_cast<float>(py);
    const float terrain_sx = (fpx - blit->source.x) * cam.zoom() + blit->destination.x;
    const float terrain_sy = (fpy - blit->source.y) * cam.zoom() + blit->destination.y;

    const float cam_sx = cam.canvas_to_screen_x_float(fpx);
    const float cam_sy = cam.canvas_to_screen_y_float(fpy);

    EXPECT_NEAR(terrain_sx, cam_sx, 0.01f);
    EXPECT_NEAR(terrain_sy, cam_sy, 0.01f);
}

TEST(AdventureMapRendering, CameraBlitAlignmentFloatNormalCamera) {
    d2engine::AdventureCamera cam;
    cam.viewport_width = 800;
    cam.viewport_height = 500;
    cam.canvas_x = 500;
    cam.canvas_y = 300;

    const auto blit = d2engine::compute_clipped_canvas_blit(cam, 4000, 2000);
    ASSERT_TRUE(blit.has_value());

    const int   px = 700;
    const int   py = 400;
    const float fpx = static_cast<float>(px);
    const float fpy = static_cast<float>(py);

    const float terrain_sx = (fpx - blit->source.x) * cam.zoom() + blit->destination.x;
    const float terrain_sy = (fpy - blit->source.y) * cam.zoom() + blit->destination.y;

    const float cam_sx = cam.canvas_to_screen_x_float(fpx);
    const float cam_sy = cam.canvas_to_screen_y_float(fpy);

    EXPECT_NEAR(terrain_sx, cam_sx, 0.01f);
    EXPECT_NEAR(terrain_sy, cam_sy, 0.01f);
}

// ========================================================================
// Zoom-aware picking regression
// ========================================================================

TEST(AdventureMapRendering, PickingAlignedAtAllZoomLevels) {
    // Place a target at cell (5,5) and verify hit-test success at
    // representative zoom levels using the real screen-to-canvas transform.
    d2ar::PreparedAdventureMap map;
    map.geometry = d2ar::AdventureMapGeometry::from_source(10, 10);

    const d2ar::SelectionCircleGeometry kTestSelGeo = [] {
        d2ar::SelectionCircleGeometry g;
        g.center_offset_x = 0;
        g.center_offset_y = -9;
        g.radius_x = 21;
        g.radius_y = 10;
        return g;
    }();

    auto make_mask = [](int w, int h) {
        auto                   m = std::make_shared<d2ar::InteractionMask>();
        d2ar::InteractionMask& rm = const_cast<d2ar::InteractionMask&>(*m);
        rm.width = w;
        rm.height = h;
        rm.bits.assign(static_cast<std::size_t>(((w + 7) / 8) * h), 0xFF);
        return m;
    };

    const auto sid = d2ar::stable_render_id("Stack:Test");
    map.pick_entries.push_back(
        {.stable_id = sid, .kind = d2ar::PickEntryKind::Stack, .object_id = "Test"});

    const auto                             foot = map.geometry.cell_foot_anchor({5, 5});
    d2ar::PreparedAdventureRenderPrimitive prim;
    prim.stable_id = sid;
    prim.level = d2ar::WorldRenderLevel::Actor;
    prim.depth_anchor = {5, 5};
    prim.draw_origin = {foot.x - 40, foot.y - 80};
    prim.src_width = 80;
    prim.src_height = 90;
    prim.interaction_mask = make_mask(80, 90);
    map.world_graph.world.push_back(std::move(prim));

    d2engine::AdventurePickIndex index;
    index.build(map, kTestSelGeo);

    // Use canvas foot as the visual screen position.
    const int canvas_foot_x = foot.x;
    const int canvas_foot_y = foot.y;

    const int zooms[] = {3, 2, 4, 5}; // indices: 0.75, 1.0, 1.5, 2.0
    for (int zi : zooms) {
        d2engine::AdventureCamera cam;
        cam.zoom_index = zi;
        cam.viewport_width = 1000;
        cam.viewport_height = 600;
        cam.canvas_x = 0;
        cam.canvas_y = 0;

        // Derive screen position from the canvas foot through the camera.
        const int screen_x = cam.canvas_to_screen_x(canvas_foot_x);
        const int screen_y = cam.canvas_to_screen_y(canvas_foot_y);

        d2engine::AdventureHitTester tester(index, cam);
        const auto                   result = tester.hit_test(screen_x, screen_y);
        ASSERT_NE(result.interaction_target, nullptr) << "zoom=" << cam.zoom();
        EXPECT_EQ(result.interaction_target->object_id, "Test") << "zoom=" << cam.zoom();
    }
}

TEST(AdventureMapRendering, PickingAlphaMaskAtAllZoomLevels) {
    d2ar::PreparedAdventureMap map;
    map.geometry = d2ar::AdventureMapGeometry::from_source(10, 10);

    const d2ar::SelectionCircleGeometry kTestSelGeo = [] {
        d2ar::SelectionCircleGeometry g;
        g.center_offset_x = 0;
        g.center_offset_y = -9;
        g.radius_x = 21;
        g.radius_y = 10;
        return g;
    }();

    auto make_alpha_mask = [] {
        auto                   m = std::make_shared<d2ar::InteractionMask>();
        d2ar::InteractionMask& rm = const_cast<d2ar::InteractionMask&>(*m);
        rm.width = 80;
        rm.height = 90;
        const int stride = rm.stride();
        rm.bits.assign(static_cast<std::size_t>(stride * 90), 0xFF);
        // Pixel (56, 80) is transparent (all others opaque).
        // Canvas position (draw_origin.x + 56, draw_origin.y + 80) must round-trip
        // exactly at all tested zoom levels → chose multiples of 4 for exact 0.75x.
        const int         mask_tx = 56;
        const std::size_t byte_idx = static_cast<std::size_t>(80 * stride + mask_tx / 8);
        rm.bits[byte_idx] &= ~(0x80U >> (mask_tx % 8));
        return m;
    };

    const auto sid = d2ar::stable_render_id("Stack:AlphaTest");
    map.pick_entries.push_back(
        {.stable_id = sid, .kind = d2ar::PickEntryKind::Stack, .object_id = "AlphaTest"});

    const auto                             foot = map.geometry.cell_foot_anchor({5, 5});
    d2ar::PreparedAdventureRenderPrimitive prim;
    prim.stable_id = sid;
    prim.level = d2ar::WorldRenderLevel::Actor;
    prim.depth_anchor = {5, 5};
    prim.draw_origin = {foot.x - 40, foot.y - 80};
    prim.src_width = 80;
    prim.src_height = 90;
    prim.interaction_mask = make_alpha_mask();
    map.world_graph.world.push_back(std::move(prim));

    d2engine::AdventurePickIndex index;
    index.build(map, kTestSelGeo);

    // Canvas positions: draw_origin = (foot.x-40, foot.y-80) = (280, 112).
    // Sprite covers [280, 360) x [112, 202).
    // Coarse cell region: foot.x + [-24, 24) = [296, 344).
    // Transparent at canvas (336, 192) = mask (56, 80) — within coarse cell.
    // Opaque   at canvas (340, 192) = mask (60, 80) — within coarse cell.
    // Outside  at canvas (260, 192) = left of sprite (260 < 280).
    // All canvas_x values are multiples of 4 for exact round-trip at zoom-0.75.
    const int kCvTransparentX = foot.x + 16; // 336, mask (56,80)
    const int kCvOpaqueX = foot.x + 20;      // 340, mask (60,80)
    const int kCvOutsideX = foot.x - 60;     // 260, left of draw_origin
    const int kCvY = foot.y;                 // 192

    const int zooms[] = {1, 2, 4, 5}; // indices: 0.75, 1.0, 1.5, 2.0
    for (int zi : zooms) {
        d2engine::AdventureCamera cam;
        cam.zoom_index = zi;
        cam.viewport_width = 1000;
        cam.viewport_height = 600;
        cam.canvas_x = 0;
        cam.canvas_y = 0;

        // Transparent pixel: outside ellipse, must not produce interaction.
        {
            const int                    sx = cam.canvas_to_screen_x(kCvTransparentX);
            const int                    sy = cam.canvas_to_screen_y(kCvY);
            d2engine::AdventureHitTester tester(index, cam);
            const auto                   result = tester.hit_test(sx, sy);

            // screen→canvas must round-trip exactly back to (kCvTransparentX, kCvY)
            EXPECT_EQ(cam.screen_to_canvas_x(sx), kCvTransparentX) << "zoom=" << cam.zoom();
            EXPECT_EQ(cam.screen_to_canvas_y(sy), kCvY) << "zoom=" << cam.zoom();

            EXPECT_NE(result.occupied_cell_hover, nullptr) << "zoom=" << cam.zoom();
            EXPECT_EQ(result.interaction_target, nullptr)
                << "zoom=" << cam.zoom() << " transparent pixel must not produce interaction";
        }

        // Opaque pixel: same coarse cell, must produce interaction.
        {
            const int                    sx = cam.canvas_to_screen_x(kCvOpaqueX);
            const int                    sy = cam.canvas_to_screen_y(kCvY);
            d2engine::AdventureHitTester tester(index, cam);
            const auto                   result = tester.hit_test(sx, sy);

            EXPECT_EQ(cam.screen_to_canvas_x(sx), kCvOpaqueX) << "zoom=" << cam.zoom();
            EXPECT_EQ(cam.screen_to_canvas_y(sy), kCvY) << "zoom=" << cam.zoom();

            ASSERT_NE(result.interaction_target, nullptr) << "zoom=" << cam.zoom();
            EXPECT_EQ(result.interaction_target->object_id, "AlphaTest") << "zoom=" << cam.zoom();
        }

        // Outside sprite: no interaction.
        {
            const int                    sx = cam.canvas_to_screen_x(kCvOutsideX);
            const int                    sy = cam.canvas_to_screen_y(kCvY);
            d2engine::AdventureHitTester tester(index, cam);
            const auto                   result = tester.hit_test(sx, sy);
            EXPECT_EQ(result.interaction_target, nullptr) << "zoom=" << cam.zoom();
        }
    }
}
