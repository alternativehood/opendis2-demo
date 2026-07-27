#include "battle_valid_actions.hpp"
#include "attack_support.hpp"
#include "battle_action_validate.hpp"
#include "battle_validate.hpp"
#include "detail/attack_target_enumeration.hpp"
#include "detail/battle_action_validate_internal.hpp"
#include "detail/battle_attack_rules.hpp"
#include "detail/battle_formation.hpp"
#include "detail/battle_valid_actions_internal.hpp"
#include "detail/bundle_support.hpp"
#include "detail/unit_effects.hpp"

#include <d2log/log.hpp>

#include <stdexcept>
#include <string>
#include <vector>

namespace d2battle {

namespace {
auto kLog = d2log::get("d2battle.valid_actions");
} // namespace

namespace detail {

std::vector<BattleAction>
valid_actions_on_valid_state(const BattleState&                state,
                             const d2engine::GameDataRegistry& game_data) {
    if (state.status != BattleStatus::InProgress) {
        D2_LOG_DEBUG(kLog, "valid_actions: battle not in progress");
        return {};
    }

    const auto* actor = state.current_actor();
    if (!actor) {
        D2_LOG_DEBUG(kLog, "valid_actions: no current actor");
        return {};
    }

    D2_LOG_INFO(kLog, "valid_actions: current_actor={} ({})", actor->id, actor->type_id);

    if (detail::is_petrified(*actor)) {
        D2_LOG_DEBUG(kLog, "valid_actions: actor {} is PETRIFIED, emitting SkipActivationAction",
                     actor->id);
        std::vector<BattleAction> forced;
        forced.emplace_back(std::in_place_type<SkipActivationAction>, actor->id,
                            SkipActivationReason::Petrified);
        return forced;
    }

    const auto* udef = game_data.find_unit(actor->type_id);
    if (!udef) {
        D2_LOG_ERROR(kLog, "valid_actions: unit type {} not in GameDataRegistry", actor->type_id);
        throw std::runtime_error("valid_actions: unit type not found: " + actor->type_id);
    }

    auto bundle_support = analyze_attack_bundle(*udef);
    if (!bundle_support.supported) {
        D2_LOG_DEBUG(kLog, "  bundle UNSUPPORTED: {}", d2battle::to_string(bundle_support.error));
        D2_LOG_WARN(kLog, "NO VALID ACTIONS DIAGNOSTIC");
        D2_LOG_WARN(kLog, "  actor={} ({})", actor->id, actor->type_id);
        D2_LOG_WARN(kLog,
                    "  bundle: support=no error={} primary(present={} class={} reach={}) "
                    "secondary(present={} class={} reach={})",
                    d2battle::to_string(bundle_support.error), bundle_support.primary.present,
                    bundle_support.primary.class_supported, bundle_support.primary.reach_supported,
                    bundle_support.secondary.present, bundle_support.secondary.class_supported,
                    bundle_support.secondary.reach_supported);
        return {};
    }

    auto bundle_def = require_supported_attack_bundle(*udef);

    std::vector<BattleAction> actions;
    const auto                targets = enumerate_bundle_unit_targets(state, *actor, bundle_def);

    if (bundle_def.reach == d2engine::AttackReach::All) {
        if (targets.empty()) {
            D2_LOG_TRACE(kLog, "  all_target: no_eligible_targets (ordered union empty)");
            return {};
        }

        AttackTarget all_target = (bundle_def.relation == AttackTargetRelation::Ally)
                                      ? AttackTarget{AllAlliedUnitsTarget{}}
                                      : AttackTarget{AllEnemyUnitsTarget{}};
        AttackAction candidate{actor->id, all_target};
        auto         err =
            detail::validate_action_on_valid_state(BattleAction{candidate}, state, game_data);
        if (err != ActionValidationError::None) {
            throw std::runtime_error(
                "valid_actions: production all-target candidate rejected: actor=" + actor->id +
                " error=" + to_string(err));
        }
        actions.emplace_back(candidate);
    } else {
        for (const auto& target_id : targets) {
            AttackAction candidate{actor->id, UnitTarget{target_id}};
            auto         err =
                detail::validate_action_on_valid_state(BattleAction{candidate}, state, game_data);
            if (err != ActionValidationError::None) {
                throw std::runtime_error(
                    "valid_actions: production enumerator rejected candidate: actor=" + actor->id +
                    " target=" + target_id + " error=" + to_string(err));
            }
            actions.emplace_back(candidate);
        }
    }

    D2_LOG_INFO(kLog, "valid_actions: {} actions", actions.size());

    if (actions.empty()) {
        D2_LOG_WARN(kLog, "NO VALID ACTIONS DIAGNOSTIC");
        D2_LOG_WARN(kLog, "  actor={} ({})", actor->id, actor->type_id);
        D2_LOG_WARN(kLog,
                    "  bundle: support=yes primary(present={} class={} reach={}) "
                    "secondary(present={} class={} reach={})",
                    bundle_support.primary.present, bundle_support.primary.class_supported,
                    bundle_support.primary.reach_supported, bundle_support.secondary.present,
                    bundle_support.secondary.class_supported,
                    bundle_support.secondary.reach_supported);
    }

    return actions;
}

} // namespace detail

std::vector<BattleAction> valid_actions(const BattleState&                state,
                                        const d2engine::GameDataRegistry& game_data) {
    validate_battle_state(state);
    return detail::valid_actions_on_valid_state(state, game_data);
}

} // namespace d2battle
