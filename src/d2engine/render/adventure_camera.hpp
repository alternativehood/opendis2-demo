#pragma once

#include "rect.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>

namespace d2engine {

// ── AdventureCamera ─────────────────────────────────────────────────────
//
// Pure camera model: position, zoom, coordinate transforms.
// ZERO SDL dependency.  Used by rendering, picking, input, pointer interaction.
//
// Camera coordinates are in canvas-pixel space.
// Zoom is applied as a viewport transform — world canvas is unchanged.
//
// Edge-pan policy: clamp_center_to_canvas_diamond allows up to approximately
// 25% of the visible extent beyond the canvas center before clamping.
// This permits the familiar isometric-map edge navigation behavior.
struct AdventureCamera {
    int canvas_x = 0;
    int canvas_y = 0;
    int viewport_width = 0;
    int viewport_height = 0;

    // ── Zoom ──────────────────────────────────────────────────────────────

    static constexpr std::array<float, 6> kZoomLevels = {0.50f, 0.75f, 1.00f, 1.25f, 1.50f, 2.00f};
    static constexpr int                  kDefaultZoomIndex = 2;

    // Edge-pan margin: visible extent / kEdgeMargin controls how far the
    // camera centre can move beyond the map diamond before clamping.
    static constexpr float kEdgeMargin = 4.0f;

    int zoom_index = kDefaultZoomIndex;

    [[nodiscard]] float zoom() const { return kZoomLevels[static_cast<std::size_t>(zoom_index)]; }

    [[nodiscard]] float visible_canvas_w_float() const {
        return static_cast<float>(viewport_width) / zoom();
    }
    [[nodiscard]] float visible_canvas_h_float() const {
        return static_cast<float>(viewport_height) / zoom();
    }

    // Integer visible extent — for clamping / hit-test bounds.
    [[nodiscard]] int visible_canvas_w() const {
        return static_cast<int>(std::floor(visible_canvas_w_float()));
    }
    [[nodiscard]] int visible_canvas_h() const {
        return static_cast<int>(std::floor(visible_canvas_h_float()));
    }

    bool zoom_in() {
        if (zoom_index >= static_cast<int>(kZoomLevels.size()) - 1)
            return false;
        const float old_zoom = zoom();
        ++zoom_index;
        recenter_on_viewport_center(old_zoom);
        return true;
    }

    bool zoom_out() {
        if (zoom_index <= 0)
            return false;
        const float old_zoom = zoom();
        --zoom_index;
        recenter_on_viewport_center(old_zoom);
        return true;
    }

    // ── Coordinate transforms ─────────────────────────────────────────────
    //
    // screen: logical viewport coordinates (mouse position, draw destination)
    // canvas: world canvas pixel coordinates (source rect, hit-test)
    //
    // screen→canvas uses floor() to keep pixel ownership stable:
    //   at zoom=2, screen_x=[0,1] both map to canvas pixel 0,
    //   screen_x=[2,3] → canvas pixel 1, etc.

    [[nodiscard]] int screen_to_canvas_x(int screen_x) const {
        return canvas_x + static_cast<int>(std::floor(static_cast<float>(screen_x) / zoom()));
    }
    [[nodiscard]] int screen_to_canvas_y(int screen_y) const {
        return canvas_y + static_cast<int>(std::floor(static_cast<float>(screen_y) / zoom()));
    }

    // Floating-point canvas→screen for rendering.
    [[nodiscard]] float canvas_to_screen_x_float(float canvas_x_in) const {
        return (canvas_x_in - static_cast<float>(canvas_x)) * zoom();
    }
    [[nodiscard]] float canvas_to_screen_y_float(float canvas_y_in) const {
        return (canvas_y_in - static_cast<float>(canvas_y)) * zoom();
    }

    // Integer canvas→screen (convenience for integral canvas coords).
    [[nodiscard]] int canvas_to_screen_x(int canvas_x_in) const {
        return static_cast<int>(
            std::lround(canvas_to_screen_x_float(static_cast<float>(canvas_x_in))));
    }
    [[nodiscard]] int canvas_to_screen_y(int canvas_y_in) const {
        return static_cast<int>(
            std::lround(canvas_to_screen_y_float(static_cast<float>(canvas_y_in))));
    }

