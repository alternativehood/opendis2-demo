#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <ranges>
#include <vector>

#include "d2engine/render/render_tree.hpp"
#include "d2engine/app/battle_tuning_state.hpp"
#include "d2engine/app/screen_config_store.hpp"
#include "d2engine/app/tree_layout_editor.hpp"
#include "d2engine/app/screen_manager.hpp"
#include "d2engine/app/battle_screen.hpp"

#include "test_battle_tree_helpers.hpp"

namespace d2engine {

// ── Tree Config Parsing ─────────────────────────────────────────────────

TEST(TreeLayout, LoadAndQuery) {
    const nlohmann::json j = R"({
        "/background": {"kind": "background_container", "x": 0, "y": 0, "w": 1600, "h": 945, "level": -8},
        "/battlefield/slot_a_back_0": {"kind": "battlefield_slot", "x": 785, "y": 312, "w": 0, "h": 0, "level": 10},
        "/battlefield/slot_a_back_0/unit": {"kind": "unit_mount", "x": 0, "y": 0, "w": 0, "h": 0, "level": 0},
        "/ui/combat_frame": {"kind": "image", "x": 1.92, "y": 630.71, "w": 1599.52, "h": 315.04, "level": 2}
    })"_json;

    TreeLayout tl;
    tl.load(j);
    EXPECT_EQ(tl.paths().size(), 4);

    auto n = tl.node("/background");
    ASSERT_TRUE(n.has_value());
    EXPECT_FLOAT_EQ(n->x, 0.0f);
    EXPECT_FLOAT_EQ(n->y, 0.0f);
    EXPECT_FLOAT_EQ(n->w, 1600.0f);
    EXPECT_FLOAT_EQ(n->h, 945.0f);

    auto m = tl.node("/battlefield/slot_a_back_0");
    ASSERT_TRUE(m.has_value());
    EXPECT_FLOAT_EQ(m->x, 785.0f);
    EXPECT_FLOAT_EQ(m->y, 312.0f);

    auto missing = tl.node("/nonexistent");
    EXPECT_FALSE(missing.has_value());
}

TEST(TreeLayout, TextStyleLoadsSavesAndDefaultsToSmallBlackText) {
    TreeLayout tl;
    tl.load(R"({
        "/ui/text_default": {"kind": "text", "x": 0, "y": 0, "w": 10, "h": 10},
        "/ui/text_custom": {
            "kind": "text", "x": 0, "y": 0, "w": 10, "h": 10,
            "color": {"r": 7, "g": 8, "b": 9, "a": 255},
            "font_size": 11
        }
    })"_json);

    const auto def = tl.node("/ui/text_default");
    ASSERT_TRUE(def.has_value());
    EXPECT_EQ(def->color.r, 0);
    EXPECT_EQ(def->color.g, 0);
    EXPECT_EQ(def->color.b, 0);
    EXPECT_EQ(def->color.a, 255);
    EXPECT_LT(def->font_size, 16.0f);

    const auto custom = tl.node("/ui/text_custom");
    ASSERT_TRUE(custom.has_value());
    EXPECT_EQ(custom->color.r, 7);
    EXPECT_EQ(custom->color.g, 8);
    EXPECT_EQ(custom->color.b, 9);
    EXPECT_FLOAT_EQ(custom->font_size, 11.0f);

    nlohmann::json saved;
    tl.save(saved);
    EXPECT_EQ(saved["/ui/text_custom"]["color"]["r"], 7);
    EXPECT_EQ(saved["/ui/text_custom"]["font_size"], 11.0f);
}

TEST(TreeLayout, DiagnosticsReportNegativeCoordinatesAndMissingParents) {
    TreeLayout tl;
    tl.set_node("/bad/left_slot", TreeNode{.kind = "slot", .x = -1.0f});
    tl.set_node("/bad/right_slot", TreeNode{.kind = "slot", .y = -2.0f});

    const auto diagnostics = tl.diagnostics();
    EXPECT_NE(std::ranges::find(diagnostics, "render_tree missing parent /bad for /bad/left_slot"),
              diagnostics.end());
}

TEST(TreeLayout, HasNode) {
    TreeLayout tl;
    tl.load(R"({"/a": {"kind": "x", "x": 1, "y": 2, "w": 3, "h": 4, "level": 5}})"_json);
    EXPECT_TRUE(tl.has_node("/a"));
    EXPECT_FALSE(tl.has_node("/b"));
}

TEST(TreeLayout, SetNode) {
    TreeLayout tl;
    tl.set_node("/test",
                TreeNode{.kind = "test", .x = 10, .y = 20, .w = 100, .h = 200, .level = 5});
    auto n = tl.node("/test");
    ASSERT_TRUE(n.has_value());
    EXPECT_FLOAT_EQ(n->x, 10.0f);
    EXPECT_EQ(n->level, 5);
}

// ── Composition ────────────────────────────────────────────────────────

TEST(TreeLayout, ComposeSingleNode) {
    TreeLayout tl;
    tl.load(
        R"({"/root": {"kind": "root", "x": 10, "y": 20, "w": 800, "h": 600, "level": 0}})"_json);
    Rect const r = tl.compose("/root");
    EXPECT_FLOAT_EQ(r.x, 10.0f);
    EXPECT_FLOAT_EQ(r.y, 20.0f);
    EXPECT_FLOAT_EQ(r.w, 800.0f);
    EXPECT_FLOAT_EQ(r.h, 600.0f);
}

