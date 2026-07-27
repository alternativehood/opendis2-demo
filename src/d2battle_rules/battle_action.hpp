#pragma once

#include "battle_state.hpp"

#include <cstdint>
#include <variant>
#include <vector>

namespace d2battle {

struct UnitTarget {
    std::string unit_id;
    bool        operator==(const UnitTarget&) const = default;
};

struct AllEnemyUnitsTarget {
    bool operator==(const AllEnemyUnitsTarget&) const = default;
    auto operator<=>(const AllEnemyUnitsTarget&) const = default;
};

struct AllAlliedUnitsTarget {
    bool operator==(const AllAlliedUnitsTarget&) const = default;
    auto operator<=>(const AllAlliedUnitsTarget&) const = default;
};

using AttackTarget = std::variant<UnitTarget, AllEnemyUnitsTarget, AllAlliedUnitsTarget>;

struct AttackAction {
    std::string  actor_id;
    AttackTarget target;

    bool operator==(const AttackAction&) const = default;
};

enum class SkipActivationReason : std::uint8_t {
    Petrified,
};

struct SkipActivationAction {
    std::string          actor_id;
    SkipActivationReason reason = SkipActivationReason::Petrified;

    bool operator==(const SkipActivationAction&) const = default;
};

using BattleAction = std::variant<AttackAction, SkipActivationAction>;

struct BattleActionOutcome {
    BattleAction action;
    BattleState  outcome;
};

} // namespace d2battle
