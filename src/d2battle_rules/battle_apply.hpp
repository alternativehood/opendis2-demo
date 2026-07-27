#pragma once

#include <d2engine/assets/game_data_registry.hpp>

#include "battle_action.hpp"
#include "battle_state.hpp"

namespace d2battle {

[[nodiscard]] BattleState apply(const BattleState& state, const BattleAction& action,
                                const d2engine::GameDataRegistry& game_data);

} // namespace d2battle
