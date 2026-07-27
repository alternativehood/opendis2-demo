#include <d2game/AdventureMovementState.hpp>
#include <d2game/AdventureUnitMovementProfileCatalog.hpp>
#include <d2game/GameCommand.hpp>
#include <d2game/GameEvent.hpp>
#include <d2game/GameSession.hpp>

#include <d2adventure_rules/AdventureRoutePreviewBuilder.hpp>

#include <d2engine/assets/unit_def.hpp>
#include <d2runtime/AdventureWorldState.hpp>

#include <gtest/gtest.h>

#include <string>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

d2engine::UnitDef unit_def(std::string id, std::vector<int> abilities = {},
                           bool water_only = false) {
    d2engine::UnitDef result;
    result.unit_id = std::move(id);
    result.native_ability_ids = std::move(abilities);
    result.water_only = water_only;
    return result;
}

d2runtime::AdventureWorldState world(int width = 3, int height = 1, int move = 20) {
    d2runtime::AdventureWorldState result;
    result.map_width = width;
    result.map_height = height;
    result.terrain.width = width;
    result.terrain.height = height;
    result.terrain.tiles.assign(static_cast<std::size_t>(width * height), {1});
    d2runtime::AdventureUnitInstance leader;
    leader.id = "leader";
    leader.type_id = "UNIT";
    result.units.push_back(std::move(leader));
    d2runtime::AdventureStack stack;
    stack.id = "STACK";
    stack.leader_id = "leader";
    stack.position = {0, 0};
    stack.move = move;
    result.stacks.push_back(std::move(stack));
    return result;
}

d2game::AdventureUnitMovementProfileCatalog walking_catalog() {
    const auto definition = unit_def("UNIT");
    return d2game::AdventureUnitMovementProfileCatalog::from_unit_defs(
        std::span<const d2engine::UnitDef>(&definition, 1));
}

template <typename Event> const Event* find_event(const d2game::GameCommandResult& result) {
    for (const auto& event : result.events) {
        if (const auto* found = std::get_if<Event>(&event)) {
            return found;
        }
    }
    return nullptr;
}

d2game::GameSession make_session(int move = 20) {
    return d2game::GameSession(world(3, 1, move), 0, 0, walking_catalog());
}

d2runtime::AdventureWorldState contained_stack_world(bool capital = false) {
    d2runtime::AdventureWorldState result;
    result.map_width = 3;
    result.map_height = 1;
    result.terrain.width = 3;
    result.terrain.height = 1;
    result.terrain.tiles.assign(3, {1});

    d2runtime::AdventureSubraceRef subrace;
    subrace.id = "SUB1";
    subrace.race_id = "g000rr0000";
    result.subraces.push_back(subrace);

    d2runtime::AdventureUnitInstance leader;
    leader.id = "leader";
    leader.type_id = "UNIT";
    result.units.push_back(std::move(leader));

    d2runtime::AdventureStack stack;
    stack.id = "STACK";
    stack.leader_id = "leader";
    stack.subrace = "SUB1";
    stack.inside = capital ? "CAP" : "CITY";
    stack.move = 20;
    result.stacks.push_back(std::move(stack));

    if (capital) {
        d2runtime::AdventureCapital capital_settlement;
        capital_settlement.id = "CAP";
        capital_settlement.visiting_stack_id = "STACK";
        capital_settlement.footprint = {{1, 0}, {2, 0}};
        result.capitals.push_back(std::move(capital_settlement));
    } else {
        d2runtime::AdventureCity city;
        city.id = "CITY";
        city.stack_id = "STACK";
        city.footprint = {{1, 0}, {2, 0}};
        result.cities.push_back(std::move(city));
    }

    return result;
}

void expect_mode_invariants(const d2game::AdventureMovementState& state) {
    switch (state.mode) {
    case d2game::AdventureInteractionMode::Idle:
        EXPECT_FALSE(state.selected_stack_id);
        EXPECT_FALSE(state.planned);
        EXPECT_FALSE(state.execution);
        break;
    case d2game::AdventureInteractionMode::StackSelected:
        EXPECT_TRUE(state.selected_stack_id);
        EXPECT_FALSE(state.planned);
        EXPECT_FALSE(state.execution);
        break;
    case d2game::AdventureInteractionMode::RoutePlanned:
        EXPECT_TRUE(state.selected_stack_id);
        ASSERT_TRUE(state.planned);
        EXPECT_EQ(state.planned->stack_id, *state.selected_stack_id);
        EXPECT_FALSE(state.execution);
        break;
    case d2game::AdventureInteractionMode::Moving:
        EXPECT_TRUE(state.selected_stack_id);
        ASSERT_TRUE(state.execution);
        EXPECT_EQ(state.execution->stack_id, *state.selected_stack_id);
        EXPECT_FALSE(state.planned);
        break;
    }
}

} // namespace

