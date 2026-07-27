#pragma once

#include <d2engine/assets/game_data_registry.hpp>

#include "../battle_action.hpp"
#include "../battle_action_validate.hpp"
#include "../battle_state.hpp"

namespace d2battle {
namespace detail {

[[nodiscard]] ActionValidationError
validate_action_on_valid_state(const BattleAction& action, const BattleState& state,
                               const d2engine::GameDataRegistry& game_data);

} // namespace detail
} // namespace d2battle
