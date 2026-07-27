#include <d2adventure_rules/AdventureMovementPolicy.hpp>
#include <d2adventure_rules/AdventureMovementProfile.hpp>
#include <d2engine/assets/unit_def.hpp>
#include <d2runtime/AdventureGroundClassifier.hpp>

#include <gtest/gtest.h>

#include <utility>
#include <vector>

namespace {

d2runtime::AdventureTerrainTileDescriptor descriptor(d2runtime::AdventureTerrainMaterial material,
                                                     bool forest = false) {
    d2runtime::AdventureTerrainTileDescriptor result;
    result.material = material;
    result.is_forest = forest;
    return result;
}

d2engine::UnitDef unit(std::vector<int> abilities = {}, bool water_only = false) {
    d2engine::UnitDef result;
    result.native_ability_ids = std::move(abilities);
    result.water_only = water_only;
    return result;
}

d2adventure::AdventureMovementDecision
move(d2adventure::AdventureMovementProfile profile, d2runtime::AdventureGroundType ground,
     bool                                road = false,
     d2adventure::AdventureCellOccupancy occupancy = d2adventure::AdventureCellOccupancy::Free) {
    return d2adventure::evaluate_adventure_movement_cell(profile, {ground, road, occupancy});
}

} // namespace

TEST(AdventureGroundClassifier, WaterMaterialIsWater) {
    EXPECT_EQ(d2runtime::classify_adventure_ground(
                  descriptor(d2runtime::AdventureTerrainMaterial::Water)),
              d2runtime::AdventureGroundType::Water);
}

TEST(AdventureGroundClassifier, WaterWinsOverForest) {
    EXPECT_EQ(d2runtime::classify_adventure_ground(
                  descriptor(d2runtime::AdventureTerrainMaterial::Water, true)),
              d2runtime::AdventureGroundType::Water);
}

TEST(AdventureGroundClassifier, KnownForestIsForest) {
    EXPECT_EQ(d2runtime::classify_adventure_ground(
                  descriptor(d2runtime::AdventureTerrainMaterial::Human, true)),
              d2runtime::AdventureGroundType::Forest);
}

TEST(AdventureGroundClassifier, KnownNonForestIsPlain) {
    EXPECT_EQ(d2runtime::classify_adventure_ground(
                  descriptor(d2runtime::AdventureTerrainMaterial::Human)),
              d2runtime::AdventureGroundType::Plain);
}

TEST(AdventureGroundClassifier, UnknownIsUnknown) {
    EXPECT_EQ(d2runtime::classify_adventure_ground(
                  descriptor(d2runtime::AdventureTerrainMaterial::Unknown)),
              d2runtime::AdventureGroundType::Unknown);
}

TEST(AdventureMovementProfile, EmptyAbilitiesWalk) {
    EXPECT_EQ(d2adventure::resolve_adventure_movement_profile(unit()),
              d2adventure::AdventureMovementProfile::Walking);
}

TEST(AdventureMovementProfile, PartialAbilitiesWalk) {
    EXPECT_EQ(d2adventure::resolve_adventure_movement_profile(unit({0, 1})),
              d2adventure::AdventureMovementProfile::Walking);
}

TEST(AdventureMovementProfile, FullAbilitiesFly) {
    EXPECT_EQ(d2adventure::resolve_adventure_movement_profile(unit({0, 1, 3})),
              d2adventure::AdventureMovementProfile::Flying);
}

TEST(AdventureMovementProfile, FullAbilitiesAnyOrderFly) {
    EXPECT_EQ(d2adventure::resolve_adventure_movement_profile(unit({3, 1, 0})),
              d2adventure::AdventureMovementProfile::Flying);
}

TEST(AdventureMovementProfile, WaterOnlySwims) {
    EXPECT_EQ(d2adventure::resolve_adventure_movement_profile(unit({}, true)),
              d2adventure::AdventureMovementProfile::Swimming);
}

TEST(AdventureMovementProfile, WaterOnlyWinsOverFlying) {
    EXPECT_EQ(d2adventure::resolve_adventure_movement_profile(unit({0, 1, 3}, true)),
              d2adventure::AdventureMovementProfile::Swimming);
}

