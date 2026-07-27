#include "battle_ui_command_builder.hpp"

#include "../../app/battle_tuning_state.hpp"
#include "battle_render_binding_helpers.hpp"
#include "battle_render_level_resolver.hpp"
#include "battle_render_tree_helpers.hpp"
#include "../battle_render_batch_helpers.hpp"
#include "../portrait_render_item.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace d2engine {
namespace {

void append_ui_panel(RenderBatch& batch, IBattleTextureProvider& textures,
                     const BattleRenderOptions& options, const std::string& asset_name,
                     const std::string& stable_id, const std::string& tree_path) {
    if (options.tree_layout == nullptr) {
        return;
    }
    BackendTextureRef const texture = textures.get_texture(options.frame_container, asset_name);
    if (!texture.present()) {
        return;
    }
    const Rect dest = options.tree_layout->compose(tree_path);
    append_tunable_command(
        batch,
        {.layer = battle_render_layer(BattleRenderPass::CombatFrame),
         .texture = texture,
         .destination = dest,
         .tree_path = tree_path},
        DebugRenderableItem{.stable_id = stable_id,
                            .label = stable_id,
                            .tree_path = tree_path,
                            .bounds = dest,
                            .kind = "ui",
                            .resource_key = options.frame_container + "/" + asset_name,
                            .current_frame_name = asset_name,
                            .logical_rect = dest,
                            .screen_rect = dest,
                            .anchor = {.x = dest.x, .y = dest.y},
                            .selectable = true,
                            .visible = true,
                            .binding = to_tuning_binding(ConfigBinding{
                                .config_file = kBattleScreenConfigPath,
                                .owner_kind = BindingOwnerKind::TreeLayout,
                                .tree_path = tree_path,
                                .role = BindingRole::CombatFrame,
                                .display_path = std::string("render_tree") + tree_path})});
}
void append_unitgroup_hud(RenderBatch& batch, const BattleRenderSnapshot& snapshot,
                          IBattleTextureProvider& textures, const BattleRenderOptions& options) {
    if (options.tree_layout == nullptr) {
        return;
    }
    for (const auto& entity : snapshot.entities) {
        if (entity.max_hp <= 0) {
            continue;
        }
        const std::string slot_path = unit_group_slot_path(entity.coord);
        const std::string portrait_path = slot_path + "/portrait";
        const std::string hp_path = slot_path + "/hp";
        if (!options.tree_layout->has_node(portrait_path) ||
            !options.tree_layout->has_node(hp_path)) {
            continue;
        }
        // Center slot HP background texture (drawn before HP text, underneath it)
        if (entity.coord.depth == BattleDepth::Center) {
            const std::string bg_path = slot_path + "/hp_background";
            const auto        bg_node = options.tree_layout->node(bg_path);
            if (bg_node.has_value() && bg_node->asset.has_value()) {
                BackendTextureRef const bg_texture =
                    textures.get_texture(options.frame_container, *bg_node->asset);
                if (bg_texture.present()) {
                    const Rect bg_rect = options.tree_layout->compose(bg_path);
                    append_tunable_command(
                        batch,
                        {.layer = battle_render_layer(BattleRenderPass::CombatFrame),
                         .texture = bg_texture,
                         .destination = bg_rect,
                         .tree_path = bg_path},
                        make_tree_command_tunable_item(std::string("ui:") + bg_path, bg_path,
                                                       "ui_texture", bg_rect,
                                                       BindingRole::CombatFrame));
                }
            }
        }
        const Rect    hp_rect = options.tree_layout->compose(hp_path);
        RenderCommand hp_command{.layer = battle_render_layer(BattleRenderPass::CombatFrame),
                                 .destination = hp_rect,
                                 .tree_path = hp_path,
                                 .text = hp_text(entity.current_hp, entity.max_hp)};
        apply_text_style(hp_command, *options.tree_layout, hp_path, options.font_face);
        append_tunable_command(batch, std::move(hp_command),
                               make_tree_command_tunable_item(std::string("ui:") + hp_path, hp_path,
                                                              "ui_text", hp_rect,
                                                              BindingRole::CombatFrame));

        const float ratio = missing_hp_ratio(entity.current_hp, entity.max_hp);
        if (entity.current_hp <= 0 || ratio <= 0.0f) {
            continue;
        }
        const Rect portrait_rect =
            portrait_damage_basis(entity, *options.tree_layout, portrait_path);
        const float fill_h = portrait_rect.h * ratio;
        batch.commands.push_back({.layer = battle_render_layer(BattleRenderPass::CombatFrame),
                                  .destination = {.x = portrait_rect.x,
                                                  .y = portrait_rect.y + portrait_rect.h - fill_h,
                                                  .w = portrait_rect.w,
                                                  .h = fill_h},
                                  .tree_path = portrait_path,
                                  .fill_color = Color{.r = 180, .g = 0, .b = 0, .a = 150}});
    }
}

} // namespace

