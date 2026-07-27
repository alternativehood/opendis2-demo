#include <gtest/gtest.h>

#include "d2engine/app/adventure_pick_index.hpp"

#include <d2adventure_render/adventure_render_types.hpp>
#include <d2adventure_render/map_geometry.hpp>
#include <d2adventure_render/prepared_adventure_map.hpp>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace d2engine {
namespace ar = adventure_render;

static const ar::SelectionCircleGeometry kTestSelGeo = [] {
    ar::SelectionCircleGeometry g;
    g.center_offset_x = 0;
    g.center_offset_y = -9;
    g.radius_x = 21;
    g.radius_y = 10;
    return g;
}();

static auto make_mask(int w, int h, bool all_opaque = true) {
    auto                 mask = std::make_shared<ar::InteractionMask>();
    ar::InteractionMask& m = const_cast<ar::InteractionMask&>(*mask);
    m.width = w;
    m.height = h;
    const int stride = (w + 7) / 8;
    m.bits.resize(static_cast<std::size_t>(stride * h));
    if (all_opaque) {
        for (auto& b : m.bits)
            b = 0xFF;
    }
    return mask;
}

struct TargetSetup {
    std::string stack_id;
    int         gx;
    int         gy;
    int         width;
    int         height;
    int         canvas_foot_x;
    int         canvas_foot_y;
    bool        all_opaque = true;
};

static void add_target(ar::PreparedAdventureMap& map, const TargetSetup& ts) {
    const auto sid = ar::stable_render_id("Stack:" + ts.stack_id);
    map.pick_entries.push_back(
        {.stable_id = sid, .kind = ar::PickEntryKind::Stack, .object_id = ts.stack_id});

    const auto foot = map.geometry.cell_foot_anchor({ts.gx, ts.gy});

    ar::PreparedAdventureRenderPrimitive prim;
    prim.stable_id = sid;
    prim.level = ar::WorldRenderLevel::Actor;
    prim.depth_anchor = {ts.gx, ts.gy};
    prim.draw_origin = {foot.x - ts.canvas_foot_x, foot.y - ts.canvas_foot_y};
    prim.src_width = ts.width;
    prim.src_height = ts.height;
    prim.interaction_mask = make_mask(ts.width, ts.height, ts.all_opaque);
    map.world_graph.world.push_back(std::move(prim));
}

// ── AdventurePickIndex / AdventureHitTester tests ──────────────────────

TEST(AdventurePickIndex, ExplicitPickMetadataReturnsCorrectTarget) {
    ar::PreparedAdventureMap map;
    map.geometry = ar::AdventureMapGeometry::from_source(10, 10);
    add_target(map, {"S143ST0001", 5, 5, 80, 90, 40, 80});

    AdventurePickIndex index;
    index.build(map, kTestSelGeo);

    const auto foot = map.geometry.cell_foot_anchor({5, 5});
    auto       result = index.hit_test(foot.x, foot.y);
    ASSERT_NE(result.occupied_cell_hover, nullptr);
    ASSERT_NE(result.interaction_target, nullptr);
    EXPECT_EQ(result.interaction_target->object_id, "S143ST0001");
}

TEST(AdventurePickIndex, HitResultDoesNotDependOnDebugLabel) {
    ar::PreparedAdventureMap map;
    map.geometry = ar::AdventureMapGeometry::from_source(10, 10);
    const auto sid = ar::stable_render_id("Stack:S143ST0002");
    map.pick_entries.push_back(
        {.stable_id = sid, .kind = ar::PickEntryKind::Stack, .object_id = "S143ST0002"});

    const auto                           foot = map.geometry.cell_foot_anchor({3, 3});
    ar::PreparedAdventureRenderPrimitive prim;
    prim.stable_id = sid;
    prim.level = ar::WorldRenderLevel::Actor;
    prim.debug_label = "arbitrary_non_machine_label";
    prim.depth_anchor = {3, 3};
    prim.draw_origin = {foot.x - 40, foot.y - 80};
    prim.src_width = 80;
    prim.src_height = 90;
    prim.interaction_mask = make_mask(80, 90);
    map.world_graph.world.push_back(prim);

    AdventurePickIndex index;
    index.build(map, kTestSelGeo);
    auto result = index.hit_test(foot.x, foot.y);
    ASSERT_NE(result.interaction_target, nullptr);
    EXPECT_EQ(result.interaction_target->object_id, "S143ST0002");
}

