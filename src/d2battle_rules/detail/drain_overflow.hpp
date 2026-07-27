#pragma once

#include "../battle_state.hpp"
#include "damage_batch.hpp"
#include "resolved_attack_context.hpp"

#include <d2engine/assets/game_data_registry.hpp>

#include <string>
#include <vector>

namespace d2battle {
namespace detail {

struct UnitHpDelta {
    std::string unit_id;
    int         hp_before = 0;
    int         hp_after = 0;
};

struct DrainOverflowResolution {
    std::vector<UnitHpDelta> damaged_units;
    std::vector<UnitHpDelta> healed_units;

    int total_actual_damage = 0;
    int healing_budget = 0;
    int healing_applied = 0;
    int healing_discarded = 0;
};

void resolve_drain_overflow_effect(BattleState& state, const ResolvedAttackContext& ctx,
                                   const d2engine::GameDataRegistry& game_data);

void distribute_drain_healing(BattleState& state, BattleSide actor_side,
                              const std::string& actor_id, int healing_budget,
                              DrainOverflowResolution&          result,
                              const d2engine::GameDataRegistry& game_data);

} // namespace detail
} // namespace d2battle
