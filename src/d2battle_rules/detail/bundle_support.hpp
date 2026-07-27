#pragma once

#include "attack_target_enumeration.hpp"
#include "battle_attack_rules.hpp"

#include <d2engine/assets/game_data_registry.hpp>

#include <functional>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace d2battle {
namespace detail {

struct ResolvedAttackBundleDefinition {
    std::reference_wrapper<const d2engine::AttackDef>                primary_attack;
    SupportedAttackRule                                              primary_rule;
    std::optional<std::reference_wrapper<const d2engine::AttackDef>> secondary_attack;
    std::optional<SupportedAttackRule>                               secondary_rule;
    AttackTargetRelation                                             relation;
    d2engine::AttackReach                                            reach;
};

[[nodiscard]] ResolvedAttackBundleDefinition
require_supported_attack_bundle(const d2engine::UnitDef& unit);

[[nodiscard]] std::vector<std::string>
enumerate_bundle_unit_targets(const BattleState& state, const BattleUnitState& actor,
                              const ResolvedAttackBundleDefinition& bundle);

} // namespace detail
} // namespace d2battle
