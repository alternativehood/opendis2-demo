#include "resolved_attack_context.hpp"
#include "attack_target_enumeration.hpp"
#include "battle_attack_rules.hpp"
#include "bundle_support.hpp"

#include <d2engine/assets/game_data_registry.hpp>

#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace d2battle {
namespace detail {

ResolvedAttackContext resolve_bundle_component_context(const BattleState&         state,
                                                       std::string_view           actor_id,
                                                       const AttackTarget&        selected_target,
                                                       const d2engine::AttackDef& attack,
                                                       const SupportedAttackRule& rule) {

    const auto* actor = state.find_unit(std::string(actor_id));
    if (!actor) {
        throw std::runtime_error("resolve_bundle_component_context: actor not found: " +
                                 std::string(actor_id));
    }

    std::vector<std::string> targets;

    if (auto* ut = std::get_if<UnitTarget>(&selected_target)) {
        const auto* target_unit = state.find_unit(ut->unit_id);
        if (target_unit) {
            bool vitality_ok = (rule.target_policy.vitality == AttackTargetVitality::Alive)
                                   ? target_unit->alive
                                   : !target_unit->alive;
            if (vitality_ok)
                targets.push_back(ut->unit_id);
        }
    } else if (std::holds_alternative<AllEnemyUnitsTarget>(selected_target)) {
        if (rule.target_policy.relation != AttackTargetRelation::Enemy) {
            throw std::runtime_error("resolve_bundle_component_context: enemy all-target used with "
                                     "non-enemy rule: actor=" +
                                     actor->id);
        }
        targets = enumerate_unit_targets(state, *actor, rule);
    } else if (std::holds_alternative<AllAlliedUnitsTarget>(selected_target)) {
        if (rule.target_policy.relation != AttackTargetRelation::Ally) {
            throw std::runtime_error("resolve_bundle_component_context: allied all-target used "
                                     "with non-ally rule: actor=" +
                                     actor->id);
        }
        targets = enumerate_unit_targets(state, *actor, rule);
    } else {
        throw std::runtime_error(
            "resolve_bundle_component_context: unknown target variant: actor=" + actor->id);
    }

    // Deduplicate by battle unit ID (large members appear once)
    std::set<std::string>    seen;
    std::vector<std::string> deduped;
    for (const auto& tid : targets) {
        if (seen.insert(tid).second)
            deduped.push_back(tid);
    }

    return ResolvedAttackContext{std::string(actor_id), std::ref(attack), std::move(deduped)};
}

ResolvedAttackBundleContext
resolve_validated_attack_bundle_context(const BattleState& state, const AttackAction& action,
                                        const d2engine::GameDataRegistry& game_data) {

    const auto* actor = state.find_unit(action.actor_id);
    if (!actor) {
        throw std::runtime_error("resolve_validated_attack_bundle_context: actor not found: " +
                                 action.actor_id);
    }

    const auto* udef = game_data.find_unit(actor->type_id);
    if (!udef) {
        throw std::runtime_error(
            "resolve_validated_attack_bundle_context: unit type not found: actor=" +
            action.actor_id + " type=" + actor->type_id);
    }

    auto bundle_def = require_supported_attack_bundle(*udef);

    // Verify target variant matches canonical relation/reach
    if (bundle_def.reach == d2engine::AttackReach::All) {
        if (bundle_def.relation == AttackTargetRelation::Ally) {
            if (!std::holds_alternative<AllAlliedUnitsTarget>(action.target)) {
                throw std::runtime_error(
                    "resolve_validated_attack_bundle_context: expected AllAlliedUnitsTarget "
                    "for Ally/All bundle: actor=" +
                    action.actor_id);
            }
        } else {
            if (!std::holds_alternative<AllEnemyUnitsTarget>(action.target)) {
                throw std::runtime_error(
                    "resolve_validated_attack_bundle_context: expected AllEnemyUnitsTarget "
                    "for Enemy/All bundle: actor=" +
                    action.actor_id);
            }
        }
    } else {
        if (!std::holds_alternative<UnitTarget>(action.target)) {
            throw std::runtime_error("resolve_validated_attack_bundle_context: expected UnitTarget "
                                     "for non-All reach bundle: actor=" +
                                     action.actor_id);
        }
    }

    auto prim_ctx = resolve_bundle_component_context(
        state, action.actor_id, action.target, bundle_def.primary_attack, bundle_def.primary_rule);

    std::optional<ResolvedAttackContext> sec_ctx;
    if (bundle_def.secondary_attack.has_value() && bundle_def.secondary_rule.has_value()) {
        sec_ctx = resolve_bundle_component_context(state, action.actor_id, action.target,
                                                   bundle_def.secondary_attack->get(),
                                                   *bundle_def.secondary_rule);
    }

    // At least one component must have targets
    if (prim_ctx.target_unit_ids.empty() &&
        (!sec_ctx.has_value() || sec_ctx->target_unit_ids.empty())) {
        throw std::runtime_error(
            "resolve_validated_attack_bundle_context: both component target lists empty: actor=" +
            action.actor_id);
    }

    return ResolvedAttackBundleContext{action.actor_id, action.target, std::move(prim_ctx),
                                       std::move(sec_ctx)};
}

} // namespace detail
} // namespace d2battle
