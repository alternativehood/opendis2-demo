#include "drain.hpp"

#include <d2log/log.hpp>

namespace d2battle {
namespace detail {

namespace {
auto kLog = d2log::get("d2battle.drain");
}

void resolve_drain_effect(BattleState& state, const ResolvedAttackContext& ctx,
                          const d2engine::GameDataRegistry& game_data) {
    int damage = ctx.attack.get().damage;

    D2_LOG_DEBUG(kLog, "drain: actor={} attack={} damage={} targets={}", ctx.actor_id,
                 ctx.attack.get().attack_id, damage, ctx.target_unit_ids.size());

    auto result = apply_raw_damage_to_targets(state, ctx.target_unit_ids, damage);

    int healing_budget = result.total_actual_damage / 2;

    D2_LOG_DEBUG(kLog, "drain: total_actual_damage={} healing_budget={}",
                 result.total_actual_damage, healing_budget);

    auto heal_result = heal_alive_unit_up_to_max(state, ctx.actor_id, healing_budget, game_data);

    D2_LOG_DEBUG(kLog, "drain heal: actor={} hp={}->{} applied={} unused={}", ctx.actor_id,
                 heal_result.hp_before, heal_result.hp_after, heal_result.applied,
                 heal_result.unused);

    if (heal_result.unused > 0)
        D2_LOG_DEBUG(kLog, "drain: {} healing discarded (no overflow)", heal_result.unused);
}

} // namespace detail
} // namespace d2battle