TEST(AdventureMovementPolicy, FlyingPlainCostsTwo) {
    const auto result =
        move(d2adventure::AdventureMovementProfile::Flying, d2runtime::AdventureGroundType::Plain);
    EXPECT_TRUE(result.passable);
    EXPECT_EQ(result.movement_cost, 2);
}
TEST(AdventureMovementPolicy, FlyingForestCostsTwo) {
    const auto result =
        move(d2adventure::AdventureMovementProfile::Flying, d2runtime::AdventureGroundType::Forest);
    EXPECT_TRUE(result.passable);
    EXPECT_EQ(result.movement_cost, 2);
}
TEST(AdventureMovementPolicy, FlyingWaterCostsTwo) {
    const auto result =
        move(d2adventure::AdventureMovementProfile::Flying, d2runtime::AdventureGroundType::Water);
    EXPECT_TRUE(result.passable);
    EXPECT_EQ(result.movement_cost, 2);
}
TEST(AdventureMovementPolicy, FlyingRoadStillCostsTwo) {
    const auto result = move(d2adventure::AdventureMovementProfile::Flying,
                             d2runtime::AdventureGroundType::Plain, true);
    EXPECT_TRUE(result.passable);
    EXPECT_EQ(result.movement_cost, 2);
}

TEST(AdventureMovementPolicy, WalkingRoadPlainCostsOne) {
    EXPECT_EQ(move(d2adventure::AdventureMovementProfile::Walking,
                   d2runtime::AdventureGroundType::Plain, true)
                  .movement_cost,
              1);
}
TEST(AdventureMovementPolicy, WalkingRoadForestCostsOne) {
    EXPECT_EQ(move(d2adventure::AdventureMovementProfile::Walking,
                   d2runtime::AdventureGroundType::Forest, true)
                  .movement_cost,
              1);
}
TEST(AdventureMovementPolicy, WalkingRoadWaterCostsOne) {
    EXPECT_EQ(move(d2adventure::AdventureMovementProfile::Walking,
                   d2runtime::AdventureGroundType::Water, true)
                  .movement_cost,
              1);
}
TEST(AdventureMovementPolicy, WalkingPlainCostsThree) {
    EXPECT_EQ(
        move(d2adventure::AdventureMovementProfile::Walking, d2runtime::AdventureGroundType::Plain)
            .movement_cost,
        3);
}
TEST(AdventureMovementPolicy, WalkingForestCostsFour) {
    EXPECT_EQ(
        move(d2adventure::AdventureMovementProfile::Walking, d2runtime::AdventureGroundType::Forest)
            .movement_cost,
        4);
}
TEST(AdventureMovementPolicy, WalkingWaterCostsSix) {
    EXPECT_EQ(
        move(d2adventure::AdventureMovementProfile::Walking, d2runtime::AdventureGroundType::Water)
            .movement_cost,
        6);
}
TEST(AdventureMovementPolicy, SwimmingWaterCostsTwo) {
    const auto result = move(d2adventure::AdventureMovementProfile::Swimming,
                             d2runtime::AdventureGroundType::Water);
    EXPECT_TRUE(result.passable);
    EXPECT_EQ(result.movement_cost, 2);
}
TEST(AdventureMovementPolicy, SwimmingPlainRestricted) {
    const auto result = move(d2adventure::AdventureMovementProfile::Swimming,
                             d2runtime::AdventureGroundType::Plain);
    EXPECT_FALSE(result.passable);
    EXPECT_EQ(result.block_reason, d2adventure::AdventureMovementBlockReason::ProfileRestriction);
}
TEST(AdventureMovementPolicy, SwimmingForestRestricted) {
    const auto result = move(d2adventure::AdventureMovementProfile::Swimming,
                             d2runtime::AdventureGroundType::Forest);
    EXPECT_FALSE(result.passable);
    EXPECT_EQ(result.block_reason, d2adventure::AdventureMovementBlockReason::ProfileRestriction);
}
TEST(AdventureMovementPolicy, SwimmingRoadPlainStillRestricted) {
    const auto result = move(d2adventure::AdventureMovementProfile::Swimming,
                             d2runtime::AdventureGroundType::Plain, true);
    EXPECT_FALSE(result.passable);
    EXPECT_EQ(result.block_reason, d2adventure::AdventureMovementBlockReason::ProfileRestriction);
}
TEST(AdventureMovementPolicy, BlockingObjectBlocksWalking) {
    const auto result =
        move(d2adventure::AdventureMovementProfile::Walking, d2runtime::AdventureGroundType::Plain,
             false, d2adventure::AdventureCellOccupancy::BlockingObject);
    EXPECT_FALSE(result.passable);
    EXPECT_EQ(result.block_reason, d2adventure::AdventureMovementBlockReason::BlockingObject);
}
TEST(AdventureMovementPolicy, BlockingObjectBlocksFlying) {
    const auto result =
        move(d2adventure::AdventureMovementProfile::Flying, d2runtime::AdventureGroundType::Plain,
             false, d2adventure::AdventureCellOccupancy::BlockingObject);
    EXPECT_FALSE(result.passable);
    EXPECT_EQ(result.block_reason, d2adventure::AdventureMovementBlockReason::BlockingObject);
}
TEST(AdventureMovementPolicy, OccupiedStackBlocksSwimming) {
    const auto result =
        move(d2adventure::AdventureMovementProfile::Swimming, d2runtime::AdventureGroundType::Water,
             false, d2adventure::AdventureCellOccupancy::OccupiedByStack);
    EXPECT_FALSE(result.passable);
    EXPECT_EQ(result.block_reason, d2adventure::AdventureMovementBlockReason::OccupiedByStack);
}
TEST(AdventureMovementPolicy, UnknownGroundUnsupported) {
    const auto result = move(d2adventure::AdventureMovementProfile::Walking,
                             d2runtime::AdventureGroundType::Unknown);
    EXPECT_FALSE(result.passable);
    EXPECT_EQ(result.block_reason, d2adventure::AdventureMovementBlockReason::UnsupportedGround);
}

