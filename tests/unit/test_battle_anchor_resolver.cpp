#include <nlohmann/json.hpp>
#include <gtest/gtest.h>

#include "d2engine/app/battle_tuning_state.hpp"

#include "d2engine/battle_view/battle_anchor_resolver.hpp"
#include "d2engine/battle_view/battle_render_tree_contract.hpp"
#include "d2engine/render/render_tree.hpp"

#include <stdexcept>
#include <string>

namespace d2engine {
namespace {

[[nodiscard]] d2engine::TreeLayout make_slot_render_tree() {
    const auto cfg =
        std::filesystem::path(OPENDIS2_SOURCE_DIR) / "configs/screens/battle_screen.json";
    std::ifstream in(cfg);
    auto          j = nlohmann::json::parse(in);
    TreeLayout    tree;
    tree.load(j["render_tree"]);
    return tree;
}

SnapshotEntity entity(BattleSide side, int lane, BattleDepth depth) {
    return {.id = VisualEntityId{1}, .coord = {.side = side, .lane = lane, .depth = depth}};
}

} // namespace

// All policies execute and return something — smoke test only, no specific values.
TEST(BattleAnchorResolver, AllPoliciesExecuteWithoutCrash) {
    const TreeLayout     tree = make_slot_render_tree();
    BattleRenderSnapshot snapshot{
        .entities = {entity(BattleSide::Attacker, 1, BattleDepth::Front),
                     entity(BattleSide::Defender, 0, BattleDepth::Front)}};
    snapshot.entities[0].position_offset = {.x = 10.0f, .y = 20.0f};
    const auto& source = snapshot.entities[0];

    for (const AnchorPolicy policy : {
             AnchorPolicy::UnitFoot,
             AnchorPolicy::TeamCentroid,
             AnchorPolicy::OppositeTeamCentroid,
             AnchorPolicy::LaneMidpoint,
             AnchorPolicy::OppositeLaneMidpoint,
             AnchorPolicy::BattlefieldReferenceRect,
             AnchorPolicy::ScreenRect,
             AnchorPolicy::WorldPoint,
         }) {
        const Vec2 result =
            BattleAnchorResolver::resolve(policy, source, snapshot, tree, LayoutMetrics{});
        // just check it returns finite values — no config-specific values
        EXPECT_FALSE(std::isnan(result.x)) << static_cast<int>(policy);
        EXPECT_FALSE(std::isnan(result.y)) << static_cast<int>(policy);
    }
    // WorldPoint shifts by position_offset
    const Vec2 foot = BattleAnchorResolver::resolve(AnchorPolicy::UnitFoot, source, snapshot, tree,
                                                    LayoutMetrics{});
    const Vec2 world = BattleAnchorResolver::resolve(AnchorPolicy::WorldPoint, source, snapshot,
                                                     tree, LayoutMetrics{});
    EXPECT_FLOAT_EQ(world.x, foot.x + 10.0f);
    EXPECT_FLOAT_EQ(world.y, foot.y + 20.0f);
}

TEST(BattleAnchorResolver, TeamCentroidFallsBackToSideSlots) {
    const TreeLayout     tree = make_slot_render_tree();
    BattleRenderSnapshot snapshot{
        .entities = {entity(BattleSide::Defender, 0, BattleDepth::Front)}};
    snapshot.entities.front().life_state = LifeVisualState::Dead;
    const auto& source = snapshot.entities.front();

    const Vec2 centroid = BattleAnchorResolver::resolve(AnchorPolicy::TeamCentroid, source,
                                                        snapshot, tree, LayoutMetrics{});

    // Dead entity → fallback to all Defender slots; should be somewhere in battlefield
    EXPECT_GT(centroid.x, 400.0f);
}

TEST(BattleAnchorResolver, ComputesFootAnchoredDestination) {
    const SnapshotTrack track{
        .canvas_foot_x = 400, .canvas_foot_y = 300, .native_canvas_w = 800, .native_canvas_h = 600};
    const Rect result = BattleAnchorResolver::compute_destination_rect({.x = 500.0f, .y = 400.0f},
                                                                       track, 800.0f, 600.0f);
    EXPECT_FLOAT_EQ(result.x, 100.0f);
    EXPECT_FLOAT_EQ(result.y, 100.0f);
    EXPECT_FLOAT_EQ(result.w, 800.0f);
    EXPECT_FLOAT_EQ(result.h, 600.0f);
}

TEST(BattleAnchorResolver, FallsBackToTextureSize) {
    const SnapshotTrack track{.native_canvas_w = 800, .native_canvas_h = 600};
    const Rect result = BattleAnchorResolver::compute_destination_rect({.x = 100.0f, .y = 200.0f},
                                                                       track, 40.0f, 60.0f);
    EXPECT_FLOAT_EQ(result.x, 80.0f);
    EXPECT_FLOAT_EQ(result.y, 140.0f);
    EXPECT_FLOAT_EQ(result.w, 40.0f);
    EXPECT_FLOAT_EQ(result.h, 60.0f);
}

// Spec 2: corpse with canvas_foot uses foot-anchor, not bottom-center
TEST(BattleAnchorResolver, CorpseWithFootAnchorNotBottomCenter) {
    SnapshotTrack corpse;
    corpse.canvas_foot_x = 80;
    corpse.canvas_foot_y = 90;
    corpse.native_canvas_w = 200;
    corpse.native_canvas_h = 100;

    const Rect result = BattleAnchorResolver::compute_destination_rect({.x = 1000.0f, .y = 500.0f},
                                                                       corpse, 200.0f, 100.0f);

    EXPECT_FLOAT_EQ(result.x, 920.0f);
    EXPECT_FLOAT_EQ(result.y, 410.0f);
}

TEST(BattleAnchorResolver, CorpseWithFootAnchorAndPlacementCorrection) {
    SnapshotTrack corpse;
    corpse.canvas_foot_x = 80;
    corpse.canvas_foot_y = 90;
    corpse.native_canvas_w = 200;
    corpse.native_canvas_h = 100;

    const Rect result = BattleAnchorResolver::compute_destination_rect(
        {.x = 1000.0f, .y = 500.0f}, corpse, 200.0f, 100.0f, 1.0f, 1.0f, 1.0f,
        {.x = 10.0f, .y = -5.0f});

    EXPECT_FLOAT_EQ(result.x, 930.0f);
    EXPECT_FLOAT_EQ(result.y, 405.0f);
}

TEST(BattleAnchorResolver, CorpseWithNoAnchorUsesBottomCenter) {
    SnapshotTrack corpse;
    corpse.canvas_foot_x = 0;
    corpse.canvas_foot_y = 0;
    corpse.native_canvas_w = 200;
    corpse.native_canvas_h = 100;

    const Rect result = BattleAnchorResolver::compute_destination_rect({.x = 1000.0f, .y = 500.0f},
                                                                       corpse, 200.0f, 100.0f);

    EXPECT_FLOAT_EQ(result.x, 900.0f);
    EXPECT_FLOAT_EQ(result.y, 400.0f);
}

// ── Render-tree structural ────────────────────────────────────────────

TEST(BattleAnchorResolver, RenderTreeHasAllSlotAndUnitChildren) {
    const TreeLayout tree = make_slot_render_tree();

    for (const char side : {'a', 'd'}) {
        for (int lane = 0; lane < 3; ++lane) {
            for (const char* depth : {"back", "center", "front"}) {
                const BattleSlotCoord coord{.side = (side == 'a') ? BattleSide::Attacker
                                                                  : BattleSide::Defender,
                                            .lane = lane,
                                            .depth = (depth[0] == 'f')   ? BattleDepth::Front
                                                     : (depth[0] == 'c') ? BattleDepth::Center
                                                                         : BattleDepth::Back};
                const std::string     slot_path = battlefield_slot_tree_path(coord);
                const std::string     unit_path = battlefield_unit_tree_path(coord);
                EXPECT_TRUE(tree.has_node(slot_path)) << "Missing: " << slot_path;
                EXPECT_TRUE(tree.has_node(unit_path)) << "Missing: " << unit_path;
            }
        }
    }
}

// UnitFoot from tree returns something non-zero (slot is placed on the battlefield).
TEST(BattleAnchorResolver, UnitFootFromRenderTreeIsNonZero) {
    const TreeLayout     tree = make_slot_render_tree();
    BattleRenderSnapshot snapshot{
        .entities = {entity(BattleSide::Attacker, 0, BattleDepth::Front)}};
    const auto& source = snapshot.entities[0];

    const Vec2 anchor = BattleAnchorResolver::resolve(AnchorPolicy::UnitFoot, source, snapshot,
                                                      tree, LayoutMetrics{});
    EXPECT_GT(anchor.x, 0.0f);
    EXPECT_GT(anchor.y, 0.0f);
}

// Moving a slot node shifts the resolved anchor by the same delta.
TEST(BattleAnchorResolver, RenderTreeSlotMoveMovesAnchor) {
    BattleRenderSnapshot snapshot{
        .entities = {entity(BattleSide::Attacker, 0, BattleDepth::Front)}};
    const auto& source = snapshot.entities[0];

    TreeLayout tree_before = make_slot_render_tree();
    const Vec2 before = BattleAnchorResolver::resolve(AnchorPolicy::UnitFoot, source, snapshot,
                                                      tree_before, LayoutMetrics{});

    TreeLayout tree_after = make_slot_render_tree();
    const auto old_node = tree_after.node("/battlefield/slot_a_front_0");
    ASSERT_TRUE(old_node.has_value());
    tree_after.set_node("/battlefield/slot_a_front_0", TreeNode{.kind = old_node->kind,
                                                                .x = old_node->x + 50.0f,
                                                                .y = old_node->y,
                                                                .level = old_node->level});
    const Vec2 after = BattleAnchorResolver::resolve(AnchorPolicy::UnitFoot, source, snapshot,
                                                     tree_after, LayoutMetrics{});

    EXPECT_FLOAT_EQ(after.x, before.x + 50.0f);
    EXPECT_FLOAT_EQ(after.y, before.y);
}

// Adding a unit mount offset shifts anchor by that offset.
TEST(BattleAnchorResolver, UnitMountOffsetMovesAnchor) {
    BattleRenderSnapshot snapshot{
        .entities = {entity(BattleSide::Attacker, 0, BattleDepth::Front)}};
    const auto& source = snapshot.entities[0];

    TreeLayout tree_before = make_slot_render_tree();
    const Vec2 before = BattleAnchorResolver::resolve(AnchorPolicy::UnitFoot, source, snapshot,
                                                      tree_before, LayoutMetrics{});

    TreeLayout tree_after = make_slot_render_tree();
    tree_after.set_node("/battlefield/slot_a_front_0/unit",
                        TreeNode{.kind = "unit_mount", .x = 10.0f, .y = -5.0f});
    const Vec2 after = BattleAnchorResolver::resolve(AnchorPolicy::UnitFoot, source, snapshot,
                                                     tree_after, LayoutMetrics{});

    EXPECT_FLOAT_EQ(after.x, before.x + 10.0f);
    EXPECT_FLOAT_EQ(after.y, before.y + (-5.0f));
}

// TeamCentroid fallback uses tree: moving a slot changes the centroid.
TEST(BattleAnchorResolver, TeamCentroidFallbackUsesRenderTreeSlots) {
    BattleRenderSnapshot snapshot;
    snapshot.entities = {entity(BattleSide::Attacker, 0, BattleDepth::Front)};
    const auto& source = snapshot.entities[0];

    TreeLayout tree_before = make_slot_render_tree();
    const Vec2 before = BattleAnchorResolver::resolve(AnchorPolicy::OppositeTeamCentroid, source,
                                                      snapshot, tree_before, LayoutMetrics{});

    TreeLayout tree_after = make_slot_render_tree();
    tree_after.set_node(
        "/battlefield/slot_d_front_0",
        TreeNode{.kind = "battlefield_slot", .x = 9999.0f, .y = 9999.0f, .level = 10});
    const Vec2 after = BattleAnchorResolver::resolve(AnchorPolicy::OppositeTeamCentroid, source,
                                                     snapshot, tree_after, LayoutMetrics{});

    EXPECT_NE(before.x, after.x);
}

// OppositeTeamCentroid (no live entities on opposite side) falls back to all D slots.
TEST(BattleAnchorResolver, OppositeTeamCentroidFallbackIsOnBattlefield) {
    const TreeLayout     tree = make_slot_render_tree();
    BattleRenderSnapshot snapshot{
        .entities = {entity(BattleSide::Attacker, 1, BattleDepth::Front)}};
    const auto& source = snapshot.entities[0];

    const Vec2 centroid = BattleAnchorResolver::resolve(AnchorPolicy::OppositeTeamCentroid, source,
                                                        snapshot, tree, LayoutMetrics{});
    EXPECT_GT(centroid.x, 400.0f);
    EXPECT_LT(centroid.x, 1600.0f);
}

// Non-null tree_layout that is missing required battlefield nodes must fail hard.
TEST(BattleAnchorResolver, EmptyTreeLayoutThrows) {
    TreeLayout const     empty_tree;
    BattleRenderSnapshot snapshot{
        .entities = {entity(BattleSide::Attacker, 1, BattleDepth::Front)}};
    const auto& source = snapshot.entities[0];

    EXPECT_THROW(static_cast<void>(BattleAnchorResolver::resolve(
                     AnchorPolicy::UnitFoot, source, snapshot, empty_tree, LayoutMetrics{})),
                 std::runtime_error);
}

// ── CENTER slot ───────────────────────────────────────────────────────

// Moving a CENTER slot node updates the anchor.
TEST(BattleAnchorResolver, CenterSlotNodeMovedUpdatesAnchor) {
    BattleRenderSnapshot snapshot{
        .entities = {entity(BattleSide::Attacker, 1, BattleDepth::Center)}};
    const auto& source = snapshot.entities[0];

    TreeLayout tree = make_slot_render_tree();
    tree.set_node("/battlefield/slot_a_center_1",
                  TreeNode{.kind = "battlefield_slot", .x = 700.0f, .y = 450.0f, .level = 10});
    tree.set_node("/battlefield/slot_a_center_1/unit",
                  TreeNode{.kind = "unit_mount", .x = 5.0f, .y = -3.0f});

    const Vec2 anchor = BattleAnchorResolver::resolve(AnchorPolicy::UnitFoot, source, snapshot,
                                                      tree, LayoutMetrics{});
    // slot (700, 450) + unit offset (5, -3) — fully controlled values
    EXPECT_FLOAT_EQ(anchor.x, 705.0f);
    EXPECT_FLOAT_EQ(anchor.y, 447.0f);
}

// Missing CENTER node now throws (no more fallback to midpoint).
TEST(BattleAnchorResolver, CenterSlotMissingNodeThrows) {
    TreeLayout tree;
    tree.set_node("/battlefield/slot_a_back_0",
                  TreeNode{.kind = "battlefield_slot", .x = 785.0f, .y = 312.0f, .level = 10});
    tree.set_node("/battlefield/slot_a_back_0/unit", TreeNode{.kind = "unit_mount"});
    tree.set_node("/battlefield/slot_a_front_0",
                  TreeNode{.kind = "battlefield_slot", .x = 939.0f, .y = 411.0f, .level = 10});
    tree.set_node("/battlefield/slot_a_front_0/unit", TreeNode{.kind = "unit_mount"});
    // Missing A_CENTER_0 and A_CENTER_0/unit — now required; should throw.
    BattleRenderSnapshot snapshot{
        .entities = {entity(BattleSide::Attacker, 0, BattleDepth::Center)}};
    const auto& source = snapshot.entities[0];

    EXPECT_THROW(static_cast<void>(BattleAnchorResolver::resolve(AnchorPolicy::UnitFoot, source,
                                                                 snapshot, tree, LayoutMetrics{})),
                 std::runtime_error);
}

// Missing /unit mount node throws with the exact /unit path in the message.
// This would fail before the fix because compose(unit_path) silently returned
// the parent slot position when /unit was absent.
TEST(BattleAnchorResolver, MissingUnitMountNodeThrows) {
    TreeLayout tree;
    tree.set_node("/battlefield/slot_a_center_1",
                  TreeNode{.kind = "battlefield_slot", .x = 700.0f, .y = 450.0f, .level = 10});
    // Deliberately omit /battlefield/slot_a_center_1/unit
    BattleRenderSnapshot snapshot{
        .entities = {entity(BattleSide::Attacker, 1, BattleDepth::Center)}};
    const auto& source = snapshot.entities[0];

    try {
        static_cast<void>(BattleAnchorResolver::resolve(AnchorPolicy::UnitFoot, source, snapshot,
                                                        tree, LayoutMetrics{}));
        FAIL() << "Expected std::runtime_error";
    } catch (const std::runtime_error& e) {
        const std::string msg(e.what());
        EXPECT_TRUE(msg.find("/battlefield/slot_a_center_1/unit") != std::string::npos)
            << "message: " << msg;
    }
}

} // namespace d2engine
