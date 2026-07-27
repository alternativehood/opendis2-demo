#include "battle_action_validate.hpp"
#include "attack_support.hpp"
#include "battle_validate.hpp"
#include "detail/attack_target_enumeration.hpp"
#include "detail/battle_action_validate_internal.hpp"
#include "detail/battle_attack_rules.hpp"
#include "detail/battle_formation.hpp"
#include "detail/bundle_support.hpp"
#include "detail/resolved_attack_context.hpp"
#include "detail/unit_effects.hpp"

#include <d2log/log.hpp>

#include <stdexcept>

namespace d2battle {

const char* to_string(ActionValidationError err) {
    switch (err) {
    case ActionValidationError::None:
        return "none";
    case ActionValidationError::BattleFinished:
        return "battle_finished";
    case ActionValidationError::ActorUnknown:
        return "actor_unknown";
    case ActionValidationError::ActorNotCurrent:
        return "actor_not_current";
    case ActionValidationError::AttackNotAvailable:
        return "attack_not_available";
    case ActionValidationError::UnsupportedAttackClass:
        return "unsupported_attack_class";
    case ActionValidationError::UnsupportedReach:
        return "unsupported_reach";
    case ActionValidationError::TargetShapeMismatch:
        return "target_shape_mismatch";
    case ActionValidationError::ActorNotOnActiveMeleeRank:
        return "actor_not_on_active_melee_rank";
    case ActionValidationError::TargetProtectedByFrontRank:
        return "target_protected_by_front_rank";
    case ActionValidationError::TargetOutOfAdjacentReach:
        return "target_out_of_adjacent_reach";
    case ActionValidationError::TargetUnknown:
        return "target_unknown";
    case ActionValidationError::FriendlyTarget:
        return "friendly_target";
    case ActionValidationError::HostileTarget:
        return "hostile_target";
    case ActionValidationError::TargetDead:
        return "target_dead";
    case ActionValidationError::TargetAlive:
        return "target_alive";
    case ActionValidationError::ActorIncapacitated:
        return "actor_incapacitated";
    case ActionValidationError::SkipNotRequired:
        return "skip_not_required";
    case ActionValidationError::SkipReasonMismatch:
        return "skip_reason_mismatch";
    case ActionValidationError::NoEligibleTargets:
        return "no_eligible_targets";
    case ActionValidationError::UnsupportedBundle:
        return "unsupported_bundle";
    }
    return "unknown";
}

namespace {

auto kLog = d2log::get("d2battle.action_validate");

} // namespace

namespace detail {

ActionValidationError validate_action_on_valid_state(const BattleAction&               action,
                                                     const BattleState&                state,
                                                     const d2engine::GameDataRegistry& game_data) {

    if (state.status != BattleStatus::InProgress)
        return ActionValidationError::BattleFinished;

    const auto* skp = std::get_if<SkipActivationAction>(&action);
    if (skp) {
        const auto* actor = state.find_unit(skp->actor_id);
        if (!actor) {
            D2_LOG_TRACE(kLog, "rejection: skip actor={} reason=actor_unknown", skp->actor_id);
            return ActionValidationError::ActorUnknown;
        }
        const auto* current = state.current_actor();
        if (!current || skp->actor_id != current->id) {
            D2_LOG_TRACE(kLog, "rejection: skip actor={} current={} reason=actor_not_current",
                         skp->actor_id, current ? current->id.c_str() : "none");
            return ActionValidationError::ActorNotCurrent;
        }
        if (!actor->alive) {
            D2_LOG_TRACE(kLog, "rejection: skip actor={} reason=actor_dead", skp->actor_id);
            return ActionValidationError::TargetDead;
        }
        if (!detail::is_petrified(*actor)) {
            D2_LOG_TRACE(kLog, "rejection: skip not required for actor={}", skp->actor_id);
            return ActionValidationError::SkipNotRequired;
        }
        if (skp->reason != SkipActivationReason::Petrified) {
            D2_LOG_TRACE(kLog, "rejection: skip reason mismatch for actor={}", skp->actor_id);
            return ActionValidationError::SkipReasonMismatch;
        }
        return ActionValidationError::None;
    }

    const auto* atk = std::get_if<AttackAction>(&action);
    if (!atk)
        return ActionValidationError::AttackNotAvailable;

    const auto* actor_unit = state.find_unit(atk->actor_id);
    if (!actor_unit) {
        D2_LOG_TRACE(kLog, "rejection: actor={} reason=actor_unknown", atk->actor_id);
        return ActionValidationError::ActorUnknown;
    }

    const auto* current = state.current_actor();
    if (!current || atk->actor_id != current->id) {
        D2_LOG_TRACE(kLog, "rejection: actor={} current={} reason=actor_not_current", atk->actor_id,
                     current ? current->id.c_str() : "none");
        return ActionValidationError::ActorNotCurrent;
    }

    if (detail::is_petrified(*actor_unit)) {
        D2_LOG_TRACE(kLog, "rejection: actor={} reason=actor_incapacitated (petrified)",
                     atk->actor_id);
        return ActionValidationError::ActorIncapacitated;
    }

    const auto* udef = game_data.find_unit(actor_unit->type_id);
    if (!udef) {
        D2_LOG_ERROR(kLog, "validate_action: unit type {} not in GameDataRegistry",
                     actor_unit->type_id);
        throw std::runtime_error("validate_action: unit type not found: " + actor_unit->type_id);
    }

    // Bundle-authoritative validation
    auto bundle_support = analyze_attack_bundle(*udef);
    if (!bundle_support.supported) {
        D2_LOG_TRACE(kLog, "rejection: reason=unsupported_bundle error={}",
                     d2battle::to_string(bundle_support.error));
        return ActionValidationError::UnsupportedBundle;
    }

    auto bundle_def = require_supported_attack_bundle(*udef);

    // Check target variant matches canonical relation/reach
    if (bundle_def.reach == d2engine::AttackReach::All) {
        if (bundle_def.relation == AttackTargetRelation::Enemy) {
            if (!std::holds_alternative<AllEnemyUnitsTarget>(atk->target))
                return ActionValidationError::TargetShapeMismatch;
        } else {
            if (!std::holds_alternative<AllAlliedUnitsTarget>(atk->target))
                return ActionValidationError::TargetShapeMismatch;
        }
    } else {
        if (!std::holds_alternative<UnitTarget>(atk->target))
            return ActionValidationError::TargetShapeMismatch;
    }

    // For UnitTarget, check the target unit exists
    if (auto* ut = std::get_if<UnitTarget>(&atk->target)) {
        const auto* target_unit = state.find_unit(ut->unit_id);
        if (!target_unit)
            return ActionValidationError::TargetUnknown;

        // Check side alignment
        bool same_side = (target_unit->side == actor_unit->side);
        if (bundle_def.relation == AttackTargetRelation::Enemy) {
            if (same_side)
                return ActionValidationError::FriendlyTarget;
            if (bundle_def.reach == d2engine::AttackReach::Adjacent) {
                auto adj =
                    detail::formation::validate_adjacent_target(state, *actor_unit, *target_unit);
                if (!adj.valid())
                    return adj.error;
            }
        } else {
            if (!same_side)
                return ActionValidationError::HostileTarget;
        }
    }

    // Check that at least one component has eligible targets
    auto prim_targets = resolve_bundle_component_context(
        state, atk->actor_id, atk->target, bundle_def.primary_attack, bundle_def.primary_rule);

    bool sec_has_targets = false;
    if (bundle_def.secondary_attack.has_value() && bundle_def.secondary_rule.has_value()) {
        auto sec_ctx = resolve_bundle_component_context(state, atk->actor_id, atk->target,
                                                        bundle_def.secondary_attack->get(),
                                                        *bundle_def.secondary_rule);
        sec_has_targets = !sec_ctx.target_unit_ids.empty();
    }

    if (prim_targets.target_unit_ids.empty() && !sec_has_targets) {
        D2_LOG_TRACE(kLog, "rejection: bundle reason=no_eligible_targets");
        return ActionValidationError::NoEligibleTargets;
    }

    return ActionValidationError::None;
}

} // namespace detail

ActionValidationError validate_action(const BattleAction& action, const BattleState& state,
                                      const d2engine::GameDataRegistry& game_data) {
    validate_battle_state(state);
    return detail::validate_action_on_valid_state(action, state, game_data);
}

} // namespace d2battle
