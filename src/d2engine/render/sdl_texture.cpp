#include "sdl_texture.hpp"

#include <cstring>

namespace d2engine {

SdlTexture create_sdl_texture(SDL_Renderer* renderer, int width, int height, const void* pixels,
                              int pitch) {
    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                             SDL_TEXTUREACCESS_STREAMING, width, height);
    if (texture == nullptr) {
        return nullptr;
    }

    void* locked_pixels = nullptr;
    int   locked_pitch = 0;
    if (!SDL_LockTexture(texture, nullptr, &locked_pixels, &locked_pitch)) {
        SDL_DestroyTexture(texture);
        return nullptr;
    }

    // Copy row-by-row in case locked_pitch != pitch
    const auto* src = static_cast<const std::uint8_t*>(pixels);
    auto*       dst = static_cast<std::uint8_t*>(locked_pixels);
    int         row_bytes = width * 4;
    for (int y = 0; y < height; ++y) {
        std::memcpy(dst + y * locked_pitch, src + y * pitch, static_cast<std::size_t>(row_bytes));
    }

    SDL_UnlockTexture(texture);

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

    return SdlTexture(texture);
}

d2res::RgbaBuffer rgba_buffer_from_sdl_surface(SDL_Surface* surface) {
    if (surface == nullptr || surface->w <= 0 || surface->h <= 0) {
        return {};
    }

    // Convert to RGBA32 format if needed
    SDL_Surface* work = surface;
    bool         needs_cleanup = false;
    if (surface->format != SDL_PIXELFORMAT_RGBA32) {
        work = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
        if (work == nullptr) {
            return {};
        }
        needs_cleanup = true;
    }

    if (!SDL_LockSurface(work)) {
        if (needs_cleanup) {
            SDL_DestroySurface(work);
        }
        return {};
    }

    d2res::RgbaBuffer buf;
    buf.width = static_cast<std::uint32_t>(work->w);
    buf.height = static_cast<std::uint32_t>(work->h);
    buf.rgba.resize(static_cast<std::size_t>(buf.width) * buf.height * 4);

    // Copy row-by-row respecting the surface's pitch
    const auto* src = static_cast<const std::uint8_t*>(work->pixels);
    auto*       dst = buf.rgba.data();
    int         row_bytes = static_cast<int>(buf.width) * 4;
    for (int y = 0; y < static_cast<int>(buf.height); ++y) {
        std::memcpy(dst + y * row_bytes, src + y * work->pitch,
                    static_cast<std::size_t>(row_bytes));
    }

    SDL_UnlockSurface(work);
    if (needs_cleanup) {
        SDL_DestroySurface(work);
    }

    return buf;
}

} // namespace d2engine
