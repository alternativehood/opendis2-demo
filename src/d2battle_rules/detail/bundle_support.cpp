#include "bundle_support.hpp"
#include "../attack_support.hpp"
#include "../battle_action_validate.hpp"
#include "attack_target_enumeration.hpp"
#include "battle_formation.hpp"

#include <set>
#include <stdexcept>

namespace d2battle {
namespace detail {

ResolvedAttackBundleDefinition require_supported_attack_bundle(const d2engine::UnitDef& unit) {
    auto support = analyze_attack_bundle(unit);
    if (!support.supported) {
        throw std::runtime_error("require_supported_attack_bundle: unit=" + unit.unit_id +
                                 " support_error=" + std::string(to_string(support.error)));
    }

    auto prim_rule = attack_rule_for_class(support.primary.definition->attack_class);
    if (!prim_rule) {
        throw std::runtime_error("require_supported_attack_bundle: invariant failure — "
                                 "supported primary has no rule: unit=" +
                                 unit.unit_id);
    }

    AttackTargetRelation  relation = prim_rule->target_policy.relation;
    d2engine::AttackReach reach = support.primary.definition->reach;

    std::optional<std::reference_wrapper<const d2engine::AttackDef>> sec_attack;
    std::optional<SupportedAttackRule>                               sec_rule;

    if (support.secondary.present) {
        if (!support.secondary.definition) {
            throw std::runtime_error("require_supported_attack_bundle: invariant failure — "
                                     "present secondary has no definition: unit=" +
                                     unit.unit_id);
        }
        sec_attack = std::ref(*support.secondary.definition);
        auto sr = attack_rule_for_class(support.secondary.definition->attack_class);
        if (!sr) {
            throw std::runtime_error("require_supported_attack_bundle: invariant failure — "
                                     "supported secondary has no rule: unit=" +
                                     unit.unit_id);
        }
        sec_rule = sr;
    }

    return ResolvedAttackBundleDefinition{
        std::ref(*support.primary.definition), *prim_rule, sec_attack, sec_rule, relation, reach};
}

std::vector<std::string>
enumerate_bundle_unit_targets(const BattleState& state, const BattleUnitState& actor,
                              const ResolvedAttackBundleDefinition& bundle) {
    std::vector<std::string> targets;
    std::set<std::string>    seen;

    auto append_targets = [&](const SupportedAttackRule& rule) {
        for (const auto& target_id : enumerate_unit_targets(state, actor, rule)) {
            if (seen.insert(target_id).second) {
                targets.push_back(target_id);
            }
        }
    };

    append_targets(bundle.primary_rule);
    if (bundle.secondary_rule.has_value()) {
        append_targets(*bundle.secondary_rule);
    }

    if (bundle.reach == d2engine::AttackReach::Adjacent) {
        std::vector<std::string> adjacent_targets;
        adjacent_targets.reserve(targets.size());
        for (const auto& target_id : targets) {
            const auto* target = state.find_unit(target_id);
            if (!target) {
                throw std::runtime_error("enumerate_bundle_unit_targets: target missing: actor=" +
                                         actor.id + " target=" + target_id);
            }
            auto adjacency = formation::validate_adjacent_target(state, actor, *target);
            if (adjacency.valid()) {
                adjacent_targets.push_back(target_id);
                continue;
            }
            if (adjacency.error == ActionValidationError::TargetProtectedByFrontRank ||
                adjacency.error == ActionValidationError::TargetOutOfAdjacentReach) {
                continue;
            }
            if (adjacency.error == ActionValidationError::TargetDead ||
                adjacency.error == ActionValidationError::TargetAlive ||
                adjacency.error == ActionValidationError::FriendlyTarget ||
                adjacency.error == ActionValidationError::HostileTarget) {
                throw std::runtime_error(
                    "enumerate_bundle_unit_targets: invariant violation: actor=" + actor.id +
                    " target=" + target_id + " error=" + std::string(to_string(adjacency.error)));
            }
            throw std::runtime_error(
                "enumerate_bundle_unit_targets: adjacency validation error: actor=" + actor.id +
                " target=" + target_id + " error=" + std::string(to_string(adjacency.error)));
        }
        return adjacent_targets;
    }

    return targets;
}

} // namespace detail
} // namespace d2battle
