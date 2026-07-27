#pragma once

#include "adventure_render_types.hpp"

namespace d2engine::adventure_render {

enum class AdventureBannerDockSide {
    LeftOfReference,
    RightOfReference,
};

[[nodiscard]] ScreenPoint dock_adventure_banner(ScreenPoint             reference_draw_origin,
                                                CanvasContentBounds     reference_content,
                                                CanvasContentBounds     banner_content,
                                                AdventureBannerDockSide side);

} // namespace d2engine::adventure_render
