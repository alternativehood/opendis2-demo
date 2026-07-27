#pragma once

#include "../battle_view/battle_viewer_action.hpp"
#include "debug_tuning_types.hpp"

#include <cstdint>
#include <string>
#include <variant>

namespace d2engine {

// Categories of battle screen actions
// Viewer/presenter actions
// Screen presentation/debug state
struct ToggleDebugHud {};
struct ToggleSoloSelected {};
struct ToggleVisualPause {};
struct RestartScenario {};
struct LogMousePosition {};

// Debug scene manipulation
struct MoveSelectedDebugUnit {
    float dx = 0.0f;
    float dy = 0.0f;
};

// Tuning lifecycle
struct ToggleTuning {};
struct SaveTuning {};
struct RevertAllTuning {};
struct RevertSelectedTuning {};
struct LogSelectedTuning {};

// Tuning edit action (semantic, no SDL)
struct ApplyTuningEdit {
    DebugTuningEditAction edit;
};

// Debug pointer position update (from PointerMoved)
struct UpdateDebugPointerPosition {
    int logical_x = 0;
    int logical_y = 0;
};

// Consumed tuning input (no game action needed, Tab when tuning enabled)
struct ConsumeTuningInput {};

struct SelectDebugItem {
    std::string item_id;
    std::string tree_path;
};

using BattleScreenAction = std::variant<
    // Viewer/presenter actions
    BattleViewerAction,

    // Screen presentation/debug state
    ToggleDebugHud, ToggleSoloSelected, ToggleVisualPause, RestartScenario, LogMousePosition,

    // Debug pointer position
    UpdateDebugPointerPosition,

    // Debug scene manipulation
    MoveSelectedDebugUnit,

    // Tuning lifecycle
    ToggleTuning, SaveTuning, RevertAllTuning, RevertSelectedTuning, LogSelectedTuning,

    // Tuning edit action
    ApplyTuningEdit,

    // Consumed tuning input (no game action needed)
    ConsumeTuningInput, SelectDebugItem>;

} // namespace d2engine
