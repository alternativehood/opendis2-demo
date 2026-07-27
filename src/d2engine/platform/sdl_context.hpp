#pragma once

#include <SDL3/SDL.h>
#include <string>

// cppcheck-suppress syntaxError
#if !SDL_VERSION_ATLEAST(3, 4, 0)
#error "OpenDis2 requires SDL 3.4.0 or newer for SDL_CreateGPURenderer"
#endif

namespace d2engine {

class SdlContext {
public:
    explicit SdlContext(const std::string& window_title, int width, int height, bool fullscreen);
    ~SdlContext();

    // Disable copy/move
    SdlContext(const SdlContext&) = delete;
    SdlContext& operator=(const SdlContext&) = delete;
    SdlContext(SdlContext&&) = delete;
    SdlContext& operator=(SdlContext&&) = delete;

    [[nodiscard]] SDL_Window*   window() const { return window_; }
    [[nodiscard]] SDL_Renderer* renderer() const { return renderer_; }

    static bool poll_event(SDL_Event& event);

private:
    SDL_Window*   window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    bool          ttf_initialized_ = false;
};

} // namespace d2engine
