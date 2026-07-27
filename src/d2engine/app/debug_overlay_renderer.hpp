#pragma once

// Dev UI policy:
// Dear ImGui is the preferred UI layer for interactive tuning panels.
// Keep tuning state and edit/apply/save logic in controllers/model classes.
// Do not add new hand-written SDL widget systems or ad-hoc overlay panels;
// expose new controls through ImGui on top of existing tuning APIs.

#include "battle_tuning_state.hpp"
#include "unit_debug_metadata.hpp"
#include "../battle_view/battle_presenter.hpp"
#include "../battle_view/battle_scene.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace d2engine {

class GameDataRegistry;
class GameTextureCache;
class Renderer2D;

struct DebugOverlayFrame {
    Renderer2D&                           renderer;
    const BattleScene&                    scene;
    const BattlePresenter&                presenter;
    const std::vector<UnitDebugMetadata>& unit_debug;
    GameTextureCache*                     textures = nullptr;
    std::string_view                      script_path;
    const BattleTuningState&              tuning;
    LayoutScale                           scale;
    float                                 mouse_ref_x = 0.0f;
    float                                 mouse_ref_y = 0.0f;
    int                                   window_width = 0;
    int                                   window_height = 0;
};

class DebugOverlayRenderer {
public:
    static void draw(const DebugOverlayFrame& frame);
};

} // namespace d2engine
