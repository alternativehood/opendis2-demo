#pragma once

#include "../render/color.hpp"
#include "../render/rect.hpp"

namespace d2engine {

struct LayoutScale {
    float sx = 1.0f;
    float sy = 1.0f;
};

struct LayoutMetrics {
    float ref_w = 1600.0f;
    float ref_h = 945.0f;
    float slot_w = 200.0f;
    float slot_h = 190.0f;
    Rect  battlefield_rect = {.x = 305.0f, .y = 120.0f, .w = 990.0f, .h = 615.0f};
};

struct DebugOverlayStyle {
    Color slot_attacker_back = {.r = 0, .g = 210, .b = 0, .a = 255};
    Color slot_attacker_front = {.r = 100, .g = 255, .b = 0, .a = 255};
    Color slot_defender_back = {.r = 220, .g = 0, .b = 0, .a = 255};
    Color slot_defender_front = {.r = 255, .g = 140, .b = 0, .a = 255};
    Color center_color = {.r = 180, .g = 100, .b = 255, .a = 220};
    Color lane_line = {.r = 200, .g = 200, .b = 255, .a = 160};
    Color depth_line = {.r = 255, .g = 255, .b = 80, .a = 140};
    Color mount_color = {.r = 255, .g = 200, .b = 0, .a = 220};
    float slot_dot_radius = 8.0f;
    float center_dot_radius = 6.0f;
    float mount_marker_radius = 4.0f;
    float label_offset_x = 2.0f;
    float label_offset_y = -4.0f;
};

[[nodiscard]] inline LayoutScale layout_scale_for(const LayoutMetrics& metrics, int window_w,
                                                  int window_h) {
    return {.sx = static_cast<float>(window_w) / metrics.ref_w,
            .sy = static_cast<float>(window_h) / metrics.ref_h};
}

[[nodiscard]] inline Rect scale_rect(const Rect& ref, LayoutScale scale) {
    return {
        .x = ref.x * scale.sx, .y = ref.y * scale.sy, .w = ref.w * scale.sx, .h = ref.h * scale.sy};
}

} // namespace d2engine
