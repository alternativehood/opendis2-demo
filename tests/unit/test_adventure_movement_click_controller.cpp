#include <d2engine/app/adventure_movement_click_controller.hpp>

#include <d2engine/assets/unit_def.hpp>
#include <d2game/AdventureUnitMovementProfileCatalog.hpp>

#include <gtest/gtest.h>

#include <span>

namespace {

d2game::GameSession session() {
    d2runtime::AdventureWorldState world;
    world.map_width = 3;
    world.map_height = 1;
    world.terrain.width = 3;
    world.terrain.height = 1;
    world.terrain.tiles.assign(3, {1});
    d2runtime::AdventureUnitInstance leader;
    leader.id = "leader";
    leader.type_id = "UNIT";
    world.units.push_back(leader);
    d2runtime::AdventureStack stack;
    stack.id = "STACK";
    stack.leader_id = "leader";
    stack.position = {0, 0};
    stack.move = 20;
    world.stacks.push_back(stack);
    d2engine::UnitDef definition;
    definition.unit_id = "UNIT";
    return d2game::GameSession(std::move(world), 0, 0,
                               d2game::AdventureUnitMovementProfileCatalog::from_unit_defs(
                                   std::span<const d2engine::UnitDef>(&definition, 1)));
}

d2game::GameSession two_stack_session() {
    auto                      game = session();
    auto                      world = game.world();
    d2runtime::AdventureStack second = world.stacks.front();
    second.id = "OTHER";
    second.position = {2, 0};
    world.stacks.push_back(second);
    d2engine::UnitDef definition;
    definition.unit_id = "UNIT";
    return d2game::GameSession(std::move(world), 0, 0,
                               d2game::AdventureUnitMovementProfileCatalog::from_unit_defs(
                                   std::span<const d2engine::UnitDef>(&definition, 1)));
}

} // namespace

TEST(AdventureMovementClickController, StackTakesPrecedenceOverCell) {
    auto       game = session();
    const auto result = d2engine::AdventureMovementClickController::handle_left_click(
        game, {std::string{"STACK"}, d2runtime::MapCellCoord{2, 0}});
    ASSERT_TRUE(result);
    EXPECT_EQ(game.adventure_movement_state().mode,
              d2game::AdventureInteractionMode::StackSelected);
}

TEST(AdventureMovementClickController, SelectedCellPlansAndUnselectedCellDoesNothing) {
    auto game = session();
    EXPECT_FALSE(d2engine::AdventureMovementClickController::handle_left_click(
        game, {std::nullopt, d2runtime::MapCellCoord{1, 0}}));
    (void)d2engine::AdventureMovementClickController::handle_left_click(
        game, {std::string{"STACK"}, std::nullopt});
    const auto result = d2engine::AdventureMovementClickController::handle_left_click(
        game, {std::nullopt, d2runtime::MapCellCoord{1, 0}});
    ASSERT_TRUE(result);
    EXPECT_EQ(game.adventure_movement_state().mode, d2game::AdventureInteractionMode::RoutePlanned);
    EXPECT_FALSE(game.adventure_movement_state().execution);
}

TEST(AdventureMovementClickController, EmptyTargetDoesNothingAndNeverStartsMovement) {
    auto game = session();
    EXPECT_FALSE(d2engine::AdventureMovementClickController::handle_left_click(game, {}));
    EXPECT_FALSE(game.adventure_movement_state().is_moving());
}

TEST(AdventureMovementClickController, SelectingAnotherStackClearsPlan) {
    auto game = two_stack_session();
    (void)d2engine::AdventureMovementClickController::handle_left_click(
        game, {std::string{"STACK"}, std::nullopt});
    ASSERT_TRUE(d2engine::AdventureMovementClickController::handle_left_click(
        game, {std::nullopt, d2runtime::MapCellCoord{1, 0}}));
    ASSERT_TRUE(game.adventure_movement_state().planned);
    ASSERT_TRUE(d2engine::AdventureMovementClickController::handle_left_click(
        game, {std::string{"OTHER"}, std::nullopt}));
    EXPECT_EQ(game.adventure_movement_state().selected_stack_id, "OTHER");
    EXPECT_FALSE(game.adventure_movement_state().planned);
}

TEST(AdventureMovementClickController, ReplanningDoesNotConfirmOrMutateStack) {
    auto       game = session();
    const auto before = game.world();
    (void)d2engine::AdventureMovementClickController::handle_left_click(
        game, {std::string{"STACK"}, std::nullopt});
    ASSERT_TRUE(d2engine::AdventureMovementClickController::handle_left_click(
        game, {std::nullopt, d2runtime::MapCellCoord{1, 0}}));
    const auto planned = game.adventure_movement_state().planned;
    ASSERT_TRUE(planned);
    ASSERT_TRUE(d2engine::AdventureMovementClickController::handle_left_click(
        game, {std::nullopt, d2runtime::MapCellCoord{2, 0}}));
    EXPECT_EQ(game.adventure_movement_state().mode, d2game::AdventureInteractionMode::RoutePlanned);
    EXPECT_FALSE(game.adventure_movement_state().execution);
    EXPECT_EQ(game.world().stacks.front().position, before.stacks.front().position);
    EXPECT_EQ(game.world().stacks.front().move, before.stacks.front().move);
    EXPECT_EQ(game.adventure_movement_state().planned->profile,
              d2adventure::AdventureMovementProfile::Walking);
    EXPECT_NE(game.adventure_movement_state().planned->route, planned->route);
}
