#pragma once

#include <d2runtime/MapCellCoord.hpp>

#include <string>
#include <variant>

namespace d2game {

struct GameQuitCommand {};
struct GameNoOpCommand {};
struct GameInspectWorldCommand {};
struct GameAdvanceFrameCommand {};

struct GameSelectAdventureStackCommand {
    std::string stack_id;
};

struct GameClearAdventureSelectionCommand {};

struct GamePlanAdventureMovementCommand {
    d2runtime::MapCellCoord destination;
};

struct GameConfirmAdventureMovementCommand {};
struct GameAdvanceAdventureMovementStepCommand {};
struct GameDebugResetSelectedAdventureMovementPointsCommand {};
struct GameDebugGrantSelectedAdventureFreeMovementPointsCommand {};

using GameCommand =
    std::variant<GameQuitCommand, GameNoOpCommand, GameInspectWorldCommand, GameAdvanceFrameCommand,
                 GameSelectAdventureStackCommand, GameClearAdventureSelectionCommand,
                 GamePlanAdventureMovementCommand, GameConfirmAdventureMovementCommand,
                 GameAdvanceAdventureMovementStepCommand,
                 GameDebugResetSelectedAdventureMovementPointsCommand,
                 GameDebugGrantSelectedAdventureFreeMovementPointsCommand>;

} // namespace d2game
