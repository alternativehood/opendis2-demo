#include "attack_support.hpp"
#include "detail/battle_attack_rules.hpp"

namespace d2battle {

bool is_attack_class_supported(const d2engine::AttackDef& attack) {
    return detail::attack_rules::is_attack_class_supported(attack);
}

bool is_attack_reach_supported(const d2engine::AttackDef& attack) {
    return detail::attack_rules::is_reach_supported(attack);
}

AttackBundleSupport analyze_attack_bundle(const d2engine::UnitDef& unit) {
    AttackBundleSupport result{};

    auto make_component = [](const std::string&         raw_id,
                             const d2engine::AttackDef* def) -> AttackComponentSupport {
        AttackComponentSupport comp{};
        comp.present = !raw_id.empty();
        comp.attack_id = raw_id;
        comp.definition = def;
        if (def) {
            comp.class_supported = detail::attack_rules::is_attack_class_supported(*def);
            if (comp.class_supported) {
                comp.reach_supported = detail::attack_rules::is_reach_supported(*def);
            } else {
                comp.reach_supported = (def->reach == d2engine::AttackReach::All ||
                                        def->reach == d2engine::AttackReach::Any);
            }
        } else {
            comp.class_supported = false;
            comp.reach_supported = false;
        }
        return comp;
    };

    result.primary = make_component(unit.primary_attack_id, unit.primary_attack);
    result.secondary = make_component(unit.secondary_attack_id, unit.secondary_attack);

    // Strict error order
    if (!result.primary.present) {
        result.supported = false;
        result.error = AttackBundleSupportError::NoPrimaryAttack;
        return result;
    }

    if (!result.primary.definition) {
        result.supported = false;
        result.error = AttackBundleSupportError::MissingPrimaryDefinition;
        return result;
    }

    if (!result.primary.class_supported) {
        result.supported = false;
        result.error = AttackBundleSupportError::UnsupportedPrimaryClass;
        return result;
    }

    if (!result.primary.reach_supported) {
        result.supported = false;
        result.error = AttackBundleSupportError::UnsupportedPrimaryReach;
        return result;
    }

    // No secondary -> bundle is supported
    if (!result.secondary.present) {
        result.supported = true;
        result.error = AttackBundleSupportError::None;
        return result;
    }

    if (!result.secondary.definition) {
        result.supported = false;
        result.error = AttackBundleSupportError::MissingSecondaryDefinition;
        return result;
    }

    if (!result.secondary.class_supported) {
        result.supported = false;
        result.error = AttackBundleSupportError::UnsupportedSecondaryClass;
        return result;
    }

    if (!result.secondary.reach_supported) {
        result.supported = false;
        result.error = AttackBundleSupportError::UnsupportedSecondaryReach;
        return result;
    }

    auto prim_rule = detail::attack_rule_for_class(result.primary.definition->attack_class);
    auto sec_rule = detail::attack_rule_for_class(result.secondary.definition->attack_class);

    if (!prim_rule || !sec_rule) {
        result.supported = false;
        result.error = AttackBundleSupportError::UnsupportedPrimaryClass;
        return result;
    }

    if (prim_rule->target_policy.relation != sec_rule->target_policy.relation) {
        result.supported = false;
        result.error = AttackBundleSupportError::IncompatibleTargetRelation;
        return result;
    }

    if (result.primary.definition->reach != result.secondary.definition->reach) {
        result.supported = false;
        result.error = AttackBundleSupportError::IncompatibleReach;
        return result;
    }

    result.supported = true;
    result.error = AttackBundleSupportError::None;
    return result;
}

std::string_view to_string(AttackBundleSupportError error) {
    switch (error) {
    case AttackBundleSupportError::None:
        return "none";
    case AttackBundleSupportError::NoPrimaryAttack:
        return "no_primary_attack";
    case AttackBundleSupportError::MissingPrimaryDefinition:
        return "missing_primary_definition";
    case AttackBundleSupportError::UnsupportedPrimaryClass:
        return "unsupported_primary_class";
    case AttackBundleSupportError::UnsupportedPrimaryReach:
        return "unsupported_primary_reach";
    case AttackBundleSupportError::MissingSecondaryDefinition:
        return "missing_secondary_definition";
    case AttackBundleSupportError::UnsupportedSecondaryClass:
        return "unsupported_secondary_class";
    case AttackBundleSupportError::UnsupportedSecondaryReach:
        return "unsupported_secondary_reach";
    case AttackBundleSupportError::IncompatibleTargetRelation:
        return "incompatible_target_relation";
    case AttackBundleSupportError::IncompatibleReach:
        return "incompatible_reach";
    }
    return "unknown";
}

} // namespace d2battle
