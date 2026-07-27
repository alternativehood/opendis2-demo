#pragma once

#include "portrait_render_item.hpp"

#include <optional>
#include <string>

namespace d2engine {

struct HudUnitInfo {
    bool        has_unit = false;
    std::string name;
    std::string unit_type;
    int         hp = 0;
    int         hp_max = 0;
    bool        flip_x = false;
    bool        flip_y = false;

    // Portrait render item built via build_portrait_render_item().
    // Carries both the texture for drawing and the metadata for debug/tuning.
    // Empty when no portrait is available (placeholder drawn instead).
    std::optional<PortraitRenderItem> portrait_item;
};

} // namespace d2engine
