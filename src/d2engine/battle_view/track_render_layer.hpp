#pragma once

#include <cstdint>

namespace d2engine {

// Render layer for battle tracks, ordered from back to front.
// Used to sort tracks before drawing within a single entity.
enum class TrackRenderLayer : std::uint8_t {
    Background = 0,
    Ground = 1,
    Shadow = 2,
    Base = 3,
    Effect = 4,
    Overlay = 5,
    Marker = 6,
    Frame = 7,
    Debug = 8,
};

} // namespace d2engine
