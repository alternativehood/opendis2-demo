#include "battle_background_command_builder.hpp"

#include "../../app/battle_tuning_state.hpp"
#include "battle_render_tree_helpers.hpp"
#include "../battle_render_batch_helpers.hpp"

#include <d2log/log.hpp>

#include <string>

namespace d2engine {
namespace {

auto kLog = d2log::get("d2.render"); // NOLINT(cert-err58-cpp)

[[nodiscard]] ComposedTransform compose_background_placement(const BattleRenderOptions& options,
                                                             const std::string&         tree_path) {
    if (options.tree_layout == nullptr) {
        return {};
    }
    const auto parent = options.tree_layout->node(kBackgroundRootPath);
    const auto child = options.tree_layout->node(tree_path);
    if (!parent.has_value() && !child.has_value()) {
        return {};
    }
    const TreeNode p = parent.value_or(TreeNode{});
    const TreeNode c = child.value_or(TreeNode{});
    return {.x = p.x + c.x,
            .y = p.y + c.y,
            .w = c.w > 0.0f ? c.w : p.w,
            .h = c.h > 0.0f ? c.h : p.h,
            .alpha = p.alpha * c.alpha};
}

} // namespace

void append_background_layer(RenderBatch& batch, IBattleTextureProvider& textures,
                             const BattleRenderOptions& options, BattleRenderPass pass,
                             const std::string& container, const std::string& image,
                             const std::string& tree_path, const std::string& stable_id,
                             const char* kind, bool selectable) {
    BackendTextureRef const texture = textures.get_texture(container, image);
    if (!texture.present()) {
        return;
    }
    const auto [texture_w, texture_h] = textures.texture_size(texture);

    const ComposedTransform ct = compose_background_placement(options, tree_path);

    const float ref_w = options.layout_metrics.ref_w;
    const float ref_h = options.layout_metrics.ref_h;
    const float out_w = (ct.w > 0.0f) ? ct.w : ref_w;
    const float out_h = (ct.h > 0.0f) ? ct.h : ref_h;
    const Rect  dest{.x = ct.x, .y = ct.y, .w = out_w, .h = out_h};

    append_tunable_command(
        batch,
        {.layer = battle_render_layer(pass),
         .texture = texture,
         .destination = dest,
         .source_rect = {.w = texture_w, .h = texture_h},
         .has_source_rect = true,
         .alpha = ct.alpha,
         .tile = pass == BattleRenderPass::GroundBackground,
         .tree_path = tree_path},
        DebugRenderableItem{.stable_id = stable_id,
                            .label = stable_id,
                            .tree_path = tree_path,
                            .bounds = dest,
                            .kind = kind,
                            .layer = static_cast<int>(TrackRenderLayer::Background),
                            .resource_key = container + "/" + image,
                            .current_frame_name = image,
                            .logical_rect = dest,
                            .screen_rect = dest,
                            .selectable = selectable,
                            .visible = true,
                            .binding = to_tuning_binding(ConfigBinding{
                                .config_file = kBattleScreenConfigPath,
                                .owner_kind = BindingOwnerKind::TreeLayout,
                                .tree_path = tree_path,
                                .role = BindingRole::Global,
                                .display_path = std::string("render_tree") + tree_path})});
}

void append_ground_background(RenderBatch& batch, IBattleTextureProvider& textures,
                              const BattleRenderOptions& options) {
    if (!options.draw_background || options.terrain_image.empty()) {
        return;
    }

    if (options.debug_enabled) {
        const float full_w = options.layout_metrics.ref_w;
        const float full_h = options.layout_metrics.ref_h;
        batch.commands.push_back({.layer = battle_render_layer(BattleRenderPass::GroundBackground),
                                  .destination = {.x = 0.0f, .y = 0.0f, .w = full_w, .h = full_h},
                                  .fill_color = Color{.r = 255, .g = 0, .b = 255, .a = 255}});
    }

    if (options.ground_image.empty()) {
        kLog->error("missing ground background image: {} / {}", options.ground_container,
                    options.ground_image);
        return;
    }

    const std::size_t before = batch.commands.size();
    append_background_layer(batch, textures, options, BattleRenderPass::GroundBackground,
                            options.ground_container, options.ground_image, kGroundBackgroundPath,
                            "backgrounds.ground", "background_ground", true);
    if (batch.commands.size() == before) {
        kLog->error("missing ground background image: {} / {}", options.ground_container,
                    options.ground_image);
    }
}

void append_background(RenderBatch& batch, IBattleTextureProvider& textures,
                       const BattleRenderOptions& options) {
    if (!options.draw_background || options.terrain_image.empty()) {
        return;
    }

    append_background_layer(batch, textures, options, BattleRenderPass::Background,
                            options.terrain_container, options.terrain_image, kBattleBackgroundPath,
                            "backgrounds.battle", "background_battle", true);
}

void append_background_overlays(RenderBatch& batch, IBattleTextureProvider& textures,
                                const BattleRenderOptions& options) {
    if (!options.draw_background || options.terrain_overlay_images.empty()) {
        return;
    }
    for (const std::string& overlay_name : options.terrain_overlay_images) {
        append_background_layer(batch, textures, options, BattleRenderPass::BackgroundOverlay,
                                options.terrain_container, overlay_name, kBattleBackgroundPath,
                                "backgrounds.battle_overlay", "background_battle", false);
    }
}

} // namespace d2engine