TEST(AdventurePickIndex, NoMatchingPickEntryReturnsNoHit) {
    ar::PreparedAdventureMap map;
    map.geometry = ar::AdventureMapGeometry::from_source(10, 10);
    const auto                           foot = map.geometry.cell_foot_anchor({3, 3});
    ar::PreparedAdventureRenderPrimitive prim;
    prim.stable_id = ar::stable_render_id("Stack:S143ST0003");
    prim.level = ar::WorldRenderLevel::Actor;
    prim.depth_anchor = {3, 3};
    prim.draw_origin = {foot.x - 40, foot.y - 80};
    prim.src_width = 80;
    prim.src_height = 90;
    prim.interaction_mask = make_mask(80, 90);
    map.world_graph.world.push_back(prim);

    AdventurePickIndex index;
    index.build(map, kTestSelGeo);
    auto result = index.hit_test(foot.x, foot.y);
    EXPECT_EQ(result.occupied_cell_hover, nullptr);
}

TEST(AdventurePickIndex, CoarseCellHoverOutsideRegionReturnsNoHit) {
    ar::PreparedAdventureMap map;
    map.geometry = ar::AdventureMapGeometry::from_source(10, 10);
    add_target(map, {"S143ST0004", 5, 5, 80, 90, 40, 80});

    AdventurePickIndex index;
    index.build(map, kTestSelGeo);
    const auto foot = map.geometry.cell_foot_anchor({5, 5});
    auto       result = index.hit_test(foot.x + 100, foot.y + 100);
    EXPECT_EQ(result.occupied_cell_hover, nullptr);
}

TEST(AdventurePickIndex, OverlappingPrimitivesLastWins) {
    ar::PreparedAdventureMap map;
    map.geometry = ar::AdventureMapGeometry::from_source(10, 10);
    add_target(map, {"S143ST0006", 5, 5, 80, 90, 40, 80});
    add_target(map, {"S143ST0005", 5, 5, 80, 90, 40, 80});

    AdventurePickIndex index;
    index.build(map, kTestSelGeo);
    const auto foot = map.geometry.cell_foot_anchor({5, 5});
    auto       result = index.hit_test(foot.x, foot.y);
    ASSERT_NE(result.interaction_target, nullptr);
    EXPECT_EQ(result.interaction_target->object_id, "S143ST0005");
}

TEST(AdventurePickIndex, CameraOffsetAffectsHitTest) {
    ar::PreparedAdventureMap map;
    map.geometry = ar::AdventureMapGeometry::from_source(10, 10);
    add_target(map, {"S143ST0007", 5, 5, 80, 90, 40, 80});

    AdventurePickIndex index;
    index.build(map, kTestSelGeo);
    const auto foot = map.geometry.cell_foot_anchor({5, 5});

    AdventureCamera camera_zero;
    camera_zero.canvas_x = 0;
    camera_zero.canvas_y = 0;
    camera_zero.viewport_width = 800;
    camera_zero.viewport_height = 600;
    AdventureHitTester tester_zero(index, camera_zero);
    auto               result_zero = tester_zero.hit_test(foot.x, foot.y);
    ASSERT_NE(result_zero.interaction_target, nullptr);

    AdventureCamera camera_offset;
    camera_offset.canvas_x = 50;
    camera_offset.canvas_y = 30;
    camera_offset.viewport_width = 800;
    camera_offset.viewport_height = 600;
    AdventureHitTester tester_offset(index, camera_offset);
    auto               result_offset = tester_offset.hit_test(foot.x - 50, foot.y - 30);
    ASSERT_NE(result_offset.interaction_target, nullptr);
    auto result_miss = tester_offset.hit_test(foot.x, foot.y);
    EXPECT_EQ(result_miss.occupied_cell_hover, nullptr);
}

