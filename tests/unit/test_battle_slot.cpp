#include <gtest/gtest.h>

#include "d2engine/battle_view/battle_render_tree_contract.hpp"
#include "d2engine/battle_view/battle_slot.hpp"

#include <stdexcept>

namespace d2engine {

TEST(BattleSlotCoord, DefaultConstruction) {
    const BattleSlotCoord c{};
    EXPECT_EQ(c.side, BattleSide::Attacker);
    EXPECT_EQ(c.lane, 0);
    EXPECT_EQ(c.depth, BattleDepth::Front);
}

TEST(BattleSlotCoord, Equality) {
    const BattleSlotCoord a{.side = BattleSide::Defender, .lane = 2, .depth = BattleDepth::Back};
    const BattleSlotCoord b{.side = BattleSide::Defender, .lane = 2, .depth = BattleDepth::Back};
    EXPECT_EQ(a, b);
}

TEST(BattleSlotCoord, Inequality) {
    const BattleSlotCoord a{.side = BattleSide::Attacker, .lane = 0, .depth = BattleDepth::Front};
    const BattleSlotCoord b{.side = BattleSide::Defender, .lane = 0, .depth = BattleDepth::Front};
    EXPECT_NE(a, b);
}

TEST(BattleSlotVisual, DefaultConstruction) {
    const BattleSlotVisual sv{};
    EXPECT_EQ(sv.coord.side, BattleSide::Attacker);
    EXPECT_FLOAT_EQ(sv.position.x, 0.0F);
    EXPECT_FLOAT_EQ(sv.debug_bounds.w, 0.0F);
}

TEST(ParsePositionString, ValidAttackerFront) {
    const auto c = parse_position_string("A_FRONT_0");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->side, BattleSide::Attacker);
    EXPECT_EQ(c->lane, 0);
    EXPECT_EQ(c->depth, BattleDepth::Front);
}

TEST(ParsePositionString, ValidDefenderBack) {
    const auto c = parse_position_string("D_BACK_2");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->side, BattleSide::Defender);
    EXPECT_EQ(c->lane, 2);
    EXPECT_EQ(c->depth, BattleDepth::Back);
}

TEST(ParsePositionString, ValidCenter) {
    const auto c = parse_position_string("A_CENTER_1");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->side, BattleSide::Attacker);
    EXPECT_EQ(c->lane, 1);
    EXPECT_EQ(c->depth, BattleDepth::Center);
}

TEST(ParsePositionString, ValidDefenderCenter) {
    const auto c = parse_position_string("D_CENTER_2");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->side, BattleSide::Defender);
    EXPECT_EQ(c->lane, 2);
    EXPECT_EQ(c->depth, BattleDepth::Center);
}

TEST(ParsePositionString, InvalidSide) {
    EXPECT_FALSE(parse_position_string("X_FRONT_0").has_value());
}

TEST(ParsePositionString, InvalidDepth) {
    EXPECT_FALSE(parse_position_string("A_MIDDLE_1").has_value());
}

TEST(ParsePositionString, InvalidLane) {
    EXPECT_FALSE(parse_position_string("A_FRONT_3").has_value());
}

TEST(ParsePositionString, TooShort) {
    EXPECT_FALSE(parse_position_string("A").has_value());
}

// ── Tree path generation ──────────────────────────────────────────────

TEST(BattlefieldSlotTreePath, ABack0) {
    const BattleSlotCoord c{.side = BattleSide::Attacker, .lane = 0, .depth = BattleDepth::Back};
    EXPECT_EQ(battlefield_slot_tree_path(c), "/battlefield/slot_a_back_0");
}

TEST(BattlefieldSlotTreePath, ABack1) {
    const BattleSlotCoord c{.side = BattleSide::Attacker, .lane = 1, .depth = BattleDepth::Back};
    EXPECT_EQ(battlefield_slot_tree_path(c), "/battlefield/slot_a_back_1");
}

TEST(BattlefieldSlotTreePath, ABack2) {
    const BattleSlotCoord c{.side = BattleSide::Attacker, .lane = 2, .depth = BattleDepth::Back};
    EXPECT_EQ(battlefield_slot_tree_path(c), "/battlefield/slot_a_back_2");
}

TEST(BattlefieldSlotTreePath, AFront0) {
    const BattleSlotCoord c{.side = BattleSide::Attacker, .lane = 0, .depth = BattleDepth::Front};
    EXPECT_EQ(battlefield_slot_tree_path(c), "/battlefield/slot_a_front_0");
}

