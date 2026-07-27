#pragma once

#include "render_command.hpp"

#include <vector>

namespace d2engine {

class Renderer2D;

struct RenderCommandScale {
    float sx = 1.0f;
    float sy = 1.0f;
};

struct SdlRenderCommandOptions {
    RenderCommandScale scale;
    float              rotation_deg = 0.0f;
};

class SdlRenderCommandRenderer {
public:
    static void render_commands(const std::vector<RenderCommand>& commands, Renderer2D& renderer,
                                const SdlRenderCommandOptions& options = {});
};

} // namespace d2engine