TEST(AdventureUnitMovementProfileCatalog, ResolvesProfilesAndNormalizesLookup) {
    const std::vector definitions = {unit_def("walk"), unit_def("fly", {0, 1, 3}),
                                     unit_def("water", {}, true)};
    const auto catalog = d2game::AdventureUnitMovementProfileCatalog::from_unit_defs(definitions);
    EXPECT_EQ(catalog.size(), 3u);
    EXPECT_EQ(catalog.find_profile("WALK"), d2adventure::AdventureMovementProfile::Walking);
    EXPECT_EQ(catalog.find_profile("Fly"), d2adventure::AdventureMovementProfile::Flying);
    EXPECT_EQ(catalog.find_profile("WATER"), d2adventure::AdventureMovementProfile::Swimming);
}

TEST(AdventureUnitMovementProfileCatalog, RejectsEmptyAndDuplicateNormalizedIds) {
    EXPECT_THROW(d2game::AdventureUnitMovementProfileCatalog(
                     std::vector<d2game::AdventureUnitMovementProfileEntry>{{"", {}}}),
                 std::invalid_argument);
    EXPECT_THROW(
        d2game::AdventureUnitMovementProfileCatalog(
            std::vector<d2game::AdventureUnitMovementProfileEntry>{{"UNIT", {}}, {"unit", {}}}),
        std::invalid_argument);
}

TEST(AdventureUnitMovementProfileCatalog, OwnsSnapshotAfterSourceDestroyed) {
    const auto catalog = [] {
        std::vector definitions = {unit_def("UNIT")};
        return d2game::AdventureUnitMovementProfileCatalog::from_unit_defs(definitions);
    }();
    EXPECT_EQ(catalog.find_profile("unit"), d2adventure::AdventureMovementProfile::Walking);
}

TEST(GameSessionAdventureMovement, GenericCommandsRemainCompatible) {
    auto session = make_session();
    EXPECT_TRUE(
        find_event<d2game::GameQuitRequested>(session.handle_command(d2game::GameQuitCommand{})));
    EXPECT_TRUE(find_event<d2game::GameInspectResult>(
        session.handle_command(d2game::GameInspectWorldCommand{})));
    const auto before = session.adventure_movement_state();
    session.handle_command(d2game::GameNoOpCommand{});
    session.handle_command(d2game::GameAdvanceFrameCommand{});
    EXPECT_EQ(session.adventure_movement_state(), before);
}

TEST(GameSessionAdventureMovement, SelectionAndClearingMaintainInvariants) {
    auto session = make_session();
    EXPECT_EQ(session.adventure_movement_state().mode, d2game::AdventureInteractionMode::Idle);
    expect_mode_invariants(session.adventure_movement_state());
    const auto selected = session.handle_command(d2game::GameSelectAdventureStackCommand{"STACK"});
    ASSERT_TRUE(find_event<d2game::AdventureStackSelected>(selected));
    expect_mode_invariants(session.adventure_movement_state());
    const auto cleared = session.handle_command(d2game::GameClearAdventureSelectionCommand{});
    ASSERT_TRUE(find_event<d2game::AdventureSelectionCleared>(cleared));
    EXPECT_EQ(find_event<d2game::AdventureSelectionCleared>(cleared)->previous_stack_id, "STACK");
    expect_mode_invariants(session.adventure_movement_state());
}

TEST(GameSessionAdventureMovement, SelectionRejectsMissingAndContainedStacks) {
    auto input = world();
    input.stacks.front().inside = "CITY";
    d2game::GameSession session(std::move(input), 0, 0, walking_catalog());
    const auto missing = session.handle_command(d2game::GameSelectAdventureStackCommand{"missing"});
    ASSERT_TRUE(find_event<d2game::AdventureMovementRejected>(missing));
    EXPECT_EQ(find_event<d2game::AdventureMovementRejected>(missing)->reason,
              d2game::AdventureMovementRejectionReason::StackNotFound);
    const auto contained = session.handle_command(d2game::GameSelectAdventureStackCommand{"STACK"});
    EXPECT_EQ(find_event<d2game::AdventureMovementRejected>(contained)->reason,
              d2game::AdventureMovementRejectionReason::StackNotVisible);
}