TEST(TreeLayout, ComposeParentChild) {
    TreeLayout tl;
    tl.load(R"({
        "/ui": {"kind": "root", "x": 0, "y": 0, "w": 1600, "h": 945, "level": 0},
        "/ui/group": {"kind": "panel", "x": 100, "y": 50, "w": 300, "h": 200, "level": 5}
    })"_json);
    Rect const r = tl.compose("/ui/group");
    EXPECT_FLOAT_EQ(r.x, 100.0f);
    EXPECT_FLOAT_EQ(r.y, 50.0f);
    EXPECT_FLOAT_EQ(r.w, 300.0f);
    EXPECT_FLOAT_EQ(r.h, 200.0f);
}

TEST(TreeLayout, ComposeDeepNesting) {
    TreeLayout tl;
    tl.load(R"({
        "/ui": {"kind": "root", "x": 2, "y": 25, "w": 277.07, "h": 705, "level": 0},
        "/ui/left_unit_group": {"kind": "panel", "x": 2, "y": 25, "w": 277.07, "h": 705, "level": 0},
        "/ui/left_unit_group/0_back": {"kind": "slot", "x": 9, "y": 116, "w": 113, "h": 160, "level": 0},
        "/ui/left_unit_group/0_back/portrait": {"kind": "portrait", "x": 1, "y": 1, "w": 112, "h": 134.32, "level": 0}
    })"_json);
    Rect const r = tl.compose("/ui/left_unit_group/0_back/portrait");
    EXPECT_FLOAT_EQ(r.x, 14.0f);
    EXPECT_FLOAT_EQ(r.y, 167.0f);
    EXPECT_FLOAT_EQ(r.w, 112.0f);
    EXPECT_FLOAT_EQ(r.h, 134.32f);
}

TEST(TreeLayout, ComposeMissingPath) {
    TreeLayout const tl;
    Rect const       r = tl.compose("/missing");
    EXPECT_FLOAT_EQ(r.x, 0.0f);
    EXPECT_FLOAT_EQ(r.y, 0.0f);
    EXPECT_FLOAT_EQ(r.w, 0.0f);
    EXPECT_FLOAT_EQ(r.h, 0.0f);
}

TEST(TreeLayout, ComposeEmptyPath) {
    TreeLayout const tl;
    Rect const       r = tl.compose("");
    EXPECT_FLOAT_EQ(r.x, 0.0f);
    EXPECT_FLOAT_EQ(r.y, 0.0f);
}

TEST(TreeLayout, ComposeWithScale) {
    TreeLayout tl;
    tl.load(R"({"/a": {"kind": "x", "x": 100, "y": 200, "w": 400, "h": 300, "level": 0}})"_json);
    Rect const r = tl.compose("/a");
    EXPECT_FLOAT_EQ(r.x, 100.0f);
    EXPECT_FLOAT_EQ(r.y, 200.0f);
    EXPECT_FLOAT_EQ(r.w, 400.0f);
    EXPECT_FLOAT_EQ(r.h, 300.0f);
}

TEST(TreeLayout, MoveParentMovesChildren) {
    TreeLayout tl;
    tl.load(R"({
        "/group": {"kind": "panel", "x": 50, "y": 50, "w": 200, "h": 200, "level": 0},
        "/group/child": {"kind": "item", "x": 10, "y": 10, "w": 50, "h": 50, "level": 0}
    })"_json);
    EXPECT_FLOAT_EQ(tl.compose("/group/child").x, 60.0f);
    tl.set_node("/group",
                TreeNode{.kind = "panel", .x = 100, .y = 100, .w = 200, .h = 200, .level = 0});
    EXPECT_FLOAT_EQ(tl.compose("/group/child").x, 110.0f);
}

TEST(TreeLayout, SaveLoadRoundTrip) {
    TreeLayout tl;
    tl.load(R"({
        "/background": {"kind": "background_container", "x": 0, "y": 0, "w": 1600, "h": 945, "level": -8},
        "/battlefield/slot_a_back_0": {"kind": "bf_slot", "x": 785, "y": 312, "w": 0, "h": 0, "level": 10},
        "/battlefield/slot_a_back_0/unit": {"kind": "unit_mount", "x": 0, "y": 0, "w": 0, "h": 0, "level": 0}
    })"_json);
    nlohmann::json saved;
    tl.save(saved);
    EXPECT_TRUE(saved.is_object());
    EXPECT_TRUE(saved.contains("/background"));
    EXPECT_EQ(saved["/background"]["kind"], "background_container");

    TreeLayout tl2;
    tl2.load(saved);
    EXPECT_EQ(tl2.paths().size(), 3);
    auto n = tl2.node("/background");
    ASSERT_TRUE(n.has_value());
    EXPECT_EQ(n->level, -8);
}

TEST(TreeLayout, EmptyTree) {
    TreeLayout const tl;
    EXPECT_TRUE(tl.empty());
    EXPECT_EQ(tl.paths().size(), 0);
    EXPECT_FALSE(tl.has_node("/anything"));
    EXPECT_FALSE(tl.node("/anything").has_value());
    EXPECT_FLOAT_EQ(tl.compose("/anything").x, 0.0f);
}

TEST(TreeLayout, Clear) {
    TreeLayout tl;
    tl.load(R"({"/a": {"kind": "x", "x": 1, "y": 2, "w": 3, "h": 4, "level": 0}})"_json);
    EXPECT_FALSE(tl.empty());
    tl.clear();
    EXPECT_TRUE(tl.empty());
}

