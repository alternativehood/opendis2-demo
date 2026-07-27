#pragma once

#include <d2engine/assets/game_data_registry.hpp>

#include "../battle_round.hpp"
#include "../battle_state.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace d2battle {
namespace detail {

void begin_round(BattleState& state, std::uint32_t round_number,
                 const d2engine::GameDataRegistry& game_data);

void advance_turn(BattleState& state, const d2engine::GameDataRegistry& game_data);

[[nodiscard]] std::vector<BattleTurnEntry>
build_turn_order(const BattleState& state, const d2engine::GameDataRegistry& game_data);

} // namespace detail
} // namespace d2battle
