#include "sdl_context.hpp"

#include <d2log/log.hpp>

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <sstream>
#include <stdexcept>

namespace d2engine {

#ifdef __APPLE__
void configure_display_p3_output(SDL_Window* window, SDL_Renderer* renderer);
#endif

// SDL_ttf lifecycle is owned by SdlContext together with SDL video.
// TextBoxRenderer assumes TTF_Init has already succeeded and only owns fonts/textures.
SdlContext::SdlContext(const std::string& window_title, int width, int height, bool fullscreen) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        const std::string error = SDL_GetError();
        d2log::get("d2.sdl")->error("SDL_Init failed: {}", error);
        throw std::runtime_error("SDL_Init failed: " + error);
    }
    if (!TTF_Init()) {
        const std::string error = SDL_GetError();
        d2log::get("d2.sdl")->error("TTF_Init failed: {}", error);
        SDL_Quit();
        throw std::runtime_error("TTF_Init failed: " + error);
    }
    ttf_initialized_ = true;

    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (fullscreen) {
        flags |= SDL_WINDOW_FULLSCREEN;
    }

    window_ = SDL_CreateWindow(window_title.c_str(), width, height, flags);
    if (window_ == nullptr) {
        const std::string error = SDL_GetError();
        TTF_Quit();
        SDL_Quit();
        throw std::runtime_error("SDL_CreateWindow failed: " + error);
    }

    renderer_ = SDL_CreateGPURenderer(nullptr, window_);
    if (renderer_ == nullptr) {
        const std::string error = SDL_GetError();
        SDL_DestroyWindow(window_);
        TTF_Quit();
        SDL_Quit();
        throw std::runtime_error("SDL_CreateGPURenderer failed: " + error);
    }
    if (SDL_GetGPURendererDevice(renderer_) == nullptr) {
        const char*       renderer_name = SDL_GetRendererName(renderer_);
        const std::string renderer_name_text =
            renderer_name != nullptr ? renderer_name : "unavailable";
        const std::string error = SDL_GetError();
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
        SDL_DestroyWindow(window_);
        TTF_Quit();
        SDL_Quit();
        std::ostringstream message;
        message << "SDL GPU renderer created without a GPU device; renderer=" << renderer_name_text
                << "; SDL error=" << error;
        throw std::runtime_error(message.str());
    }

#ifdef __APPLE__
    try {
        // Keep asset/framebuffer RGB values unchanged; configure only the final Metal layer to
        // match the wider Display P3 gamut observed in the legacy reference rendering on macOS.
        configure_display_p3_output(window_, renderer_);
    } catch (...) {
        SDL_DestroyRenderer(renderer_);
        SDL_DestroyWindow(window_);
        TTF_Quit();
        SDL_Quit();
        throw;
    }
#endif
}

SdlContext::~SdlContext() {
    if (renderer_ != nullptr) {
        SDL_DestroyRenderer(renderer_);
    }
    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
    }
    if (ttf_initialized_) {
        TTF_Quit();
    }
    SDL_Quit();
}

bool SdlContext::poll_event(SDL_Event& event) {
    return SDL_PollEvent(&event);
}

} // namespace d2engine