TEST(TreeLayout, Erase) {
    TreeLayout tl;
    tl.load(R"({"/a": {"kind": "x", "x": 1, "y": 2, "w": 3, "h": 4, "level": 0}})"_json);
    EXPECT_TRUE(tl.has_node("/a"));
    tl.erase("/a");
    EXPECT_FALSE(tl.has_node("/a"));
}

TEST(TreeLayout, LoadEmptyObject) {
    TreeLayout tl;
    tl.load(nlohmann::json::object());
    EXPECT_TRUE(tl.empty());
}

TEST(TreeLayout, LoadNonObject) {
    TreeLayout tl;
    tl.load(nlohmann::json::array());
    EXPECT_TRUE(tl.empty());
}

TEST(TreeLayout, SetNodeUpdatesSlotPosition) {
    TreeLayout tl;
    tl.set_node("/battlefield/slot_a_front_0",
                TreeNode{.kind = "battlefield_slot", .x = 939.0f, .y = 411.0f, .level = 10});
    tl.set_node("/battlefield/slot_a_front_0/unit",
                TreeNode{.kind = "unit_mount", .x = 0.0f, .y = 0.0f});

    tl.set_node("/battlefield/slot_a_front_0",
                TreeNode{.kind = "battlefield_slot", .x = 989.0f, .y = 411.0f, .level = 10});

    auto n = tl.node("/battlefield/slot_a_front_0");
    ASSERT_TRUE(n.has_value());
    EXPECT_FLOAT_EQ(n->x, 989.0f);
    EXPECT_EQ(n->level, 10);

    auto m = tl.node("/battlefield/slot_a_front_0/unit");
    ASSERT_TRUE(m.has_value());
    EXPECT_FLOAT_EQ(m->x, 0.0f);
    EXPECT_FLOAT_EQ(m->y, 0.0f);

    Rect const r = tl.compose("/battlefield/slot_a_front_0/unit");
    EXPECT_FLOAT_EQ(r.x, 989.0f);
    EXPECT_FLOAT_EQ(r.y, 411.0f);
}

TEST(TreeLayout, SetNodeUpdatesMountOffsetOnly) {
    TreeLayout tl;
    tl.set_node("/battlefield/slot_a_front_0",
                TreeNode{.kind = "battlefield_slot", .x = 939.0f, .y = 411.0f, .level = 10});
    tl.set_node("/battlefield/slot_a_front_0/unit",
                TreeNode{.kind = "unit_mount", .x = 0.0f, .y = 0.0f});

    tl.set_node("/battlefield/slot_a_front_0/unit",
                TreeNode{.kind = "unit_mount", .x = 10.0f, .y = -5.0f});

    auto n = tl.node("/battlefield/slot_a_front_0");
    ASSERT_TRUE(n.has_value());
    EXPECT_FLOAT_EQ(n->x, 939.0f);

    auto m = tl.node("/battlefield/slot_a_front_0/unit");
    ASSERT_TRUE(m.has_value());
    EXPECT_FLOAT_EQ(m->x, 10.0f);
    EXPECT_FLOAT_EQ(m->y, -5.0f);

    Rect const r = tl.compose("/battlefield/slot_a_front_0/unit");
    EXPECT_FLOAT_EQ(r.x, 949.0f);
    EXPECT_FLOAT_EQ(r.y, 406.0f);
}

TEST(TreeLayout, SlotTuningDoesNotCreateAnimationProfiles) {
    TreeLayout tl;
    tl.set_node("/battlefield/slot_a_front_0",
                TreeNode{.kind = "battlefield_slot", .x = 939.0f, .y = 411.0f, .level = 10});
    tl.set_node("/battlefield/slot_a_front_0/unit",
                TreeNode{.kind = "unit_mount", .x = 0.0f, .y = 0.0f});

    EXPECT_TRUE(tl.has_node("/battlefield/slot_a_front_0"));
    EXPECT_TRUE(tl.has_node("/battlefield/slot_a_front_0/unit"));

    tl.set_node("/battlefield/slot_a_front_0",
                TreeNode{.kind = "battlefield_slot", .x = 1000.0f, .y = 400.0f, .level = 10});

    auto n = tl.node("/battlefield/slot_a_front_0");
    ASSERT_TRUE(n.has_value());
    EXPECT_FLOAT_EQ(n->x, 1000.0f);
}

TEST(TreeLayout, SaveLoadWithTuningChanges) {
    TreeLayout tl;
    tl.set_node("/battlefield/slot_a_front_0", TreeNode{.kind = "battlefield_slot",
                                                        .x = 989.0f,
                                                        .y = 411.0f,
                                                        .w = 0.0f,
                                                        .h = 0.0f,
                                                        .level = 10});
    tl.set_node(
        "/battlefield/slot_a_front_0/unit",
        TreeNode{.kind = "unit_mount", .x = 10.0f, .y = -5.0f, .w = 0.0f, .h = 0.0f, .level = 0});

    nlohmann::json json;
    tl.save(json);

    EXPECT_TRUE(json.contains("/battlefield/slot_a_front_0"));
    EXPECT_EQ(json["/battlefield/slot_a_front_0"]["x"], 989.0f);
    EXPECT_EQ(json["/battlefield/slot_a_front_0"]["y"], 411.0f);

    EXPECT_TRUE(json.contains("/battlefield/slot_a_front_0/unit"));
    EXPECT_EQ(json["/battlefield/slot_a_front_0/unit"]["x"], 10.0f);
    EXPECT_EQ(json["/battlefield/slot_a_front_0/unit"]["y"], -5.0f);

    TreeLayout tl2;
    tl2.load(json);
    auto n = tl2.node("/battlefield/slot_a_front_0");
    ASSERT_TRUE(n.has_value());
    EXPECT_FLOAT_EQ(n->x, 989.0f);
    auto m = tl2.node("/battlefield/slot_a_front_0/unit");
    ASSERT_TRUE(m.has_value());
    EXPECT_FLOAT_EQ(m->x, 10.0f);
}

