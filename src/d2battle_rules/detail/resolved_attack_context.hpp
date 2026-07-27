#pragma once

#include "../battle_action.hpp"
#include "battle_attack_rules.hpp"

#include <d2engine/assets/attack_def.hpp>
#include <d2engine/assets/game_data_registry.hpp>

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace d2battle {
namespace detail {

struct ResolvedAttackContext {
    std::string                                       actor_id;
    std::reference_wrapper<const d2engine::AttackDef> attack;
    std::vector<std::string>                          target_unit_ids;
};

struct ResolvedAttackBundleContext {
    std::string                          actor_id;
    AttackTarget                         selected_target;
    ResolvedAttackContext                primary;
    std::optional<ResolvedAttackContext> secondary;
};

[[nodiscard]] ResolvedAttackBundleContext
resolve_validated_attack_bundle_context(const BattleState& state, const AttackAction& action,
                                        const d2engine::GameDataRegistry& game_data);

[[nodiscard]] ResolvedAttackContext resolve_bundle_component_context(
    const BattleState& state, std::string_view actor_id, const AttackTarget& selected_target,
    const d2engine::AttackDef& attack, const SupportedAttackRule& rule);

} // namespace detail
} // namespace d2battle
