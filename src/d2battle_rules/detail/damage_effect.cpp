#include "damage_effect.hpp"
#include "damage_batch.hpp"

#include <d2log/log.hpp>

namespace d2battle {
namespace detail {

namespace {
auto kLog = d2log::get("d2battle.resolve");
}

void resolve_damage_effect(BattleState& state, const ResolvedAttackContext& ctx) {
    int damage = ctx.attack.get().damage;

    D2_LOG_DEBUG(kLog, "damage effect: actor={} attack={} damage={} targets={}", ctx.actor_id,
                 ctx.attack.get().attack_id, damage, ctx.target_unit_ids.size());

    auto result = apply_raw_damage_to_targets(state, ctx.target_unit_ids, damage);

    for (const auto& d : result.units) {
        D2_LOG_DEBUG(kLog, "  unit {} hp: {} -> {} killed={}", d.unit_id, d.hp_before, d.hp_after,
                     d.killed);
    }
    D2_LOG_DEBUG(kLog, "damage resolved: affected={} killed={}", result.affected, result.killed);
}

} // namespace detail
} // namespace d2battle