TEST(AdventurePickIndex, SizeReturnsCorrectCount) {
    ar::PreparedAdventureMap map;
    map.geometry = ar::AdventureMapGeometry::from_source(10, 10);
    for (int i = 0; i < 3; ++i)
        add_target(map, {"S" + std::to_string(i + 10), i, i, 80, 90, 40, 80});

    AdventurePickIndex index;
    index.build(map, kTestSelGeo);
    EXPECT_EQ(index.size(), 3);
    EXPECT_FALSE(index.empty());
}

TEST(AdventurePickIndex, NonActorPrimitivesAreSkipped) {
    ar::PreparedAdventureMap map;
    map.geometry = ar::AdventureMapGeometry::from_source(10, 10);
    const auto sid = ar::stable_render_id("Stack:S143ST0013");
    map.pick_entries.push_back(
        {.stable_id = sid, .kind = ar::PickEntryKind::Stack, .object_id = "S143ST0013"});
    ar::PreparedAdventureRenderPrimitive prim;
    prim.stable_id = sid;
    prim.level = ar::WorldRenderLevel::GroundObject;
    prim.depth_anchor = {3, 3};
    map.world_graph.world.push_back(prim);

    AdventurePickIndex index;
    index.build(map, kTestSelGeo);
    EXPECT_TRUE(index.empty());
}

TEST(AdventurePickIndex, TransparentPixelFineMiss) {
    ar::PreparedAdventureMap map;
    map.geometry = ar::AdventureMapGeometry::from_source(10, 10);
    const auto foot = map.geometry.cell_foot_anchor({5, 5});
    const auto sid = ar::stable_render_id("Stack:S143ST0014");
    map.pick_entries.push_back(
        {.stable_id = sid, .kind = ar::PickEntryKind::Stack, .object_id = "S143ST0014"});

    auto      mask = make_mask(80, 90, true);
    const int stride = mask->stride();
    const int byte_idx = 10 * stride + 10 / 8;
    const int bit_off = 10 % 8;
    auto&     bits = const_cast<ar::InteractionMask&>(*mask).bits;
    bits[static_cast<std::size_t>(byte_idx)] &= ~(0x80U >> bit_off);

    const int                            draw_x = foot.x + 12;
    const int                            draw_y = foot.y - 10;
    ar::PreparedAdventureRenderPrimitive prim;
    prim.stable_id = sid;
    prim.level = ar::WorldRenderLevel::Actor;
    prim.depth_anchor = {5, 5};
    prim.draw_origin = {draw_x, draw_y};
    prim.src_width = 80;
    prim.src_height = 90;
    prim.interaction_mask = mask;
    map.world_graph.world.push_back(prim);

    AdventurePickIndex index;
    index.build(map, kTestSelGeo);

    auto result = index.hit_test(foot.x + 22, foot.y);
    ASSERT_NE(result.occupied_cell_hover, nullptr);
    EXPECT_EQ(result.interaction_target, nullptr);

    auto result2 = index.hit_test(foot.x + 23, foot.y);
    ASSERT_NE(result2.occupied_cell_hover, nullptr);
    ASSERT_NE(result2.interaction_target, nullptr);
}

TEST(AdventurePickIndex, CoarseHoverIndependentFromSpriteSize) {
    ar::PreparedAdventureMap map;
    map.geometry = ar::AdventureMapGeometry::from_source(10, 10);
    {
        const auto                           foot = map.geometry.cell_foot_anchor({3, 3});
        ar::PreparedAdventureRenderPrimitive prim;
        prim.stable_id = ar::stable_render_id("Stack:small");
        prim.level = ar::WorldRenderLevel::Actor;
        prim.depth_anchor = {3, 3};
        prim.draw_origin = {foot.x - 15, foot.y - 25};
        prim.src_width = 30;
        prim.src_height = 30;
        prim.interaction_mask = make_mask(30, 30);
        map.world_graph.world.push_back(prim);
        map.pick_entries.push_back({.stable_id = ar::stable_render_id("Stack:small"),
                                    .kind = ar::PickEntryKind::Stack,
                                    .object_id = "small"});
    }
    {
        const auto                           foot = map.geometry.cell_foot_anchor({5, 5});
        ar::PreparedAdventureRenderPrimitive prim;
        prim.stable_id = ar::stable_render_id("Stack:large");
        prim.level = ar::WorldRenderLevel::Actor;
        prim.depth_anchor = {5, 5};
        prim.draw_origin = {foot.x - 100, foot.y - 180};
        prim.src_width = 200;
        prim.src_height = 200;
        prim.interaction_mask = make_mask(200, 200);
        map.world_graph.world.push_back(prim);
        map.pick_entries.push_back({.stable_id = ar::stable_render_id("Stack:large"),
                                    .kind = ar::PickEntryKind::Stack,
                                    .object_id = "large"});
    }
    AdventurePickIndex index;
    index.build(map, kTestSelGeo);
    const auto foot_small = map.geometry.cell_foot_anchor({3, 3});
    const auto foot_large = map.geometry.cell_foot_anchor({5, 5});
    auto       result_small = index.hit_test(foot_small.x - 20, foot_small.y - 10);
    auto       result_large = index.hit_test(foot_large.x - 20, foot_large.y - 10);
    ASSERT_NE(result_small.occupied_cell_hover, nullptr);
    ASSERT_NE(result_large.occupied_cell_hover, nullptr);
}

