// TODO(opendis2-debug-ui): expose this tuning model through Dear ImGui.
// Do not create another custom SDL widget/overlay framework.
// ImGui should provide: selected item panel, x/y/w/h fields, font_size/alpha,
// save/revert buttons, dirty state indicator.

#include "debug_overlay_renderer.hpp"
#include "app_build_info.hpp"

#include "../render/color.hpp"
#include "../render/game_texture_cache.hpp"
#include "../render/renderer2d.hpp"

#include <d2log/log.hpp>

#include <SDL3/SDL.h>
#include <algorithm>
#include <sstream>

namespace d2engine {
namespace {

auto kLog = d2log::get("d2.debug"); // NOLINT(cert-err58-cpp)

constexpr float   kDebugTextScale = 3.0f;
constexpr float   kBuildLabelScale = 14.0f / 8.0f;
const char* const kBuildTimestamp = app_build_timestamp();
constexpr Color   kBuildLabelColor = {.r = 255, .g = 165, .b = 0, .a = 255};

[[nodiscard]] Vec2 clamped_label_position(const Rect& rect, std::string_view label,
                                          int window_width, int window_height, float scale) {
    const float char_w = 8.0f * scale;
    const float char_h = 8.0f * scale;
    const float w = char_w * static_cast<float>(label.size());
    float const x = rect.x;
    float       y = rect.y - char_h - 2.0f;
    if (y < 2.0f) {
        y = rect.y + rect.h + 2.0f;
    }
    const float max_x = std::max(2.0f, static_cast<float>(window_width) - w - 2.0f);
    const float max_y = std::max(2.0f, static_cast<float>(window_height) - char_h - 2.0f);
    return {.x = std::clamp(x, 2.0f, max_x), .y = std::clamp(y, 2.0f, max_y)};
}

} // namespace

void DebugOverlayRenderer::draw(const DebugOverlayFrame& frame) {
    frame.renderer.draw_debug_text_colored_scaled(4.0f, 4.0f, kBuildTimestamp, kBuildLabelColor,
                                                  kBuildLabelScale);

    const DebugRenderableItem* selected = frame.tuning.selected_item();
    if (selected != nullptr) {
        frame.renderer.draw_rect(selected->screen_rect, Color{.r = 255, .g = 80, .b = 0, .a = 220},
                                 false);
        const float ax = selected->anchor.x;
        const float ay = selected->anchor.y;
        const Color cross_color{.r = 255, .g = 80, .b = 0, .a = 220};
        for (int d = -3; d <= 3; ++d) {
            frame.renderer.draw_line(ax - 10.0f, ay + static_cast<float>(d), ax + 10.0f,
                                     ay + static_cast<float>(d), cross_color);
            frame.renderer.draw_line(ax + static_cast<float>(d), ay - 10.0f,
                                     ax + static_cast<float>(d), ay + 10.0f, cross_color);
        }
        const Vec2 label =
            clamped_label_position(selected->screen_rect, selected->label, frame.window_width,
                                   frame.window_height, kDebugTextScale);
        frame.renderer.draw_debug_text_scaled(label.x, label.y, selected->label.c_str(),
                                              kDebugTextScale);
    }
}

} // namespace d2engine
