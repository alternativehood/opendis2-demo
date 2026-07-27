#pragma once

#include "resolved_attack_context.hpp"

#include <d2engine/assets/game_data_registry.hpp>

#include "../battle_state.hpp"

namespace d2battle {
namespace detail {

void resolve_revive_effect(BattleState& state, const ResolvedAttackContext& context,
                           const d2engine::GameDataRegistry& game_data);

} // namespace detail
} // namespace d2battle