TEST(AdventurePickIndex, PressUsesFreshHitTest) {
    ar::PreparedAdventureMap map;
    map.geometry = ar::AdventureMapGeometry::from_source(10, 10);
    add_target(map, {"stack_a", 3, 3, 80, 90, 40, 80});
    add_target(map, {"stack_b", 7, 7, 80, 90, 40, 80});

    AdventurePickIndex index;
    index.build(map, kTestSelGeo);
    const auto foot_a = map.geometry.cell_foot_anchor({3, 3});
    const auto foot_b = map.geometry.cell_foot_anchor({7, 7});
    auto       result = index.hit_test(foot_a.x, foot_a.y);
    ASSERT_NE(result.interaction_target, nullptr);
    EXPECT_EQ(result.interaction_target->object_id, "stack_a");
    auto result_b = index.hit_test(foot_b.x, foot_b.y);
    ASSERT_NE(result_b.interaction_target, nullptr);
    EXPECT_EQ(result_b.interaction_target->object_id, "stack_b");
}

// ── Regression: ellipse-based interaction ─────────────────────────────

TEST(AdventurePickIndex, CoarseOuterRegionWithoutInteraction) {
    ar::PreparedAdventureMap map;
    map.geometry = ar::AdventureMapGeometry::from_source(10, 10);
    const auto                           foot = map.geometry.cell_foot_anchor({5, 5});
    ar::PreparedAdventureRenderPrimitive prim;
    prim.stable_id = ar::stable_render_id("Stack:coarse_outer");
    prim.level = ar::WorldRenderLevel::Actor;
    prim.depth_anchor = {5, 5};
    prim.draw_origin = {foot.x - 15, foot.y - 25};
    prim.src_width = 30;
    prim.src_height = 30;
    prim.interaction_mask = make_mask(30, 30);
    map.world_graph.world.push_back(prim);
    map.pick_entries.push_back({.stable_id = ar::stable_render_id("Stack:coarse_outer"),
                                .kind = ar::PickEntryKind::Stack,
                                .object_id = "coarse_outer"});

    AdventurePickIndex index;
    index.build(map, kTestSelGeo);
    auto result = index.hit_test(foot.x + 23, foot.y);
    ASSERT_NE(result.occupied_cell_hover, nullptr);
    EXPECT_EQ(result.interaction_target, nullptr);
}

TEST(AdventurePickIndex, InsideEllipseCenterAlwaysInteraction) {
    ar::PreparedAdventureMap map;
    map.geometry = ar::AdventureMapGeometry::from_source(10, 10);
    const auto foot = map.geometry.cell_foot_anchor({5, 5});
    auto       mask = make_mask(80, 90, true);
    const_cast<ar::InteractionMask&>(*mask).bits[0] &= ~0x80U;

    map.pick_entries.push_back({.stable_id = ar::stable_render_id("Stack:ellipse_center"),
                                .kind = ar::PickEntryKind::Stack,
                                .object_id = "ellipse_center"});
    ar::PreparedAdventureRenderPrimitive prim;
    prim.stable_id = ar::stable_render_id("Stack:ellipse_center");
    prim.level = ar::WorldRenderLevel::Actor;
    prim.depth_anchor = {5, 5};
    prim.draw_origin = {foot.x, foot.y};
    prim.src_width = 80;
    prim.src_height = 90;
    prim.interaction_mask = mask;
    map.world_graph.world.push_back(prim);

    AdventurePickIndex index;
    index.build(map, kTestSelGeo);

    auto result = index.hit_test(foot.x, foot.y - 9);
    ASSERT_NE(result.occupied_cell_hover, nullptr);
    ASSERT_NE(result.interaction_target, nullptr);
}

