#pragma once
#include "layered_animation_clip.hpp"
#include <string>
#include <utility>

namespace d2engine {

struct UnitStateClip {
    LayeredAnimationClip clip;
    std::string          action;
    char                 direction = 'A';

    [[nodiscard]] bool is_empty() const noexcept { return clip.is_empty(); }

    // Convenience factory: wraps one AnimationSequence as the A1 layer
    [[nodiscard]] static UnitStateClip from_a1(AnimationSequence seq, std::string act = {},
                                               char dir = 'A') {
        UnitStateClip c;
        c.clip.a1 = std::move(seq);
        c.action = std::move(act);
        c.direction = dir;
        return c;
    }
};

} // namespace d2engine
