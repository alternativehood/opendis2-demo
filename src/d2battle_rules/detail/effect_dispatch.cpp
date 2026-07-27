#include "effect_dispatch.hpp"
#include "cure.hpp"
#include "damage_effect.hpp"
#include "drain.hpp"
#include "drain_overflow.hpp"
#include "heal.hpp"
#include "petrify.hpp"
#include "revive.hpp"

#include <stdexcept>

namespace d2battle {
namespace detail {

void dispatch_attack_effect(BattleState& state, const ResolvedAttackContext& ctx,
                            const d2engine::GameDataRegistry& game_data) {
    auto kind = effect_kind_for_attack_class(ctx.attack.get().attack_class);
    if (!kind.has_value()) {
        throw std::runtime_error(
            "dispatch_attack_effect: validated attack class has no effect resolver: class=" +
            std::to_string(static_cast<int>(ctx.attack.get().attack_class)));
    }

    switch (*kind) {
    case AttackEffectKind::Damage:
        resolve_damage_effect(state, ctx);
        break;

    case AttackEffectKind::Drain:
        resolve_drain_effect(state, ctx, game_data);
        break;

    case AttackEffectKind::DrainOverflow:
        resolve_drain_overflow_effect(state, ctx, game_data);
        break;

    case AttackEffectKind::Petrify:
        resolve_petrify_effect(state, ctx);
        break;

    case AttackEffectKind::Heal:
        resolve_heal_effect(state, ctx, game_data);
        break;

    case AttackEffectKind::Cure:
        resolve_cure_effect(state, ctx);
        break;

    case AttackEffectKind::Revive:
        resolve_revive_effect(state, ctx, game_data);
        break;
    }
}

} // namespace detail
} // namespace d2battle
