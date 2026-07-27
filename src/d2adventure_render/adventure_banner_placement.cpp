#include "adventure_banner_placement.hpp"

namespace d2engine::adventure_render {

ScreenPoint dock_adventure_banner(ScreenPoint             reference_draw_origin,
                                  CanvasContentBounds     reference_content,
                                  CanvasContentBounds     banner_content,
                                  AdventureBannerDockSide side) {
    const int y = reference_draw_origin.y + reference_content.max_y - banner_content.max_y;
    const int x = side == AdventureBannerDockSide::RightOfReference
                      ? reference_draw_origin.x + reference_content.max_x - banner_content.min_x
                      : reference_draw_origin.x + reference_content.min_x - banner_content.max_x;
    return {.x = x, .y = y};
}

} // namespace d2engine::adventure_render
