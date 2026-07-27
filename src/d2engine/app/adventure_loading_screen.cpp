#include "adventure_loading_screen.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace d2engine {

AdventureLoadingScreen::AdventureLoadingScreen(SDL_Texture* background) : background_(background) {}

void AdventureLoadingScreen::render(Renderer2D& renderer, int viewport_w, int viewport_h) const {
    renderer.clear({0, 0, 0, 255});
    render_background(renderer, viewport_w, viewport_h);
    render_progress_bar(renderer, viewport_w, viewport_h);
}

void AdventureLoadingScreen::render_background(Renderer2D& renderer, int vw, int vh) const {
    float tw = 0.0f;
    float th = 0.0f;
    if (background_ == nullptr)
        return;
    SDL_GetTextureSize(background_, &tw, &th);
    if (tw <= 0.0f || th <= 0.0f || vw <= 0 || vh <= 0)
        return;

    // Fit-contain / letterbox: preserve the full authored image.
    const float scale_w = static_cast<float>(vw) / tw;
    const float scale_h = static_cast<float>(vh) / th;
    const float scale = std::min(scale_w, scale_h);
    const float dst_w = tw * scale;
    const float dst_h = th * scale;
    const float dst_x = (static_cast<float>(vw) - dst_w) * 0.5f;
    const float dst_y = (static_cast<float>(vh) - dst_h) * 0.5f;

    const Rect src{0.0f, 0.0f, tw, th};
    const Rect dst{dst_x, dst_y, dst_w, dst_h};
    renderer.draw_texture(background_, src, dst);
}

void AdventureLoadingScreen::render_progress_bar(Renderer2D& renderer, int vw, int vh) const {
    const float bar_x = std::max(0.0f, static_cast<float>(kProgressBarMarginLeft));
    const float bar_y = static_cast<float>(vh) - static_cast<float>(kProgressBarHeight) -
                        static_cast<float>(kProgressBarMarginBottom);
    const float bar_w = static_cast<float>(kProgressBarWidth);
    const float bar_h = static_cast<float>(kProgressBarHeight);
    const float border = static_cast<float>(kProgressBarBorderWidth);

    const float visible_x = std::min(bar_x, std::max(0.0f, static_cast<float>(vw) - bar_w));
    const float visible_y = std::max(0.0f, bar_y);

    // Backdrop plate (slightly larger than the bar)
    constexpr int kBackdropPad = 3;
    const float   bp_x = visible_x - static_cast<float>(kBackdropPad);
    const float   bp_y = visible_y - static_cast<float>(kBackdropPad);
    const float   bp_w = bar_w + static_cast<float>(kBackdropPad) * 2.0f;
    const float   bp_h = bar_h + static_cast<float>(kBackdropPad) * 2.0f;
    renderer.draw_rect({bp_x, bp_y, bp_w, bp_h}, kBackdropColor, true);

    // Outer border
    renderer.draw_rect({visible_x, visible_y, bar_w, bar_h}, kBorderColor, false);

    // Background track (inside the border)
    const float track_x = visible_x + border;
    const float track_y = visible_y + border;
    const float track_w = bar_w - border * 2.0f;
    const float track_h = bar_h - border * 2.0f;
    renderer.draw_rect({track_x, track_y, track_w, track_h}, kTrackColor, true);

    // Fill
    const float fill_w = track_w * std::clamp(progress_, 0.0f, 1.0f);
    if (fill_w > 0.0f) {
        renderer.draw_rect({track_x, track_y, fill_w, track_h}, kFillColor, true);

        // Subtle highlight band (top half of fill)
        constexpr float kHighlightHeightRatio = 0.45f;
        const float     hh = track_h * kHighlightHeightRatio;
        renderer.draw_rect({track_x, track_y, fill_w, hh}, kFillHighlight, true);
    }
}

} // namespace d2engine