TEST(GameSessionAdventureMovement, SelectionAllowsContainedVillageAndCapitalStacks) {
    {
        d2game::GameSession session(contained_stack_world(false), 0, 0, walking_catalog());
        const auto          selected =
            session.handle_command(d2game::GameSelectAdventureStackCommand{"STACK"});
        ASSERT_TRUE(find_event<d2game::AdventureStackSelected>(selected));
        EXPECT_EQ(session.adventure_movement_state().mode,
                  d2game::AdventureInteractionMode::StackSelected);
        EXPECT_EQ(session.adventure_movement_state().selected_stack_id,
                  std::optional<std::string>("STACK"));
    }

    {
        d2game::GameSession session(contained_stack_world(true), 0, 0, walking_catalog());
        const auto          selected =
            session.handle_command(d2game::GameSelectAdventureStackCommand{"STACK"});
        ASSERT_TRUE(find_event<d2game::AdventureStackSelected>(selected));
        EXPECT_EQ(session.adventure_movement_state().mode,
                  d2game::AdventureInteractionMode::StackSelected);
        EXPECT_EQ(session.adventure_movement_state().selected_stack_id,
                  std::optional<std::string>("STACK"));
    }
}

TEST(GameSessionAdventureMovement, ContainedStackPlanningIsRejectedWithoutClearingSelection) {
    auto session = d2game::GameSession(contained_stack_world(false), 0, 0, walking_catalog());
    session.handle_command(d2game::GameSelectAdventureStackCommand{"STACK"});

    const auto planned = session.handle_command(d2game::GamePlanAdventureMovementCommand{{2, 0}});
    ASSERT_TRUE(find_event<d2game::AdventureMovementRejected>(planned));
    EXPECT_EQ(find_event<d2game::AdventureMovementRejected>(planned)->reason,
              d2game::AdventureMovementRejectionReason::StackNotVisible);
    EXPECT_EQ(session.adventure_movement_state().mode,
              d2game::AdventureInteractionMode::StackSelected);
    ASSERT_TRUE(session.adventure_movement_state().selected_stack_id);
    EXPECT_EQ(*session.adventure_movement_state().selected_stack_id, "STACK");
    EXPECT_FALSE(session.adventure_movement_state().planned);
    EXPECT_FALSE(session.adventure_movement_state().execution);
}

TEST(GameSessionAdventureMovement, SelectingVisibleStackAfterContainedStackStillWorks) {
    auto                             input = contained_stack_world(false);
    d2runtime::AdventureUnitInstance leader2;
    leader2.id = "leader2";
    leader2.type_id = "UNIT";
    input.units.push_back(std::move(leader2));
    d2runtime::AdventureStack visible;
    visible.id = "VISIBLE";
    visible.leader_id = "leader2";
    visible.position = {0, 0};
    visible.move = 20;
    input.stacks.push_back(std::move(visible));

    d2game::GameSession session(std::move(input), 0, 0, walking_catalog());
    session.handle_command(d2game::GameSelectAdventureStackCommand{"STACK"});
    const auto selected =
        session.handle_command(d2game::GameSelectAdventureStackCommand{"VISIBLE"});
    ASSERT_TRUE(find_event<d2game::AdventureStackSelected>(selected));
    EXPECT_EQ(session.adventure_movement_state().selected_stack_id,
              std::optional<std::string>("VISIBLE"));
    EXPECT_EQ(session.adventure_movement_state().mode,
              d2game::AdventureInteractionMode::StackSelected);
}

