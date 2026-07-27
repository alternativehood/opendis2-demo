#include <gtest/gtest.h>

#include <d2runtime/AdventureGroundType.hpp>
#include <d2runtime/MovementCapabilities.hpp>
#include <d2runtime/AdventureStackPresentationResolver.hpp>
#include <d2runtime/AdventureActorAnimationResolver.hpp>

#include <span>
#include <vector>

using namespace d2runtime;

// ── AdventureGroundType ────────────────────────────────────────────────

TEST(AdventureGroundType, CanonicalMapping) {
    EXPECT_EQ(adventure_ground_type_from_id(0), AdventureGroundType::Plain);
    EXPECT_EQ(adventure_ground_type_from_id(1), AdventureGroundType::Forest);
    EXPECT_EQ(adventure_ground_type_from_id(2), AdventureGroundType::Exceptional);
    EXPECT_EQ(adventure_ground_type_from_id(3), AdventureGroundType::Water);
    EXPECT_EQ(adventure_ground_type_from_id(4), AdventureGroundType::Mountain);
    EXPECT_EQ(adventure_ground_type_from_id(5), AdventureGroundType::Hill);
}

TEST(AdventureGroundType, UnknownIdReturnsUnknown) {
    EXPECT_EQ(adventure_ground_type_from_id(99), AdventureGroundType::Unknown);
    EXPECT_EQ(adventure_ground_type_from_id(-1), AdventureGroundType::Unknown);
    EXPECT_EQ(adventure_ground_type_from_id(6), AdventureGroundType::Unknown);
}

// ── MovementCapabilities ───────────────────────────────────────────────

TEST(MovementCapabilities, WaterAbilityFromNativeId) {
    auto caps = MovementCapabilities::from_native_ability_ids({3});
    EXPECT_TRUE(caps.can_natively_traverse(AdventureGroundType::Water));
    EXPECT_FALSE(caps.can_natively_traverse(AdventureGroundType::Plain));
}

TEST(MovementCapabilities, NoWaterReturnsFalse) {
    auto caps = MovementCapabilities::from_native_ability_ids({0, 1});
    EXPECT_FALSE(caps.can_natively_traverse(AdventureGroundType::Water));
}

TEST(MovementCapabilities, MultipleCapabilities) {
    auto caps = MovementCapabilities::from_native_ability_ids({0, 1, 3});
    EXPECT_TRUE(caps.can_natively_traverse(AdventureGroundType::Plain));
    EXPECT_TRUE(caps.can_natively_traverse(AdventureGroundType::Forest));
    EXPECT_TRUE(caps.can_natively_traverse(AdventureGroundType::Water));
    EXPECT_FALSE(caps.can_natively_traverse(AdventureGroundType::Mountain));
}

TEST(MovementCapabilities, UnknownRawIdNotAdded) {
    auto caps = MovementCapabilities::from_native_ability_ids({3, 99, -1});
    EXPECT_TRUE(caps.can_natively_traverse(AdventureGroundType::Water));
    EXPECT_EQ(caps.capabilities().size(), 1u);
}

TEST(MovementCapabilities, SpanConstructorWorks) {
    std::vector<int> ids = {0, 3, 5};
    auto caps = MovementCapabilities::from_native_ability_ids(std::span<const int>(ids));
    EXPECT_TRUE(caps.can_natively_traverse(AdventureGroundType::Plain));
    EXPECT_TRUE(caps.can_natively_traverse(AdventureGroundType::Water));
    EXPECT_TRUE(caps.can_natively_traverse(AdventureGroundType::Hill));
    EXPECT_EQ(caps.capabilities().size(), 3u);
}

// ── AdventureStackPresentationResolver ─────────────────────────────────

TEST(AdventureStackPresentation, GroundUnitOnLandIsUnit) {
    AdventureStackPresentationInput input;
    input.ground = AdventureGroundType::Plain;
    input.leader_movement = MovementCapabilities::from_native_ability_ids({0});
    input.leader_water_only = false;
    AdventureStackPresentationResolver resolver;
    EXPECT_EQ(resolver.resolve(input).kind, AdventureActorPresentationKind::Unit);
}

TEST(AdventureStackPresentation, GroundUnitOnWaterIsBoat) {
    AdventureStackPresentationInput input;
    input.ground = AdventureGroundType::Water;
    input.leader_movement = MovementCapabilities::from_native_ability_ids({0});
    input.leader_water_only = false;
    AdventureStackPresentationResolver resolver;
    EXPECT_EQ(resolver.resolve(input).kind, AdventureActorPresentationKind::Boat);
}

