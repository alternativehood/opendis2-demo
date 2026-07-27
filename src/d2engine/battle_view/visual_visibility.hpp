#pragma once

#include <cstdint>

namespace d2engine {

enum class TrackVisibility : std::uint8_t {
    Visible,          // updated and rendered
    HiddenButPlaying, // updated, not rendered
    PausedHidden,     // not updated, not rendered
    Disabled,         // inactive, ignored
};

} // namespace d2engine
