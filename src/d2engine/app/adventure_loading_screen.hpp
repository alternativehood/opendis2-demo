#pragma once

#include "../render/color.hpp"
#include "../render/rect.hpp"
#include "../render/renderer2d.hpp"

#include <SDL3/SDL.h>

namespace d2engine {

class AdventureLoadingScreen {
public:
    // ── Named layout constants ──────────────────────────────────────
    static constexpr int kProgressBarWidth = 260;
    static constexpr int kProgressBarHeight = 16;
    static constexpr int kProgressBarMarginLeft = 40;
    static constexpr int kProgressBarMarginBottom = 34;
    static constexpr int kProgressBarBorderWidth = 1;

    // ── Authored visual colours ─────────────────────────────────────
    static constexpr Color kBackdropColor{15, 14, 12, 160};
    static constexpr Color kTrackColor{20, 18, 16, 200};
    static constexpr Color kBorderColor{110, 100, 85, 220};
    static constexpr Color kFillColor{140, 175, 120, 230};
    static constexpr Color kFillHighlight{185, 200, 165, 200};

    explicit AdventureLoadingScreen(SDL_Texture* background);

    void                set_progress(float p) noexcept { progress_ = p; }
    [[nodiscard]] float progress() const noexcept { return progress_; }

    void render(Renderer2D& renderer, int viewport_w, int viewport_h) const;

private:
    SDL_Texture* background_ = nullptr;
    float        progress_ = 0.0f;

    void render_background(Renderer2D& renderer, int vw, int vh) const;
    void render_progress_bar(Renderer2D& renderer, int vw, int vh) const;
};

// ── Progress stage boundaries ──────────────────────────────────────
// Bootstrap complete (background already loaded):    0.00
// BuildPlan:                                         0.00 → 0.20
// SubmitAssetPreparation:                            0.20 (instant)
// UploadInitialAssets:                               0.20 → 0.90 (linear by ready handles)
// Finalize:                                          0.90 → 1.00
// Running:                                           1.00
inline constexpr float kProgressBootstrapDone = 0.0f;
inline constexpr float kProgressBuildPlanEnd = 0.20f;
inline constexpr float kProgressUploadMin = 0.20f;
inline constexpr float kProgressUploadMax = 0.90f;
inline constexpr float kProgressFinalizeEnd = 1.0f;

} // namespace d2engine
