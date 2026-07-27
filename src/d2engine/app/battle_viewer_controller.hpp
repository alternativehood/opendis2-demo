#pragma once

#include "../battle_view/battle_presenter.hpp"
#include "../battle_view/battle_scene.hpp"
#include "../battle_view/battle_viewer_action.hpp"

namespace d2engine {

// Translates BattleViewerAction to domain-level BattlePresenter calls.
// Presenter-specific actions are handled by the BattleViewerController.
// Screen-level actions (Quit, ToggleLayers, etc.) are handled by BattleScreen.
// Application does not participate in Battle key/action interpretation.
void apply_battle_viewer_action(BattleViewerAction action, BattlePresenter& presenter);

} // namespace d2engine
