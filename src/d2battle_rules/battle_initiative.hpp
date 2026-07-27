#pragma once

#include <d2engine/assets/game_data_registry.hpp>

#include "battle_state.hpp"

namespace d2battle {

struct InitiativeCandidate {
    std::string unit_id;
    int         initiative = 0;
    BattleSide  side = BattleSide::Party1;
    int         member_index = 0;
};

[[nodiscard]] int effective_initiative(const BattleState& state, const std::string& unit_id,
                                       const d2engine::GameDataRegistry& game_data);

} // namespace d2battle
