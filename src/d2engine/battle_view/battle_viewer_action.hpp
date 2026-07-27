#pragma once

#include <cstdint>

namespace d2engine {

enum class BattleViewerAction : std::uint8_t {
    None,
    Quit,
    ToggleDebugOverlay,
    StepForward,
    StepBackward,
    SelectNextUnit,
    SelectNextTarget,
    SetRoleIdle,
    SetRoleHit,
    SetRoleDeath,
    SetRoleAttack,
    SetRoleHeff,
    SetRoleTuch,
    TriggerAttack,
    ToggleLayers,
    ToggleTransparent,
    CycleBackground,
};

} // namespace d2engine
