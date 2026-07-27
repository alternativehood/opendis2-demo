#pragma once

#include "../animation/animation_sequence.hpp"

#include <string>

namespace d2engine {

struct UnitAnimations {
    AnimationSequence idle;
    AnimationSequence hit;
    AnimationSequence death;
    AnimationSequence attack;
    AnimationSequence heff;
    AnimationSequence tuch;
    bool              size_small = true; // selects REVIVEANIMS vs REVIVEANIML
    std::string bones_sprite_base; // e.g. "DEAD_HUMAN_SMALL.PNG" — passed to corpse_sequence()
};

} // namespace d2engine
