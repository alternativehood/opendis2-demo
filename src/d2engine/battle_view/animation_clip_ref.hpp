#pragma once

#include <cstdint>
#include <string>

namespace d2engine {

// Stable reference to an animation clip within a container.
// Used internally by the animation catalog.
struct AnimationClipRef {
    std::string container_path;
    std::string sprite_name;

    [[nodiscard]] bool operator==(const AnimationClipRef& other) const noexcept {
        return container_path == other.container_path && sprite_name == other.sprite_name;
    }
    [[nodiscard]] bool operator!=(const AnimationClipRef& other) const noexcept {
        return !(*this == other);
    }
};

struct AnimationClipHandle {
    std::uint32_t value = 0;

    [[nodiscard]] bool valid() const noexcept { return value != 0; }
    [[nodiscard]] bool operator==(const AnimationClipHandle&) const noexcept = default;
};

} // namespace d2engine