TEST(TreeLayout, DebugItemsHaveOwnerKindTreeLayout) {
    TreeLayout tl;
    tl.set_node("/battlefield/slot_a_front_0",
                TreeNode{.kind = "battlefield_slot", .x = 939.0f, .y = 411.0f, .level = 10});
    tl.set_node("/battlefield/slot_a_front_0/unit",
                TreeNode{.kind = "unit_mount", .x = 0.0f, .y = 0.0f});

    auto slot_node = tl.node("/battlefield/slot_a_front_0");
    ASSERT_TRUE(slot_node.has_value());
    EXPECT_EQ(slot_node->kind, "battlefield_slot");
    EXPECT_FLOAT_EQ(slot_node->x, 939.0f);

    auto mount_node = tl.node("/battlefield/slot_a_front_0/unit");
    ASSERT_TRUE(mount_node.has_value());
    EXPECT_EQ(mount_node->kind, "unit_mount");
    EXPECT_FLOAT_EQ(mount_node->x, 0.0f);
    EXPECT_FLOAT_EQ(mount_node->y, 0.0f);
}

// ── Production config invariants (migrated from BattleTuningState::render_tree) ──

TEST(TreeLayout, BattleScreenConfigPreservesBackground) {
    auto tree = test::load_battle_tree_layout();
    ASSERT_FALSE(tree.empty());

    auto bg = tree.node("/background");
    ASSERT_TRUE(bg.has_value()) << "/background not found in render_tree";
    EXPECT_FLOAT_EQ(bg->x, 0.0f);
    EXPECT_FLOAT_EQ(bg->y, 0.0f);
    EXPECT_FLOAT_EQ(bg->w, 1600.0f);
    EXPECT_FLOAT_EQ(bg->h, 945.0f);
    EXPECT_EQ(bg->level, -8);
}

TEST(TreeLayout, BattleScreenConfigPreservesCombatFrame) {
    auto tree = test::load_battle_tree_layout();
    auto cf = tree.node("/ui/combat_frame");
    ASSERT_TRUE(cf.has_value());
    EXPECT_FLOAT_EQ(cf->x, -1.0f);
    EXPECT_FLOAT_EQ(cf->y, 630.0f);
    EXPECT_FLOAT_EQ(cf->w, 1602.0f);
    EXPECT_FLOAT_EQ(cf->h, 315.0f);
    EXPECT_EQ(cf->level, 2);
}

TEST(TreeLayout, BattleScreenConfigPreservesLeftPortrait) {
    auto tree = test::load_battle_tree_layout();
    auto p = tree.node("/ui/left_unit_group/0_back/portrait");
    ASSERT_TRUE(p.has_value());
    EXPECT_FLOAT_EQ(p->x, 0.0f);
    EXPECT_FLOAT_EQ(p->y, 0.0f);
    EXPECT_FLOAT_EQ(p->w, 113.0f);
    EXPECT_FLOAT_EQ(p->h, 135.0f);
    EXPECT_EQ(p->level, 0);
}

TEST(TreeLayout, BattleScreenConfigPreservesRightSlot) {
    auto tree = test::load_battle_tree_layout();
    auto s = tree.node("/ui/right_unit_group/0_back");
    ASSERT_TRUE(s.has_value());
    EXPECT_FLOAT_EQ(s->x, 156.0f);
    EXPECT_FLOAT_EQ(s->y, 117.0f);
    EXPECT_FLOAT_EQ(s->w, 113.0f);
    EXPECT_FLOAT_EQ(s->h, 160.0f);
    EXPECT_EQ(s->level, 0);
}

TEST(TreeLayout, BattleScreenConfigHasBattlefieldSlots) {
    auto tree = test::load_battle_tree_layout();
    for (const char side : {'a', 'd'}) {
        for (int lane = 0; lane < 3; ++lane) {
            for (const char* depth : {"back", "center", "front"}) {
                char path[64];
                std::snprintf(path, sizeof(path), "/battlefield/slot_%c_%s_%d", side, depth, lane);
                EXPECT_TRUE(tree.has_node(path)) << "Missing: " << path;
                char unit_path[80];
                std::snprintf(unit_path, sizeof(unit_path), "%s/unit", path);
                EXPECT_TRUE(tree.has_node(unit_path)) << "Missing: " << unit_path;
            }
        }
    }
}

TEST(TreeLayout, BattleScreenConfigHasCenterPortraits) {
    auto tree = test::load_battle_tree_layout();
    for (const char* prefix : {"/ui/left_unit_group", "/ui/right_unit_group"}) {
        for (int lane = 0; lane < 3; ++lane) {
            char cp[80];
            std::snprintf(cp, sizeof(cp), "%s/%d_center/portrait", prefix, lane);
            EXPECT_TRUE(tree.has_node(cp)) << "Missing center portrait: " << cp;
        }
    }
}