TEST(GameSessionAdventureMovement, PlansCompleteAndOverBudgetRoutes) {
    auto session = make_session(3);
    session.handle_command(d2game::GameSelectAdventureStackCommand{"STACK"});
    const auto planned = session.handle_command(d2game::GamePlanAdventureMovementCommand{{2, 0}});
    ASSERT_TRUE(find_event<d2game::AdventureRoutePlanned>(planned));
    EXPECT_EQ(session.adventure_movement_state().mode,
              d2game::AdventureInteractionMode::RoutePlanned);
    ASSERT_TRUE(session.adventure_movement_state().planned);
    EXPECT_EQ(session.adventure_movement_state().planned->route.size(), 2u);
    EXPECT_EQ(session.adventure_movement_state().planned->affordable_step_count, 1u);
    EXPECT_EQ(session.adventure_movement_state().planned->first_unaffordable_step_index, 1u);
    const auto  confirm = session.handle_command(d2game::GameConfirmAdventureMovementCommand{});
    const auto* started = find_event<d2game::AdventureMovementStarted>(confirm);
    ASSERT_NE(started, nullptr);
    EXPECT_EQ(started->requested_destination, (d2runtime::MapCellCoord{2, 0}));
    EXPECT_EQ(started->execution_destination, (d2runtime::MapCellCoord{1, 0}));
    EXPECT_EQ(started->step_count, 1U);
    EXPECT_TRUE(started->limited_by_movement_points);
    ASSERT_TRUE(session.adventure_movement_state().execution);
    EXPECT_EQ(session.adventure_movement_state().execution->route.size(), 1U);
    EXPECT_EQ(session.adventure_movement_state().execution->route.destination,
              (d2runtime::MapCellCoord{1, 0}));
}

TEST(GameSessionAdventureMovement, ZeroAffordableStepsRemainPlanned) {
    auto session = make_session(0);
    session.handle_command(d2game::GameSelectAdventureStackCommand{"STACK"});
    session.handle_command(d2game::GamePlanAdventureMovementCommand{{2, 0}});
    const auto confirm = session.handle_command(d2game::GameConfirmAdventureMovementCommand{});
    ASSERT_TRUE(find_event<d2game::AdventureMovementRejected>(confirm));
    EXPECT_EQ(find_event<d2game::AdventureMovementRejected>(confirm)->reason,
              d2game::AdventureMovementRejectionReason::InsufficientMovementPoints);
    EXPECT_TRUE(session.adventure_movement_state().planned);
    EXPECT_EQ(session.adventure_movement_state().mode,
              d2game::AdventureInteractionMode::RoutePlanned);
}

TEST(GameSessionAdventureMovement, PartialExecutionCompletesAtAffordableEndpoint) {
    auto session = make_session(3);
    session.handle_command(d2game::GameSelectAdventureStackCommand{"STACK"});
    session.handle_command(d2game::GamePlanAdventureMovementCommand{{2, 0}});
    session.handle_command(d2game::GameConfirmAdventureMovementCommand{});
    const auto completed =
        session.handle_command(d2game::GameAdvanceAdventureMovementStepCommand{});
    const auto* event = find_event<d2game::AdventureMovementCompleted>(completed);
    ASSERT_NE(event, nullptr);
    EXPECT_EQ(event->final_position, (d2runtime::MapCellCoord{1, 0}));
    EXPECT_EQ(event->requested_destination, (d2runtime::MapCellCoord{2, 0}));
    EXPECT_TRUE(event->limited_by_movement_points);
    EXPECT_EQ(event->remaining_movement_points, 0);
    EXPECT_EQ(session.world().find_stack("STACK")->position, (d2runtime::MapCellCoord{1, 0}));
    EXPECT_EQ(session.world().find_stack("STACK")->move, 0);
    EXPECT_EQ(session.adventure_movement_state().mode,
              d2game::AdventureInteractionMode::StackSelected);
}

TEST(GameSessionAdventureMovement, DebugSnapshotAndMovementPointCommands) {
    auto session = make_session(7);
    EXPECT_FALSE(session.adventure_movement_debug_snapshot().current_movement_points);
    session.handle_command(d2game::GameSelectAdventureStackCommand{"STACK"});
    const auto selected = session.adventure_movement_debug_snapshot();
    ASSERT_TRUE(selected.current_movement_points);
    ASSERT_TRUE(selected.reset_movement_points);
    EXPECT_EQ(*selected.current_movement_points, 7);
    EXPECT_EQ(*selected.reset_movement_points, 7);
    const auto free =
        session.handle_command(d2game::GameDebugGrantSelectedAdventureFreeMovementPointsCommand{});
    const auto* free_event = find_event<d2game::AdventureMovementPointsDebugChanged>(free);
    ASSERT_NE(free_event, nullptr);
    EXPECT_EQ(free_event->current_movement_points, d2game::kAdventureDebugFreeMovementPoints);
    EXPECT_EQ(session.world().find_stack("STACK")->move, d2game::kAdventureDebugFreeMovementPoints);
    const auto reset =
        session.handle_command(d2game::GameDebugResetSelectedAdventureMovementPointsCommand{});
    ASSERT_TRUE(find_event<d2game::AdventureMovementPointsDebugChanged>(reset));
    EXPECT_EQ(session.world().find_stack("STACK")->move, 7);
}

