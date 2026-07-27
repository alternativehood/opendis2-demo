#pragma once

#include <d2engine/assets/game_data_registry.hpp>

#include "battle_action.hpp"
#include "battle_state.hpp"

#include <vector>

namespace d2battle {

[[nodiscard]] std::vector<BattleAction> valid_actions(const BattleState&                state,
                                                      const d2engine::GameDataRegistry& game_data);

} // namespace d2battle
