#include "cure.hpp"
#include "unit_effects.hpp"

#include <d2log/log.hpp>

#include <stdexcept>

namespace d2battle {
namespace detail {

namespace {
auto kLog = d2log::get("d2battle.cure");
}

void resolve_cure_effect(BattleState& state, const ResolvedAttackContext& ctx) {
    D2_LOG_DEBUG(kLog, "cure: actor={} attack={} targets={}", ctx.actor_id,
                 ctx.attack.get().attack_id, ctx.target_unit_ids.size());

    for (const auto& uid : ctx.target_unit_ids) {
        auto* unit = state.find_unit(uid);
        if (!unit)
            throw std::runtime_error("resolve_cure_effect: unit not found: " + uid);
        if (!unit->alive)
            throw std::runtime_error("resolve_cure_effect: unit is dead: " + uid);

        auto removed = remove_curable_negative_effects(*unit);
        D2_LOG_DEBUG(kLog, "  unit {} effects removed={}", uid, removed);
    }
}

} // namespace detail
} // namespace d2battle
