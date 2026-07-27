#include "revive.hpp"
#include "healing_primitive.hpp"

#include <d2log/log.hpp>

#include <stdexcept>

namespace d2battle {
namespace detail {

namespace {
auto kLog = d2log::get("d2battle.revive");
}

void resolve_revive_effect(BattleState& state, const ResolvedAttackContext& ctx,
                           const d2engine::GameDataRegistry& game_data) {
    int heal_amount = ctx.attack.get().heal;
    if (heal_amount <= 0)
        throw std::runtime_error("resolve_revive_effect: attack.heal <= 0 for attack " +
                                 ctx.attack.get().attack_id);

    D2_LOG_DEBUG(kLog, "revive: actor={} attack={} heal={} targets={}", ctx.actor_id,
                 ctx.attack.get().attack_id, heal_amount, ctx.target_unit_ids.size());

    for (const auto& uid : ctx.target_unit_ids) {
        auto result = revive_dead_unit_with_hp(state, uid, heal_amount, game_data);
        D2_LOG_DEBUG(kLog, "  unit {} hp: {} -> {} applied={}", uid, result.hp_before,
                     result.hp_after, result.applied);
    }
}

} // namespace detail
} // namespace d2battle
