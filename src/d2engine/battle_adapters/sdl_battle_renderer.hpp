#pragma once

#include "../battle_view/battle_render_snapshot.hpp"
#include "../battle_view/battle_renderer.hpp"

#include <vector>

namespace d2engine {

class Renderer2D;

class SdlBattleRenderer {
public:
    static void render_commands(const std::vector<RenderCommand>& commands, Renderer2D& renderer,
                                LayoutScale scale, const BattleRenderOptions& options = {});

    static void draw_debug(const BattleRenderSnapshot& snapshot, LayoutScale scale,
                           Renderer2D& renderer, const DebugRenderOptions& options = {});
};

} // namespace d2engine