TEST(TreeLayout, NoForbiddenAggregatePaths) {
    auto        tree = test::load_battle_tree_layout();
    const char* forbidden[] = {
        "/battlefield/slot_a_center", "/battlefield/slot_d_center", "/battlefield/slot_a_front",
        "/battlefield/slot_a_back",   "/battlefield/slot_d_front",  "/battlefield/slot_d_back",
    };
    for (const char* path : forbidden) {
        EXPECT_FALSE(tree.has_node(path)) << "Forbidden aggregate path present: " << path;
    }
}

TEST(TreeLayout, NoSideBOrCommon) {
    auto tree = test::load_battle_tree_layout();
    for (const auto& p : tree.paths()) {
        if (p.starts_with("/battlefield/")) {
            EXPECT_EQ(p.find("_b_"), std::string::npos) << "Path contains 'b' side variant: " << p;
            EXPECT_EQ(p.find("/common"), std::string::npos) << "Path contains 'common': " << p;
        }
    }
}

TEST(TreeLayout, BattleScreenConfigHasCleanRootToLeafCoordinates) {
    auto       tree = test::load_battle_tree_layout();
    const auto diagnostics = tree.diagnostics();
    EXPECT_TRUE(diagnostics.empty()) << (diagnostics.empty() ? "" : diagnostics.front());
}

TEST(TreeLayout, BattleScreenConfigUnitgroupHpGeometryMatchesPortraitStrip) {
    auto tree = test::load_battle_tree_layout();
    for (const char* prefix : {"/ui/left_unit_group", "/ui/right_unit_group"}) {
        for (int lane = 0; lane < 3; ++lane) {
            for (const char* depth : {"back", "center", "front"}) {
                const std::string slot_path =
                    std::string(prefix) + "/" + std::to_string(lane) + "_" + depth;
                const std::string portrait_path = slot_path + "/portrait";
                const std::string hp_path = slot_path + "/hp";
                const auto        slot = tree.node(slot_path);
                const auto        portrait = tree.node(portrait_path);
                const auto        hp = tree.node(hp_path);
                ASSERT_TRUE(slot.has_value()) << slot_path;
                ASSERT_TRUE(portrait.has_value()) << portrait_path;
                ASSERT_TRUE(hp.has_value()) << hp_path;

                EXPECT_GE(portrait->x, 0.0f) << portrait_path;
                EXPECT_GE(portrait->y, -1.0f) << portrait_path;
                EXPECT_FLOAT_EQ(hp->x, 0.0f) << hp_path;
                EXPECT_GE(hp->y, 0.0f) << hp_path;
                EXPECT_LE(hp->y, portrait->h) << hp_path;
                EXPECT_GT(hp->w, 0.0f) << hp_path;
                EXPECT_FLOAT_EQ(hp->h, slot->h - portrait->h) << hp_path;
            }
        }
    }
}

TEST(TreeLayout, BattleScreenConfigCombatFrameInfoIsSymmetricAndLocalChildrenAreClean) {
    auto       tree = test::load_battle_tree_layout();
    const auto left = tree.node("/ui/combat_frame/info_left");
    const auto right = tree.node("/ui/combat_frame/info_right");
    const auto left_name = tree.node("/ui/combat_frame/info_left/name");
    const auto left_hp = tree.node("/ui/combat_frame/info_left/hp");
    const auto right_name = tree.node("/ui/combat_frame/info_right/name");
    const auto right_hp = tree.node("/ui/combat_frame/info_right/hp");
    ASSERT_TRUE(left.has_value());
    ASSERT_TRUE(right.has_value());
    ASSERT_TRUE(left_name.has_value());
    ASSERT_TRUE(left_hp.has_value());
    ASSERT_TRUE(right_name.has_value());
    ASSERT_TRUE(right_hp.has_value());

    EXPECT_FLOAT_EQ(left_hp->x, left_name->x);
    EXPECT_FLOAT_EQ(right_hp->x, right_name->x);
    EXPECT_FLOAT_EQ(left_hp->y, right_hp->y);
    EXPECT_FLOAT_EQ(left_name->y, right_name->y);
    for (const auto* node : {&*left_name, &*left_hp, &*right_name, &*right_hp}) {
        EXPECT_GE(node->x, 0.0f);
        EXPECT_GE(node->y, 0.0f);
    }
}

TEST(TreeLayout, BattleScreenConfigCombatFrameDirectSlotsAreNonNegative) {
    auto tree = test::load_battle_tree_layout();
    for (const char* path :
         {"/ui/combat_frame", "/ui/combat_frame/left_slot", "/ui/combat_frame/right_slot"}) {
        const auto node = tree.node(path);
        ASSERT_TRUE(node.has_value()) << path;
        EXPECT_GE(node->x, -2.0f) << path;
        EXPECT_GE(node->y, 0.0f) << path;
    }
}

// ── ConfigBinding tests ────────────────────────────────────────────────

TEST(ConfigBinding, KeyUsesTargetId) {
    ConfigBinding b;
    b.owner_kind = BindingOwnerKind::TreeLayout;
    b.target_id = "/background";
    b.role = BindingRole::Background;
    EXPECT_EQ(b.key(), "TreeLayout:/background:Background");
}

TEST(ConfigBinding, KeyFallsBackToTreePath) {
    ConfigBinding b;
    b.owner_kind = BindingOwnerKind::TreeLayout;
    b.tree_path = "/background";
    b.role = BindingRole::Background;
    EXPECT_EQ(b.key(), "TreeLayout:/background:Background");
}

