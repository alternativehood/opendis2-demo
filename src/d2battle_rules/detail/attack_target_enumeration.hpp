#pragma once

#include "battle_attack_rules.hpp"

#include "../battle_state.hpp"
#include "../battle_types.hpp"

#include <d2engine/assets/game_data_registry.hpp>

#include <string>
#include <vector>

namespace d2battle {
namespace detail {

// Enumerates unit target IDs for the given actor and canonical rule.
// Targets are in canonical member-slot order (0..5).
// Each member appears at most once (large units spanning two cells appear once).
// Enemy policy: enumerates opposite side's members of required vitality.
// Ally policy: enumerates actor's side members of required vitality.
// Heal/Cure: all alive allied members (including actor).
// Revive: only dead allied members.
[[nodiscard]] std::vector<std::string> enumerate_unit_targets(const BattleState&         state,
                                                              const BattleUnitState&     actor,
                                                              const SupportedAttackRule& rule);

} // namespace detail
} // namespace d2battle