void append_battle_frame_info(RenderBatch& batch, const BattleRenderSnapshot& snapshot,
                              const BattleRenderOptions& options) {
    if (options.tree_layout == nullptr) {
        return;
    }
    const auto append_side = [&](UnitInstanceId id, const std::string& base_path) {
        const SnapshotEntity* entity = find_entity_by_unit(snapshot, id);
        if (entity == nullptr || entity->max_hp <= 0) {
            return;
        }
        const std::string name_path = base_path + "/name";
        const std::string hp_path = base_path + "/hp";
        if (!options.tree_layout->has_node(name_path) || !options.tree_layout->has_node(hp_path)) {
            return;
        }
        const Rect    name_rect = options.tree_layout->compose(name_path);
        const Rect    hp_rect = options.tree_layout->compose(hp_path);
        RenderCommand name_command{.layer = battle_render_layer(BattleRenderPass::CombatFrame),
                                   .destination = name_rect,
                                   .tree_path = name_path,
                                   .text = entity->display_name.empty() ? entity->unit_type
                                                                        : entity->display_name};
        apply_text_style(name_command, *options.tree_layout, name_path, options.font_face);
        append_tunable_command(batch, std::move(name_command),
                               make_tree_command_tunable_item(std::string("ui:") + name_path,
                                                              name_path, "ui_name", name_rect,
                                                              BindingRole::CombatFrame));

        RenderCommand hp_command{.layer = battle_render_layer(BattleRenderPass::CombatFrame),
                                 .destination = hp_rect,
                                 .tree_path = hp_path,
                                 .text = hp_text(entity->current_hp, entity->max_hp)};
        apply_text_style(hp_command, *options.tree_layout, hp_path, options.font_face);
        append_tunable_command(batch, std::move(hp_command),
                               make_tree_command_tunable_item(std::string("ui:") + hp_path, hp_path,
                                                              "ui_hp", hp_rect,
                                                              BindingRole::CombatFrame));
    };
    append_side(options.info_left_unit, "/ui/combat_frame/info_left");
    append_side(options.info_right_unit, "/ui/combat_frame/info_right");
}

namespace {

void append_portrait_command(RenderBatch& batch, const PortraitRenderItem& item,
                             bool fill_placeholder) {
    RenderCommand command{
        .layer = battle_render_layer(BattleRenderPass::CombatFrame),
        .destination = item.dest_rect,
        .alpha = item.visual_category.find("DeadMask") != std::string::npos ? 0.7f : 1.0f,
        .flip_x = item.flip_x,
        .flip_y = item.flip_y,
        .tree_path = item.tree_path};
    auto tunable_item = item.to_tunable_item();
    if (item.has_texture) {
        command.texture = BackendTextureRef{.native = item.texture};
    } else if (fill_placeholder) {
        command.fill_color = Color{.r = 60, .g = 60, .b = 60, .a = 255};
    }
    append_tunable_command(batch, std::move(command), std::move(tunable_item));
}

void append_unit_group_portraits(RenderBatch& batch, const BattleRenderSnapshot& snapshot,
                                 const BattleRenderOptions& options) {
    if (options.tree_layout == nullptr) {
        return;
    }
    for (const auto& entity : snapshot.entities) {
        const bool        is_left = entity.coord.side == BattleSide::Attacker;
        const std::string side = is_left ? "a" : "d";
        const bool        flip_x = !is_left;
        const std::string slot_path = unit_group_slot_path(entity.coord);
        const std::string portrait_path = slot_path + "/portrait";
        if (!options.tree_layout->has_node(portrait_path)) {
            continue;
        }
        Rect portrait_rect = options.tree_layout->compose(portrait_path);
        if (entity.is_large && entity.coord.depth == BattleDepth::Center) {
            portrait_rect = portrait_damage_basis(entity, *options.tree_layout, portrait_path);
        }

        PortraitRenderItem item;
        if (options.portrait_index != nullptr && options.texture_cache != nullptr) {
            auto resolved = build_portrait_render_item(
                entity.unit_type, *options.portrait_index, *options.texture_cache, portrait_path,
                portrait_rect, flip_x, side, PortraitTextureKind::Face);
            item =
                resolved.has_value()
                    ? *resolved
                    : build_portrait_placeholder_item(portrait_path, portrait_rect, flip_x, side);
        } else {
            item = build_portrait_placeholder_item(portrait_path, portrait_rect, flip_x, side);
        }
        append_portrait_command(batch, item, true);

        const bool is_dead = entity.life_state == LifeVisualState::Dead ||
                             entity.life_state == LifeVisualState::FadingOut;
        if (!is_dead) {
            continue;
        }
        const std::string  mask_path = slot_path + "/dead_mask";
        PortraitRenderItem mask =
            options.texture_cache != nullptr
                ? build_dead_mask_item(entity.is_large, *options.texture_cache, mask_path,
                                       portrait_rect, flip_x, side)
                : build_portrait_placeholder_item(mask_path, portrait_rect, flip_x, side);
        mask.layer = 101;
        mask.visual_category = entity.is_large ? "PortraitDeadMaskLarge" : "PortraitDeadMaskSmall";
        append_portrait_command(batch, mask, false);
    }
}

} // namespace