TEST(ConfigBinding, KeyPrefersTargetId) {
    ConfigBinding b;
    b.owner_kind = BindingOwnerKind::TreeLayout;
    b.tree_path = "/old/path";
    b.target_id = "/new/path";
    b.role = BindingRole::Background;
    EXPECT_EQ(b.key(), "TreeLayout:/new/path:Background");
}

TEST(ConfigBinding, WritableWithTargetId) {
    ConfigBinding b;
    b.config_file = "cfg.json";
    b.target_id = "/foo";
    EXPECT_TRUE(b.writable());
}

TEST(ConfigBinding, WritableWithTreePath) {
    ConfigBinding b;
    b.config_file = "cfg.json";
    b.tree_path = "/foo";
    EXPECT_TRUE(b.writable());
}

TEST(ConfigBinding, NotWritableEmpty) {
    ConfigBinding const b;
    EXPECT_FALSE(b.writable());
}

// ── DebugRenderableItem tests ──────────────────────────────────────────

TEST(DebugRenderableItem, TreePathField) {
    DebugRenderableItem item;
    item.tree_path = "/background";
    EXPECT_EQ(item.tree_path, "/background");
}

// ── TreeLayoutEditor dirty tracking ────────────────────────────────────

namespace {

class TestEditorScreen : public Screen {
public:
    explicit TestEditorScreen(TreeLayout tl, std::string cs = "test://editor")
        : Screen(std::move(tl), std::move(cs)) {}
    std::string_view name() const override { return "TestEditor"; }
    bool             handle_input(const InputEvent&) override { return false; }
    void             update(const d2::app::ScreenUpdateContext&) override {}
    void             render(Renderer2D&) override {}
};

struct EditorFixture {
    ScreenManager     manager;
    ScreenConfigStore config_store{"."};
    TreeLayoutEditor  editor{manager, config_store};

    TreeLayout make_tree() {
        TreeLayout t;
        t.set_node("/panel",
                   TreeNode{.kind = "slot", .x = 100, .y = 200, .w = 300, .h = 400, .level = 5});
        t.set_node("/panel/child", TreeNode{.kind = "mount", .x = 10, .y = 20, .w = 50, .h = 60});
        return t;
    }

    TestEditorScreen* install() {
        auto  screen = std::make_unique<TestEditorScreen>(make_tree());
        auto* ptr = screen.get();
        manager.switch_to(std::move(screen));
        return ptr;
    }
};

} // namespace

TEST(TreeLayout, SelectNodeAndEdit) {
    EditorFixture    fx;
    auto*            screen = fx.install();
    ScreenInstanceId id = screen->instance_id();
    ASSERT_NE(id, 0u);

    EXPECT_TRUE(fx.editor.select_node("/panel"));
    auto node = screen->tree_layout().node("/panel");
    ASSERT_TRUE(node.has_value());
    EXPECT_FLOAT_EQ(node->x, 100.0f);

    TreeNode u = *node;
    u.x = 150.0f;
    EXPECT_TRUE(fx.editor.update_selected_node(u));

    EXPECT_FLOAT_EQ(screen->tree_layout().node("/panel")->x, 150.0f);
    EXPECT_TRUE(fx.editor.is_dirty());
}

TEST(TreeLayout, RevertRestoresOriginal) {
    EditorFixture fx;
    auto*         screen = fx.install();
    fx.editor.select_node("/panel");
    TreeNode u = *screen->tree_layout().node("/panel");
    u.x += 50.0f;
    fx.editor.update_selected_node(u);
    EXPECT_TRUE(fx.editor.revert_selected());
    EXPECT_FLOAT_EQ(screen->tree_layout().node("/panel")->x, 100.0f);
}

TEST(TreeLayout, MissingNodeRejected) {
    EditorFixture fx;
    fx.install();
    EXPECT_FALSE(fx.editor.select_node("/nonexistent"));
}

TEST(TreeLayout, EditClampsLevelAndAlpha) {
    EditorFixture fx;
    auto*         screen = fx.install();
    fx.editor.select_node("/panel");
    TreeNode u;
    u.level = 999;
    u.alpha = 2.0f;
    fx.editor.update_selected_node(u);
    EXPECT_EQ(screen->tree_layout().node("/panel")->level, kMaxDrawLevel);
}

TEST(TreeLayout, TreeLayoutEditMarksSlotDirty) {
    EditorFixture fx;
    auto*         screen = fx.install();
    fx.editor.select_node("/panel");
    TreeNode u = *screen->tree_layout().node("/panel");
    u.x += 5.0f;
    fx.editor.update_selected_node(u);
    EXPECT_TRUE(fx.editor.is_dirty("/panel"));
    EXPECT_FALSE(fx.editor.is_dirty("/panel/child"));
}

TEST(TreeLayout, TreeLayoutEditMarksChildDirty) {
    EditorFixture fx;
    auto*         screen = fx.install();
    fx.editor.select_node("/panel/child");
    TreeNode u = *screen->tree_layout().node("/panel/child");
    u.y = -5.0f;
    fx.editor.update_selected_node(u);
    EXPECT_TRUE(fx.editor.is_dirty("/panel/child"));
    EXPECT_FALSE(fx.editor.is_dirty("/panel"));
}

