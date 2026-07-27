#pragma once

#include "../battle_unit.hpp"
#include "unit_effects.hpp"

#include <algorithm>

namespace d2battle {
namespace detail {

struct DamageResolution {
    int  hp_before = 0;
    int  hp_after = 0;
    int  actual_damage = 0;
    bool killed = false;
};

[[nodiscard]] inline DamageResolution apply_raw_damage(BattleUnitState& target,
                                                       int              requested_damage) {
    int  hp_before = target.current_hp;
    bool prev_alive = target.alive;

    target.current_hp = std::max(0, target.current_hp - requested_damage);
    int hp_after = target.current_hp;
    target.alive = (target.current_hp > 0);

    if (prev_alive && !target.alive)
        clear_transient_effects_on_death(target);

    return {hp_before, hp_after, hp_before - hp_after, prev_alive && !target.alive};
}

} // namespace detail
} // namespace d2battle