TEST(AdventurePickIndex, EllipseEdgeBoundary) {
    ar::PreparedAdventureMap map;
    map.geometry = ar::AdventureMapGeometry::from_source(10, 10);
    const auto                           foot = map.geometry.cell_foot_anchor({5, 5});
    ar::PreparedAdventureRenderPrimitive prim;
    prim.stable_id = ar::stable_render_id("Stack:ellipse_edge");
    prim.level = ar::WorldRenderLevel::Actor;
    prim.depth_anchor = {5, 5};
    prim.draw_origin = {foot.x - 15, foot.y - 25};
    prim.src_width = 30;
    prim.src_height = 30;
    prim.interaction_mask = make_mask(30, 30);
    map.world_graph.world.push_back(prim);
    map.pick_entries.push_back({.stable_id = ar::stable_render_id("Stack:ellipse_edge"),
                                .kind = ar::PickEntryKind::Stack,
                                .object_id = "ellipse_edge"});

    AdventurePickIndex index;
    index.build(map, kTestSelGeo);

    auto inside = index.hit_test(foot.x + 20, foot.y - 9);
    ASSERT_NE(inside.occupied_cell_hover, nullptr);
    ASSERT_NE(inside.interaction_target, nullptr);
    auto outside = index.hit_test(foot.x + 22, foot.y);
    ASSERT_NE(outside.occupied_cell_hover, nullptr);
    EXPECT_EQ(outside.interaction_target, nullptr);
}

TEST(AdventurePickIndex, SpriteAlphaOutsideEllipseStillHits) {
    ar::PreparedAdventureMap map;
    map.geometry = ar::AdventureMapGeometry::from_source(10, 10);
    const auto                           foot = map.geometry.cell_foot_anchor({5, 5});
    ar::PreparedAdventureRenderPrimitive prim;
    prim.stable_id = ar::stable_render_id("Stack:alpha_outside");
    prim.level = ar::WorldRenderLevel::Actor;
    prim.depth_anchor = {5, 5};
    prim.draw_origin = {foot.x + 22, foot.y};
    prim.src_width = 80;
    prim.src_height = 90;
    prim.interaction_mask = make_mask(80, 90);
    map.world_graph.world.push_back(prim);
    map.pick_entries.push_back({.stable_id = ar::stable_render_id("Stack:alpha_outside"),
                                .kind = ar::PickEntryKind::Stack,
                                .object_id = "alpha_outside"});

    AdventurePickIndex index;
    index.build(map, kTestSelGeo);
    auto result = index.hit_test(foot.x + 22, foot.y);
    ASSERT_NE(result.occupied_cell_hover, nullptr);
    ASSERT_NE(result.interaction_target, nullptr);
}

TEST(AdventurePickIndex, InsideEllipseInvariant) {
    ar::PreparedAdventureMap map;
    map.geometry = ar::AdventureMapGeometry::from_source(10, 10);
    const auto                           foot = map.geometry.cell_foot_anchor({5, 5});
    ar::PreparedAdventureRenderPrimitive prim;
    prim.stable_id = ar::stable_render_id("Stack:ellipse_only");
    prim.level = ar::WorldRenderLevel::Actor;
    prim.depth_anchor = {5, 5};
    prim.draw_origin = {foot.x, foot.y};
    prim.src_width = 80;
    prim.src_height = 90;
    map.world_graph.world.push_back(prim);
    map.pick_entries.push_back({.stable_id = ar::stable_render_id("Stack:ellipse_only"),
                                .kind = ar::PickEntryKind::Stack,
                                .object_id = "ellipse_only"});

    AdventurePickIndex index;
    index.build(map, kTestSelGeo);
    struct {
        int dx;
        int dy;
    } points[] = {{0, 0}, {10, 0}, {-10, 0}, {0, -5}, {20, 0}, {-20, 0}};
    for (const auto [px, py] : points) {
        auto result = index.hit_test(foot.x + px, foot.y - 9 + py);
        ASSERT_NE(result.occupied_cell_hover, nullptr);
        ASSERT_NE(result.interaction_target, nullptr);
    }
}

