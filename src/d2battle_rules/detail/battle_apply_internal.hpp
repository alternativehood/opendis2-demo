#pragma once

#include <d2engine/assets/game_data_registry.hpp>

#include "../battle_action.hpp"
#include "../battle_state.hpp"

namespace d2battle {
namespace detail {

[[nodiscard]] BattleState
apply_validated_action_on_valid_state(const BattleState& state, const BattleAction& action,
                                      const d2engine::GameDataRegistry& game_data);

} // namespace detail
} // namespace d2battle
