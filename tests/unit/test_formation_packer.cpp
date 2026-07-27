#include <gtest/gtest.h>

#include "opendis2_battle_scenario_gen/formation_packer.hpp"
#include "d2engine/battle_view/battle_slot.hpp"

#include <algorithm>
#include <random>
#include <stdexcept>

using namespace d2engine;
using namespace d2gen;

namespace {

std::mt19937 seeded_rng(unsigned seed = 42) {
    return std::mt19937{seed};
}

static d2engine::UnitDef make_unit(const std::string& id, bool small) {
    d2engine::UnitDef u;
    u.unit_id = id;
    u.size_small = small;
    return u;
}

// Returns false if any two assignments share a physical cell.
bool no_overlap(const std::vector<FormationSlotAssignment>& assigns) {
    std::vector<BattleSlotCoord> occupied;
    for (const auto& a : assigns) {
        if (a.is_large) {
            for (const auto& c : large_footprint_slots(a.coord)) {
                if (std::find(occupied.begin(), occupied.end(), c) != occupied.end())
                    return false;
                occupied.push_back(c);
            }
        } else {
            if (std::find(occupied.begin(), occupied.end(), a.coord) != occupied.end())
                return false;
            occupied.push_back(a.coord);
        }
    }
    return true;
}

// ── Packer tests ──────────────────────────────────────────────────────────────

TEST(FormationPacker, SmallOnlyNoCenterSlots) {
    auto u1 = make_unit("U001", true);
    auto u2 = make_unit("U002", true);
    auto rng = seeded_rng();
    auto out = pack_formation(BattleSide::Attacker, {}, {&u1, &u2}, rng);
    ASSERT_EQ(out.size(), 2u);
    for (const auto& a : out) {
        EXPECT_FALSE(a.is_large);
        EXPECT_NE(a.coord.depth, BattleDepth::Center);
    }
}

TEST(FormationPacker, OneLargeAttackerGetsCenter) {
    auto big = make_unit("BIG1", false);
    auto rng = seeded_rng();
    auto out = pack_formation(BattleSide::Attacker, {&big}, {}, rng);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_TRUE(out[0].is_large);
    EXPECT_EQ(out[0].coord.side, BattleSide::Attacker);
    EXPECT_EQ(out[0].coord.depth, BattleDepth::Center);
}

TEST(FormationPacker, OneLargeDefenderGetsCenter) {
    auto big = make_unit("BIG1", false);
    auto rng = seeded_rng();
    auto out = pack_formation(BattleSide::Defender, {&big}, {}, rng);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_TRUE(out[0].is_large);
    EXPECT_EQ(out[0].coord.side, BattleSide::Defender);
    EXPECT_EQ(out[0].coord.depth, BattleDepth::Center);
}

TEST(FormationPacker, LargeFootprintBlocksSmallFromSameLane) {
    auto big = make_unit("BIG1", false);
    auto s1 = make_unit("S1", true);
    auto s2 = make_unit("S2", true);
    auto s3 = make_unit("S3", true);
    auto s4 = make_unit("S4", true);
    auto rng = seeded_rng();
    // 1 large occupies 2 cells; 4 small fill the remaining 4 cells
    auto out = pack_formation(BattleSide::Attacker, {&big}, {&s1, &s2, &s3, &s4}, rng);
    ASSERT_EQ(out.size(), 5u);
    EXPECT_TRUE(no_overlap(out));

    const int large_lane = out[0].coord.lane;
    for (std::size_t i = 1; i < out.size(); ++i) {
        EXPECT_NE(out[i].coord.lane, large_lane)
            << "small unit placed in large unit's occupied lane";
    }
}

TEST(FormationPacker, SmallNeverInCenter) {
    auto s1 = make_unit("S1", true);
    auto s2 = make_unit("S2", true);
    auto s3 = make_unit("S3", true);
    auto rng = seeded_rng();
    auto out = pack_formation(BattleSide::Defender, {}, {&s1, &s2, &s3}, rng);
    for (const auto& a : out) {
        EXPECT_NE(a.coord.depth, BattleDepth::Center) << "small unit ended up in CENTER slot";
        EXPECT_EQ(a.coord.side, BattleSide::Defender);
    }
}

TEST(FormationPacker, LargeNeverInFrontBack) {
    auto b1 = make_unit("B1", false);
    auto b2 = make_unit("B2", false);
    auto rng = seeded_rng();
    auto out = pack_formation(BattleSide::Attacker, {&b1, &b2}, {}, rng);
    for (const auto& a : out) {
        EXPECT_EQ(a.coord.depth, BattleDepth::Center) << "large unit not in CENTER slot";
    }
}

TEST(FormationPacker, NoOverlapInMixedFormation) {
    auto b1 = make_unit("B1", false);
    auto s1 = make_unit("S1", true);
    auto s2 = make_unit("S2", true);
    auto s3 = make_unit("S3", true);
    auto s4 = make_unit("S4", true);
    auto rng = seeded_rng();
    auto out = pack_formation(BattleSide::Attacker, {&b1}, {&s1, &s2, &s3, &s4}, rng);
    EXPECT_TRUE(no_overlap(out));
    EXPECT_EQ(out.size(), 5u);
}

TEST(FormationPacker, ThreeLargeAllGetCenter) {
    // a_count=3, a_large_count=3: footprint=6 cells — valid
    auto b1 = make_unit("B1", false);
    auto b2 = make_unit("B2", false);
    auto b3 = make_unit("B3", false);
    auto rng = seeded_rng();
    auto out = pack_formation(BattleSide::Attacker, {&b1, &b2, &b3}, {}, rng);
    ASSERT_EQ(out.size(), 3u);
    EXPECT_TRUE(no_overlap(out));
    for (const auto& a : out)
        EXPECT_EQ(a.coord.depth, BattleDepth::Center);
    // Each large unit must be in a different lane
    std::array<bool, 3> seen = {false, false, false};
    for (const auto& a : out) {
        EXPECT_FALSE(seen[static_cast<std::size_t>(a.coord.lane)]) << "duplicate lane";
        seen[static_cast<std::size_t>(a.coord.lane)] = true;
    }
}

TEST(FormationPacker, OneLargeAndFiveSmallFillSide) {
    // a_count=5, a_large_count=1: footprint=5+2=6 ≤ 6 — valid; packer only gets 4 small + 1 large
    auto big = make_unit("BIG", false);
    auto s1 = make_unit("S1", true);
    auto s2 = make_unit("S2", true);
    auto s3 = make_unit("S3", true);
    auto s4 = make_unit("S4", true);
    auto rng = seeded_rng();
    // Pass 4 smalls: large takes 1 lane (2 cells), smalls fill remaining 4 cells
    auto out = pack_formation(BattleSide::Attacker, {&big}, {&s1, &s2, &s3, &s4}, rng);
    ASSERT_EQ(out.size(), 5u);
    EXPECT_TRUE(no_overlap(out));
}

TEST(FormationPacker, EmptyInputProducesNoAssignments) {
    auto rng = seeded_rng();
    auto out = pack_formation(BattleSide::Attacker, {}, {}, rng);
    EXPECT_TRUE(out.empty());
}

// ── Validation error tests ────────────────────────────────────────────────────

TEST(FormationPackerValidation, ThrowsForFourLargeUnits) {
    auto b1 = make_unit("B1", false);
    auto b2 = make_unit("B2", false);
    auto b3 = make_unit("B3", false);
    auto b4 = make_unit("B4", false);
    auto rng = seeded_rng();
    EXPECT_THROW(pack_formation(BattleSide::Attacker, {&b1, &b2, &b3, &b4}, {}, rng),
                 std::invalid_argument);
}

TEST(FormationPackerValidation, ThrowsForOneLargeAndFiveSmall) {
    // footprint: 1*2 + 5 = 7 > 6
    auto big = make_unit("BIG", false);
    auto s1 = make_unit("S1", true);
    auto s2 = make_unit("S2", true);
    auto s3 = make_unit("S3", true);
    auto s4 = make_unit("S4", true);
    auto s5 = make_unit("S5", true);
    auto rng = seeded_rng();
    EXPECT_THROW(pack_formation(BattleSide::Attacker, {&big}, {&s1, &s2, &s3, &s4, &s5}, rng),
                 std::invalid_argument);
}

TEST(FormationPackerValidation, ThrowsWhenSmallDefInLargeDefs) {
    auto small = make_unit("SM", true); // size_small == true → wrong pool
    auto rng = seeded_rng();
    EXPECT_THROW(pack_formation(BattleSide::Attacker, {&small}, {}, rng), std::invalid_argument);
}

TEST(FormationPackerValidation, ThrowsWhenLargeDefInSmallDefs) {
    auto big = make_unit("BIG", false); // size_small == false → wrong pool
    auto rng = seeded_rng();
    EXPECT_THROW(pack_formation(BattleSide::Attacker, {}, {&big}, rng), std::invalid_argument);
}

TEST(FormationPackerValidation, ThrowsOnNullLargeDef) {
    auto rng = seeded_rng();
    EXPECT_THROW(pack_formation(BattleSide::Attacker, {nullptr}, {}, rng), std::invalid_argument);
}

TEST(FormationPackerValidation, ThrowsOnNullSmallDef) {
    auto rng = seeded_rng();
    EXPECT_THROW(pack_formation(BattleSide::Attacker, {}, {nullptr}, rng), std::invalid_argument);
}

} // namespace
