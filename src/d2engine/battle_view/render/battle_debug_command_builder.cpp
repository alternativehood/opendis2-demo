#include "battle_debug_command_builder.hpp"

#include "../../app/battle_tuning_state.hpp"
#include "battle_render_tree_helpers.hpp"
#include "../battle_render_batch_helpers.hpp"
#include "../battle_render_tree_contract.hpp"
#include "../battle_slot.hpp"

#include <string>

namespace d2engine {

void append_tree_layout_items(RenderBatch& batch, const BattleRenderOptions& options) {
    if (options.tree_layout == nullptr) {
        return;
    }
    const auto& tree = *options.tree_layout;
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
                if (!tree.has_node(slot_path)) {
                    continue;
                }
                const Rect        slot_rect = tree.compose(slot_path);
                const std::string coord_label = slot_coord_to_string(coord);
                const std::string side_str = (side == 'a') ? "a" : "d";
                append_tunable_command(
                    batch,
                    {.layer = battle_render_layer(BattleRenderPass::Debug), .tree_path = slot_path},
                    DebugRenderableItem{
                        .stable_id = std::string("tree:") + coord_label,
                        .label = std::string("render_tree.") + slot_path,
                        .tree_path = slot_path,
                        .bounds = slot_rect,
                        .kind = "battlefield_slot",
                        .layer = static_cast<int>(TrackRenderLayer::Debug),
                        .logical_rect = slot_rect,
                        .screen_rect = slot_rect,
                        .anchor = {.x = slot_rect.x, .y = slot_rect.y},
                        .selectable = true,
                        .visible = true,
                        .binding = to_tuning_binding(
                            ConfigBinding{.config_file = kBattleScreenConfigPath,
                                          .owner_kind = BindingOwnerKind::TreeLayout,
                                          .tree_path = slot_path,
                                          .role = BindingRole::Global,
                                          .side = side_str,
                                          .display_path = std::string("render_tree") + slot_path}),
                        .render_side = side_str});

                if (!tree.has_node(unit_path)) {
                    continue;
                }
                const Rect unit_rect = tree.compose(unit_path);
                append_tunable_command(
                    batch,
                    {.layer = battle_render_layer(BattleRenderPass::Debug), .tree_path = unit_path},
                    DebugRenderableItem{
                        .stable_id = std::string("tree:") + coord_label + "/unit",
                        .label = std::string("render_tree.") + unit_path,
                        .tree_path = unit_path,
                        .bounds = unit_rect,
                        .kind = "unit_mount",
                        .layer = static_cast<int>(TrackRenderLayer::Debug),
                        .logical_rect = unit_rect,
                        .screen_rect = unit_rect,
                        .anchor = {.x = unit_rect.x, .y = unit_rect.y},
                        .selectable = true,
                        .visible = true,
                        .binding = to_tuning_binding(
                            ConfigBinding{.config_file = kBattleScreenConfigPath,
                                          .owner_kind = BindingOwnerKind::TreeLayout,
                                          .tree_path = unit_path,
                                          .role = BindingRole::Global,
                                          .side = side_str,
                                          .display_path = std::string("render_tree") + unit_path}),
                        .render_side = side_str});
            }
        }
    }
}

void append_background_container(RenderBatch& batch, const BattleRenderOptions& options) {
    if (options.terrain_image.empty()) {
        return;
    }
    // Read container offset from render_tree /background node; rects are for debug display.
    Rect cr = {};
    if (options.tree_layout != nullptr) {
        cr = options.tree_layout->compose(kBackgroundRootPath);
    }
    append_tunable_command(
        batch,
        {.layer = battle_render_layer(BattleRenderPass::Debug), .tree_path = kBackgroundRootPath},
        DebugRenderableItem{
            .stable_id = "scene:background_container",
            .label = "scene:background_container",
            .tree_path = kBackgroundRootPath,
            .bounds = cr,
            .kind = "background_container",
            .layer = static_cast<int>(TrackRenderLayer::Debug),
            .logical_rect = cr,
            .screen_rect = cr,
            .selectable = true,
            .visible = true,
            .binding = to_tuning_binding(ConfigBinding{.config_file = kBattleScreenConfigPath,
                                                       .owner_kind = BindingOwnerKind::TreeLayout,
                                                       .tree_path = kBackgroundRootPath,
                                                       .role = BindingRole::Global,
                                                       .display_path = "render_tree/background"})});
}

} // namespace d2engine