TEST(GameSessionAdventureMovement, DebugFreeRefreshesPlannedAffordabilityWithoutPathfinding) {
    auto session = make_session(3);
    session.handle_command(d2game::GameSelectAdventureStackCommand{"STACK"});
    session.handle_command(d2game::GamePlanAdventureMovementCommand{{2, 0}});
    const auto free =
        session.handle_command(d2game::GameDebugGrantSelectedAdventureFreeMovementPointsCommand{});
    ASSERT_TRUE(find_event<d2game::AdventureRoutePlanned>(free));
    ASSERT_TRUE(session.adventure_movement_state().planned);
    EXPECT_EQ(session.adventure_movement_state().planned->route.destination,
              (d2runtime::MapCellCoord{2, 0}));
    EXPECT_EQ(session.adventure_movement_state().planned->affordable_step_count, 2U);
    EXPECT_EQ(session.adventure_movement_state().mode,
              d2game::AdventureInteractionMode::RoutePlanned);
}

TEST(GameSessionAdventureMovement, DebugCommandsAreBusyDuringMovement) {
    auto session = make_session();
    session.handle_command(d2game::GameSelectAdventureStackCommand{"STACK"});
    session.handle_command(d2game::GamePlanAdventureMovementCommand{{2, 0}});
    session.handle_command(d2game::GameConfirmAdventureMovementCommand{});
    const auto result =
        session.handle_command(d2game::GameDebugResetSelectedAdventureMovementPointsCommand{});
    ASSERT_TRUE(find_event<d2game::AdventureMovementRejected>(result));
    EXPECT_EQ(find_event<d2game::AdventureMovementRejected>(result)->reason,
              d2game::AdventureMovementRejectionReason::Busy);
}

TEST(GameSessionAdventureMovement, RoutePreviewIsAbsentOutsidePlannedMode) {
    auto session = make_session();
    EXPECT_FALSE(session.adventure_route_preview());
    session.handle_command(d2game::GameSelectAdventureStackCommand{"STACK"});
    EXPECT_FALSE(session.adventure_route_preview());
}

TEST(GameSessionAdventureMovement, RoutePreviewMatchesAuthoritativePlan) {
    auto session = make_session();
    session.handle_command(d2game::GameSelectAdventureStackCommand{"STACK"});
    session.handle_command(d2game::GamePlanAdventureMovementCommand{{2, 0}});
    ASSERT_TRUE(session.adventure_movement_state().planned);
    const auto expected = d2adventure::AdventureRoutePreviewBuilder{}.build(
        session.adventure_movement_state().planned->route,
        session.adventure_movement_state().planned->available_movement_points,
        session.adventure_movement_state().planned->first_unaffordable_step_index);
    ASSERT_TRUE(session.adventure_route_preview());
    EXPECT_EQ(*session.adventure_route_preview(), expected);
    EXPECT_EQ(session.adventure_route_preview(), session.adventure_route_preview());
}

TEST(GameSessionAdventureMovement, OverBudgetPreviewStopsAtActionLimitAndKeepsDestination) {
    auto session = make_session(3);
    session.handle_command(d2game::GameSelectAdventureStackCommand{"STACK"});
    session.handle_command(d2game::GamePlanAdventureMovementCommand{{2, 0}});
    const auto preview = session.adventure_route_preview();
    ASSERT_TRUE(preview);
    ASSERT_EQ(preview->steps.size(), 2u);
    EXPECT_EQ(preview->steps[0].marker, d2adventure::AdventureRouteMarkerKind::Normal);
    EXPECT_EQ(preview->steps[1].marker, d2adventure::AdventureRouteMarkerKind::ActionLimit);
    EXPECT_EQ(preview->destination, (d2runtime::MapCellCoord{2, 0}));
    EXPECT_EQ(session.adventure_movement_state().mode,
              d2game::AdventureInteractionMode::RoutePlanned);
}

