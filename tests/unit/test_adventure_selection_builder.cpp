#include <d2engine/app/adventure_selection_builder.hpp>

#include <gtest/gtest.h>

#include <string>
#include <utility>

namespace {

d2runtime::AdventureWorldState make_world() {
    d2runtime::AdventureWorldState world;
    world.map_width = 20;
    world.map_height = 20;
    world.terrain.width = 20;
    world.terrain.height = 20;
    world.terrain.tiles.assign(400, {});

    d2runtime::AdventureSubraceRef subrace;
    subrace.id = "SUB1";
    subrace.race_id = "g000rr0000";
    world.subraces.push_back(subrace);

    return world;
}

d2runtime::AdventureStack make_stack(std::string id, std::string inside) {
    d2runtime::AdventureStack stack;
    stack.id = std::move(id);
    stack.position = {4, 4};
    stack.inside = std::move(inside);
    return stack;
}

d2runtime::AdventureCity make_city(std::string id, std::string stack_id) {
    d2runtime::AdventureCity city;
    city.id = std::move(id);
    city.stack_id = std::move(stack_id);
    city.footprint = {{5, 5}, {6, 5}, {5, 6}, {6, 6}};
    return city;
}

d2runtime::AdventureCapital make_capital(std::string id, std::string visiting_stack_id) {
    d2runtime::AdventureCapital capital;
    capital.id = std::move(id);
    capital.visiting_stack_id = std::move(visiting_stack_id);
    capital.footprint = {{8, 8}, {9, 8}, {8, 9}, {9, 9}};
    return capital;
}

} // namespace

TEST(AdventureSelectionBuilder, MapVisibleStackReturnsOwnCell) {
    auto world = make_world();
    world.stacks.push_back(make_stack("STACK", ""));

    const auto* stack = world.find_stack("STACK");
    ASSERT_NE(stack, nullptr);
    const auto cell = d2engine::resolve_stack_selection_cell(world, *stack);
    ASSERT_TRUE(cell.has_value());
    EXPECT_EQ(*cell, (d2runtime::MapCellCoord{4, 4}));
}

TEST(AdventureSelectionBuilder, VillageContainedStackResolvesToSettlementDepthAnchor) {
    auto world = make_world();
    world.cities.push_back(make_city("CITY", "STACK"));
    world.stacks.push_back(make_stack("STACK", "CITY"));

    const auto* stack = world.find_stack("STACK");
    ASSERT_NE(stack, nullptr);
    const auto cell = d2engine::resolve_stack_selection_cell(world, *stack);
    ASSERT_TRUE(cell.has_value());
    EXPECT_EQ(*cell, d2engine::adventure_render::AdventureMapGeometry::derive_depth_anchor(
                         world.cities.front().footprint));
}

TEST(AdventureSelectionBuilder, CapitalContainedStackResolvesToSettlementDepthAnchor) {
    auto world = make_world();
    world.capitals.push_back(make_capital("CAP", "STACK"));
    world.stacks.push_back(make_stack("STACK", "CAP"));

    const auto* stack = world.find_stack("STACK");
    ASSERT_NE(stack, nullptr);
    const auto cell = d2engine::resolve_stack_selection_cell(world, *stack);
    ASSERT_TRUE(cell.has_value());
    EXPECT_EQ(*cell, d2engine::adventure_render::AdventureMapGeometry::derive_depth_anchor(
                         world.capitals.front().footprint));
}

TEST(AdventureSelectionBuilder, InvalidContainedStackReturnsNullopt) {
    auto world = make_world();
    world.cities.push_back(make_city("CITY", "STACK"));
    world.stacks.push_back(make_stack("STACK", "MISSING"));

    const auto* stack = world.find_stack("STACK");
    ASSERT_NE(stack, nullptr);
    EXPECT_FALSE(d2engine::resolve_stack_selection_cell(world, *stack).has_value());
}