TEST(AdventureMovementPolicy, ExceptionalGroundUnsupported) {
    EXPECT_EQ(move(d2adventure::AdventureMovementProfile::Walking,
                   d2runtime::AdventureGroundType::Exceptional)
                  .block_reason,
              d2adventure::AdventureMovementBlockReason::UnsupportedGround);
}
TEST(AdventureMovementPolicy, MountainGroundUnsupported) {
    EXPECT_EQ(move(d2adventure::AdventureMovementProfile::Walking,
                   d2runtime::AdventureGroundType::Mountain)
                  .block_reason,
              d2adventure::AdventureMovementBlockReason::UnsupportedGround);
}
TEST(AdventureMovementPolicy, HillGroundUnsupported) {
    EXPECT_EQ(
        move(d2adventure::AdventureMovementProfile::Walking, d2runtime::AdventureGroundType::Hill)
            .block_reason,
        d2adventure::AdventureMovementBlockReason::UnsupportedGround);
}

TEST(AdventureMovementPolicy, EveryBlockedResultCostsZero) {
    for (const auto result : {move(d2adventure::AdventureMovementProfile::Walking,
                                   d2runtime::AdventureGroundType::Unknown),
                              move(d2adventure::AdventureMovementProfile::Walking,
                                   d2runtime::AdventureGroundType::Plain, false,
                                   d2adventure::AdventureCellOccupancy::BlockingObject),
                              move(d2adventure::AdventureMovementProfile::Swimming,
                                   d2runtime::AdventureGroundType::Plain)})
        if (!result.passable)
            EXPECT_EQ(result.movement_cost, 0);
}

TEST(AdventureMovementPolicy, EveryPassableResultHasNoBlockReason) {
    for (const auto result : {move(d2adventure::AdventureMovementProfile::Walking,
                                   d2runtime::AdventureGroundType::Plain),
                              move(d2adventure::AdventureMovementProfile::Flying,
                                   d2runtime::AdventureGroundType::Water),
                              move(d2adventure::AdventureMovementProfile::Swimming,
                                   d2runtime::AdventureGroundType::Water)})
        if (result.passable)
            EXPECT_EQ(result.block_reason, d2adventure::AdventureMovementBlockReason::None);
}
