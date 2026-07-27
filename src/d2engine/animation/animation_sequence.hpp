#pragma once

#include "animation_frame.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace d2engine {

struct AnimationSequence {
    std::string                 name;               // Animation name
    std::string                 container_path;     // Container this animation came from
    std::vector<AnimationFrame> frames;             // Frame sequence
    bool                        is_looping = false; // Loop mode
    // Native canvas size of the source sprites (from ImageFrame.output_width/height).
    // Used to normalize foot anchor from sprite space to reference space.
    // Default 800×600 matches Batunits.ff; other containers set their own values.
    int32_t native_canvas_w = 800;
    int32_t native_canvas_h = 600;
    // Canonical content bounding box derived from the first non-empty frame's pieces.
    // foot_x = horizontal center, foot_y = bottom edge, top_y = top edge.
    // All values are in native_canvas_w×native_canvas_h space.
    int32_t canvas_foot_x = 0;
    int32_t canvas_foot_y = 0;
    int32_t canvas_top_y = 0;
};

} // namespace d2engine
