#pragma once

#include "../battle_effect.hpp"
#include "../battle_unit.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace d2battle {
namespace detail {

const PetrifiedEffect* find_petrified_effect(const BattleUnitState& unit);

bool is_petrified(const BattleUnitState& unit);

void apply_or_refresh_petrified(BattleUnitState& target, const std::string& source_actor_id,
                                const std::string& source_attack_id,
                                std::uint32_t      activation_skips);

void consume_one_petrified_activation_skip(BattleUnitState& unit);

void clear_transient_effects_on_death(BattleUnitState& unit);

// Returns true if the effect is a curable negative transient battle effect.
// Currently: PetrifiedEffect is the only curable negative effect.
[[nodiscard]] bool is_curable_negative_effect(const BattleUnitEffectState& effect);

// Removes all curable negative effects from the unit.
// Unit must be alive. Preserves non-curable effects.
// Returns the number of effects removed (0 is a valid no-op).
std::size_t remove_curable_negative_effects(BattleUnitState& unit);

} // namespace detail
} // namespace d2battle