TEST(AdventureStackPresentation, SwimmerOnWaterIsUnit) {
    AdventureStackPresentationInput input;
    input.ground = AdventureGroundType::Water;
    input.leader_movement = MovementCapabilities::from_native_ability_ids({3});
    input.leader_water_only = true;
    AdventureStackPresentationResolver resolver;
    EXPECT_EQ(resolver.resolve(input).kind, AdventureActorPresentationKind::Unit);
}

TEST(AdventureStackPresentation, NativeWaterNonWaterOnlyIsUnit) {
    AdventureStackPresentationInput input;
    input.ground = AdventureGroundType::Water;
    input.leader_movement = MovementCapabilities::from_native_ability_ids({3});
    input.leader_water_only = false;
    AdventureStackPresentationResolver resolver;
    EXPECT_EQ(resolver.resolve(input).kind, AdventureActorPresentationKind::Unit);
}

TEST(AdventureStackPresentation, WaterOnlyUnitOnNonWaterIsUnit) {
    AdventureStackPresentationInput input;
    input.ground = AdventureGroundType::Plain;
    input.leader_movement = MovementCapabilities::from_native_ability_ids({3});
    input.leader_water_only = true;
    AdventureStackPresentationResolver resolver;
    EXPECT_EQ(resolver.resolve(input).kind, AdventureActorPresentationKind::Unit);
}

// ── AdventureActorAnimationResolver — Unit ─────────────────────────────

TEST(AdventureActorAnimationResolver, UnitIdleUsesStop0Convention) {
    AdventureActorAnimationResolver resolver;
    AdventureActorPresentation      pres{.kind = AdventureActorPresentationKind::Unit};
    auto id = resolver.resolve(pres, AdventureActorAnimationRole::Idle,
                               AdventureActorAnimationLayer::Main, "G000UU0001", "G000RR0000",
                               AdventureIsoDirection::D0);
    ASSERT_TRUE(id.has_value());
    EXPECT_EQ(id->container_path, "Imgs/Isounit.ff");
    EXPECT_EQ(id->logical_animation_name, "G000UU0001STOP0");
}

TEST(AdventureActorAnimationResolver, UnitIdleD3) {
    AdventureActorAnimationResolver resolver;
    AdventureActorPresentation      pres{.kind = AdventureActorPresentationKind::Unit};
    auto id = resolver.resolve(pres, AdventureActorAnimationRole::Idle,
                               AdventureActorAnimationLayer::Main, "G000UU0001", "G000RR0000",
                               AdventureIsoDirection::D3);
    ASSERT_TRUE(id.has_value());
    EXPECT_EQ(id->container_path, "Imgs/Isounit.ff");
    EXPECT_EQ(id->logical_animation_name, "G000UU0001STOP3");
}

TEST(AdventureActorAnimationResolver, UnitIdleD7) {
    AdventureActorAnimationResolver resolver;
    AdventureActorPresentation      pres{.kind = AdventureActorPresentationKind::Unit};
    auto id = resolver.resolve(pres, AdventureActorAnimationRole::Idle,
                               AdventureActorAnimationLayer::Main, "G000UU0001", "G000RR0000",
                               AdventureIsoDirection::D7);
    ASSERT_TRUE(id.has_value());
    EXPECT_EQ(id->logical_animation_name, "G000UU0001STOP7");
}

TEST(AdventureActorAnimationResolver, UnitMoveD0) {
    AdventureActorAnimationResolver resolver;
    AdventureActorPresentation      pres{.kind = AdventureActorPresentationKind::Unit};
    auto id = resolver.resolve(pres, AdventureActorAnimationRole::Move,
                               AdventureActorAnimationLayer::Main, "G000UU0001", "G000RR0000",
                               AdventureIsoDirection::D0);
    ASSERT_TRUE(id.has_value());
    EXPECT_EQ(id->logical_animation_name, "G000UU0001MOVE0");
}

TEST(AdventureActorAnimationResolver, UnitMoveD5) {
    AdventureActorAnimationResolver resolver;
    AdventureActorPresentation      pres{.kind = AdventureActorPresentationKind::Unit};
    auto id = resolver.resolve(pres, AdventureActorAnimationRole::Move,
                               AdventureActorAnimationLayer::Main, "G000UU0001", "G000RR0000",
                               AdventureIsoDirection::D5);
    ASSERT_TRUE(id.has_value());
    EXPECT_EQ(id->logical_animation_name, "G000UU0001MOVE5");
}

