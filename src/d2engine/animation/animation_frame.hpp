#pragma once

#include <cstddef>
#include <string>

#include <d2adventure_render/adventure_render_types.hpp>

namespace d2engine {

struct AnimationFrame {
    std::string image_name;        // Name of the frame image for GameTextureCache lookup
    std::size_t index = 0;         // Frame index in the sequence
    std::size_t duration_ms = 100; // Duration in milliseconds (configurable fallback)
    d2engine::adventure_render::CanvasContentBounds content_bounds;
};

} // namespace d2engine
