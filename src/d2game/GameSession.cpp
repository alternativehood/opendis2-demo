#include "GameSession.hpp"

#include <d2adventure_rules/AdventureNavigationMapBuilder.hpp>
#include <d2adventure_rules/AdventurePathfinder.hpp>
#include <d2adventure_rules/AdventureRoutePreviewBuilder.hpp>
#include <d2adventure_rules/AdventureRoutePrefix.hpp>
#include <d2runtime/AdventureMovementDirection.hpp>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <sstream>
#include <string>
#include <utility>
#include <variant>

namespace d2game {

GameSession::GameSession(d2runtime::AdventureWorldState world, std::size_t build_warnings,
                         std::size_t                         build_errors,
                         AdventureUnitMovementProfileCatalog movement_profiles)
    : world_(std::move(world)), build_warnings_(build_warnings), build_errors_(build_errors),
      movement_profiles_(std::move(movement_profiles)) {
    initial_stack_movement_points_.reserve(world_.stacks.size());
    for (const auto& stack : world_.stacks)
        initial_stack_movement_points_.push_back({stack.id, std::max(stack.move, 0)});
    std::sort(initial_stack_movement_points_.begin(), initial_stack_movement_points_.end(),
              [](const auto& left, const auto& right) { return left.stack_id < right.stack_id; });
    for (std::size_t index = 1; index < initial_stack_movement_points_.size(); ++index) {
        if (initial_stack_movement_points_[index - 1].stack_id ==
            initial_stack_movement_points_[index].stack_id)
            throw std::invalid_argument("duplicate adventure stack ID");
    }
}

GameCommandResult GameSession::handle_command(const GameCommand& command) {
    return std::visit([this](const auto& typed_command) { return handle(typed_command); }, command);
}

std::optional<d2adventure::AdventureRoutePreview> GameSession::adventure_route_preview() const {
    if (adventure_movement_state_.mode == AdventureInteractionMode::RoutePlanned) {
        if (!adventure_movement_state_.planned || !adventure_movement_state_.selected_stack_id)
            throw std::logic_error("planned movement state is incomplete");
        const auto& planned = *adventure_movement_state_.planned;
        if (planned.stack_id != *adventure_movement_state_.selected_stack_id)
            throw std::logic_error("planned movement does not match selected stack");
        return d2adventure::AdventureRoutePreviewBuilder{}.build(
            planned.route, planned.available_movement_points,
            planned.first_unaffordable_step_index);
    }
    if (adventure_movement_state_.mode != AdventureInteractionMode::Moving)
        return std::nullopt;
    if (!adventure_movement_state_.execution || !adventure_movement_state_.selected_stack_id)
        throw std::logic_error("moving state is incomplete");
    const auto& execution = *adventure_movement_state_.execution;
    if (execution.stack_id != *adventure_movement_state_.selected_stack_id)
        throw std::logic_error("moving execution does not match selected stack");
    if (execution.route.empty() || execution.next_step_index >= execution.route.steps.size())
        throw std::logic_error("moving execution has no remaining route step");
    const auto* stack = world_.find_stack(execution.stack_id);
    if (stack == nullptr || !d2runtime::is_stack_on_adventure_map(*stack))
        throw std::logic_error("moving stack is unavailable");
    const auto budget = static_cast<std::int64_t>(stack->move) + execution.spent_cost;
    if (budget < 0 || budget > std::numeric_limits<int>::max())
        throw std::logic_error("moving movement-point budget is invalid");
    return d2adventure::AdventureRoutePreviewBuilder{}.build(
        execution.route, static_cast<int>(budget), std::nullopt, execution.next_step_index);
}

AdventureMovementDebugSnapshot GameSession::adventure_movement_debug_snapshot() const {
    AdventureMovementDebugSnapshot snapshot;
    snapshot.mode = adventure_movement_state_.mode;
    snapshot.selected_stack_id = adventure_movement_state_.selected_stack_id;
    if (!snapshot.selected_stack_id)
        return snapshot;
    const auto* stack = world_.find_stack(*snapshot.selected_stack_id);
    if (stack == nullptr)
        return snapshot;
    snapshot.current_movement_points = stack->move;
    snapshot.reset_movement_points = initial_movement_points(stack->id);
    return snapshot;
}

GameCommandResult GameSession::handle(const GameQuitCommand&) {
    GameCommandResult result;
    result.quit_requested = true;
    result.events.emplace_back(GameQuitRequested{});
    return result;
}

GameCommandResult GameSession::handle(const GameNoOpCommand&) {
    return {};
}

GameCommandResult GameSession::handle(const GameAdvanceFrameCommand&) {
    return {};
}

GameCommandResult GameSession::handle(const GameDebugResetSelectedAdventureMovementPointsCommand&) {
    return change_selected_movement_points(AdventureMovementDebugChange::Reset);
}

GameCommandResult
GameSession::handle(const GameDebugGrantSelectedAdventureFreeMovementPointsCommand&) {
    return change_selected_movement_points(AdventureMovementDebugChange::Free);
}

GameCommandResult GameSession::handle(const GameInspectWorldCommand&) {
    GameCommandResult  result;
    std::ostringstream ss;
    ss << "Scenario: " << world_.scenario_id << " (\"" << world_.scenario_name << "\")\n"
       << "Map: " << world_.map_width << "x" << world_.map_height << "\n"
       << "Terrain tiles: " << world_.terrain_tiles << "\n"
       << "Semantic objects: " << world_.semantic_object_count << "\n"
       << "Runtime objects: " << world_.runtime_object_count << "\n"
       << "Build warnings: " << build_warnings_ << "\n"
       << "Build errors: " << build_errors_;
    result.events.emplace_back(GameInspectResult{ss.str()});
    return result;
}

GameCommandResult GameSession::handle(const GameSelectAdventureStackCommand& command) {
    GameCommandResult result;
    if (adventure_movement_state_.is_moving()) {
        result.events.emplace_back(AdventureMovementRejected{
            AdventureMovementAction::SelectStack, AdventureMovementRejectionReason::Busy});
        return result;
    }
    const auto* stack = world_.find_stack(command.stack_id);
    if (stack == nullptr) {
        result.events.emplace_back(AdventureMovementRejected{
            AdventureMovementAction::SelectStack, AdventureMovementRejectionReason::StackNotFound});
        return result;
    }
    if (!d2runtime::is_stack_on_adventure_map(*stack) &&
        !world_.find_contained_stack_location(*stack).has_value()) {
        result.events.emplace_back(
            AdventureMovementRejected{AdventureMovementAction::SelectStack,
                                      AdventureMovementRejectionReason::StackNotVisible});
        return result;
    }
    adventure_movement_state_.mode = AdventureInteractionMode::StackSelected;
    adventure_movement_state_.selected_stack_id = stack->id;
    adventure_movement_state_.planned.reset();
    adventure_movement_state_.execution.reset();
    result.events.emplace_back(AdventureStackSelected{stack->id});
    return result;
}

GameCommandResult GameSession::handle(const GameClearAdventureSelectionCommand&) {
    GameCommandResult result;
    if (adventure_movement_state_.is_moving()) {
        result.events.emplace_back(AdventureMovementRejected{
            AdventureMovementAction::ClearSelection, AdventureMovementRejectionReason::Busy});
        return result;
    }
    const auto previous = adventure_movement_state_.selected_stack_id.value_or("");
    reset_idle();
    result.events.emplace_back(AdventureSelectionCleared{previous});
    return result;
}

GameCommandResult GameSession::handle(const GamePlanAdventureMovementCommand& command) {
    GameCommandResult result;
    if (adventure_movement_state_.is_moving()) {
        result.events.emplace_back(AdventureMovementRejected{
            AdventureMovementAction::PlanRoute, AdventureMovementRejectionReason::Busy});
        return result;
    }
    adventure_movement_state_.planned.reset();
    if (adventure_movement_state_.selected_stack_id) {
        adventure_movement_state_.mode = AdventureInteractionMode::StackSelected;
    } else {
        adventure_movement_state_.mode = AdventureInteractionMode::Idle;
    }

    if (!adventure_movement_state_.selected_stack_id) {
        result.events.emplace_back(AdventureMovementRejected{
            AdventureMovementAction::PlanRoute, AdventureMovementRejectionReason::NoSelectedStack});
        return result;
    }
    const auto* selected = selected_stack();
    if (selected == nullptr) {
        reset_idle();
        result.events.emplace_back(AdventureMovementRejected{
            AdventureMovementAction::PlanRoute, AdventureMovementRejectionReason::StackNotFound});
        return result;
    }
    auto* stack = mutable_stack(*adventure_movement_state_.selected_stack_id);
    if (!d2runtime::is_stack_on_adventure_map(*stack)) {
        result.events.emplace_back(AdventureMovementRejected{
            AdventureMovementAction::PlanRoute, AdventureMovementRejectionReason::StackNotVisible});
        return result;
    }
    const auto* leader = leader_instance(*stack);
    if (leader == nullptr) {
        result.events.emplace_back(
            AdventureMovementRejected{AdventureMovementAction::PlanRoute,
                                      AdventureMovementRejectionReason::LeaderInstanceMissing});
        return result;
    }
    const auto profile = movement_profile(*stack);
    if (!profile) {
        result.events.emplace_back(AdventureMovementRejected{
            AdventureMovementAction::PlanRoute,
            AdventureMovementRejectionReason::MovementProfileUnavailable});
        return result;
    }
    const auto route = find_route(*stack, *profile, command.destination, result,
                                  AdventureMovementAction::PlanRoute);
    if (!route)
        return result;
    adventure_movement_state_.planned = planned_movement(*stack, *profile, *route);
    adventure_movement_state_.mode = AdventureInteractionMode::RoutePlanned;
    const auto& planned = *adventure_movement_state_.planned;
    result.events.emplace_back(
        AdventureRoutePlanned{planned.stack_id, planned.route.destination, planned.route.total_cost,
                              planned.available_movement_points, planned.affordable_step_count,
                              planned.first_unaffordable_step_index, false});
    return result;
}

GameCommandResult GameSession::handle(const GameConfirmAdventureMovementCommand&) {
    GameCommandResult result;
    if (adventure_movement_state_.is_moving()) {
        result.events.emplace_back(AdventureMovementRejected{
            AdventureMovementAction::ConfirmRoute, AdventureMovementRejectionReason::Busy});
        return result;
    }
    if (!adventure_movement_state_.selected_stack_id) {
        result.events.emplace_back(
            AdventureMovementRejected{AdventureMovementAction::ConfirmRoute,
                                      AdventureMovementRejectionReason::NoSelectedStack});
        return result;
    }
    if (!adventure_movement_state_.planned) {
        result.events.emplace_back(
            AdventureMovementRejected{AdventureMovementAction::ConfirmRoute,
                                      AdventureMovementRejectionReason::NoPlannedRoute});
        return result;
    }
    auto& planned = *adventure_movement_state_.planned;
    if (planned.stack_id != *adventure_movement_state_.selected_stack_id) {
        adventure_movement_state_.planned.reset();
        adventure_movement_state_.mode = AdventureInteractionMode::StackSelected;
        result.events.emplace_back(
            AdventureMovementRejected{AdventureMovementAction::ConfirmRoute,
                                      AdventureMovementRejectionReason::SelectedStackChanged});
        return result;
    }
    auto* stack = mutable_stack(planned.stack_id);
    if (stack == nullptr) {
        reset_idle();
        result.events.emplace_back(
            AdventureMovementRejected{AdventureMovementAction::ConfirmRoute,
                                      AdventureMovementRejectionReason::StackNotFound});
        return result;
    }
    if (!d2runtime::is_stack_on_adventure_map(*stack)) {
        reset_idle();
        result.events.emplace_back(
            AdventureMovementRejected{AdventureMovementAction::ConfirmRoute,
                                      AdventureMovementRejectionReason::StackNotVisible});
        return result;
    }
    if (stack->position != planned.route.start) {
        adventure_movement_state_.planned.reset();
        adventure_movement_state_.mode = AdventureInteractionMode::StackSelected;
        result.events.emplace_back(
            AdventureMovementRejected{AdventureMovementAction::ConfirmRoute,
                                      AdventureMovementRejectionReason::StackPositionChanged});
        return result;
    }
    const auto* leader = leader_instance(*stack);
    if (leader == nullptr) {
        adventure_movement_state_.planned.reset();
        adventure_movement_state_.mode = AdventureInteractionMode::StackSelected;
        result.events.emplace_back(
            AdventureMovementRejected{AdventureMovementAction::ConfirmRoute,
                                      AdventureMovementRejectionReason::LeaderInstanceMissing});
        return result;
    }
    const auto profile = movement_profile(*stack);
    if (!profile) {
        adventure_movement_state_.planned.reset();
        adventure_movement_state_.mode = AdventureInteractionMode::StackSelected;
        result.events.emplace_back(AdventureMovementRejected{
            AdventureMovementAction::ConfirmRoute,
            AdventureMovementRejectionReason::MovementProfileUnavailable});
        return result;
    }
    if (*profile != planned.profile) {
        adventure_movement_state_.planned.reset();
        adventure_movement_state_.mode = AdventureInteractionMode::StackSelected;
        result.events.emplace_back(
            AdventureMovementRejected{AdventureMovementAction::ConfirmRoute,
                                      AdventureMovementRejectionReason::MovementProfileChanged});
        return result;
    }
    const auto route = find_route(*stack, *profile, planned.route.destination, result,
                                  AdventureMovementAction::ConfirmRoute);
    if (!route) {
        adventure_movement_state_.planned.reset();
        adventure_movement_state_.mode = AdventureInteractionMode::StackSelected;
        return result;
    }
    auto refreshed = planned_movement(*stack, *profile, *route);
    if (refreshed.route != planned.route) {
        adventure_movement_state_.planned = refreshed;
        adventure_movement_state_.mode = AdventureInteractionMode::RoutePlanned;
        result.events.emplace_back(AdventureRoutePlanned{
            refreshed.stack_id, refreshed.route.destination, refreshed.route.total_cost,
            refreshed.available_movement_points, refreshed.affordable_step_count,
            refreshed.first_unaffordable_step_index, true});
        result.events.emplace_back(AdventureMovementRejected{
            AdventureMovementAction::ConfirmRoute, AdventureMovementRejectionReason::RouteChanged});
        return result;
    }
    planned = refreshed;
    if (planned.route.empty()) {
        const auto cell = stack->position;
        reset_selected();
        result.events.emplace_back(AdventureMovementNotRequired{stack->id, cell});
        return result;
    }
    if (planned.affordable_step_count == 0) {
        result.events.emplace_back(AdventureMovementRejected{
            AdventureMovementAction::ConfirmRoute,
            AdventureMovementRejectionReason::InsufficientMovementPoints});
        return result;
    }
    AdventureMovementExecution execution;
    execution.stack_id = planned.stack_id;
    execution.profile = planned.profile;
    execution.route =
        planned.fully_affordable()
            ? planned.route
            : d2adventure::adventure_route_prefix(planned.route, planned.affordable_step_count);
    execution.requested_destination = planned.route.destination;
    execution.limited_by_movement_points = !planned.fully_affordable();
    const AdventureMovementStarted started{planned.stack_id,
                                           planned.route.start,
                                           execution.requested_destination,
                                           execution.route.destination,
                                           execution.route.size(),
                                           execution.route.total_cost,
                                           execution.limited_by_movement_points};
    adventure_movement_state_.execution = std::move(execution);
    adventure_movement_state_.planned.reset();
    adventure_movement_state_.mode = AdventureInteractionMode::Moving;
    result.events.emplace_back(started);
    return result;
}

GameCommandResult GameSession::handle(const GameAdvanceAdventureMovementStepCommand&) {
    GameCommandResult result;
    if (!adventure_movement_state_.is_moving() || !adventure_movement_state_.execution) {
        result.events.emplace_back(
            AdventureMovementRejected{AdventureMovementAction::AdvanceStep,
                                      AdventureMovementRejectionReason::NoActiveMovement});
        return result;
    }
    auto& execution = *adventure_movement_state_.execution;
    if (execution.route.empty() || execution.next_step_index >= execution.route.steps.size() ||
        !adventure_movement_state_.selected_stack_id ||
        *adventure_movement_state_.selected_stack_id != execution.stack_id) {
        interrupt(result, AdventureMovementInterruptionReason::InvalidRouteState);
        return result;
    }
    auto* stack = mutable_stack(execution.stack_id);
    if (stack == nullptr) {
        interrupt(result, AdventureMovementInterruptionReason::StackMissing);
        reset_idle();
        return result;
    }
    if (!d2runtime::is_stack_on_adventure_map(*stack)) {
        interrupt(result, AdventureMovementInterruptionReason::StackNotVisible);
        reset_idle();
        return result;
    }
    const auto& route = execution.route;
    const auto  expected = execution.next_step_index == 0
                               ? route.start
                               : route.steps[execution.next_step_index - 1].cell;
    if (stack->position != expected) {
        interrupt(result, AdventureMovementInterruptionReason::PositionMismatch);
        return result;
    }
    const auto& step = route.steps[execution.next_step_index];
    const auto  delta_x = step.cell.x - stack->position.x;
    const auto  delta_y = step.cell.y - stack->position.y;
    if (delta_x < -1 || delta_x > 1 || delta_y < -1 || delta_y > 1 ||
        (delta_x == 0 && delta_y == 0) || delta_x != step.delta_x || delta_y != step.delta_y) {
        interrupt(result, AdventureMovementInterruptionReason::InvalidRouteState);
        return result;
    }
    const auto build_result = d2adventure::AdventureNavigationMapBuilder().build(world_);
    if (!build_result.map || build_result.error_count() != 0) {
        interrupt(result, AdventureMovementInterruptionReason::NavigationMapInvalid);
        return result;
    }
    const auto traversal = build_result.map->traversal_cell_at(step.cell, execution.stack_id);
    if (!traversal) {
        interrupt(result, AdventureMovementInterruptionReason::StepBlocked);
        return result;
    }
    const auto decision =
        d2adventure::evaluate_adventure_movement_cell(execution.profile, *traversal);
    if (!decision.passable) {
        interrupt(result, AdventureMovementInterruptionReason::StepBlocked, decision.block_reason);
        return result;
    }
    if (decision.movement_cost != step.step_cost) {
        interrupt(result, AdventureMovementInterruptionReason::StepCostChanged);
        return result;
    }
    if (stack->move < step.step_cost) {
        interrupt(result, AdventureMovementInterruptionReason::InsufficientMovementPoints);
        return result;
    }
    const auto direction = d2runtime::adventure_direction_for_delta(step.delta_x, step.delta_y);
    const auto from = stack->position;
    stack->position = step.cell;
    stack->facing = direction;
    stack->move -= step.step_cost;
    execution.spent_cost += step.step_cost;
    const auto step_index = execution.next_step_index++;
    result.events.emplace_back(AdventureMovementStepCommitted{
        execution.stack_id, from, stack->position, step_index, step.step_cost, execution.spent_cost,
        stack->move, direction});
    if (!execution.complete())
        return result;
    if (stack->position != route.destination || execution.spent_cost != route.total_cost) {
        interrupt(result, AdventureMovementInterruptionReason::InvalidRouteState);
        return result;
    }
    result.events.emplace_back(AdventureMovementCompleted{
        execution.stack_id, stack->position, execution.requested_destination, execution.spent_cost,
        stack->move, execution.limited_by_movement_points});
    reset_selected();
    return result;
}

d2runtime::AdventureStack* GameSession::mutable_stack(const std::string_view id) {
    for (auto& stack : world_.stacks) {
        if (stack.id == id)
            return &stack;
    }
    return nullptr;
}

const d2runtime::AdventureStack* GameSession::selected_stack() const {
    if (!adventure_movement_state_.selected_stack_id)
        return nullptr;
    return world_.find_stack(*adventure_movement_state_.selected_stack_id);
}

const d2runtime::AdventureUnitInstance*
GameSession::leader_instance(const d2runtime::AdventureStack& stack) const {
    return world_.find_unit(stack.leader_id);
}

std::optional<d2adventure::AdventureMovementProfile>
GameSession::movement_profile(const d2runtime::AdventureStack& stack) const {
    const auto* leader = leader_instance(stack);
    if (leader == nullptr)
        return std::nullopt;
    return movement_profiles_.find_profile(leader->type_id);
}

std::optional<d2adventure::AdventureRoute>
GameSession::find_route(const d2runtime::AdventureStack&            stack,
                        const d2adventure::AdventureMovementProfile profile,
                        const d2runtime::MapCellCoord destination, GameCommandResult& result,
                        const AdventureMovementAction action) const {
    const auto build_result = d2adventure::AdventureNavigationMapBuilder().build(world_);
    if (!build_result.map || build_result.error_count() != 0) {
        result.events.emplace_back(AdventureMovementRejected{
            action, AdventureMovementRejectionReason::NavigationMapInvalid, std::nullopt,
            d2adventure::AdventureMovementBlockReason::None, build_result.error_count()});
        return std::nullopt;
    }
    const auto path_result = d2adventure::AdventurePathfinder().find_path(
        *build_result.map, stack.position, destination, profile, stack.id);
    if (!path_result.success() || !path_result.route) {
        result.events.emplace_back(
            AdventureMovementRejected{action, AdventureMovementRejectionReason::PathUnavailable,
                                      path_result.status, path_result.block_reason});
        return std::nullopt;
    }
    return path_result.route;
}

AdventurePlannedMovement
GameSession::planned_movement(const d2runtime::AdventureStack&            stack,
                              const d2adventure::AdventureMovementProfile profile,
                              d2adventure::AdventureRoute                 route) const {
    AdventurePlannedMovement planned;
    planned.stack_id = stack.id;
    planned.profile = profile;
    planned.route = std::move(route);
    planned.available_movement_points = std::max(stack.move, 0);
    for (std::size_t index = 0; index < planned.route.steps.size(); ++index) {
        const auto& step = planned.route.steps[index];
        if (step.cumulative_cost > planned.available_movement_points) {
            planned.first_unaffordable_step_index = index;
            break;
        }
        planned.affordable_step_count = index + 1;
        planned.affordable_cost = step.cumulative_cost;
    }
    return planned;
}

std::optional<int> GameSession::initial_movement_points(const std::string_view stack_id) const {
    const auto it = std::lower_bound(
        initial_stack_movement_points_.begin(), initial_stack_movement_points_.end(), stack_id,
        [](const auto& entry, const std::string_view id) { return entry.stack_id < id; });
    if (it == initial_stack_movement_points_.end() || it->stack_id != stack_id)
        return std::nullopt;
    return it->movement_points;
}

void GameSession::refresh_planned_movement_after_debug_change() {
    if (adventure_movement_state_.mode != AdventureInteractionMode::RoutePlanned ||
        !adventure_movement_state_.planned || !adventure_movement_state_.selected_stack_id)
        return;
    auto* stack = mutable_stack(*adventure_movement_state_.selected_stack_id);
    if (stack == nullptr || adventure_movement_state_.planned->stack_id != stack->id)
        throw std::logic_error("debug refresh planned stack mismatch");
    auto& planned = *adventure_movement_state_.planned;
    planned = planned_movement(*stack, planned.profile, planned.route);
}

GameCommandResult
GameSession::change_selected_movement_points(const AdventureMovementDebugChange change) {
    GameCommandResult result;
    const auto        action = change == AdventureMovementDebugChange::Reset
                                   ? AdventureMovementAction::DebugResetMovementPoints
                                   : AdventureMovementAction::DebugGrantFreeMovementPoints;
    if (adventure_movement_state_.is_moving()) {
        result.events.emplace_back(
            AdventureMovementRejected{action, AdventureMovementRejectionReason::Busy});
        return result;
    }
    if (!adventure_movement_state_.selected_stack_id) {
        result.events.emplace_back(
            AdventureMovementRejected{action, AdventureMovementRejectionReason::NoSelectedStack});
        return result;
    }
    auto* stack = mutable_stack(*adventure_movement_state_.selected_stack_id);
    if (stack == nullptr) {
        reset_idle();
        result.events.emplace_back(
            AdventureMovementRejected{action, AdventureMovementRejectionReason::StackNotFound});
        return result;
    }
    if (!d2runtime::is_stack_on_adventure_map(*stack)) {
        reset_idle();
        result.events.emplace_back(
            AdventureMovementRejected{action, AdventureMovementRejectionReason::StackNotVisible});
        return result;
    }
    const auto baseline = initial_movement_points(stack->id);
    if (!baseline) {
        result.events.emplace_back(AdventureMovementRejected{
            action, AdventureMovementRejectionReason::InitialMovementPointsUnavailable});
        return result;
    }
    const int previous = stack->move;
    stack->move = change == AdventureMovementDebugChange::Reset ? *baseline
                                                                : kAdventureDebugFreeMovementPoints;
    refresh_planned_movement_after_debug_change();
    result.events.emplace_back(
        AdventureMovementPointsDebugChanged{change, stack->id, previous, stack->move, *baseline});
    if (adventure_movement_state_.mode == AdventureInteractionMode::RoutePlanned) {
        const auto& planned = *adventure_movement_state_.planned;
        result.events.emplace_back(AdventureRoutePlanned{
            planned.stack_id, planned.route.destination, planned.route.total_cost,
            planned.available_movement_points, planned.affordable_step_count,
            planned.first_unaffordable_step_index, true});
    }
    return result;
}

void GameSession::reset_idle() {
    adventure_movement_state_ = {};
}

void GameSession::reset_selected() {
    const auto selected = adventure_movement_state_.selected_stack_id;
    adventure_movement_state_ = {};
    adventure_movement_state_.mode = AdventureInteractionMode::StackSelected;
    adventure_movement_state_.selected_stack_id = selected;
}

void GameSession::interrupt(GameCommandResult&                              result,
                            const AdventureMovementInterruptionReason       reason,
                            const d2adventure::AdventureMovementBlockReason block_reason) {
    const auto stack_id = adventure_movement_state_.execution
                              ? adventure_movement_state_.execution->stack_id
                              : adventure_movement_state_.selected_stack_id.value_or("");
    const auto next_step = adventure_movement_state_.execution
                               ? adventure_movement_state_.execution->next_step_index
                               : 0;
    result.events.emplace_back(
        AdventureMovementInterrupted{stack_id, reason, next_step, block_reason});
    adventure_movement_state_.execution.reset();
    adventure_movement_state_.planned.reset();
    const auto* stack = world_.find_stack(stack_id);
    if (stack != nullptr && d2runtime::is_stack_on_adventure_map(*stack)) {
        adventure_movement_state_.mode = AdventureInteractionMode::StackSelected;
        adventure_movement_state_.selected_stack_id = stack_id;
    } else {
        reset_idle();
    }
}

WorldInspectSummary GameSession::inspect() const {
    return {world_.scenario_id,          world_.scenario_name, world_.map_width,
            world_.map_height,           world_.terrain_tiles, world_.semantic_object_count,
            world_.runtime_object_count, build_warnings_,      build_errors_};
}

} // namespace d2game
