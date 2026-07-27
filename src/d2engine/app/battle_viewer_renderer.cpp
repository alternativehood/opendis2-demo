#include "battle_viewer_renderer.hpp"

#include "../battle_adapters/sdl_battle_renderer.hpp"
#include "../battle_view/battle_texture_provider.hpp"
#include "../render/renderer2d.hpp"

#include <utility>

namespace d2engine {
namespace {

[[nodiscard]] DebugRenderableItem screen_scaled(DebugRenderableItem item, LayoutScale scale) {
    item.screen_rect = scale_rect(item.logical_rect, scale);
    item.anchor = {.x = item.anchor.x * scale.sx, .y = item.anchor.y * scale.sy};
    return item;
}

} // namespace

BattleRenderOptions BattleViewerRenderer::make_options(
    const BattleScenePresentationState& presentation, const BattleTuningState& tuning,
    const TreeLayout& tree_layout, std::string terrain_image,
    std::vector<std::string> overlay_images, bool transparent_background, bool debug_enabled,
    std::string_view solo_filter, const PortraitManifestIndex* portrait_index,
    GameTextureCache* texture_cache) {
    // Unit scale lives in scene_layout.unit — separate from effect overlay offset.
    const auto                 unit_it = tuning.placements.find("SceneLayout:scene:Unit");
    const VisualPlacementValue unit_val =
        unit_it != tuning.placements.end() ? unit_it->second : VisualPlacementValue{};
    // Effect overlay offset lives in scene_layout.global (x/y only, scale ignored here).
    const auto                 global_it = tuning.placements.find("SceneLayout:scene:Global");
    const VisualPlacementValue global =
        global_it != tuning.placements.end() ? global_it->second : VisualPlacementValue{};
    return BattleRenderOptions{
        .ground_image = ground_image_for_battle_background(terrain_image),
        .terrain_image = std::move(terrain_image),
        .terrain_overlay_images = std::move(overlay_images),
        .draw_background = !transparent_background && presentation.background_visible,
        .draw_frame = !transparent_background && presentation.frame_visible,
        .draw_unit_groups = true,
        .magnitude = tuning.unit_magnitude,
        .scale_x = unit_val.scale_x,
        .scale_y = unit_val.scale_y,
        .rotation_deg = tuning.unit_rotation,
        .overlay_offset = {.x = global.x, .y = global.y},
        .unit_level = unit_val.level,
        .lifecycle_level = 0,
        .effect_level = 0,
        .ui_level = 0,
        .placements = tuning.placements,
        .position_levels = tuning.position_levels,
        .tree_layout = &tree_layout,
        .layout_metrics = tuning.layout_metrics,
        .info_left_unit = presentation.info_left_unit,
        .info_right_unit = presentation.info_right_unit,
        .portrait_index = portrait_index,
        .texture_cache = texture_cache,
        .debug_enabled = debug_enabled,
        .font_face = tuning.font_face,
        .solo_filter = std::string(solo_filter),
    };
}

void BattleViewerRenderer::render(
    const BattleScene& scene, LayoutScale scale, IBattleTextureProvider& textures,
    Renderer2D& renderer, const BattleScenePresentationState& presentation,
    const BattleTuningState& tuning, const TreeLayout& tree_layout, std::string terrain_image,
    std::vector<std::string> overlay_images, bool transparent_background, bool draw_debug_slots,
    bool debug_enabled, std::string_view solo_filter, const PortraitManifestIndex* portrait_index,
    GameTextureCache* texture_cache, std::vector<DebugRenderableItem>* tunable_items) {
    const BattleRenderSnapshot snapshot = scene.snapshot();
    const BattleRenderOptions  options = make_options(
        presentation, tuning, tree_layout, std::move(terrain_image), std::move(overlay_images),
        transparent_background, debug_enabled, solo_filter, portrait_index, texture_cache);
    auto batch = BattleRenderer::build_render_batch(snapshot, textures, options);
    if (tunable_items != nullptr) {
        tunable_items->clear();
        for (const auto& item : batch.tunable_items) {
            if (item.selectable) {
                tunable_items->push_back(screen_scaled(item, scale));
            }
        }
        // Add render tree container nodes as selectable debug items
        if (options.tree_layout) {
            const auto paths = options.tree_layout->paths();
            for (const auto& tree_path : paths) {
                bool already = false;
                for (const auto& item : *tunable_items) {
                    if (item.tree_path == tree_path) {
                        already = true;
                        break;
                    }
                }
                if (already) {
                    continue;
                }
                const auto node = options.tree_layout->node(tree_path);
                if (!node) {
                    continue;
                }
                const Rect          rect = options.tree_layout->compose(tree_path);
                DebugRenderableItem item;
                item.stable_id = "tree:" + tree_path;
                item.label = tree_path;
                item.tree_path = tree_path;
                item.kind = node->kind;
                item.logical_rect = rect;
                item.bounds = rect;
                item.selectable = true;
                item.visible = true;
                item.binding = to_tuning_binding(ConfigBinding{
                    .config_file = tuning.config_file,
                    .owner_kind = BindingOwnerKind::TreeLayout,
                    .tree_path = tree_path,
                    .role = BindingRole::Global,
                    .side = "",
                    .display_path = std::string("render_tree.") + tree_path,
                });
                tunable_items->push_back(screen_scaled(item, scale));
            }
        }
    }
    SdlBattleRenderer::render_commands(batch.commands, renderer, scale, options);
    SdlBattleRenderer::draw_debug(snapshot, scale, renderer,
                                  DebugRenderOptions{.draw_slot_anchors = draw_debug_slots,
                                                     .tree_layout = options.tree_layout,
                                                     .overlay_style = &tuning.debug_overlay_style});
}

} // namespace d2engine