TEST(AdventurePickIndex, EllipsePriorityOverNeighbourAlpha) {
    ar::PreparedAdventureMap map;
    map.geometry = ar::AdventureMapGeometry::from_source(10, 10);
    const auto foot_a = map.geometry.cell_foot_anchor({5, 5});
    const auto foot_b = map.geometry.cell_foot_anchor({6, 5});

    {
        ar::PreparedAdventureRenderPrimitive prim;
        prim.stable_id = ar::stable_render_id("Stack:A");
        prim.level = ar::WorldRenderLevel::Actor;
        prim.depth_anchor = {5, 5};
        prim.draw_origin = {foot_a.x - 40, foot_a.y - 80};
        prim.src_width = 80;
        prim.src_height = 90;
        map.world_graph.world.push_back(prim);
        map.pick_entries.push_back({.stable_id = ar::stable_render_id("Stack:A"),
                                    .kind = ar::PickEntryKind::Stack,
                                    .object_id = "stack_a"});
    }
    {
        ar::PreparedAdventureRenderPrimitive prim;
        prim.stable_id = ar::stable_render_id("Stack:B");
        prim.level = ar::WorldRenderLevel::Actor;
        prim.depth_anchor = {6, 5};
        prim.draw_origin = {foot_b.x - 40, foot_b.y - 80};
        prim.src_width = 80;
        prim.src_height = 90;
        prim.interaction_mask = make_mask(80, 90);
        map.world_graph.world.push_back(prim);
        map.pick_entries.push_back({.stable_id = ar::stable_render_id("Stack:B"),
                                    .kind = ar::PickEntryKind::Stack,
                                    .object_id = "stack_b"});
    }

    AdventurePickIndex index;
    index.build(map, kTestSelGeo);
    auto result = index.hit_test(foot_a.x, foot_a.y - 9);
    ASSERT_NE(result.occupied_cell_hover, nullptr);
    ASSERT_NE(result.interaction_target, nullptr);
    EXPECT_EQ(result.interaction_target->object_id, "stack_a");
}

TEST(AdventurePickIndex, SingleStackInsideCircleAlwaysInteracts) {
    ar::PreparedAdventureMap map;
    map.geometry = ar::AdventureMapGeometry::from_source(10, 10);
    const auto                           foot = map.geometry.cell_foot_anchor({5, 5});
    ar::PreparedAdventureRenderPrimitive prim;
    prim.stable_id = ar::stable_render_id("Stack:single");
    prim.level = ar::WorldRenderLevel::Actor;
    prim.depth_anchor = {5, 5};
    prim.draw_origin = {foot.x - 40, foot.y - 80};
    prim.src_width = 80;
    prim.src_height = 90;
    map.world_graph.world.push_back(prim);
    map.pick_entries.push_back({.stable_id = ar::stable_render_id("Stack:single"),
                                .kind = ar::PickEntryKind::Stack,
                                .object_id = "single"});

    AdventurePickIndex index;
    index.build(map, kTestSelGeo);
    auto result = index.hit_test(foot.x, foot.y - 9);
    ASSERT_NE(result.occupied_cell_hover, nullptr);
    ASSERT_NE(result.interaction_target, nullptr);
    EXPECT_EQ(result.interaction_target->object_id, "single");
}

// ── Explicit render rank tests ─────────────────────────────────────────

TEST(AdventurePickIndex, TopmostCoarseCandidateWins) {
    ar::PreparedAdventureMap map;
    map.geometry = ar::AdventureMapGeometry::from_source(10, 10);
    add_target(map, {"back", 5, 5, 80, 90, 40, 80});
    add_target(map, {"front", 5, 5, 80, 90, 40, 80});

    AdventurePickIndex index;
    index.build(map, kTestSelGeo);
    const auto foot = map.geometry.cell_foot_anchor({5, 5});
    auto       result = index.hit_test(foot.x, foot.y);
    ASSERT_NE(result.occupied_cell_hover, nullptr);
    EXPECT_EQ(result.occupied_cell_hover->object_id, "front");
}

