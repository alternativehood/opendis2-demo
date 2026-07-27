#pragma once

#include <d2engine/assets/game_data_registry.hpp>

#include "battle_action.hpp"
#include "battle_state.hpp"

namespace d2battle {

enum class ActionValidationError {
    None,
    BattleFinished,
    ActorUnknown,
    ActorNotCurrent,
    AttackNotAvailable,
    UnsupportedAttackClass,
    UnsupportedReach,
    TargetShapeMismatch,
    ActorNotOnActiveMeleeRank,
    TargetProtectedByFrontRank,
    TargetOutOfAdjacentReach,
    TargetUnknown,
    FriendlyTarget,
    HostileTarget,
    TargetDead,
    TargetAlive,
    ActorIncapacitated,
    SkipNotRequired,
    SkipReasonMismatch,
    NoEligibleTargets,
    UnsupportedBundle,
};

[[nodiscard]] ActionValidationError validate_action(const BattleAction&               action,
                                                    const BattleState&                state,
                                                    const d2engine::GameDataRegistry& game_data);

[[nodiscard]] const char* to_string(ActionValidationError err);

} // namespace d2battle
