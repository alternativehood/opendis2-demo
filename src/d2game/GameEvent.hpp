#pragma once

#include <d2adventure_rules/AdventurePathfinder.hpp>
#include <d2adventure_rules/AdventureMovementPolicy.hpp>
#include <d2runtime/MapCellCoord.hpp>
#include <d2runtime/AdventureIsoDirection.hpp>

#include "AdventureMovementDebug.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <utility>
#include <vector>

namespace d2game {

struct GameQuitRequested {};

struct GameInspectResult {
    std::string world_summary;
};

enum class AdventureMovementAction : std::uint8_t {
    SelectStack,
    ClearSelection,
    PlanRoute,
    ConfirmRoute,
    AdvanceStep,
    DebugResetMovementPoints,
    DebugGrantFreeMovementPoints,
};

enum class AdventureMovementRejectionReason : std::uint8_t {
    Busy,
    StackNotFound,
    StackNotVisible,
    NoSelectedStack,
    LeaderInstanceMissing,
    MovementProfileUnavailable,
    NavigationMapInvalid,
    PathUnavailable,
    NoPlannedRoute,
    SelectedStackChanged,
    StackPositionChanged,
    MovementProfileChanged,
    RouteChanged,
    InsufficientMovementPoints,
    NoActiveMovement,
    ExecutionStateInvalid,
    StepBlocked,
    StepCostChanged,
    InsufficientMovementPointsDuringExecution,
    InitialMovementPointsUnavailable,
};

enum class AdventureMovementInterruptionReason : std::uint8_t {
    StackMissing,
    StackNotVisible,
    PositionMismatch,
    NavigationMapInvalid,
    StepBlocked,
    StepCostChanged,
    InsufficientMovementPoints,
    InvalidRouteState,
};

struct AdventureStackSelected {
    std::string stack_id;
};
struct AdventureSelectionCleared {
    std::string previous_stack_id;
};
struct AdventureRoutePlanned {
    std::string                stack_id;
    d2runtime::MapCellCoord    destination{};
    std::int64_t               total_cost = 0;
    int                        available_movement_points = 0;
    std::size_t                affordable_step_count = 0;
    std::optional<std::size_t> first_unaffordable_step_index;
    bool                       refreshed = false;
};
struct AdventureMovementNotRequired {
    std::string             stack_id;
    d2runtime::MapCellCoord cell{};
};
struct AdventureMovementStarted {
    std::string             stack_id;
    d2runtime::MapCellCoord start{};
    d2runtime::MapCellCoord requested_destination{};
    d2runtime::MapCellCoord execution_destination{};
    std::size_t             step_count = 0;
    std::int64_t            total_cost = 0;
    bool                    limited_by_movement_points = false;
};
struct AdventureMovementStepCommitted {
    std::string                      stack_id;
    d2runtime::MapCellCoord          from{};
    d2runtime::MapCellCoord          to{};
    std::size_t                      step_index = 0;
    int                              step_cost = 0;
    std::int64_t                     spent_cost = 0;
    int                              remaining_movement_points = 0;
    d2runtime::AdventureIsoDirection direction = d2runtime::AdventureIsoDirection::D0;
};
struct AdventureMovementCompleted {
    std::string             stack_id;
    d2runtime::MapCellCoord final_position{};
    d2runtime::MapCellCoord requested_destination{};
    std::int64_t            total_spent_cost = 0;
    int                     remaining_movement_points = 0;
    bool                    limited_by_movement_points = false;
};
struct AdventureMovementRejected {
    AdventureMovementAction          action = AdventureMovementAction::PlanRoute;
    AdventureMovementRejectionReason reason = AdventureMovementRejectionReason::PathUnavailable;
    std::optional<d2adventure::AdventurePathStatus> path_status;
    d2adventure::AdventureMovementBlockReason       block_reason =
        d2adventure::AdventureMovementBlockReason::None;
    std::size_t navigation_error_count = 0;

    AdventureMovementRejected() = default;
    AdventureMovementRejected(
        AdventureMovementAction action_value, AdventureMovementRejectionReason reason_value,
        std::optional<d2adventure::AdventurePathStatus> path_status_value = {},
        d2adventure::AdventureMovementBlockReason       block_reason_value =
            d2adventure::AdventureMovementBlockReason::None,
        std::size_t navigation_error_count_value = 0)
        : action(action_value), reason(reason_value), path_status(std::move(path_status_value)),
          block_reason(block_reason_value), navigation_error_count(navigation_error_count_value) {}
};
struct AdventureMovementInterrupted {
    std::string                         stack_id;
    AdventureMovementInterruptionReason reason =
        AdventureMovementInterruptionReason::InvalidRouteState;
    std::size_t                               next_step_index = 0;
    d2adventure::AdventureMovementBlockReason block_reason =
        d2adventure::AdventureMovementBlockReason::None;
};

struct AdventureMovementPointsDebugChanged {
    AdventureMovementDebugChange change = AdventureMovementDebugChange::Reset;
    std::string                  stack_id;
    int                          previous_movement_points = 0;
    int                          current_movement_points = 0;
    int                          reset_movement_points = 0;
};

using GameEvent =
    std::variant<GameQuitRequested, GameInspectResult, AdventureStackSelected,
                 AdventureSelectionCleared, AdventureRoutePlanned, AdventureMovementNotRequired,
                 AdventureMovementStarted, AdventureMovementStepCommitted,
                 AdventureMovementCompleted, AdventureMovementRejected,
                 AdventureMovementInterrupted, AdventureMovementPointsDebugChanged>;

struct GameCommandResult {
    bool                   quit_requested = false;
    std::vector<GameEvent> events;
};

} // namespace d2game
