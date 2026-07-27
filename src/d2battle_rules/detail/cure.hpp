#pragma once

#include "resolved_attack_context.hpp"

#include "../battle_state.hpp"

namespace d2battle {
namespace detail {

void resolve_cure_effect(BattleState& state, const ResolvedAttackContext& context);

} // namespace detail
} // namespace d2battle