// ── AdventureActorAnimationResolver — Boat idle ────────────────────────

TEST(AdventureActorAnimationResolver, BoatIdleD0) {
    AdventureActorAnimationResolver resolver;
    AdventureActorPresentation      pres{.kind = AdventureActorPresentationKind::Boat};
    auto id = resolver.resolve(pres, AdventureActorAnimationRole::Idle,
                               AdventureActorAnimationLayer::Main, "G000UU0001", "G000RR0000",
                               AdventureIsoDirection::D0);
    ASSERT_TRUE(id.has_value());
    EXPECT_EQ(id->logical_animation_name, "G000RR0000BOAT0");
}

TEST(AdventureActorAnimationResolver, BoatIdleD3) {
    AdventureActorAnimationResolver resolver;
    AdventureActorPresentation      pres{.kind = AdventureActorPresentationKind::Boat};
    auto id = resolver.resolve(pres, AdventureActorAnimationRole::Idle,
                               AdventureActorAnimationLayer::Main, "G000UU0001", "G000RR0000",
                               AdventureIsoDirection::D3);
    ASSERT_TRUE(id.has_value());
    EXPECT_EQ(id->logical_animation_name, "G000RR0000BOAT3");
}

TEST(AdventureActorAnimationResolver, BoatIdleD7) {
    AdventureActorAnimationResolver resolver;
    AdventureActorPresentation      pres{.kind = AdventureActorPresentationKind::Boat};
    auto id = resolver.resolve(pres, AdventureActorAnimationRole::Idle,
                               AdventureActorAnimationLayer::Main, "G000UU0001", "G000RR0000",
                               AdventureIsoDirection::D7);
    ASSERT_TRUE(id.has_value());
    EXPECT_EQ(id->logical_animation_name, "G000RR0000BOAT7");
}

// ── AdventureActorAnimationResolver — Boat move ────────────────────────

TEST(AdventureActorAnimationResolver, BoatMoveD0) {
    AdventureActorAnimationResolver resolver;
    AdventureActorPresentation      pres{.kind = AdventureActorPresentationKind::Boat};
    auto id = resolver.resolve(pres, AdventureActorAnimationRole::Move,
                               AdventureActorAnimationLayer::Main, "G000UU0001", "G000RR0002",
                               AdventureIsoDirection::D0);
    ASSERT_TRUE(id.has_value());
    EXPECT_EQ(id->logical_animation_name, "G000RR0002BTMV0");
}

TEST(AdventureActorAnimationResolver, BoatMoveD5) {
    AdventureActorAnimationResolver resolver;
    AdventureActorPresentation      pres{.kind = AdventureActorPresentationKind::Boat};
    auto id = resolver.resolve(pres, AdventureActorAnimationRole::Move,
                               AdventureActorAnimationLayer::Main, "G000UU0001", "G000RR0002",
                               AdventureIsoDirection::D5);
    ASSERT_TRUE(id.has_value());
    EXPECT_EQ(id->logical_animation_name, "G000RR0002BTMV5");
}

// ── G000RR0005: standard naming confirmed (BTMV has 16 frames, BBTMV is
// a catalog anomaly — not a different animation family) ──────────────

TEST(AdventureActorAnimationResolver, G000RR0005BoatMoveUsesStandardBtmv) {
    AdventureActorAnimationResolver resolver;
    AdventureActorPresentation      pres{.kind = AdventureActorPresentationKind::Boat};
    auto id = resolver.resolve(pres, AdventureActorAnimationRole::Move,
                               AdventureActorAnimationLayer::Main, "G000UU0001", "G000RR0005",
                               AdventureIsoDirection::D0);
    ASSERT_TRUE(id.has_value());
    EXPECT_EQ(id->logical_animation_name, "G000RR0005BTMV0");
}

TEST(AdventureActorAnimationResolver, G000RR0005BoatIdleUsesStandardNaming) {
    AdventureActorAnimationResolver resolver;
    AdventureActorPresentation      pres{.kind = AdventureActorPresentationKind::Boat};
    auto id = resolver.resolve(pres, AdventureActorAnimationRole::Idle,
                               AdventureActorAnimationLayer::Main, "G000UU0001", "G000RR0005",
                               AdventureIsoDirection::D4);
    ASSERT_TRUE(id.has_value());
    EXPECT_EQ(id->logical_animation_name, "G000RR0005BOAT4");
}