void append_bottom_portraits(RenderBatch& batch, const BattleRenderSnapshot& snapshot,
                             const BattleRenderOptions& options) {
    if (options.tree_layout == nullptr) {
        return;
    }
    const auto append_side = [&](UnitInstanceId id, const std::string& path, bool flip_x,
                                 const std::string& side) {
        const SnapshotEntity* entity = find_entity_by_unit(snapshot, id);
        if (entity == nullptr || !options.tree_layout->has_node(path)) {
            return;
        }
        const Rect         rect = options.tree_layout->compose(path);
        PortraitRenderItem item;
        if (options.portrait_index != nullptr && options.texture_cache != nullptr) {
            auto resolved = build_portrait_render_item(entity->unit_type, *options.portrait_index,
                                                       *options.texture_cache, path, rect, flip_x,
                                                       side, PortraitTextureKind::FaceB);
            item = resolved.has_value() ? *resolved
                                        : build_portrait_placeholder_item(path, rect, flip_x, side);
        } else {
            item = build_portrait_placeholder_item(path, rect, flip_x, side);
        }
        append_portrait_command(batch, item, true);
    };
    append_side(options.info_left_unit, "/ui/combat_frame/left_slot/portrait", false, "a");
    append_side(options.info_right_unit, "/ui/combat_frame/right_slot/portrait", true, "d");
}

void append_combat_frame(RenderBatch& batch, IBattleTextureProvider& textures,
                         const BattleRenderOptions& options) {
    if (!options.draw_frame || options.tree_layout == nullptr) {
        return;
    }
    BackendTextureRef const texture =
        textures.get_texture(options.frame_container, options.frame_image);
    if (!texture.present()) {
        return;
    }
    const Rect             destination = options.tree_layout->compose("/ui/combat_frame");
    const TrackRenderLayer layer = leveled_layer(TrackRenderLayer::Frame, options.ui_level);
    append_tunable_command(
        batch,
        {.layer = battle_render_layer(draw_pass_for(layer)),
         .texture = texture,
         .destination = destination,
         .tree_path = "/ui/combat_frame"},
        DebugRenderableItem{.stable_id = "ui:combat_frame",
                            .label = "ui:combat_frame",
                            .tree_path = "/ui/combat_frame",
                            .bounds = destination,
                            .kind = "ui",
                            .layer = static_cast<int>(layer),
                            .resource_key = options.frame_container + "/" + options.frame_image,
                            .current_frame_name = options.frame_image,
                            .logical_rect = destination,
                            .screen_rect = destination,
                            .anchor = {.x = destination.x, .y = destination.y},
                            .selectable = true,
                            .visible = true,
                            .binding = to_tuning_binding(
                                ConfigBinding{.config_file = kBattleScreenConfigPath,
                                              .owner_kind = BindingOwnerKind::TreeLayout,
                                              .tree_path = "/ui/combat_frame",
                                              .role = BindingRole::CombatFrame,
                                              .display_path = "render_tree/ui/combat_frame"})});
}

void append_unit_groups(RenderBatch& batch, const BattleRenderSnapshot& snapshot,
                        IBattleTextureProvider& textures, const BattleRenderOptions& options) {
    if (!options.draw_unit_groups || options.tree_layout == nullptr) {
        return;
    }
    append_ui_panel(batch, textures, options, options.left_unit_group_image, "ui:left_unit_group",
                    "/ui/left_unit_group");
    append_ui_panel(batch, textures, options, options.right_unit_group_image, "ui:right_unit_group",
                    "/ui/right_unit_group");
    append_unit_group_portraits(batch, snapshot, options);
    append_unitgroup_hud(batch, snapshot, textures, options);
}

} // namespace d2engine