TEST(TreeLayout, TreeLayoutEditRevertViaConfigBinding) {
    EditorFixture fx;
    auto*         screen = fx.install();
    fx.editor.select_node("/panel");
    TreeNode u = *screen->tree_layout().node("/panel");
    u.x += 50.0f;
    fx.editor.update_selected_node(u);
    EXPECT_TRUE(fx.editor.is_dirty());

    EXPECT_TRUE(fx.editor.revert_selected());
    EXPECT_FLOAT_EQ(screen->tree_layout().node("/panel")->x, 100.0f);
}

TEST(TreeLayout, TreeLayoutEditRevertAllRestoresAll) {
    EditorFixture fx;
    auto*         screen = fx.install();

    fx.editor.select_node("/panel");
    TreeNode u = *screen->tree_layout().node("/panel");
    u.x += 10.0f;
    fx.editor.update_selected_node(u);

    fx.editor.select_node("/panel/child");
    TreeNode v = *screen->tree_layout().node("/panel/child");
    v.y -= 20.0f;
    fx.editor.update_selected_node(v);

    EXPECT_TRUE(fx.editor.is_dirty("/panel"));
    EXPECT_TRUE(fx.editor.is_dirty("/panel/child"));

    fx.editor.revert_all();
    EXPECT_FALSE(fx.editor.is_dirty("/panel"));
    EXPECT_FALSE(fx.editor.is_dirty("/panel/child"));
    EXPECT_FLOAT_EQ(screen->tree_layout().node("/panel")->x, 100.0f);
    EXPECT_FLOAT_EQ(screen->tree_layout().node("/panel/child")->y, 20.0f);
}

TEST(TreeLayout, TreeLayoutEditSameValueClearsDirty) {
    EditorFixture fx;
    auto*         screen = fx.install();

    fx.editor.select_node("/panel");
    TreeNode u = *screen->tree_layout().node("/panel");
    u.x += 5.0f;
    fx.editor.update_selected_node(u);
    EXPECT_TRUE(fx.editor.is_dirty("/panel"));

    TreeNode back = *screen->tree_layout().node("/panel");
    back.x = 100.0f;
    fx.editor.update_selected_node(back);
    EXPECT_FALSE(fx.editor.is_dirty("/panel"));
}

TEST(TreeLayout, TreeLayoutDirtyAfterEditBackToOriginalClearsDirty) {
    EditorFixture fx;
    auto*         screen = fx.install();

    fx.editor.select_node("/panel");
    TreeNode u = *screen->tree_layout().node("/panel");
    u.x += 5.0f;
    fx.editor.update_selected_node(u);
    EXPECT_TRUE(fx.editor.is_dirty("/panel"));

    TreeNode v = *screen->tree_layout().node("/panel");
    v.x = 100.0f;
    fx.editor.update_selected_node(v);
    EXPECT_FALSE(fx.editor.is_dirty("/panel"));
}

TEST(TreeLayout, TreeLayoutEditDoesNotCreatePlacementEntries) {
    EditorFixture fx;
    auto*         screen = fx.install();
    fx.editor.select_node("/panel");
    TreeNode u = *screen->tree_layout().node("/panel");
    u.x += 5.0f;
    fx.editor.update_selected_node(u);
    EXPECT_TRUE(fx.editor.is_dirty());
}

// ── TreeLayoutEditor edit coverage (replacing BattleTuningController TreeLayout tests) ──

TEST(TreeLayout, WidthIncreaseChangesSelectedNodeW) {
    EditorFixture fx;
    auto*         screen = fx.install();
    fx.editor.select_node("/panel");
    float orig_w = screen->tree_layout().node("/panel")->w;

    fx.editor.apply_edit(DebugTuningEditAction{DebugTuningEditKind::WidthIncrease, 1.0f});

    EXPECT_FLOAT_EQ(screen->tree_layout().node("/panel")->w, orig_w + 1.0f);
}

TEST(TreeLayout, ScaleBothIncreaseChangesSelectedNodeWAndH) {
    EditorFixture fx;
    auto*         screen = fx.install();
    fx.editor.select_node("/panel");
    float orig_w = screen->tree_layout().node("/panel")->w;
    float orig_h = screen->tree_layout().node("/panel")->h;

    fx.editor.apply_edit(DebugTuningEditAction{DebugTuningEditKind::ScaleBothIncrease, 1.0f});

    EXPECT_FLOAT_EQ(screen->tree_layout().node("/panel")->w, orig_w + 1.0f);
    EXPECT_FLOAT_EQ(screen->tree_layout().node("/panel")->h, orig_h + 1.0f);
}

TEST(TreeLayout, LevelIncreaseAndDecreaseChangeSelectedNodeLevel) {
    EditorFixture fx;
    auto*         screen = fx.install();
    fx.editor.select_node("/panel");
    EXPECT_EQ(screen->tree_layout().node("/panel")->level, 5);

    fx.editor.apply_edit(DebugTuningEditAction{DebugTuningEditKind::LevelIncrease, 1.0f});
    EXPECT_EQ(screen->tree_layout().node("/panel")->level, 6);

    fx.editor.apply_edit(DebugTuningEditAction{DebugTuningEditKind::LevelDecrease, 1.0f});
    EXPECT_EQ(screen->tree_layout().node("/panel")->level, 5);
}

