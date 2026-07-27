#pragma once

#include "battle_tuning_state.hpp"
#include "../battle_view/battle_renderer.hpp"
#include "../battle_view/battle_scene.hpp"
#include "../battle_view/battle_scene_presentation_state.hpp"
#include "../render/render_tree.hpp"

namespace d2engine {

class GameTextureCache;
class PortraitManifestIndex;
class Renderer2D;

class BattleViewerRenderer {
public:
    [[nodiscard]] static BattleRenderOptions
    make_options(const BattleScenePresentationState& presentation, const BattleTuningState& tuning,
                 const TreeLayout& tree_layout, std::string terrain_image,
                 std::vector<std::string> overlay_images, bool transparent_background,
                 bool debug_enabled, std::string_view solo_filter = {},
                 const PortraitManifestIndex* portrait_index = nullptr,
                 GameTextureCache*            texture_cache = nullptr);

    static void render(const BattleScene& scene, LayoutScale scale,
                       IBattleTextureProvider& textures, Renderer2D& renderer,
                       const BattleScenePresentationState& presentation,
                       const BattleTuningState& tuning, const TreeLayout& tree_layout,
                       std::string terrain_image, std::vector<std::string> overlay_images,
                       bool transparent_background, bool draw_debug_slots, bool debug_enabled,
                       std::string_view                  solo_filter = {},
                       const PortraitManifestIndex*      portrait_index = nullptr,
                       GameTextureCache*                 texture_cache = nullptr,
                       std::vector<DebugRenderableItem>* tunable_items = nullptr);
};

} // namespace d2engine