    void clamp_center_to_canvas_diamond(int canvas_w, int canvas_h) {
        const double map_cx = static_cast<double>(canvas_w) / 2.0;
        const double map_cy = static_cast<double>(canvas_h) / 2.0;
        const double vp_w = static_cast<double>(visible_canvas_w_float());
        const double vp_h = static_cast<double>(visible_canvas_h_float());
        const double limit_x = map_cx + vp_w / static_cast<double>(kEdgeMargin);
        const double limit_y = map_cy + vp_h / static_cast<double>(kEdgeMargin);
        if (limit_x <= 0.0 || limit_y <= 0.0)
            return;

        double       dx = static_cast<double>(canvas_x) + vp_w / 2.0 - map_cx;
        double       dy = static_cast<double>(canvas_y) + vp_h / 2.0 - map_cy;
        const double distance = std::abs(dx) / limit_x + std::abs(dy) / limit_y;
        if (distance > 1.0) {
            dx /= distance;
            dy /= distance;
        }

        canvas_x = static_cast<int>(std::lround(map_cx + dx - vp_w / 2.0));
        canvas_y = static_cast<int>(std::lround(map_cy + dy - vp_h / 2.0));
    }

    [[nodiscard]] static AdventureCamera centered(int canvas_w, int canvas_h, int viewport_w,
                                                  int viewport_h) {
        AdventureCamera cam;
        cam.viewport_width = viewport_w;
        cam.viewport_height = viewport_h;
        cam.canvas_x = (canvas_w - viewport_w) / 2;
        cam.canvas_y = (canvas_h - viewport_h) / 2;
        return cam;
    }

private:
    void recenter_on_viewport_center(float old_zoom) {
        const float vp_cx_f = static_cast<float>(viewport_width) / 2.0f;
        const float vp_cy_f = static_cast<float>(viewport_height) / 2.0f;
        const float world_cx = static_cast<float>(canvas_x) + vp_cx_f / old_zoom;
        const float world_cy = static_cast<float>(canvas_y) + vp_cy_f / old_zoom;
        const float new_zoom = zoom();
        canvas_x = static_cast<int>(std::lround(world_cx - vp_cx_f / new_zoom));
        canvas_y = static_cast<int>(std::lround(world_cy - vp_cy_f / new_zoom));
    }
};

// ── Clipped canvas blit ─────────────────────────────────────────────────
//
// Pure function: computes source→destination texture mapping for a
// rectangular canvas texture viewed through a camera.  Zero SDL dependency.
// Uses floating-point render extents for exact viewport coverage.

struct AdventureCameraBlit {
    Rect source;      // region of the canvas texture to draw
    Rect destination; // where it maps onto the viewport
};

[[nodiscard]] inline std::optional<AdventureCameraBlit>
compute_clipped_canvas_blit(const AdventureCamera& cam, int canvas_w, int canvas_h) {
    const float vis_w = cam.visible_canvas_w_float();
    const float vis_h = cam.visible_canvas_h_float();

    // Camera-visible rectangle in canvas space (floating-point).
    const float cam_x0 = static_cast<float>(cam.canvas_x);
    const float cam_y0 = static_cast<float>(cam.canvas_y);
    const float cam_x1 = cam_x0 + vis_w;
    const float cam_y1 = cam_y0 + vis_h;

    // Intersect with canvas texture bounds.
    const float src_x = std::max(cam_x0, 0.0f);
    const float src_y = std::max(cam_y0, 0.0f);
    const float src_x1 = std::min(cam_x1, static_cast<float>(canvas_w));
    const float src_y1 = std::min(cam_y1, static_cast<float>(canvas_h));

    if (src_x >= src_x1 || src_y >= src_y1)
        return std::nullopt;

    const float src_w = src_x1 - src_x;
    const float src_h = src_y1 - src_y;

    // Destination: project source endpoints through the camera transform.
    const float dst_x = cam.canvas_to_screen_x_float(src_x);
    const float dst_y = cam.canvas_to_screen_y_float(src_y);
    const float dst_x1 = cam.canvas_to_screen_x_float(src_x1);
    const float dst_y1 = cam.canvas_to_screen_y_float(src_y1);

    AdventureCameraBlit blit;
    blit.source = {src_x, src_y, src_w, src_h};
    blit.destination = {dst_x, dst_y, dst_x1 - dst_x, dst_y1 - dst_y};
    return blit;
}

} // namespace d2engine