TEST(BattlefieldSlotTreePath, AFront1) {
    const BattleSlotCoord c{.side = BattleSide::Attacker, .lane = 1, .depth = BattleDepth::Front};
    EXPECT_EQ(battlefield_slot_tree_path(c), "/battlefield/slot_a_front_1");
}

TEST(BattlefieldSlotTreePath, AFront2) {
    const BattleSlotCoord c{.side = BattleSide::Attacker, .lane = 2, .depth = BattleDepth::Front};
    EXPECT_EQ(battlefield_slot_tree_path(c), "/battlefield/slot_a_front_2");
}

TEST(BattlefieldSlotTreePath, DBack0) {
    const BattleSlotCoord c{.side = BattleSide::Defender, .lane = 0, .depth = BattleDepth::Back};
    EXPECT_EQ(battlefield_slot_tree_path(c), "/battlefield/slot_d_back_0");
}

TEST(BattlefieldSlotTreePath, DBack1) {
    const BattleSlotCoord c{.side = BattleSide::Defender, .lane = 1, .depth = BattleDepth::Back};
    EXPECT_EQ(battlefield_slot_tree_path(c), "/battlefield/slot_d_back_1");
}

TEST(BattlefieldSlotTreePath, DBack2) {
    const BattleSlotCoord c{.side = BattleSide::Defender, .lane = 2, .depth = BattleDepth::Back};
    EXPECT_EQ(battlefield_slot_tree_path(c), "/battlefield/slot_d_back_2");
}

TEST(BattlefieldSlotTreePath, DFront0) {
    const BattleSlotCoord c{.side = BattleSide::Defender, .lane = 0, .depth = BattleDepth::Front};
    EXPECT_EQ(battlefield_slot_tree_path(c), "/battlefield/slot_d_front_0");
}

TEST(BattlefieldSlotTreePath, DFront1) {
    const BattleSlotCoord c{.side = BattleSide::Defender, .lane = 1, .depth = BattleDepth::Front};
    EXPECT_EQ(battlefield_slot_tree_path(c), "/battlefield/slot_d_front_1");
}

TEST(BattlefieldSlotTreePath, DFront2) {
    const BattleSlotCoord c{.side = BattleSide::Defender, .lane = 2, .depth = BattleDepth::Front};
    EXPECT_EQ(battlefield_slot_tree_path(c), "/battlefield/slot_d_front_2");
}

TEST(BattlefieldUnitTreePath, AppendsUnitSuffix) {
    const BattleSlotCoord c{.side = BattleSide::Attacker, .lane = 0, .depth = BattleDepth::Front};
    EXPECT_EQ(battlefield_unit_tree_path(c), "/battlefield/slot_a_front_0/unit");
}

TEST(BattlefieldUnitTreePath, DefenderBack2) {
    const BattleSlotCoord c{.side = BattleSide::Defender, .lane = 2, .depth = BattleDepth::Back};
    EXPECT_EQ(battlefield_unit_tree_path(c), "/battlefield/slot_d_back_2/unit");
}

// ── CENTER slot tree paths ────────────────────────────────────────────────

TEST(BattlefieldSlotTreePath, ACenter1) {
    const BattleSlotCoord c{.side = BattleSide::Attacker, .lane = 1, .depth = BattleDepth::Center};
    EXPECT_EQ(battlefield_slot_tree_path(c), "/battlefield/slot_a_center_1");
}

TEST(BattlefieldSlotTreePath, DCenter1) {
    const BattleSlotCoord c{.side = BattleSide::Defender, .lane = 1, .depth = BattleDepth::Center};
    EXPECT_EQ(battlefield_slot_tree_path(c), "/battlefield/slot_d_center_1");
}

TEST(SlotCoordToString, Center) {
    const BattleSlotCoord c{.side = BattleSide::Defender, .lane = 1, .depth = BattleDepth::Center};
    EXPECT_EQ(slot_coord_to_string(c), "D_CENTER_1");
}

// ── Slot classification helpers ───────────────────────────────────────────

TEST(SlotHelpers, IsFrontSlot) {
    EXPECT_TRUE(
        (BattleSlotCoord{.side = BattleSide::Attacker, .lane = 0, .depth = BattleDepth::Front}
             .depth == BattleDepth::Front));
    EXPECT_FALSE(
        (BattleSlotCoord{.side = BattleSide::Attacker, .lane = 0, .depth = BattleDepth::Back}
             .depth == BattleDepth::Front));
    EXPECT_FALSE(
        (BattleSlotCoord{.side = BattleSide::Attacker, .lane = 0, .depth = BattleDepth::Center}
             .depth == BattleDepth::Front));
}

