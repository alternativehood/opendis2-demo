#include "adventure_render_state.hpp"
#include "sdl_texture.hpp"

#include <d2adventure_render/terrain/adventure_terrain_surface.hpp>

namespace d2engine {

AdventureRenderState
// cppcheck-suppress unusedFunction
AdventureRenderState::from_terrain_surface(SDL_Renderer*                  renderer,
                                           const AdventureTerrainSurface& surface) {
    AdventureRenderState state;

    if (surface.width > 0 && surface.height > 0 && !surface.pixels.empty()) {
        auto tex = create_sdl_texture(renderer, surface.width, surface.height,
                                      surface.pixels.data(), surface.width * 4);
        if (tex) {
            state.set_terrain_texture(std::move(tex), surface.width, surface.height);
        }
    }

    return state;
}

} // namespace d2engine