TEST(AdventurePickIndex, TopmostEllipseCandidateWins) {
    ar::PreparedAdventureMap map;
    map.geometry = ar::AdventureMapGeometry::from_source(10, 10);
    add_target(map, {"lower_ellipse", 5, 5, 80, 90, 40, 80});
    add_target(map, {"upper_ellipse", 5, 5, 80, 90, 40, 80});

    AdventurePickIndex index;
    index.build(map, kTestSelGeo);
    const auto foot = map.geometry.cell_foot_anchor({5, 5});
    auto       result = index.hit_test(foot.x, foot.y - 9);
    ASSERT_NE(result.interaction_target, nullptr);
    EXPECT_EQ(result.interaction_target->object_id, "upper_ellipse");
}

TEST(AdventurePickIndex, NoEllipseTopmostAlphaWins) {
    ar::PreparedAdventureMap map;
    map.geometry = ar::AdventureMapGeometry::from_source(10, 10);
    // Two targets with alpha, outside ellipse, same cell region
    const auto foot = map.geometry.cell_foot_anchor({5, 5});
    {
        ar::PreparedAdventureRenderPrimitive prim;
        prim.stable_id = ar::stable_render_id("Stack:alpha_lower");
        prim.level = ar::WorldRenderLevel::Actor;
        prim.depth_anchor = {5, 5};
        prim.draw_origin = {foot.x + 22, foot.y};
        prim.src_width = 80;
        prim.src_height = 90;
        prim.interaction_mask = make_mask(80, 90);
        map.world_graph.world.push_back(prim);
        map.pick_entries.push_back({.stable_id = ar::stable_render_id("Stack:alpha_lower"),
                                    .kind = ar::PickEntryKind::Stack,
                                    .object_id = "alpha_lower"});
    }
    {
        ar::PreparedAdventureRenderPrimitive prim;
        prim.stable_id = ar::stable_render_id("Stack:alpha_upper");
        prim.level = ar::WorldRenderLevel::Actor;
        prim.depth_anchor = {5, 5};
        prim.draw_origin = {foot.x + 22, foot.y};
        prim.src_width = 80;
        prim.src_height = 90;
        prim.interaction_mask = make_mask(80, 90);
        map.world_graph.world.push_back(prim);
        map.pick_entries.push_back({.stable_id = ar::stable_render_id("Stack:alpha_upper"),
                                    .kind = ar::PickEntryKind::Stack,
                                    .object_id = "alpha_upper"});
    }

    AdventurePickIndex index;
    index.build(map, kTestSelGeo);
    auto result = index.hit_test(foot.x + 22, foot.y);
    ASSERT_NE(result.interaction_target, nullptr);
    EXPECT_EQ(result.interaction_target->object_id, "alpha_upper");
}

// ── Stationary pointer zoom regression ───────────────────────────────
//
// Verifies that zoom (camera change) triggers a fresh hit test at the
// stored pointer position, causing interaction to be acquired or released.
// Exercises the exact code path: zoom → camera.recenter → hit_test(same_screen).

