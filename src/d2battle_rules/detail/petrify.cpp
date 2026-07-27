#include "petrify.hpp"
#include "unit_effects.hpp"

#include <d2log/log.hpp>

#include <stdexcept>

namespace d2battle {
namespace detail {

namespace {
auto kLog = d2log::get("d2battle.petrify");
}

void resolve_petrify_effect(BattleState& state, const ResolvedAttackContext& ctx) {
    D2_LOG_DEBUG(kLog, "petrify: actor={} attack={} targets={}", ctx.actor_id,
                 ctx.attack.get().attack_id, ctx.target_unit_ids.size());

    std::size_t affected = 0;
    for (const auto& uid : ctx.target_unit_ids) {
        auto* target = state.find_unit(uid);
        if (!target) {
            throw std::runtime_error("resolve_petrify_effect: target not found: " + uid);
        }
        if (!target->alive) {
            throw std::runtime_error("resolve_petrify_effect: target is dead: " + uid);
        }

        apply_or_refresh_petrified(*target, ctx.actor_id, ctx.attack.get().attack_id, 1);

        D2_LOG_DEBUG(kLog, "  unit {} petrified", uid);
        ++affected;
    }

    D2_LOG_DEBUG(kLog, "petrify resolved: affected={}", affected);
}

} // namespace detail
} // namespace d2battle
