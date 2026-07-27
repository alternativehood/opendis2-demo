#include <gtest/gtest.h>

#include <fstream>
#include <nlohmann/json.hpp>

#include "d2engine/app/battle_tuning_state.hpp"
#include "d2engine/render/render_tree.hpp"

#include "d2engine/app/debug_command_handler.hpp"
#include "d2engine/battle_view/debug_renderable_item.hpp"
#include "d2engine/battle_view/battle_slot.hpp"

namespace {
[[nodiscard]] d2engine::TreeLayout load_battle_tree_from_config() {
    const auto cfg =
        std::filesystem::path(OPENDIS2_SOURCE_DIR) / "configs/screens/battle_screen.json";
    std::ifstream        in(cfg);
    auto                 j = nlohmann::json::parse(in);
    d2engine::TreeLayout tree;
    tree.load(j["render_tree"]);
    return tree;
}
} // namespace

namespace d2engine {
namespace {

// Populate state with a realistic set of selectable tree-layout items
// for CENTER slots A_CENTER_0, A_CENTER_1 and their /unit children.
void add_center_items(BattleTuningState& state) {
    std::vector<DebugRenderableItem> items;

    struct Entry {
        std::string stable_id;
        std::string tree_path;
        std::string kind;
    };
    const Entry entries[] = {
        {"tree:A_CENTER_0", "/battlefield/slot_a_center_0", "battlefield_slot"},
        {"tree:A_CENTER_0/unit", "/battlefield/slot_a_center_0/unit", "unit_mount"},
        {"tree:A_CENTER_1", "/battlefield/slot_a_center_1", "battlefield_slot"},
        {"tree:A_CENTER_1/unit", "/battlefield/slot_a_center_1/unit", "unit_mount"},
    };
    for (const auto& e : entries) {
        items.push_back(DebugRenderableItem{
            .stable_id = e.stable_id,
            .label = "render_tree." + e.tree_path,
            .tree_path = e.tree_path,
            .kind = e.kind,
            .selectable = true,
            .binding = ConfigBinding{.config_file = kBattleScreenConfigPath,
                                     .owner_kind = BindingOwnerKind::TreeLayout,
                                     .tree_path = e.tree_path,
                                     .display_path = "render_tree" + e.tree_path},
        });
    }
    state.set_current_items(std::move(items));
}

} // namespace

TEST(DebugCommandHandler, RenderTreeNodesAreNotImplicitDebugItems) {
    BattleTuningState state;
    auto              layout_tree = load_battle_tree_from_config();
    layout_tree.set_node("/ui/left_unit_group", TreeNode{.kind = "ui", .x = 10.0f, .y = 20.0f});
    layout_tree.set_node("/ui/left_unit_group/0_front",
                         TreeNode{.kind = "unit_group_slot", .x = 30.0f, .y = 40.0f});
    layout_tree.set_node("/ui/left_unit_group/0_front/hp",
                         TreeNode{.kind = "text", .x = 0.0f, .y = 136.0f, .w = 113.0f, .h = 24.0f});
    state.set_current_items({});
    DebugCommandHandler handler{state};

    const auto tree = handler.execute("tree");
    EXPECT_NE(tree.output.find("debug items are not ready yet"), std::string::npos);

    const auto find = handler.execute("find 0_front/hp");
    EXPECT_NE(find.output.find("debug items are not ready yet"), std::string::npos);

    (void)handler.execute("select /ui/left_unit_group/0_front/hp");
    EXPECT_NE(state.selected_debug_id, "/ui/left_unit_group/0_front/hp");
}

TEST(DebugCommandHandler, SelectByAlias) {
    BattleTuningState state;
    add_center_items(state);
    DebugCommandHandler handler{state};

    const auto result = handler.execute("select A_CENTER_1");
    EXPECT_FALSE(result.request_quit);
    EXPECT_EQ(state.selected_debug_id, "tree:A_CENTER_1");
    EXPECT_NE(result.output.find("tree:A_CENTER_1"), std::string::npos);
}

TEST(DebugCommandHandler, SelectUnitAlias) {
    BattleTuningState state;
    add_center_items(state);
    DebugCommandHandler handler{state};

    const auto result = handler.execute("select A_CENTER_1/unit");
    EXPECT_EQ(state.selected_debug_id, "tree:A_CENTER_1/unit");
}

TEST(DebugCommandHandler, SelectByTreePath) {
    BattleTuningState state;
    add_center_items(state);
    DebugCommandHandler handler{state};

    const auto result = handler.execute("select /battlefield/slot_a_center_1/unit");
    EXPECT_EQ(state.selected_debug_id, "tree:A_CENTER_1/unit");
}

TEST(DebugCommandHandler, SelectByStableId) {
    BattleTuningState state;
    add_center_items(state);
    DebugCommandHandler handler{state};

    const auto result = handler.execute("select tree:A_CENTER_0");
    EXPECT_EQ(state.selected_debug_id, "tree:A_CENTER_0");
}

TEST(DebugCommandHandler, SelectWithMultipleSpaces) {
    BattleTuningState state;
    add_center_items(state);
    DebugCommandHandler handler{state};

    const auto result = handler.execute("select   A_CENTER_1");
    EXPECT_EQ(state.selected_debug_id, "tree:A_CENTER_1") << "Multiple spaces must be accepted";
}

TEST(DebugCommandHandler, SelectUnknownDoesNotChangeSelection) {
    BattleTuningState state;
    add_center_items(state);
    state.selected_debug_id = "tree:A_CENTER_0";
    DebugCommandHandler handler{state};

    const auto result = handler.execute("select NONEXISTENT_ITEM");
    EXPECT_EQ(state.selected_debug_id, "tree:A_CENTER_0")
        << "Unknown select must not change selection";
    EXPECT_NE(result.output.find("no selectable item"), std::string::npos);
}

TEST(DebugCommandHandler, TreeOutputIncludesLabel) {
    BattleTuningState state;
    add_center_items(state);
    DebugCommandHandler handler{state};

    const auto result = handler.execute("tree");
    EXPECT_NE(result.output.find("label="), std::string::npos) << "tree output must include label=";
    EXPECT_NE(result.output.find("render_tree."), std::string::npos);
}

TEST(DebugCommandHandler, FindCaseInsensitive) {
    BattleTuningState state;
    add_center_items(state);
    DebugCommandHandler handler{state};

    const auto result = handler.execute("find center");
    EXPECT_NE(result.output.find("A_CENTER"), std::string::npos);
}

TEST(DebugCommandHandler, FindWithMultipleSpaces) {
    BattleTuningState state;
    add_center_items(state);
    DebugCommandHandler handler{state};

    const auto result = handler.execute("find   center");
    EXPECT_NE(result.output.find("A_CENTER"), std::string::npos);
}

TEST(DebugCommandHandler, QuitSetsRequestQuit) {
    BattleTuningState   state;
    DebugCommandHandler handler{state};

    const auto result = handler.execute("quit");
    EXPECT_TRUE(result.request_quit);
}

TEST(DebugCommandHandler, HelpOutputIsNonEmpty) {
    BattleTuningState   state;
    DebugCommandHandler handler{state};

    const auto result = handler.execute("help");
    EXPECT_FALSE(result.output.empty());
    EXPECT_FALSE(result.request_quit);
}

TEST(DebugCommandHandler, EmptyLineIsNoOp) {
    BattleTuningState   state;
    DebugCommandHandler handler{state};

    const auto result = handler.execute("   ");
    EXPECT_TRUE(result.output.empty());
    EXPECT_FALSE(result.request_quit);
}

TEST(DebugCommandHandler, CaseInsensitiveCommand) {
    BattleTuningState   state;
    DebugCommandHandler handler{state};

    // "HELP" should work the same as "help"
    const auto result = handler.execute("HELP");
    EXPECT_FALSE(result.output.empty());
    EXPECT_NE(result.output.find("commands"), std::string::npos);
}

TEST(DebugCommandHandler, NotReadyWithoutItems) {
    BattleTuningState   state; // no items
    DebugCommandHandler handler{state};

    const auto result = handler.execute("tree");
    EXPECT_NE(result.output.find("not ready"), std::string::npos);
}

// ─── Tree ordering by tree path ───────────────────────────────────────────

TEST(DebugCommandHandler, TreeOutputOrderedByTreePath) {
    BattleTuningState state;
    // Add items in intentionally shuffled order
    std::vector<DebugRenderableItem> items;
    items.push_back(DebugRenderableItem{
        .stable_id = "backgrounds.battle",
        .tree_path = "/background/battle",
        .kind = "background_battle",
        .selectable = true,
        .binding = ConfigBinding{.config_file = kBattleScreenConfigPath,
                                 .owner_kind = BindingOwnerKind::TreeLayout,
                                 .tree_path = "/background/battle",
                                 .role = BindingRole::Global,
                                 .display_path = "render_tree/background/battle"},
    });
    items.push_back(DebugRenderableItem{
        .stable_id = "backgrounds.ground",
        .tree_path = "/background/ground",
        .kind = "background_ground",
        .selectable = true,
        .binding = ConfigBinding{.config_file = kBattleScreenConfigPath,
                                 .owner_kind = BindingOwnerKind::TreeLayout,
                                 .tree_path = "/background/ground",
                                 .role = BindingRole::Global,
                                 .display_path = "render_tree/background/ground"},
    });
    items.push_back(DebugRenderableItem{
        .stable_id = "sprite_profiles.X",
        .tree_path = "",
        .kind = "sprite",
        .selectable = true,
        .binding = ConfigBinding{.config_file = kBattleScreenConfigPath,
                                 .owner_kind = BindingOwnerKind::SpriteProfile,
                                 .tree_path = "X",
                                 .role = BindingRole::Global,
                                 .display_path = "sprite_profiles.X"},
    });
    items.push_back(DebugRenderableItem{
        .stable_id = "sprite_profiles.A",
        .tree_path = "",
        .kind = "sprite",
        .selectable = true,
        .binding = ConfigBinding{.config_file = kBattleScreenConfigPath,
                                 .owner_kind = BindingOwnerKind::SpriteProfile,
                                 .tree_path = "A",
                                 .role = BindingRole::Global,
                                 .display_path = "sprite_profiles.A"},
    });
    // Item with no binding — falls back to tree_path, then stable_id
    items.push_back(DebugRenderableItem{
        .stable_id = "tree:D_BACK_0",
        .tree_path = "/battlefield/slot_d_back_0",
        .kind = "battlefield_slot",
        .selectable = true,
    });
    state.set_current_items(std::move(items));
    DebugCommandHandler handler{state};

    const auto result = handler.execute("tree");

    const auto bg_battle_pos = result.output.find("/background/battle");
    const auto bg_ground_pos = result.output.find("/background/ground");
    const auto slot_pos = result.output.find("/battlefield/slot_d_back_0");
    const auto sp_a_pos = result.output.find("sprite_profiles.A");
    const auto sp_x_pos = result.output.find("sprite_profiles.X");

    EXPECT_LT(bg_battle_pos, bg_ground_pos);
    EXPECT_LT(bg_ground_pos, slot_pos);
    EXPECT_LT(slot_pos, sp_a_pos);
    EXPECT_LT(sp_a_pos, sp_x_pos) << "sprite_profiles.A must sort before sprite_profiles.X";
}

TEST(DebugCommandHandler, TreeOutputPutsParentBeforeChild) {
    BattleTuningState state;
    state.set_current_items({
        DebugRenderableItem{.stable_id = "tree:unit",
                            .tree_path = "/battlefield/slot_a_back_0/unit",
                            .kind = "unit_mount",
                            .selectable = true},
        DebugRenderableItem{.stable_id = "backgrounds.ground",
                            .tree_path = "/background/ground",
                            .kind = "background_ground",
                            .selectable = true},
        DebugRenderableItem{.stable_id = "tree:slot",
                            .tree_path = "/battlefield/slot_a_back_0",
                            .kind = "battlefield_slot",
                            .selectable = true},
        DebugRenderableItem{.stable_id = "tree:background",
                            .tree_path = "/background",
                            .kind = "background_container",
                            .selectable = true},
    });
    DebugCommandHandler handler{state};

    const auto result = handler.execute("tree");

    EXPECT_LT(result.output.find("/background"), result.output.find("/background/ground"));
    EXPECT_LT(result.output.find("tree:slot"), result.output.find("tree:unit"));
}

TEST(DebugCommandHandler, TreeOutputFallsBackToStableIdWhenNoTreePathOrDisplayPath) {
    BattleTuningState                state;
    std::vector<DebugRenderableItem> items;
    // No binding, no tree_path → fallback to stable_id
    items.push_back(DebugRenderableItem{
        .stable_id = "zzz_no_path",
        .kind = "readonly",
        .selectable = true,
    });
    items.push_back(DebugRenderableItem{
        .stable_id = "aaa_no_path",
        .kind = "readonly",
        .selectable = true,
    });
    state.set_current_items(std::move(items));
    DebugCommandHandler handler{state};

    const auto result = handler.execute("tree");
    const auto a_pos = result.output.find("aaa_no_path");
    const auto z_pos = result.output.find("zzz_no_path");
    EXPECT_LT(a_pos, z_pos) << "aaa_no_path must sort before zzz_no_path by stable_id";
}

// ─── Background selection via select and find ─────────────────────────────

TEST(DebugCommandHandler, SelectBackgroundByExactStableId) {
    BattleTuningState                state;
    std::vector<DebugRenderableItem> items;
    items.push_back(DebugRenderableItem{
        .stable_id = "backgrounds.battle",
        .tree_path = "/background/battle",
        .kind = "background_battle",
        .selectable = true,
        .binding = ConfigBinding{.config_file = kBattleScreenConfigPath,
                                 .owner_kind = BindingOwnerKind::TreeLayout,
                                 .tree_path = "/background/battle",
                                 .display_path = "render_tree/background/battle"},
    });
    state.set_current_items(std::move(items));
    DebugCommandHandler handler{state};

    const auto result = handler.execute("select backgrounds.battle");
    EXPECT_EQ(state.selected_debug_id, "backgrounds.battle");
}

TEST(DebugCommandHandler, SelectBackgroundByTreePath) {
    BattleTuningState                state;
    std::vector<DebugRenderableItem> items;
    items.push_back(DebugRenderableItem{
        .stable_id = "backgrounds.ground",
        .tree_path = "/background/ground",
        .kind = "background_ground",
        .selectable = true,
        .binding = ConfigBinding{.config_file = kBattleScreenConfigPath,
                                 .owner_kind = BindingOwnerKind::TreeLayout,
                                 .tree_path = "/background/ground",
                                 .display_path = "render_tree/background/ground"},
    });
    state.set_current_items(std::move(items));
    DebugCommandHandler handler{state};

    const auto result = handler.execute("select /background/ground");
    EXPECT_EQ(state.selected_debug_id, "backgrounds.ground");
}

TEST(DebugCommandHandler, SelectBackgroundByBattleTreePath) {
    BattleTuningState                state;
    std::vector<DebugRenderableItem> items;
    items.push_back(DebugRenderableItem{
        .stable_id = "backgrounds.battle",
        .tree_path = "/background/battle",
        .kind = "background_battle",
        .selectable = true,
        .binding = ConfigBinding{.config_file = kBattleScreenConfigPath,
                                 .owner_kind = BindingOwnerKind::TreeLayout,
                                 .tree_path = "/background/battle",
                                 .display_path = "render_tree/background/battle"},
    });
    state.set_current_items(std::move(items));
    DebugCommandHandler handler{state};

    const auto result = handler.execute("select /background/battle");
    EXPECT_EQ(state.selected_debug_id, "backgrounds.battle");
}

TEST(DebugCommandHandler, FindShowsBackgroundItems) {
    BattleTuningState                state;
    std::vector<DebugRenderableItem> items;
    items.push_back(DebugRenderableItem{
        .stable_id = "backgrounds.ground",
        .tree_path = "/background/ground",
        .kind = "background_ground",
        .selectable = true,
        .binding = ConfigBinding{.config_file = kBattleScreenConfigPath,
                                 .owner_kind = BindingOwnerKind::TreeLayout,
                                 .tree_path = "/background/ground",
                                 .display_path = "render_tree/background/ground"},
    });
    items.push_back(DebugRenderableItem{
        .stable_id = "backgrounds.battle",
        .tree_path = "/background/battle",
        .kind = "background_battle",
        .selectable = true,
        .binding = ConfigBinding{.config_file = kBattleScreenConfigPath,
                                 .owner_kind = BindingOwnerKind::TreeLayout,
                                 .tree_path = "/background/battle",
                                 .display_path = "render_tree/background/battle"},
    });
    items.push_back(DebugRenderableItem{
        .stable_id = "tree:A_CENTER_0",
        .tree_path = "/battlefield/slot_a_center_0",
        .kind = "battlefield_slot",
        .selectable = true,
    });
    state.set_current_items(std::move(items));
    DebugCommandHandler handler{state};

    const auto result = handler.execute("find background");
    EXPECT_NE(result.output.find("/background/ground"), std::string::npos);
    EXPECT_NE(result.output.find("/background/battle"), std::string::npos);
    // Should NOT match tree:A_CENTER_0
    EXPECT_EQ(result.output.find("A_CENTER_0"), std::string::npos);
}

// ─── Regression: center slot and sprite profile selection still works ─────

TEST(DebugCommandHandler, CenterSlotSelectionStillWorksWithBackgrounds) {
    BattleTuningState                state;
    std::vector<DebugRenderableItem> items;
    // Background item (new)
    items.push_back(DebugRenderableItem{
        .stable_id = "backgrounds.battle",
        .kind = "background_battle",
        .selectable = true,
    });
    // Center slot item (existing)
    items.push_back(DebugRenderableItem{
        .stable_id = "tree:A_CENTER_1",
        .tree_path = "/battlefield/slot_a_center_1",
        .kind = "battlefield_slot",
        .selectable = true,
        .binding = ConfigBinding{.display_path = "render_tree/battlefield/slot_a_center_1"},
    });
    // Center /unit mount item (existing)
    items.push_back(DebugRenderableItem{
        .stable_id = "tree:A_CENTER_1/unit",
        .tree_path = "/battlefield/slot_a_center_1/unit",
        .kind = "unit_mount",
        .selectable = true,
    });
    state.set_current_items(std::move(items));
    DebugCommandHandler handler{state};

    auto r1 = handler.execute("select A_CENTER_1");
    EXPECT_EQ(state.selected_debug_id, "tree:A_CENTER_1");

    auto r2 = handler.execute("select A_CENTER_1/unit");
    EXPECT_EQ(state.selected_debug_id, "tree:A_CENTER_1/unit");
}

} // namespace d2engine
