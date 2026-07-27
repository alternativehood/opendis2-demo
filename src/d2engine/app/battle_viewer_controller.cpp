#include "battle_viewer_controller.hpp"

namespace d2engine {

void apply_battle_viewer_action(BattleViewerAction action, BattlePresenter& p) {
    switch (action) {
    case BattleViewerAction::TriggerAttack:
        p.trigger_attack();
        break;
    case BattleViewerAction::SelectNextUnit:
        p.cycle_actor();
        break;
    case BattleViewerAction::SelectNextTarget:
        p.cycle_target();
        break;
    case BattleViewerAction::SetRoleIdle:
        p.preview_role_idle();
        break;
    case BattleViewerAction::SetRoleHit:
        p.preview_role_hit();
        break;
    case BattleViewerAction::SetRoleDeath:
        p.preview_role_death();
        break;
    case BattleViewerAction::SetRoleAttack:
        p.preview_role_attack();
        break;
    case BattleViewerAction::SetRoleHeff:
        p.trigger_heff();
        break;
    case BattleViewerAction::SetRoleTuch:
        p.trigger_tuch();
        break;
    case BattleViewerAction::StepForward:
        p.step_debug_frame(1);
        break;
    case BattleViewerAction::StepBackward:
        p.step_debug_frame(-1);
        break;
    // App-layer actions: handled by Application, not by presenter
    case BattleViewerAction::ToggleDebugOverlay:
    case BattleViewerAction::ToggleLayers:
    case BattleViewerAction::ToggleTransparent:
    case BattleViewerAction::CycleBackground:
    case BattleViewerAction::Quit:
    case BattleViewerAction::None:
        break;
    }
}

} // namespace d2engine