TEST(AdventurePickIndex, StationaryPointerZoomCaseA) {
    // Case A: pointer over stack A → zoom → stack moves away → no interaction.
    // Stack A at cell (9,1): foot=(576,192), sprite covers [536,616)×[112,202)
    ar::PreparedAdventureMap map;
    map.geometry = ar::AdventureMapGeometry::from_source(10, 10);

    const auto foot_a = map.geometry.cell_foot_anchor({9, 1});
    EXPECT_EQ(foot_a.x, 576);
    EXPECT_EQ(foot_a.y, 192);

    const auto sid_a = ar::stable_render_id("Stack:A");
    map.pick_entries.push_back(
        {.stable_id = sid_a, .kind = ar::PickEntryKind::Stack, .object_id = "A"});

    ar::PreparedAdventureRenderPrimitive prim_a;
    prim_a.stable_id = sid_a;
    prim_a.level = ar::WorldRenderLevel::Actor;
    prim_a.depth_anchor = {9, 1};
    prim_a.draw_origin = {foot_a.x - 40, foot_a.y - 80};
    prim_a.src_width = 80;
    prim_a.src_height = 90;
    prim_a.interaction_mask = make_mask(80, 90);
    map.world_graph.world.push_back(std::move(prim_a));

    AdventurePickIndex index;
    index.build(map, kTestSelGeo);

    // Camera centered on 10×10 grid at viewport 1416×852, zoom=1.00.
    AdventureCamera cam;
    cam.canvas_x = -388;
    cam.canvas_y = -266;
    cam.viewport_width = 1416;
    cam.viewport_height = 852;
    cam.zoom_index = 2; // 1.00

    // Screen position that maps to the stack foot before zoom.
    const int screen_x = cam.canvas_to_screen_x(foot_a.x);
    const int screen_y = cam.canvas_to_screen_y(foot_a.y);
    EXPECT_EQ(screen_x, 964);
    EXPECT_EQ(screen_y, 458);

    {
        AdventureHitTester tester(index, cam);
        const auto         result = tester.hit_test(screen_x, screen_y);
        ASSERT_NE(result.interaction_target, nullptr) << "stack A must be hit before zoom";
        EXPECT_EQ(result.interaction_target->object_id, "A");
    }

    // Zoom in: camera recenters, same screen → different canvas.
    ASSERT_TRUE(cam.zoom_in());
    EXPECT_EQ(cam.zoom_index, 3); // 1.25
    EXPECT_EQ(cam.canvas_x, -246);
    EXPECT_EQ(cam.canvas_y, -181);

    {
        AdventureHitTester tester(index, cam);
        const auto         result = tester.hit_test(screen_x, screen_y);
        EXPECT_EQ(result.interaction_target, nullptr)
            << "stack A must not be hit after zoom at same screen position";
    }
}

TEST(AdventurePickIndex, StationaryPointerZoomCaseB) {
    // Case B: pointer over empty area → zoom → stack moves under pointer → interaction acquired.
    // Stack B at cell (8,2): foot=(512,192), sprite covers [472,552)×[112,202)
    ar::PreparedAdventureMap map;
    map.geometry = ar::AdventureMapGeometry::from_source(10, 10);

    const auto foot_b = map.geometry.cell_foot_anchor({8, 2});
    EXPECT_EQ(foot_b.x, 512);
    EXPECT_EQ(foot_b.y, 192);

    const auto sid_b = ar::stable_render_id("Stack:B");
    map.pick_entries.push_back(
        {.stable_id = sid_b, .kind = ar::PickEntryKind::Stack, .object_id = "B"});

    ar::PreparedAdventureRenderPrimitive prim_b;
    prim_b.stable_id = sid_b;
    prim_b.level = ar::WorldRenderLevel::Actor;
    prim_b.depth_anchor = {8, 2};
    prim_b.draw_origin = {foot_b.x - 40, foot_b.y - 80};
    prim_b.src_width = 80;
    prim_b.src_height = 90;
    prim_b.interaction_mask = make_mask(80, 90);
    map.world_graph.world.push_back(std::move(prim_b));

    AdventurePickIndex index;
    index.build(map, kTestSelGeo);

    AdventureCamera cam;
    cam.canvas_x = -388;
    cam.canvas_y = -266;
    cam.viewport_width = 1416;
    cam.viewport_height = 852;
    cam.zoom_index = 2; // 1.00

    // Same screen position as Case A — maps to canvas (576,192) before zoom.
    const int screen_x = 964;
    const int screen_y = 458;

    {
        AdventureHitTester tester(index, cam);
        const auto         result = tester.hit_test(screen_x, screen_y);
        EXPECT_EQ(result.interaction_target, nullptr)
            << "stack B must not be hit before zoom (canvas pos outside sprite)";
    }

    ASSERT_TRUE(cam.zoom_in());
    EXPECT_EQ(cam.canvas_x, -246);
    EXPECT_EQ(cam.canvas_y, -181);

    {
        AdventureHitTester tester(index, cam);
        const auto         result = tester.hit_test(screen_x, screen_y);
        ASSERT_NE(result.interaction_target, nullptr) << "stack B must be hit after zoom";
        EXPECT_EQ(result.interaction_target->object_id, "B");
    }
}

} // namespace d2engine
