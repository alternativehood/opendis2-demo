#pragma once

#include "input_event.hpp"

#include <SDL3/SDL.h>

#include <optional>

namespace d2engine {

class SdlInputBackend {
public:
    [[nodiscard]] static std::optional<InputEvent> translate(const SDL_Event& event,
                                                             SDL_Renderer*    renderer);
};

} // namespace d2engine
