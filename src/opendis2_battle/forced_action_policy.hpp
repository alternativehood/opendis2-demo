#pragma once

#include <d2battle_rules/battle_action.hpp>
#include <d2battle_rules/battle_outcomes.hpp>

#include <vector>

namespace opendis2_battle {

[[nodiscard]] const d2battle::BattleActionOutcome*
resolve_forced_outcome(const std::vector<d2battle::BattleActionOutcome>& outcomes);

} // namespace opendis2_battle
