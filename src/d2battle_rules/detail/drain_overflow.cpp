#include "drain_overflow.hpp"
#include "healing_primitive.hpp"

#include <d2log/log.hpp>

#include <algorithm>
#include <stdexcept>

namespace d2battle {
namespace detail {

namespace {
auto kLog = d2log::get("d2battle.drain_overflow");
}

void resolve_drain_overflow_effect(BattleState& state, const ResolvedAttackContext& ctx,
                                   const d2engine::GameDataRegistry& game_data) {
    auto* actor = state.find_unit(ctx.actor_id);
    if (!actor)
        throw std::runtime_error("resolve_drain_overflow_effect: actor not found: " + ctx.actor_id);
    if (!actor->alive)
        throw std::runtime_error("resolve_drain_overflow_effect: actor is dead: " + ctx.actor_id);

    int damage = ctx.attack.get().damage;

    D2_LOG_DEBUG(kLog, "drain_overflow: actor={} attack={} damage={} targets={}", ctx.actor_id,
                 ctx.attack.get().attack_id, damage, ctx.target_unit_ids.size());

    auto result = apply_raw_damage_to_targets(state, ctx.target_unit_ids, damage);

    int healing_budget = result.total_actual_damage / 2;

    D2_LOG_DEBUG(kLog, "drain_overflow: total_actual_damage={} healing_budget={}",
                 result.total_actual_damage, healing_budget);

    DrainOverflowResolution drn_result;
    drn_result.total_actual_damage = result.total_actual_damage;
    drn_result.healing_budget = healing_budget;
    for (const auto& d : result.units) {
        drn_result.damaged_units.push_back({d.unit_id, d.hp_before, d.hp_after});
    }

    distribute_drain_healing(state, actor->side, ctx.actor_id, healing_budget, drn_result,
                             game_data);
}

void distribute_drain_healing(BattleState& state, BattleSide actor_side,
                              const std::string& actor_id, int healing_budget,
                              DrainOverflowResolution&          result,
                              const d2engine::GameDataRegistry& game_data) {

    auto heal_result = heal_alive_unit_up_to_max(state, actor_id, healing_budget, game_data);

    if (heal_result.applied > 0) {
        result.healed_units.push_back({actor_id, heal_result.hp_before, heal_result.hp_after});
        D2_LOG_DEBUG(kLog, "heal: unit={} hp={}->{} amount={}", actor_id, heal_result.hp_before,
                     heal_result.hp_after, heal_result.applied);
    }

    int remaining = heal_result.unused;
    if (remaining <= 0) {
        result.healing_applied = healing_budget;
        result.healing_discarded = 0;
        return;
    }

    const auto& side_state = state.side(actor_side);
    for (std::size_t slot = 0; slot < 6; ++slot) {
        if (!side_state.members[slot].has_value())
            continue;
        const auto& uid = *side_state.members[slot];
        if (uid == actor_id)
            continue;

        auto* ally = state.find_unit(uid);
        if (!ally || !ally->alive)
            continue;

        int ally_max_hp = resolve_unit_max_hp(*ally, game_data);
        int ally_missing = ally_max_hp - ally->current_hp;
        if (ally_missing <= 0)
            continue;

        int ally_heal_amount = std::min(remaining, ally_missing);
        int hp_before = ally->current_hp;
        ally->current_hp += ally_heal_amount;
        result.healed_units.push_back({ally->id, hp_before, ally->current_hp});
        remaining -= ally_heal_amount;
        D2_LOG_DEBUG(kLog, "heal: unit={} hp={}->{} amount={}", ally->id, hp_before,
                     ally->current_hp, ally_heal_amount);

        if (remaining <= 0)
            break;
    }

    result.healing_applied = healing_budget - remaining;
    result.healing_discarded = remaining;
}

} // namespace detail
} // namespace d2battle
