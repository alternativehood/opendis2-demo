#pragma once

#include "adventure_camera.hpp"
#include "sdl_texture.hpp"

#include <cstdint>

namespace d2engine::adventure_render {
struct PreparedAdventureMap;
} // namespace d2engine::adventure_render

namespace d2engine {
struct AdventureTerrainSurface;

// ── AdventureRenderState ────────────────────────────────────────────────
//
// Owns backend render resources for the adventure map.
// Created from a PreparedAdventureMap; consumed by AdventureScreen.
class AdventureRenderState {
public:
    AdventureRenderState() = default;

    AdventureRenderState(const AdventureRenderState&) = delete;
    AdventureRenderState& operator=(const AdventureRenderState&) = delete;
    AdventureRenderState(AdventureRenderState&&) = default;
    AdventureRenderState& operator=(AdventureRenderState&&) = default;
    ~AdventureRenderState() = default;

    [[nodiscard]] bool has_terrain_texture() const { return terrain_texture_ != nullptr; }

    [[nodiscard]] SDL_Texture* terrain_texture() const { return terrain_texture_.get(); }

    [[nodiscard]] int texture_width() const { return texture_width_; }
    [[nodiscard]] int texture_height() const { return texture_height_; }

    void set_terrain_texture(SdlTexture tex, int pixel_w, int pixel_h) {
        terrain_texture_ = std::move(tex);
        texture_width_ = pixel_w;
        texture_height_ = pixel_h;
    }

    [[nodiscard]] const AdventureCamera& camera() const { return camera_; }
    void                                 set_camera(AdventureCamera cam) { camera_ = cam; }

    static AdventureRenderState from_terrain_surface(SDL_Renderer*                  renderer,
                                                     const AdventureTerrainSurface& surface);

private:
    SdlTexture      terrain_texture_;
    int             texture_width_ = 0;
    int             texture_height_ = 0;
    AdventureCamera camera_;
};

} // namespace d2engine