TEST(GameSessionAdventureMovement, PreviewLifecycleFollowsMovementState) {
    auto session = make_session();
    session.handle_command(d2game::GameSelectAdventureStackCommand{"STACK"});
    session.handle_command(d2game::GamePlanAdventureMovementCommand{{2, 0}});
    ASSERT_TRUE(session.adventure_route_preview());
    const auto planned_before = session.adventure_movement_state().planned;
    const auto world_position_before = session.world().find_stack("STACK")->position;
    const auto world_move_before = session.world().find_stack("STACK")->move;
    EXPECT_EQ(session.adventure_movement_state().mode,
              d2game::AdventureInteractionMode::RoutePlanned);
    EXPECT_EQ(session.adventure_movement_state().planned, planned_before);
    EXPECT_EQ(session.world().find_stack("STACK")->position, world_position_before);
    EXPECT_EQ(session.world().find_stack("STACK")->move, world_move_before);

    session.handle_command(d2game::GameConfirmAdventureMovementCommand{});
    ASSERT_TRUE(session.adventure_route_preview());
    session.handle_command(d2game::GameAdvanceAdventureMovementStepCommand{});
    ASSERT_TRUE(session.adventure_route_preview());
    session.handle_command(d2game::GameAdvanceAdventureMovementStepCommand{});
    EXPECT_FALSE(session.adventure_route_preview());
    session.handle_command(d2game::GameClearAdventureSelectionCommand{});
    EXPECT_FALSE(session.adventure_route_preview());
}

TEST(GameSessionAdventureMovement, ReplanningReplacesPreviewAndFailedPlanClearsIt) {
    auto session = make_session();
    session.handle_command(d2game::GameSelectAdventureStackCommand{"STACK"});
    session.handle_command(d2game::GamePlanAdventureMovementCommand{{2, 0}});
    const auto first = session.adventure_route_preview();
    ASSERT_TRUE(first);
    session.handle_command(d2game::GamePlanAdventureMovementCommand{{1, 0}});
    const auto second = session.adventure_route_preview();
    ASSERT_TRUE(second);
    EXPECT_NE(*second, *first);
    EXPECT_EQ(second->destination, (d2runtime::MapCellCoord{1, 0}));
    session.handle_command(d2game::GamePlanAdventureMovementCommand{{99, 0}});
    EXPECT_FALSE(session.adventure_route_preview());
    EXPECT_EQ(session.adventure_movement_state().mode,
              d2game::AdventureInteractionMode::StackSelected);
}

TEST(GameSessionAdventureMovement, ConfirmationStartsWithoutImmediateMutation) {
    auto session = make_session();
    session.handle_command(d2game::GameSelectAdventureStackCommand{"STACK"});
    session.handle_command(d2game::GamePlanAdventureMovementCommand{{2, 0}});
    const auto before = session.world().find_stack("STACK")->move;
    const auto confirmed = session.handle_command(d2game::GameConfirmAdventureMovementCommand{});
    ASSERT_TRUE(find_event<d2game::AdventureMovementStarted>(confirmed));
    EXPECT_EQ(session.adventure_movement_state().mode, d2game::AdventureInteractionMode::Moving);
    EXPECT_EQ(session.world().find_stack("STACK")->position, (d2runtime::MapCellCoord{0, 0}));
    EXPECT_EQ(session.world().find_stack("STACK")->move, before);
}

TEST(GameSessionAdventureMovement, StepExecutionIsCellByCellAndCompletes) {
    auto session = make_session();
    session.handle_command(d2game::GameSelectAdventureStackCommand{"STACK"});
    session.handle_command(d2game::GamePlanAdventureMovementCommand{{2, 0}});
    session.handle_command(d2game::GameConfirmAdventureMovementCommand{});
    const auto first = session.handle_command(d2game::GameAdvanceAdventureMovementStepCommand{});
    ASSERT_TRUE(find_event<d2game::AdventureMovementStepCommitted>(first));
    EXPECT_EQ(session.world().find_stack("STACK")->position, (d2runtime::MapCellCoord{1, 0}));
    EXPECT_EQ(session.adventure_movement_state().execution->next_step_index, 1u);
    EXPECT_EQ(session.world().find_stack("STACK")->move, 17);
    EXPECT_EQ(session.adventure_movement_state().mode, d2game::AdventureInteractionMode::Moving);
    const auto second = session.handle_command(d2game::GameAdvanceAdventureMovementStepCommand{});
    ASSERT_TRUE(find_event<d2game::AdventureMovementStepCommitted>(second));
    ASSERT_TRUE(find_event<d2game::AdventureMovementCompleted>(second));
    EXPECT_EQ(session.world().find_stack("STACK")->position, (d2runtime::MapCellCoord{2, 0}));
    EXPECT_EQ(session.world().find_stack("STACK")->move, 14);
    EXPECT_EQ(session.adventure_movement_state().mode,
              d2game::AdventureInteractionMode::StackSelected);
    EXPECT_TRUE(session.adventure_movement_state().selected_stack_id);
    EXPECT_FALSE(session.adventure_movement_state().execution);
    EXPECT_FALSE(session.adventure_movement_state().planned);
}

