#pragma once

#include <d2engine/assets/game_data_registry.hpp>
#include <d2runtime/AdventureWorldState.hpp>

#include "battle_state.hpp"

#include <string>

namespace d2battle {

[[nodiscard]] BattleState bootstrap_battle(const d2runtime::AdventureStack&      party1_stack,
                                           const d2runtime::AdventureStack&      party2_stack,
                                           const d2runtime::AdventureWorldState& world,
                                           const d2engine::GameDataRegistry&     game_data);

[[nodiscard]] BattleState bootstrap_battle_from_stack_ids(
    const d2runtime::AdventureWorldState& world, const std::string& party1_stack_id,
    const std::string& party2_stack_id, const d2engine::GameDataRegistry& game_data);

} // namespace d2battle
