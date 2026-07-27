#pragma once

#include "../../d2res/rgba_buffer.hpp"
#include <SDL3/SDL.h>
#include <memory>

namespace d2engine {

struct SdlTextureDeleter {
    void operator()(SDL_Texture* texture) const noexcept {
        if (texture != nullptr) {
            SDL_DestroyTexture(texture);
        }
    }
};

using SdlTexture = std::unique_ptr<SDL_Texture, SdlTextureDeleter>;

// Create an SDL texture from raw RGBA pixel data.
// Returns nullptr on failure.
[[nodiscard]] SdlTexture create_sdl_texture(SDL_Renderer* renderer, int width, int height,
                                            const void* pixels, int pitch);

// Extract RGBA pixel data from an SDL_Surface.
// Converts to RGBA32 format if needed.
// Copies pixels row-by-row respecting the surface's pitch.
// Returns empty buffer on failure.
[[nodiscard]] d2res::RgbaBuffer rgba_buffer_from_sdl_surface(SDL_Surface* surface);

} // namespace d2engine
