#include <d2runtime/AdventureWorldBuilder.hpp>

#include <d2scenario/ScenarioTemplate.hpp>

#include <gtest/gtest.h>

#include <utility>

TEST(AdventureWorldBuilder, ResolvesRuinPlacementFromTerrain) {
    d2scenario::ScenarioTemplate scenario;
    scenario.info.id = "test";
    scenario.info.name = "test";
    scenario.map.terrain.width = 1;
    scenario.map.terrain.height = 1;
    scenario.map.terrain.tiles = {{7}};

    d2scenario::SgRuin ruin;
    ruin.id = "S143RU0000";
    ruin.image = 7;
    ruin.pos_x = 0;
    ruin.pos_y = 0;
    scenario.ruins.push_back(std::move(ruin));

    d2runtime::AdventureWorldBuilder builder;
    const auto                       result = builder.build(scenario);

    ASSERT_EQ(result.world.ruins.size(), 1u);
    EXPECT_EQ(result.world.ruins.front().placement, d2runtime::AdventureSurfacePlacement::Water);
}