TEST(TreeLayout, LevelIsClampedToMinMax) {
    EditorFixture fx;
    auto*         screen = fx.install();
    fx.editor.select_node("/panel");

    // Increase way past max
    for (int i = 0; i < 20; ++i)
        fx.editor.apply_edit(DebugTuningEditAction{DebugTuningEditKind::LevelIncrease, 1.0f});
    EXPECT_EQ(screen->tree_layout().node("/panel")->level, kMaxDrawLevel);

    // Decrease way past min
    for (int i = 0; i < 30; ++i)
        fx.editor.apply_edit(DebugTuningEditAction{DebugTuningEditKind::LevelDecrease, 1.0f});
    EXPECT_EQ(screen->tree_layout().node("/panel")->level, kMinDrawLevel);
}

// ── selected_composed_rect() tests ───────────────────────────────────────

TEST(TreeLayout, SelectedComposedRectReturnsNulloptWhenNoNodeSelected) {
    EditorFixture fx;
    fx.install();
    EXPECT_FALSE(fx.editor.selected_composed_rect().has_value());
}

TEST(TreeLayout, SelectedComposedRectReturnsComposedPosition) {
    EditorFixture fx;
    static_cast<void>(fx.install());
    // /panel at (100, 200, 300, 400), /panel/child at (10, 20, 50, 60)
    // compose("/panel/child") = (110, 220, 50, 60)
    ASSERT_TRUE(fx.editor.select_node("/panel/child"));
    auto rect = fx.editor.selected_composed_rect();
    ASSERT_TRUE(rect.has_value());
    EXPECT_FLOAT_EQ(rect->x, 110.0f);
    EXPECT_FLOAT_EQ(rect->y, 220.0f);
    EXPECT_FLOAT_EQ(rect->w, 50.0f);
    EXPECT_FLOAT_EQ(rect->h, 60.0f);
}

TEST(TreeLayout, SelectedComposedRectUpdatesAfterEdit) {
    EditorFixture fx;
    auto*         screen = fx.install();
    ASSERT_TRUE(fx.editor.select_node("/panel/child"));

    // Edit x/y — composed rect must reflect the change immediately
    TreeNode u = *screen->tree_layout().node("/panel/child");
    u.x = 30.0f;
    u.y = 40.0f;
    ASSERT_TRUE(fx.editor.update_selected_node(u));

    auto rect = fx.editor.selected_composed_rect();
    ASSERT_TRUE(rect.has_value());
    EXPECT_FLOAT_EQ(rect->x, 130.0f); // parent.x(100) + child.x(30)
    EXPECT_FLOAT_EQ(rect->y, 240.0f); // parent.y(200) + child.y(40)
}

// ── Source-vs-runtime persistence test ─────────────────────────────────

TEST(TreeLayout, PersistenceChangesOnlySourceRoot) {
    namespace fs = std::filesystem;

    const auto src = fs::temp_directory_path() / "d2_persist_src";
    const auto rt = fs::temp_directory_path() / "d2_persist_rt";
    fs::remove_all(src);
    fs::remove_all(rt);
    fs::create_directories(src / "screens");
    fs::create_directories(rt / "screens");

    nlohmann::json doc;
    doc["render_tree"]["/test_node"] = {{"kind", "slot"}, {"x", 10}, {"y", 20},
                                        {"w", 30},        {"h", 40}, {"level", 1}};

    const auto write_json = [](const fs::path& p, const nlohmann::json& d) {
        std::ofstream out(p);
        out << d.dump(2) << '\n';
    };
    write_json(src / "screens/test_screen.json", doc);
    write_json(rt / "screens/test_screen.json", doc);

    ScreenConfigStore config_store(src);
    auto              config = config_store.load("test_screen");

    auto  screen = std::make_unique<TestEditorScreen>(std::move(config.tree_layout),
                                                      config.config_path.string());
    auto* screen_ptr = screen.get();

    ScreenManager    manager;
    TreeLayoutEditor editor(manager, config_store);
    manager.switch_to(std::move(screen));

    ASSERT_TRUE(editor.select_screen(screen_ptr->instance_id()));
    ASSERT_TRUE(editor.select_node("/test_node"));

    TreeNode updated = *screen_ptr->tree_layout().node("/test_node");
    updated.x = 99;
    ASSERT_TRUE(editor.update_selected_node(updated));
    ASSERT_TRUE(editor.save());

    const auto read_bytes = [](const fs::path& p) -> std::vector<uint8_t> {
        std::ifstream in(p, std::ios::binary);
        return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    };

    const auto src_bytes = read_bytes(src / "screens/test_screen.json");
    const auto rt_bytes = read_bytes(rt / "screens/test_screen.json");

    const std::string src_str(src_bytes.begin(), src_bytes.end());
    const std::string rt_str(rt_bytes.begin(), rt_bytes.end());

    EXPECT_NE(src_str.find("\"x\": 99"), std::string::npos)
        << "source root should contain edited x=99";
    EXPECT_NE(rt_str.find("\"x\": 10"), std::string::npos)
        << "runtime root should still contain x=10";
    EXPECT_EQ(rt_str.find("\"x\": 99"), std::string::npos)
        << "runtime root must NOT contain edited x=99";

    ASSERT_TRUE(editor.reload());
    const auto reloaded = screen_ptr->tree_layout().node("/test_node");
    ASSERT_TRUE(reloaded.has_value());
    EXPECT_FLOAT_EQ(reloaded->x, 99) << "reloaded Screen must read saved value";

    std::error_code ec;
    fs::remove_all(src, ec);
    fs::remove_all(rt, ec);
}

} // namespace d2engine
