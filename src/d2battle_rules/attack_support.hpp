#pragma once

#include <d2engine/assets/attack_def.hpp>
#include <d2engine/assets/unit_def.hpp>

#include <string>
#include <string_view>

namespace d2battle {

enum class AttackBundleSupportError {
    None,
    NoPrimaryAttack,
    MissingPrimaryDefinition,
    UnsupportedPrimaryClass,
    UnsupportedPrimaryReach,
    MissingSecondaryDefinition,
    UnsupportedSecondaryClass,
    UnsupportedSecondaryReach,
    IncompatibleTargetRelation,
    IncompatibleReach
};

struct AttackComponentSupport {
    bool                       present;
    std::string                attack_id;
    const d2engine::AttackDef* definition;
    bool                       class_supported;
    bool                       reach_supported;
};

struct AttackBundleSupport {
    bool                     supported;
    AttackBundleSupportError error;
    AttackComponentSupport   primary;
    AttackComponentSupport   secondary;
};

[[nodiscard]] AttackBundleSupport analyze_attack_bundle(const d2engine::UnitDef& unit);

[[nodiscard]] std::string_view to_string(AttackBundleSupportError error);

[[nodiscard]] bool is_attack_class_supported(const d2engine::AttackDef& attack);

[[nodiscard]] bool is_attack_reach_supported(const d2engine::AttackDef& attack);

} // namespace d2battle
