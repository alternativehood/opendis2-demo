#pragma once

#include <d2engine/assets/attack_def.hpp>
#include <d2engine/assets/game_data_registry.hpp>

#include "../battle_action.hpp"
#include "../battle_state.hpp"

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>

namespace d2battle {
namespace detail {

enum class AttackEffectKind {
    Damage,
    Drain,
    DrainOverflow,
    Petrify,
    Heal,
    Cure,
    Revive,
};

enum class AttackTargetRelation {
    Enemy,
    Ally,
};

enum class AttackTargetVitality {
    Alive,
    Dead,
};

struct AttackTargetPolicy {
    AttackTargetRelation relation;
    AttackTargetVitality vitality;

    bool operator==(const AttackTargetPolicy&) const = default;
};

struct SupportedAttackRule {
    AttackEffectKind   effect_kind;
    AttackTargetPolicy target_policy;
};

[[nodiscard]] inline std::optional<SupportedAttackRule>
attack_rule_for_class(d2engine::AttackClass cls) {
    if (cls == d2engine::AttackClass::Damage) {
        return SupportedAttackRule{AttackEffectKind::Damage,
                                   {AttackTargetRelation::Enemy, AttackTargetVitality::Alive}};
    }
    if (cls == d2engine::AttackClass::Drain) {
        return SupportedAttackRule{AttackEffectKind::Drain,
                                   {AttackTargetRelation::Enemy, AttackTargetVitality::Alive}};
    }
    if (cls == d2engine::AttackClass::DrainOverflow) {
        return SupportedAttackRule{AttackEffectKind::DrainOverflow,
                                   {AttackTargetRelation::Enemy, AttackTargetVitality::Alive}};
    }
    if (cls == d2engine::AttackClass::Petrify) {
        return SupportedAttackRule{AttackEffectKind::Petrify,
                                   {AttackTargetRelation::Enemy, AttackTargetVitality::Alive}};
    }
    if (cls == d2engine::AttackClass::Heal) {
        return SupportedAttackRule{AttackEffectKind::Heal,
                                   {AttackTargetRelation::Ally, AttackTargetVitality::Alive}};
    }
    if (cls == d2engine::AttackClass::Cure) {
        return SupportedAttackRule{AttackEffectKind::Cure,
                                   {AttackTargetRelation::Ally, AttackTargetVitality::Alive}};
    }
    if (cls == d2engine::AttackClass::Revive) {
        return SupportedAttackRule{AttackEffectKind::Revive,
                                   {AttackTargetRelation::Ally, AttackTargetVitality::Dead}};
    }
    return std::nullopt;
}

[[nodiscard]] inline std::optional<AttackEffectKind>
effect_kind_for_attack_class(d2engine::AttackClass cls) {
    auto rule = attack_rule_for_class(cls);
    if (rule)
        return rule->effect_kind;
    return std::nullopt;
}

[[nodiscard]] inline bool is_reach_supported(const SupportedAttackRule& rule,
                                             d2engine::AttackReach      reach) {
    if (reach == d2engine::AttackReach::All || reach == d2engine::AttackReach::Any)
        return true;
    if (reach == d2engine::AttackReach::Adjacent)
        return rule.target_policy.relation == AttackTargetRelation::Enemy;
    return false;
}

namespace attack_rules {

[[nodiscard]] inline bool is_attack_class_supported(const d2engine::AttackDef& attack) {
    return attack_rule_for_class(attack.attack_class).has_value();
}

[[nodiscard]] inline bool is_reach_supported(const d2engine::AttackDef& attack) {
    auto rule = attack_rule_for_class(attack.attack_class);
    if (!rule) {
        return false;
    }
    return detail::is_reach_supported(*rule, attack.reach);
}

[[nodiscard]] inline std::string lower_str(std::string s) {
    std::ranges::transform(s, s.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

} // namespace attack_rules
} // namespace detail
} // namespace d2battle
