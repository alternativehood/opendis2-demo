#pragma once

#include <cstdint>

namespace d2engine {

enum class LifeVisualState : std::uint8_t {
    Alive,
    FadingOut,
    Dead,
    Reviving,
};

} // namespace d2engine
