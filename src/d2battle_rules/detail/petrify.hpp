#pragma once

#include "resolved_attack_context.hpp"

#include "../battle_state.hpp"

namespace d2battle {
namespace detail {

// TODO(D2 parity):
// reverse-engineer original Petrify duration and hit-resolution formula.

void resolve_petrify_effect(BattleState& state, const ResolvedAttackContext& ctx);

} // namespace detail
} // namespace d2battle
