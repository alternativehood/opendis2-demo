#include "battle_apply_internal.hpp"
#include "battle_derived.hpp"
#include "battle_status.hpp"
#include "battle_turn.hpp"
#include "bundle_support.hpp"
#include "effect_dispatch.hpp"
#include "resolved_attack_context.hpp"
#include "unit_effects.hpp"
#include "../battle_validate.hpp"

#include <stdexcept>

namespace d2battle {
namespace detail {

BattleState apply_validated_action_on_valid_state(const BattleState&                state,
                                                  const BattleAction&               action,
                                                  const d2engine::GameDataRegistry& game_data) {

    const auto* skp = std::get_if<SkipActivationAction>(&action);
    if (skp) {
        BattleState next = state;
        auto*       actor = next.find_unit(skp->actor_id);
        if (!actor) {
            throw std::runtime_error("apply forced skip: actor not found: " + skp->actor_id);
        }
        if (!actor->alive) {
            throw std::runtime_error("apply forced skip: actor is dead: " + skp->actor_id);
        }

        consume_one_petrified_activation_skip(*actor);

        detail::normalize_derived_side_state(next);
        detail::normalize_battle_status(next);

        if (!next.is_terminal())
            detail::advance_turn(next, game_data);

        validate_battle_state(next);
        return next;
    }

    const auto* atk_action = std::get_if<AttackAction>(&action);
    if (!atk_action)
        throw std::runtime_error("apply_validated_action_on_valid_state: not an AttackAction");

    // Copy the state once
    BattleState next = state;

    // Get bundle context once (on original state for validation, re-resolve after primary)
    const auto* actor_b = next.find_unit(atk_action->actor_id);
    if (!actor_b) {
        throw std::runtime_error("apply: actor not found: " + atk_action->actor_id);
    }

    auto bundle_context = resolve_validated_attack_bundle_context(next, *atk_action, game_data);

    // Apply primary component
    if (!bundle_context.primary.target_unit_ids.empty()) {
        dispatch_attack_effect(next, bundle_context.primary, game_data);
    }

    // Re-resolve secondary after primary mutation on intermediate state
    if (bundle_context.secondary.has_value()) {
        const auto* actor_after_prim = next.find_unit(atk_action->actor_id);
        if (!actor_after_prim) {
            throw std::runtime_error("apply: actor died after primary: " + atk_action->actor_id);
        }

        const auto* udef_after = game_data.find_unit(actor_after_prim->type_id);
        if (!udef_after) {
            throw std::runtime_error("apply: unit type not found after primary: " +
                                     actor_after_prim->type_id);
        }

        auto bundle_def_after = require_supported_attack_bundle(*udef_after);

        if (!bundle_def_after.secondary_attack.has_value() ||
            !bundle_def_after.secondary_rule.has_value()) {
            throw std::runtime_error(
                "apply: secondary expected but not present after primary mutation: actor=" +
                atk_action->actor_id);
        }

        auto sec_ctx = resolve_bundle_component_context(
            next, atk_action->actor_id, bundle_context.selected_target,
            bundle_def_after.secondary_attack->get(), *bundle_def_after.secondary_rule);

        if (!sec_ctx.target_unit_ids.empty()) {
            dispatch_attack_effect(next, sec_ctx, game_data);
        }
    }

    detail::normalize_derived_side_state(next);
    detail::normalize_battle_status(next);

    if (!next.is_terminal())
        detail::advance_turn(next, game_data);

    validate_battle_state(next);
    return next;
}

} // namespace detail
} // namespace d2battle