TEST(GameSessionAdventureMovement, RepeatedMovementCompletionRemainsStable) {
    auto session = make_session();

    session.handle_command(d2game::GameSelectAdventureStackCommand{"STACK"});
    session.handle_command(d2game::GamePlanAdventureMovementCommand{{1, 0}});
    const auto first_confirm =
        session.handle_command(d2game::GameConfirmAdventureMovementCommand{});
    ASSERT_TRUE(find_event<d2game::AdventureMovementStarted>(first_confirm));
    const auto first_step =
        session.handle_command(d2game::GameAdvanceAdventureMovementStepCommand{});
    ASSERT_TRUE(find_event<d2game::AdventureMovementStepCommitted>(first_step));
    ASSERT_TRUE(find_event<d2game::AdventureMovementCompleted>(first_step));
    EXPECT_EQ(session.world().find_stack("STACK")->position, (d2runtime::MapCellCoord{1, 0}));

    session.handle_command(d2game::GameSelectAdventureStackCommand{"STACK"});
    session.handle_command(d2game::GamePlanAdventureMovementCommand{{2, 0}});
    const auto confirmed = session.handle_command(d2game::GameConfirmAdventureMovementCommand{});
    ASSERT_TRUE(find_event<d2game::AdventureMovementStarted>(confirmed));

    const auto second_step =
        session.handle_command(d2game::GameAdvanceAdventureMovementStepCommand{});
    ASSERT_TRUE(find_event<d2game::AdventureMovementStepCommitted>(second_step));
    ASSERT_TRUE(find_event<d2game::AdventureMovementCompleted>(second_step));
    EXPECT_EQ(session.world().find_stack("STACK")->position, (d2runtime::MapCellCoord{2, 0}));
    EXPECT_EQ(session.adventure_movement_state().mode,
              d2game::AdventureInteractionMode::StackSelected);
}

TEST(GameSessionAdventureMovement, MovementCommandsAreLockedButAdvanceFrameIsNotMovement) {
    auto session = make_session();
    session.handle_command(d2game::GameSelectAdventureStackCommand{"STACK"});
    session.handle_command(d2game::GamePlanAdventureMovementCommand{{2, 0}});
    session.handle_command(d2game::GameConfirmAdventureMovementCommand{});
    const auto before = session.adventure_movement_state();
    for (const auto& result :
         {session.handle_command(d2game::GameSelectAdventureStackCommand{"STACK"}),
          session.handle_command(d2game::GameClearAdventureSelectionCommand{}),
          session.handle_command(d2game::GamePlanAdventureMovementCommand{{1, 0}}),
          session.handle_command(d2game::GameConfirmAdventureMovementCommand{})}) {
        ASSERT_TRUE(find_event<d2game::AdventureMovementRejected>(result));
        EXPECT_EQ(find_event<d2game::AdventureMovementRejected>(result)->reason,
                  d2game::AdventureMovementRejectionReason::Busy);
    }
    session.handle_command(d2game::GameAdvanceFrameCommand{});
    EXPECT_EQ(session.adventure_movement_state(), before);
}

TEST(GameSessionAdventureMovement, AdvanceWithoutMovementIsRejected) {
    auto       session = make_session();
    const auto result = session.handle_command(d2game::GameAdvanceAdventureMovementStepCommand{});
    ASSERT_TRUE(find_event<d2game::AdventureMovementRejected>(result));
    EXPECT_EQ(find_event<d2game::AdventureMovementRejected>(result)->reason,
              d2game::AdventureMovementRejectionReason::NoActiveMovement);
}