TEST(SlotHelpers, IsBackSlot) {
    EXPECT_TRUE(
        (BattleSlotCoord{.side = BattleSide::Defender, .lane = 2, .depth = BattleDepth::Back}
             .depth == BattleDepth::Back));
    EXPECT_FALSE(
        (BattleSlotCoord{.side = BattleSide::Defender, .lane = 2, .depth = BattleDepth::Front}
             .depth == BattleDepth::Back));
    EXPECT_FALSE(
        (BattleSlotCoord{.side = BattleSide::Defender, .lane = 2, .depth = BattleDepth::Center}
             .depth == BattleDepth::Back));
}

TEST(SlotHelpers, IsCenterSlot) {
    EXPECT_TRUE(
        is_center_slot({.side = BattleSide::Defender, .lane = 1, .depth = BattleDepth::Center}));
    EXPECT_FALSE(
        is_center_slot({.side = BattleSide::Defender, .lane = 1, .depth = BattleDepth::Front}));
    EXPECT_FALSE(
        is_center_slot({.side = BattleSide::Defender, .lane = 1, .depth = BattleDepth::Back}));
}

TEST(SlotHelpers, SameLaneFrontBack) {
    const BattleSlotCoord center{
        .side = BattleSide::Defender, .lane = 1, .depth = BattleDepth::Center};
    const auto f = same_lane_front(center);
    const auto b = same_lane_back(center);
    EXPECT_EQ(f.side, BattleSide::Defender);
    EXPECT_EQ(f.lane, 1);
    EXPECT_EQ(f.depth, BattleDepth::Front);
    EXPECT_EQ(b.depth, BattleDepth::Back);
}

TEST(SlotHelpers, LargeFootprintSlots) {
    const BattleSlotCoord center{
        .side = BattleSide::Defender, .lane = 1, .depth = BattleDepth::Center};
    const auto fp = large_footprint_slots(center);
    EXPECT_EQ(fp[0].depth, BattleDepth::Front);
    EXPECT_EQ(fp[1].depth, BattleDepth::Back);
    EXPECT_EQ(fp[0].side, BattleSide::Defender);
    EXPECT_EQ(fp[0].lane, 1);
}

// ── validate_unit_slot ────────────────────────────────────────────────────

TEST(ValidateUnitSlot, LargeInCenterOk) {
    const BattleSlotCoord center{
        .side = BattleSide::Defender, .lane = 1, .depth = BattleDepth::Center};
    EXPECT_NO_THROW(validate_unit_slot(center, true));
}

TEST(ValidateUnitSlot, SmallInFrontOk) {
    const BattleSlotCoord front{
        .side = BattleSide::Attacker, .lane = 0, .depth = BattleDepth::Front};
    EXPECT_NO_THROW(validate_unit_slot(front, false));
}

TEST(ValidateUnitSlot, SmallInBackOk) {
    const BattleSlotCoord back{.side = BattleSide::Attacker, .lane = 2, .depth = BattleDepth::Back};
    EXPECT_NO_THROW(validate_unit_slot(back, false));
}

TEST(ValidateUnitSlot, LargeInFrontFails) {
    const BattleSlotCoord front{
        .side = BattleSide::Defender, .lane = 1, .depth = BattleDepth::Front};
    EXPECT_THROW(validate_unit_slot(front, true), std::invalid_argument);
}

TEST(ValidateUnitSlot, LargeInBackFails) {
    const BattleSlotCoord back{.side = BattleSide::Defender, .lane = 1, .depth = BattleDepth::Back};
    EXPECT_THROW(validate_unit_slot(back, true), std::invalid_argument);
}

TEST(ValidateUnitSlot, SmallInCenterFails) {
    const BattleSlotCoord center{
        .side = BattleSide::Attacker, .lane = 0, .depth = BattleDepth::Center};
    EXPECT_THROW(validate_unit_slot(center, false), std::invalid_argument);
}

TEST(ValidateUnitSlot, ErrorMessageContainsSlotAndType) {
    const BattleSlotCoord front{
        .side = BattleSide::Defender, .lane = 1, .depth = BattleDepth::Front};
    try {
        validate_unit_slot(front, true, "UU5021", "blue_dragon");
        FAIL() << "expected exception";
    } catch (const std::invalid_argument& e) {
        const std::string msg{e.what()};
        EXPECT_NE(msg.find("D_FRONT_1"), std::string::npos);
        EXPECT_NE(msg.find("UU5021"), std::string::npos);
        EXPECT_NE(msg.find("CENTER"), std::string::npos);
    }
}

} // namespace d2engine
