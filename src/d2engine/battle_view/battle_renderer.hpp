#pragma once

#include "battle_debug_binding.hpp"
#include "battle_render_snapshot.hpp"
#include "battle_texture_provider.hpp"
#include "layout_types.hpp"
#include "../render/render_batch.hpp"
#include "../render/render_tuning.hpp"
#include "../render/render_tree.hpp"
#include "../render/vec2.hpp"

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace d2engine {

class GameTextureCache;
class PortraitManifestIndex;

enum class BattleRenderPass : std::uint8_t {
    GroundBackground,
    Background,
    GroundEffects,
    Shadows,
    UnitsBack,
    UnitsFront,
    Effects,
    BackgroundOverlay, // foreground overlay part of composite backgrounds (_FG)
    Markers,
    CombatFrame,
    Debug,
};

struct DebugRenderOptions {
    bool              draw_slot_anchors = false;
    bool              draw_entity_ids = false;
    bool              draw_track_states = false;
    bool              draw_frame_names = false;
    const TreeLayout* tree_layout = nullptr;
    // Optional overlay style; nullptr → use DebugOverlayStyle defaults.
    const DebugOverlayStyle* overlay_style = nullptr;

    [[nodiscard]] bool enabled() const {
        return draw_slot_anchors || draw_entity_ids || draw_track_states || draw_frame_names;
    }
};

struct BattleRenderOptions {
    std::string ground_container = "Imgs/Ground.ff";
    std::string ground_image;
    std::string terrain_container = "Imgs/Battle.ff";
    std::string terrain_image;
    std::vector<std::string>
        terrain_overlay_images; // optional overlays sharing same image id (e.g. "UNDEAD_0_FG")
    std::string frame_container = "Interf/Interf.ff";
    std::string frame_image = "DLG_BATTLE_A_MAINCOMBATBG";
    std::string left_unit_group_image = "DLG_BATTLE_A_LUNITGROUP";
    std::string right_unit_group_image = "DLG_BATTLE_A_RUNITGROUP";
    bool        draw_background = true;
    bool        draw_frame = true;
    bool        draw_unit_groups = false;
    float       magnitude = 1.0f;
    float       scale_x = 1.0f;
    float       scale_y = 1.0f;
    float       rotation_deg = 0.0f;
    Vec2        overlay_offset;
    int         unit_level = 0;
    int         lifecycle_level = 0;
    int         effect_level = 0;
    int         ui_level = 0;
    // Placement overrides for visual profiles (unit/effect/lifecycle — x/y/scale/level/offset)
    std::map<std::string, VisualPlacementValue> placements;
    // Per-slot-position depth level keyed by "A_FRONT_0" etc
    std::map<std::string, int> position_levels;
    // Canonical static render tree for layout
    const TreeLayout* tree_layout = nullptr;
    // Reference canvas metrics loaded from config (ref_w/ref_h/slot_w/slot_h/battlefield_rect)
    LayoutMetrics                layout_metrics;
    UnitInstanceId               info_left_unit;
    UnitInstanceId               info_right_unit;
    const PortraitManifestIndex* portrait_index = nullptr;
    GameTextureCache*            texture_cache = nullptr;
    // When true, render a magenta fill behind the background for tuning contrast
    bool debug_enabled = false;

    // Font face for game text rendering (e.g. "Charis SIL")
    std::string font_face = "Charis SIL";

    // If non-empty, only commands linked to a tunable item with this stable_id are rendered.
    std::string solo_filter;
};

class BattleRenderer {
public:
    [[nodiscard]] static constexpr std::array<BattleRenderPass, 11> pass_order() {
        return {BattleRenderPass::GroundBackground,
                BattleRenderPass::Background,
                BattleRenderPass::GroundEffects,
                BattleRenderPass::Shadows,
                BattleRenderPass::UnitsBack,
                BattleRenderPass::UnitsFront,
                BattleRenderPass::Effects,
                BattleRenderPass::BackgroundOverlay,
                BattleRenderPass::Markers,
                BattleRenderPass::CombatFrame,
                BattleRenderPass::Debug};
    }

    [[nodiscard]] static RenderBatch build_render_batch(const BattleRenderSnapshot& snapshot,
                                                        IBattleTextureProvider&     textures,
                                                        const BattleRenderOptions&  options = {});
};

[[nodiscard]] RenderLayer battle_render_layer(BattleRenderPass pass);

[[nodiscard]] inline std::string ground_image_for_battle_background(std::string_view image) {
    if (image.find("HUMAN") != std::string_view::npos)
        return "HU_00.PNG";
    if (image.find("ELF") != std::string_view::npos)
        return "EL_00.PNG";
    if (image.find("DWARF") != std::string_view::npos)
        return "DW_00.PNG";
    if (image.find("HERETIC") != std::string_view::npos)
        return "HE_00.PNG";
    if (image.find("UNDEAD") != std::string_view::npos)
        return "UN_00.PNG";
    if (image.find("NEUTRAL") != std::string_view::npos)
        return "NE_00.PNG";
    if (image.find("WATER") != std::string_view::npos)
        return "WA_00.PNG";
    if (image.find("BOAT") != std::string_view::npos)
        return "WA_00.PNG";
    if (image.find("CITY") != std::string_view::npos ||
        image.find("RUIN") != std::string_view::npos)
        return "NE_00.PNG";
    return {};
}

} // namespace d2engine
