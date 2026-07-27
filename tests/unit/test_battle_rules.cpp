#include <gtest/gtest.h>

#include "tests/test_dbf_builder.hpp"

#include <d2battle_rules/battle_action.hpp>
#include <d2battle_rules/battle_action_validate.hpp>
#include <d2battle_rules/battle_apply.hpp>
#include <d2battle_rules/battle_bootstrap.hpp>
#include <d2battle_rules/battle_effect.hpp>
#include <d2battle_rules/battle_fingerprint.hpp>
#include <d2battle_rules/battle_initiative.hpp>
#include <d2battle_rules/battle_outcomes.hpp>
#include <d2battle_rules/battle_state.hpp>
#include <d2battle_rules/battle_valid_actions.hpp>
#include <d2battle_rules/battle_validate.hpp>
#include <d2battle_rules/battle_formation.hpp>
#include <d2battle_rules/detail/battle_formation.hpp>
#include <d2battle_rules/detail/effect_dispatch.hpp>
#include <d2battle_rules/detail/healing_primitive.hpp>
#include <d2battle_rules/detail/unit_effects.hpp>
#include <d2engine/assets/game_data_registry.hpp>
#include <d2runtime/AdventureWorldState.hpp>
#include <d2runtime/MapCellCoord.hpp>

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

#include "tests/test_process.hpp"

namespace {

using namespace d2battle;
using namespace d2runtime;
using test_dbf::DbfBuilder;

std::filesystem::path write_gd() {
    static std::atomic<unsigned> c{0};
    auto tmp = std::filesystem::temp_directory_path() /
               ("d2bt_" + std::to_string(test_support::process_id()) + "_" + std::to_string(c++));
    std::filesystem::remove_all(tmp);
    std::filesystem::create_directories(tmp);

    {
        DbfBuilder b{{"TXT_ID", 'C', 20}, {"TEXT", 'C', 50}};
        b.add_record({"L_HUMAN", "Human"});
        b.write(tmp / "Tglobal.dbf");
    }
    for (const auto& n : {"LunitB.dbf", "LunitC.dbf", "LDthAnim.dbf", "GDynUpgr.dbf", "GMabi.dbf",
                          "Gimmu.dbf", "GimmuC.dbf"}) {
        std::ofstream ofs(tmp / n, std::ios::binary);
        ofs.put(0x1a);
    }
    {
        DbfBuilder b{{"ID", 'N', 2}, {"TEXT", 'C', 20}};
        b.add_record({"1", "L_DAMAGE"});
        b.add_record({"2", "L_DRAIN"});
        b.add_record({"12", "L_DRAIN_OVERFLOW"});
        b.add_record({"13", "L_HEAL"});
        b.write(tmp / "LattC.dbf");
    }
    {
        DbfBuilder b{{"ID", 'N', 2}, {"TEXT", 'C', 20}};
        b.add_record({"1", "L_ALL"});
        b.add_record({"2", "L_ANY"});
        b.add_record({"3", "L_ADJACENT"});
        b.write(tmp / "LAttR.dbf");
    }
    {
        DbfBuilder b{{"ID", 'N', 2}, {"TEXT", 'C', 20}};
        b.add_record({"1", "L_WEAPON"});
        b.write(tmp / "LattS.dbf");
    }
    {
        DbfBuilder b{{"RACE_ID", 'C', 16},   {"NAME_TXT", 'C', 20},  {"RACE_TYPE", 'N', 3},
                     {"PLAYABLE", 'C', 1},   {"REGEN_H", 'N', 3},    {"GUARDIAN", 'C', 16},
                     {"LEADER_1", 'C', 16},  {"LEADER_2", 'C', 16},  {"LEADER_3", 'C', 16},
                     {"LEADER_4", 'C', 16},  {"SOLDIER_1", 'C', 16}, {"SOLDIER_2", 'C', 16},
                     {"SOLDIER_3", 'C', 16}, {"SOLDIER_4", 'C', 16}, {"SOLDIER_5", 'C', 16}};
        b.add_record({"G000SU0001", "L_HUMAN", "1", "T", "10", "G000UU0001", "", "", "", "", "", "",
                      "", "", ""});
        b.write(tmp / "Grace.dbf");
    }
    {
        DbfBuilder b{{"UNIT_ID", 'C', 16},       {"NAME_TXT", 'C', 20},   {"DESC_TXT", 'C', 20},
                     {"ABIL_TXT", 'C', 20},      {"RACE_ID", 'C', 16},    {"SUBRACE", 'N', 2},
                     {"UNIT_CAT", 'N', 2},       {"BRANCH", 'N', 2},      {"SIZE_SMALL", 'N', 1},
                     {"SEX_M", 'N', 1},          {"ATCK_TWICE", 'N', 1},  {"WATER_ONLY", 'N', 1},
                     {"LEVEL", 'N', 3},          {"HIT_POINT", 'N', 4},   {"ARMOR", 'N', 3},
                     {"REGEN", 'N', 3},          {"MOVE", 'N', 3},        {"SCOUT", 'N', 3},
                     {"LEADERSHIP", 'N', 3},     {"NEGOTIATE", 'N', 3},   {"XP_KILLED", 'N', 5},
                     {"XP_NEXT", 'N', 6},        {"ENROLL_C", 'C', 40},   {"ENROLL_B", 'C', 40},
                     {"REVIVE_C", 'C', 40},      {"HEAL_C", 'C', 40},     {"TRAINING_C", 'C', 40},
                     {"ATTACK_ID", 'C', 16},     {"ATTACK2_ID", 'C', 16}, {"PREV_ID", 'C', 16},
                     {"BASE_UNIT", 'C', 16},     {"UPGRADE_B", 'C', 16},  {"DYN_UPG1", 'C', 16},
                     {"DYN_UPG2", 'C', 16},      {"DYN_UPG_LV", 'N', 3},  {"DEATH_ANIM", 'N', 3},
                     {"DEATH_ANIM_LBL", 'C', 20}};
        b.add_record({"G000UU0001", "",  "",  "",           "G000SU0001", "0",      "1", "0",
                      "1",          "1", "0", "0",          "1",          "65",     "2", "0",
                      "3",          "1", "5", "1",          "50",         "100",    "",  "",
                      "",           "",  "",  "G000AT0010", "G000AT0010", "",       "",  "",
                      "",           "",  "",  "",           "1",          "L_HUMAN"}); // primary ==
                                                                                       // secondary
                                                                                       // dedup test
        b.add_record({"G000UU0002", "",  "",  "",           "G000SU0001", "0",   "1", "0",
                      "1",          "1", "0", "0",          "1",          "70",  "3", "0",
                      "2",          "2", "0", "3",          "55",         "110", "",  "",
                      "",           "",  "",  "G000AT0020", "",           "",    "",  "",
                      "",           "",  "",  "1",          "L_HUMAN"});
        b.add_record({"G000UU0005", "",   "",  "",   "G000SU0001", "0", "1", "0",      "1", "1",
                      "0",          "0",  "1", "50", "1",          "0", "2", "1",      "0", "1",
                      "25",         "50", "",  "",   "",           "",  "",  "",       "",  "",
                      "",           "",   "",  "",   "",           "",  "1", "L_HUMAN"});
        b.add_record(
            {"G000UU0006", "",   "",  "",   "G000SU0001", "0", "1", "0",          "1", "1",
             "0",          "0",  "1", "50", "1",          "0", "2", "1",          "0", "1",
             "25",         "50", "",  "",   "",           "",  "",  "G000AT0098", "",  "",
             "",           "",   "",  "",   "",           "",  "1", "L_HUMAN"}); // no attacks
        b.add_record({"G000UU0099", "",  "",  "",           "G000SU0001", "0",   "1", "0",
                      "1",          "1", "0", "0",          "1",          "70",  "2", "0",
                      "3",          "1", "5", "1",          "50",         "100", "",  "",
                      "",           "",  "",  "G000AT0099", "",           "",    "",  "",
                      "",           "",  "",  "1",          "L_HUMAN"});
        b.add_record({"G000UU0098", "",  "",  "",           "G000SU0001", "0",   "1", "0",
                      "1",          "1", "0", "0",          "1",          "65",  "2", "0",
                      "3",          "1", "5", "1",          "50",         "100", "",  "",
                      "",           "",  "",  "G000AT0099", "G000AT0099", "",    "",  "",
                      "",           "",  "",  "1",          "L_HUMAN"});
        b.write(tmp / "Gunits.dbf");
    }
    {
        DbfBuilder b{{"ATT_ID", 'C', 16},    {"NAME_TXT", 'C', 20},  {"DESC_TXT", 'C', 20},
                     {"CLASS", 'N', 2},      {"SOURCE", 'N', 2},     {"REACH", 'N', 2},
                     {"INITIATIVE", 'N', 3}, {"QTY_DAM", 'N', 4},    {"QTY_HEAL", 'N', 3},
                     {"POWER", 'N', 3},      {"INFINITE", 'N', 1},   {"CRIT_HIT", 'N', 1},
                     {"WARD1", 'C', 40},     {"WARD2", 'C', 40},     {"WARD3", 'C', 40},
                     {"WARD4", 'C', 40},     {"ALT_ATTACK", 'C', 16}};
        b.add_record({"G000AT0010", "", "", "1", "1", "2", "40", "25", "0", "50", "1", "0", "", "",
                      "", "", ""});
        b.add_record({"G000AT0020", "", "", "1", "1", "2", "35", "20", "0", "45", "1", "0", "", "",
                      "", "", ""});
        b.add_record({"G000AT0098", "", "", "1", "1", "3", "50", "15", "0", "40", "1", "0", "", "",
                      "", "", ""}); // Adjacent reach, high init
        b.add_record({"G000AT0099", "", "", "1", "1", "1", "50", "30", "0", "60", "1", "0", "", "",
                      "", "", ""}); // All reach
        b.write(tmp / "Gattacks.dbf");
    }
    return tmp;
}

struct F {
    std::filesystem::path      gp;
    d2engine::GameDataRegistry gd;
    AdventureWorldState        w;

    F(int hp1 = 65, int hp2 = 65) : gp(write_gd()), gd(gp) {
        w.units.push_back({"U1", "g000uu0001", 1, {"MOD_A"}, 5, "Unit1", 0, {}, hp1, 0});
        w.units.push_back({"U2", "g000uu0002", 1, {}, 0, "Unit2", 0, {}, hp2, 0});
        AdventureStack s1;
        s1.id = "S1";
        s1.owner = "O1";
        s1.subrace = "R1";
        s1.leader_id = "U1";
        s1.morale = 50;
        s1.group.members[0] = "U1";
        s1.group.positions[0] = 2;
        s1.group.cell_members[2] = 0;
        s1.banner = "B1";
        s1.position = {21, 23};
        AdventureStack s2;
        s2.id = "S2";
        s2.owner = "O2";
        s2.subrace = "R2";
        s2.leader_id = "U2";
        s2.morale = 40;
        s2.group.members[0] = "U2";
        s2.group.positions[0] = 1;
        s2.group.cell_members[1] = 0;
        s2.position = {24, 20};
        w.stacks.push_back(s1);
        w.stacks.push_back(s2);
    }
};

} // namespace

// ═══════════════════════════════════════════════════════════════════════════════
TEST(BattleBootstrapTest, CreatesValid) {
    F    f;
    auto s = bootstrap_battle(f.w.stacks[0], f.w.stacks[1], f.w, f.gd);
    EXPECT_EQ(s.status, BattleStatus::InProgress);
    EXPECT_EQ(s.units.size(), 2u);
}

TEST(BattleBootstrapTest, SparseSlots) {
    F f;
    f.w.units.push_back({"UA", "g000uu0001", 2, {}, 0, "A", 0, {}, 50, 0});
    f.w.units.push_back({"UB", "g000uu0001", 3, {}, 0, "B", 0, {}, 60, 0});
    f.w.stacks[0].group.members[0].reset();
    f.w.stacks[0].group.positions[0] = -1;
    f.w.stacks[0].group.cell_members[2] = -1;
    f.w.stacks[0].group.members[1] = "UA";
    f.w.stacks[0].group.positions[1] = 3;
    f.w.stacks[0].group.cell_members[3] = 1;
    f.w.stacks[0].group.members[4] = "UB";
    f.w.stacks[0].group.positions[4] = 5;
    f.w.stacks[0].group.cell_members[5] = 4;
    auto s = bootstrap_battle(f.w.stacks[0], f.w.stacks[1], f.w, f.gd);
    EXPECT_EQ(s.find_unit("UA")->member_index, 1);
    EXPECT_EQ(s.find_unit("UA")->formation_cell, 3);
    EXPECT_EQ(s.find_unit("UB")->member_index, 4);
    EXPECT_EQ(s.find_unit("UB")->formation_cell, 5);
    EXPECT_FALSE(s.party1.members[0].has_value());
}

TEST(BattleBootstrapTest, LargeUnitCells) {
    F f;
    f.w.stacks[0].group.cell_members[2] = 0;
    f.w.stacks[0].group.cell_members[3] = 0;
    auto s = bootstrap_battle(f.w.stacks[0], f.w.stacks[1], f.w, f.gd);
    EXPECT_EQ(s.party1.cell_members[2], 0);
    EXPECT_EQ(s.party1.cell_members[3], 0);
    EXPECT_EQ(s.find_unit("U1")->formation_cell, 2);
}

TEST(BattleBootstrapTest, InitialTerminal) {
    F f;
    f.w.units[1].current_hp = 0;
    auto s = bootstrap_battle(f.w.stacks[0], f.w.stacks[1], f.w, f.gd);
    EXPECT_TRUE(s.is_terminal());
    EXPECT_EQ(s.winner, BattleSide::Party1);
}

TEST(BattleBootstrapTest, LeaderAliveNormalized) {
    F    f;
    auto s = bootstrap_battle(f.w.stacks[0], f.w.stacks[1], f.w, f.gd);
    EXPECT_EQ(s.party1.leader_alive, 1);
}

TEST(BattleBootstrapTest, EquipmentPreserved) {
    F f;
    f.w.stacks[0].artifact1 = "ART1";
    auto s = bootstrap_battle(f.w.stacks[0], f.w.stacks[1], f.w, f.gd);
    EXPECT_EQ(s.party1.artifact1, "ART1");
}

// ═══════════════════════════════════════════════════════════════════════════════
TEST(ValidationTest, FinishedBothDeadNoWinner) {
    F f;
    f.w.units[0].current_hp = 0;
    f.w.units[1].current_hp = 0;
    auto s = bootstrap_battle(f.w.stacks[0], f.w.stacks[1], f.w, f.gd);
    EXPECT_TRUE(s.is_terminal());
    EXPECT_FALSE(s.winner.has_value());
    validate_battle_state(s);
}

TEST(ValidationTest, FinishedBothDeadWithWinnerFails) {
    F f;
    f.w.units[0].current_hp = 0;
    f.w.units[1].current_hp = 0;
    auto s = bootstrap_battle(f.w.stacks[0], f.w.stacks[1], f.w, f.gd);
    s.winner = BattleSide::Party1;
    EXPECT_THROW(validate_battle_state(s), std::runtime_error);
}

TEST(ValidationTest, CrossSideFails) {
    F    f;
    auto s = bootstrap_battle(f.w.stacks[0], f.w.stacks[1], f.w, f.gd);
    s.party2.members[0] = "U1";
    s.units[0].side = BattleSide::Party2;
    EXPECT_THROW(validate_battle_state(s), std::runtime_error);
}

TEST(ValidationTest, OrphanFails) {
    F    f;
    auto s = bootstrap_battle(f.w.stacks[0], f.w.stacks[1], f.w, f.gd);
    s.party1.members[0].reset();
    EXPECT_THROW(validate_battle_state(s), std::runtime_error);
}

TEST(ValidationTest, SideMismatchFails) {
    F    f;
    auto s = bootstrap_battle(f.w.stacks[0], f.w.stacks[1], f.w, f.gd);
    s.units[0].side = BattleSide::Party2;
    EXPECT_THROW(validate_battle_state(s), std::runtime_error);
}

TEST(ValidationTest, InProgressDeadPartyFails) {
    F    f;
    auto s = bootstrap_battle(f.w.stacks[0], f.w.stacks[1], f.w, f.gd);
    s.units[0].alive = false;
    s.units[0].current_hp = 0;
    s.party1.leader_alive = 0;
    EXPECT_THROW(validate_battle_state(s), std::runtime_error);
}

TEST(ValidationTest, InProgressWinnerFails) {
    F    f;
    auto s = bootstrap_battle(f.w.stacks[0], f.w.stacks[1], f.w, f.gd);
    s.winner = BattleSide::Party1;
    EXPECT_THROW(validate_battle_state(s), std::runtime_error);
}

TEST(ValidationTest, NoFormationFails) {
    F    f;
    auto s = bootstrap_battle(f.w.stacks[0], f.w.stacks[1], f.w, f.gd);
    s.units[0].formation_cell = -1;
    EXPECT_THROW(validate_battle_state(s), std::runtime_error);
}

TEST(ValidationTest, CellRefsEmptySlotFails) {
    F    f;
    auto s = bootstrap_battle(f.w.stacks[0], f.w.stacks[1], f.w, f.gd);
    s.party1.cell_members[5] = 3;
    EXPECT_THROW(validate_battle_state(s), std::runtime_error);
}

TEST(ValidationTest, FormationAnchorMustBeFirstOccupiedCell) {
    F f;
    f.w.stacks[0].group.cell_members[2] = 0;
    f.w.stacks[0].group.cell_members[4] = 0;
    f.w.stacks[0].group.positions[0] = 4;
    EXPECT_THROW((void)bootstrap_battle(f.w.stacks[0], f.w.stacks[1], f.w, f.gd),
                 std::runtime_error);
}

TEST(ValidationTest, LargeUnitCanonicalAnchorIsPreserved) {
    F f;
    f.w.stacks[0].group.cell_members[2] = 0;
    f.w.stacks[0].group.cell_members[3] = 0;
    auto s = bootstrap_battle(f.w.stacks[0], f.w.stacks[1], f.w, f.gd);
    EXPECT_EQ(s.find_unit("U1")->formation_cell, 2);
    validate_battle_state(s);
}

// ═══════════════════════════════════════════════════════════════════════════════
TEST(ActionValidationTest, UnknownActor) {
    F    f;
    auto s = bootstrap_battle(f.w.stacks[0], f.w.stacks[1], f.w, f.gd);
    EXPECT_EQ(validate_action(BattleAction{AttackAction{"NONEXISTENT", UnitTarget{"U2"}}}, s, f.gd),
              ActionValidationError::ActorUnknown);
}

TEST(ActionValidationTest, SelfTargetReturnsFriendlyTarget) {
    F    f;
    auto s = bootstrap_battle(f.w.stacks[0], f.w.stacks[1], f.w, f.gd);
    EXPECT_EQ(validate_action(BattleAction{AttackAction{s.current_actor()->id,
                                                        UnitTarget{s.current_actor()->id}}},
                              s, f.gd),
              ActionValidationError::FriendlyTarget);
}

TEST(ActionValidationTest, AdjacentReachIsExposed) {
    F f;
    f.w.units[0].type_id = "g000uu0006";
    f.w.units[1].type_id = "g000uu0006";
    auto s = bootstrap_battle(f.w.stacks[0], f.w.stacks[1], f.w, f.gd);
    EXPECT_FALSE(valid_actions(s, f.gd).empty());
}

TEST(ActionValidationTest, AdjacentReachValidation) {
    F f;
    f.w.units[0].type_id = "g000uu0006";
    auto s = bootstrap_battle(f.w.stacks[0], f.w.stacks[1], f.w, f.gd);
    EXPECT_EQ(validate_action(BattleAction{AttackAction{s.current_actor()->id, UnitTarget{"U2"}}},
                              s, f.gd),
              ActionValidationError::None);
}

// ═══════════════════════════════════════════════════════════════════════════════
TEST(ValidActionsTest, DuplicatePrimarySecondaryEmittedOnce) {
    F    f;
    auto s = bootstrap_battle(f.w.stacks[0], f.w.stacks[1], f.w, f.gd);
    auto acts = valid_actions(s, f.gd);
    ASSERT_EQ(acts.size(), 1u);
    const auto* atk = std::get_if<AttackAction>(&acts[0]);
    ASSERT_NE(atk, nullptr);
    EXPECT_EQ(atk->actor_id, s.current_actor()->id);
    BattleSide  enemy = opposite_side(s.current_actor()->side);
    const auto* target = s.find_unit(std::get<UnitTarget>(atk->target).unit_id);
    ASSERT_NE(target, nullptr);
    EXPECT_EQ(target->side, enemy);
    EXPECT_TRUE(target->alive);
    // Two components (same attack) both apply: damage should be 25*2 = 50
    auto  target_hp_before = target->current_hp;
    auto  next = apply(s, acts[0], f.gd);
    auto* target_after = next.find_unit(target->id);
    ASSERT_NE(target_after, nullptr);
    EXPECT_EQ(target_after->current_hp, target_hp_before - 50);
}

TEST(ValidActionsTest, UniqueAndDeterministic) {
    F f;
    f.w.units.push_back({"U3", "g000uu0002", 1, {}, 0, "U3", 0, {}, 65, 0});
    f.w.units.push_back({"U4", "g000uu0002", 1, {}, 0, "U4", 0, {}, 65, 0});
    f.w.stacks[1].group.members[0] = "U3";
    f.w.stacks[1].group.positions[0] = 0;
    f.w.stacks[1].group.cell_members[0] = 0;
    f.w.stacks[1].group.members[1] = "U4";
    f.w.stacks[1].group.positions[1] = 1;
    f.w.stacks[1].group.cell_members[1] = 1;
    auto s = bootstrap_battle(f.w.stacks[0], f.w.stacks[1], f.w, f.gd);
    auto a1 = valid_actions(s, f.gd);
    auto a2 = valid_actions(s, f.gd);
    ASSERT_EQ(a1.size(), a2.size());
    for (std::size_t i = 0; i < a1.size(); ++i)
        EXPECT_EQ(a1[i], a2[i]);
}

// ═══════════════════════════════════════════════════════════════════════════════
TEST(ApplyTest, ExactDamage) {
    F    f;
    auto s = bootstrap_battle(f.w.stacks[0], f.w.stacks[1], f.w, f.gd);
    auto next = apply(s, valid_actions(s, f.gd)[0], f.gd);
    EXPECT_EQ(next.find_unit("U2")->current_hp, 15);
}

TEST(ApplyTest, Immutability) {
    F    f;
    auto s = bootstrap_battle(f.w.stacks[0], f.w.stacks[1], f.w, f.gd);
    auto copy = s;
    (void)apply(s, valid_actions(s, f.gd)[0], f.gd);
    EXPECT_EQ(s, copy);
}

TEST(ApplyTest, TerminalKillNoRoundAdvance) {
    F    f(65, 1);
    auto s = bootstrap_battle(f.w.stacks[0], f.w.stacks[1], f.w, f.gd);
    auto next = apply(s, valid_actions(s, f.gd)[0], f.gd);
    EXPECT_TRUE(next.is_terminal());
    EXPECT_EQ(next.round_state.round_number, 1u);
}

TEST(ApplyTest, LeaderAliveOnDeath) {
    F    f(65, 1);
    auto s = bootstrap_battle(f.w.stacks[0], f.w.stacks[1], f.w, f.gd);
    auto next = apply(s, valid_actions(s, f.gd)[0], f.gd);
    EXPECT_EQ(next.party2.leader_alive, 0);
}

// ═══════════════════════════════════════════════════════════════════════════════
TEST(RoundRolloverTest, ViaApply) {
    F f(65, 65);
    f.w.units.push_back({"U3", "g000uu0002", 1, {}, 0, "U3", 0, {}, 100, 0});
    f.w.stacks[1].group.members[0] = "U2";
    f.w.stacks[1].group.positions[0] = 0;
    f.w.stacks[1].group.cell_members[0] = 0;
    f.w.stacks[1].group.members[1] = "U3";
    f.w.stacks[1].group.positions[1] = 1;
    f.w.stacks[1].group.cell_members[1] = 1;
    auto s = bootstrap_battle(f.w.stacks[0], f.w.stacks[1], f.w, f.gd);
    EXPECT_EQ(s.round_state.round_number, 1u);
    EXPECT_EQ(s.current_actor()->id, "U1");
    s = apply(s, valid_actions(s, f.gd)[0], f.gd);
    EXPECT_EQ(s.round_state.round_number, 1u);
    EXPECT_EQ(s.current_actor()->id, "U2");
    s = apply(s, valid_actions(s, f.gd)[0], f.gd);
    EXPECT_EQ(s.round_state.round_number, 1u);
    EXPECT_EQ(s.current_actor()->id, "U3");
    s = apply(s, valid_actions(s, f.gd)[0], f.gd);
    EXPECT_EQ(s.status, BattleStatus::InProgress);
    EXPECT_EQ(s.round_state.round_number, 2u);
}

// ═══════════════════════════════════════════════════════════════════════════════
TEST(OutcomeTest, SemanticEquality) {
    F    f;
    auto s = bootstrap_battle(f.w.stacks[0], f.w.stacks[1], f.w, f.gd);
    for (const auto& oc : valid_action_outcomes(s, f.gd))
        EXPECT_EQ(oc.outcome, apply(s, oc.action, f.gd));
}

// ═══════════════════════════════════════════════════════════════════════════════
TEST(FingerprintTest, SameState) {
    F    f;
    auto s = bootstrap_battle(f.w.stacks[0], f.w.stacks[1], f.w, f.gd);
    EXPECT_EQ(compute_fingerprint(s), compute_fingerprint(s));
}

TEST(FingerprintTest, DifferentState) {
    F    f;
    auto s = bootstrap_battle(f.w.stacks[0], f.w.stacks[1], f.w, f.gd);
    auto fp = compute_fingerprint(s);
    EXPECT_NE(fp, compute_fingerprint(apply(s, valid_actions(s, f.gd)[0], f.gd)));
}

TEST(FingerprintTest, ModifierChanges) {
    F    f;
    auto s = bootstrap_battle(f.w.stacks[0], f.w.stacks[1], f.w, f.gd);
    auto fp = compute_fingerprint(s);
    s.units[0].modifier_ids.emplace_back("NEW");
    EXPECT_NE(fp, compute_fingerprint(s));
}

TEST(FingerprintTest, KnownVector) {
    F    f;
    auto s = bootstrap_battle(f.w.stacks[0], f.w.stacks[1], f.w, f.gd);
    EXPECT_EQ(compute_fingerprint(s), "853522d7279567c5");
}

// ═══════════════════════════════════════════════════════════════════════════════
TEST(InitiativeTest, ReturnsAttackInitiative) {
    F    f;
    auto s = bootstrap_battle(f.w.stacks[0], f.w.stacks[1], f.w, f.gd);
    EXPECT_EQ(effective_initiative(s, "U1", f.gd), 40);
    EXPECT_EQ(effective_initiative(s, "U2", f.gd), 35);
}

TEST(InitiativeTest, DeterministicTieBreak) {
    F f;
    f.w.units[1].type_id = "g000uu0001";
    auto s = bootstrap_battle(f.w.stacks[0], f.w.stacks[1], f.w, f.gd);
    ASSERT_GE(s.round_state.turn_order.size(), 2u);
    EXPECT_EQ(s.round_state.turn_order[0].unit_id, "U1");
}

// ═══════════════════════════════════════════════════════════════════════════════
// Formation mapping
// ═══════════════════════════════════════════════════════════════════════════════

TEST(FormationMappingTest, InvalidFormationCellFails) {
    EXPECT_THROW((void)d2battle::formation::cell_to_position(-1), std::runtime_error);
    EXPECT_THROW((void)d2battle::formation::cell_to_position(6), std::runtime_error);
}

TEST(FormationMappingTest, ExactCellMapping) {
    using namespace d2battle::detail::formation;
    EXPECT_EQ(cell_to_position(0), (CellPosition{FormationRank::Front, FormationRow::Top}));
    EXPECT_EQ(cell_to_position(1), (CellPosition{FormationRank::Back, FormationRow::Top}));
    EXPECT_EQ(cell_to_position(2), (CellPosition{FormationRank::Front, FormationRow::Middle}));
    EXPECT_EQ(cell_to_position(3), (CellPosition{FormationRank::Back, FormationRow::Middle}));
    EXPECT_EQ(cell_to_position(4), (CellPosition{FormationRank::Front, FormationRow::Bottom}));
    EXPECT_EQ(cell_to_position(5), (CellPosition{FormationRank::Back, FormationRow::Bottom}));
}

TEST(FormationMappingTest, AdjacentRowMapping) {
    using namespace d2battle::detail::formation;
    EXPECT_TRUE(adjacent_rows_reachable(FormationRow::Top, FormationRow::Top));
    EXPECT_TRUE(adjacent_rows_reachable(FormationRow::Top, FormationRow::Middle));
    EXPECT_FALSE(adjacent_rows_reachable(FormationRow::Top, FormationRow::Bottom));

    EXPECT_TRUE(adjacent_rows_reachable(FormationRow::Middle, FormationRow::Top));
    EXPECT_TRUE(adjacent_rows_reachable(FormationRow::Middle, FormationRow::Middle));
    EXPECT_TRUE(adjacent_rows_reachable(FormationRow::Middle, FormationRow::Bottom));

    EXPECT_FALSE(adjacent_rows_reachable(FormationRow::Bottom, FormationRow::Top));
    EXPECT_TRUE(adjacent_rows_reachable(FormationRow::Bottom, FormationRow::Middle));
    EXPECT_TRUE(adjacent_rows_reachable(FormationRow::Bottom, FormationRow::Bottom));
}

// ═══════════════════════════════════════════════════════════════════════════════
// AiArena shape regression
// ═══════════════════════════════════════════════════════════════════════════════

TEST(AiArenaShapeTest, FrontMiddleVsFrontMiddleHasAdjacentAction) {
    using namespace d2battle::detail::formation;

    auto                       gp = write_gd();
    d2engine::GameDataRegistry gd(gp);
    AdventureWorldState        w;

    w.units.push_back({"P1U", "g000uu0006", 1, {}, 0, "P1Unit", 0, {}, 65, 0});
    w.units.push_back({"P2U", "g000uu0006", 1, {}, 0, "P2Unit", 0, {}, 65, 0});

    AdventureStack s1;
    s1.id = "S1";
    s1.owner = "O1";
    s1.subrace = "R1";
    s1.leader_id = "P1U";
    s1.group.members[0] = "P1U";
    s1.group.positions[0] = 2;
    s1.group.cell_members[2] = 0;
    s1.position = {21, 23};

    AdventureStack s2;
    s2.id = "S2";
    s2.owner = "O2";
    s2.subrace = "R2";
    s2.leader_id = "P2U";
    s2.group.members[0] = "P2U";
    s2.group.positions[0] = 2;
    s2.group.cell_members[2] = 0;
    s2.position = {24, 20};

    w.stacks.push_back(s1);
    w.stacks.push_back(s2);

    auto state = bootstrap_battle(s1, s2, w, gd);

    EXPECT_EQ(cell_to_position(2), (CellPosition{FormationRank::Front, FormationRow::Middle}));

    auto acts = valid_actions(state, gd);
    ASSERT_EQ(acts.size(), 1u);

    const auto* atk = std::get_if<AttackAction>(&acts[0]);
    ASSERT_NE(atk, nullptr);
    EXPECT_EQ(std::get<UnitTarget>(atk->target).unit_id, "P2U");

    EXPECT_EQ(validate_action(acts[0], state, gd), ActionValidationError::None);
}

TEST(AdjacentTest, FrontMiddleFrontMiddleValidBothSides) {
    using namespace d2battle::detail::formation;
    auto                       gp = write_gd();
    d2engine::GameDataRegistry gd(gp);

    AdventureWorldState w;
    w.units.push_back({"P1U", "g000uu0006", 1, {}, 0, "P1", 0, {}, 65, 0});
    w.units.push_back({"P2U", "g000uu0006", 1, {}, 0, "P2", 0, {}, 65, 0});
    AdventureStack s1;
    s1.id = "S1";
    s1.owner = "O1";
    s1.leader_id = "P1U";
    s1.group.members[0] = "P1U";
    s1.group.positions[0] = 2;
    s1.group.cell_members[2] = 0;
    s1.position = {21, 23};
    AdventureStack s2;
    s2.id = "S2";
    s2.owner = "O2";
    s2.leader_id = "P2U";
    s2.group.members[0] = "P2U";
    s2.group.positions[0] = 2;
    s2.group.cell_members[2] = 0;
    s2.position = {24, 20};
    w.stacks.push_back(s1);
    w.stacks.push_back(s2);

    auto s = bootstrap_battle(s1, s2, w, gd);
    ASSERT_EQ(s.current_actor()->side, BattleSide::Party1);

    auto acts1 = valid_actions(s, gd);
    ASSERT_EQ(acts1.size(), 1u);
    const auto* a1 = std::get_if<AttackAction>(&acts1[0]);
    ASSERT_NE(a1, nullptr);
    EXPECT_EQ(std::get<UnitTarget>(a1->target).unit_id, "P2U");

    auto outcomes = valid_action_outcomes(s, gd);
    ASSERT_EQ(outcomes.size(), 1u);
    s = apply(s, outcomes[0].action, gd);
    ASSERT_EQ(s.status, BattleStatus::InProgress);
    ASSERT_EQ(s.current_actor()->side, BattleSide::Party2);

    auto acts2 = valid_actions(s, gd);
    ASSERT_EQ(acts2.size(), 1u);
    const auto* a2 = std::get_if<AttackAction>(&acts2[0]);
    ASSERT_NE(a2, nullptr);
    EXPECT_EQ(std::get<UnitTarget>(a2->target).unit_id, "P1U");
}

TEST(AdjacentTest, FrontTopToFrontBottomInvalid) {
    using namespace d2battle::detail::formation;
    auto                       gp = write_gd();
    d2engine::GameDataRegistry gd(gp);
    AdventureWorldState        w;
    w.units.push_back({"AU", "g000uu0006", 1, {}, 0, "A", 0, {}, 65, 0});
    w.units.push_back({"TU", "g000uu0006", 1, {}, 0, "T", 0, {}, 65, 0});
    AdventureStack s1;
    s1.id = "S1";
    s1.owner = "O1";
    s1.leader_id = "AU";
    s1.group.members[0] = "AU";
    s1.group.positions[0] = 0;
    s1.group.cell_members[0] = 0;
    s1.position = {21, 23};
    AdventureStack s2;
    s2.id = "S2";
    s2.owner = "O2";
    s2.leader_id = "TU";
    s2.group.members[0] = "TU";
    s2.group.positions[0] = 4;
    s2.group.cell_members[4] = 0;
    s2.position = {24, 20};
    w.stacks.push_back(s1);
    w.stacks.push_back(s2);
    auto s = bootstrap_battle(s1, s2, w, gd);

    const auto before_fp = compute_fingerprint(s);
    EXPECT_EQ(
        validate_action(BattleAction{AttackAction{s.current_actor()->id, UnitTarget{"TU"}}}, s, gd),
        ActionValidationError::TargetOutOfAdjacentReach);

    auto acts = valid_actions(s, gd);
    EXPECT_TRUE(acts.empty());
    EXPECT_EQ(compute_fingerprint(s), before_fp);
}

TEST(AdjacentTest, FrontTopToFrontMiddleAndBottomFiltersUnreachableTarget) {
    auto                       gp = write_gd();
    d2engine::GameDataRegistry gd(gp);

    AdventureWorldState w;
    w.units.push_back({"P1U", "g000uu0006", 1, {}, 0, "P1", 0, {}, 65, 0});
    w.units.push_back({"M1", "g000uu0006", 1, {}, 0, "M1", 0, {}, 65, 0});
    w.units.push_back({"B1", "g000uu0006", 1, {}, 0, "B1", 0, {}, 65, 0});

    AdventureStack s1;
    s1.id = "S1";
    s1.owner = "O1";
    s1.leader_id = "P1U";
    s1.group.members[0] = "P1U";
    s1.group.positions[0] = 0;
    s1.group.cell_members[0] = 0;
    s1.position = {21, 23};

    AdventureStack s2;
    s2.id = "S2";
    s2.owner = "O2";
    s2.leader_id = "M1";
    s2.group.members[0] = "M1";
    s2.group.positions[0] = 2;
    s2.group.cell_members[2] = 0;
    s2.group.members[1] = "B1";
    s2.group.positions[1] = 4;
    s2.group.cell_members[4] = 1;
    s2.position = {24, 20};

    w.stacks.push_back(s1);
    w.stacks.push_back(s2);

    auto       s = bootstrap_battle(s1, s2, w, gd);
    const auto before_fp = compute_fingerprint(s);

    auto acts = valid_actions(s, gd);
    ASSERT_EQ(acts.size(), 1u);
    const auto* atk = std::get_if<AttackAction>(&acts[0]);
    ASSERT_NE(atk, nullptr);
    EXPECT_EQ(std::get<UnitTarget>(atk->target).unit_id, "M1");
    EXPECT_EQ(compute_fingerprint(s), before_fp);
}

// ═══════════════════════════════════════════════════════════════════════════════
// All reach: 3 enemies -> 1 action
// ═══════════════════════════════════════════════════════════════════════════════

TEST(AllReachTest, AllDamageEmitsSingleAction) {
    auto                       gp = write_gd();
    d2engine::GameDataRegistry gd(gp);
    AdventureWorldState        w;
    w.units.push_back({"AU", "g000uu0099", 1, {}, 0, "Actor", 0, {}, 70, 0});
    w.units.push_back({"E1", "g000uu0002", 1, {}, 0, "E1", 0, {}, 70, 0});
    w.units.push_back({"E2", "g000uu0002", 1, {}, 0, "E2", 0, {}, 80, 0});
    w.units.push_back({"E3", "g000uu0002", 1, {}, 0, "E3", 0, {}, 90, 0});
    AdventureStack s1;
    s1.id = "S1";
    s1.owner = "O1";
    s1.leader_id = "AU";
    s1.group.members[0] = "AU";
    s1.group.positions[0] = 0;
    s1.group.cell_members[0] = 0;
    s1.position = {21, 23};
    AdventureStack s2;
    s2.id = "S2";
    s2.owner = "O2";
    s2.leader_id = "E1";
    s2.group.members[0] = "E1";
    s2.group.positions[0] = 1;
    s2.group.cell_members[1] = 0;
    s2.group.members[1] = "E2";
    s2.group.positions[1] = 3;
    s2.group.cell_members[3] = 1;
    s2.group.members[2] = "E3";
    s2.group.positions[2] = 5;
    s2.group.cell_members[5] = 2;
    s2.position = {24, 20};
    w.stacks.push_back(s1);
    w.stacks.push_back(s2);

    auto s = bootstrap_battle(s1, s2, w, gd);
    auto acts = valid_actions(s, gd);
    ASSERT_EQ(acts.size(), 1u);
    const auto* atk = std::get_if<AttackAction>(&acts[0]);
    ASSERT_NE(atk, nullptr);
    EXPECT_EQ(atk->actor_id, "AU");
    EXPECT_TRUE(std::holds_alternative<AllEnemyUnitsTarget>(atk->target));
}

// ═══════════════════════════════════════════════════════════════════════════════
// All reach: mass damage applied to every alive enemy
// ═══════════════════════════════════════════════════════════════════════════════

TEST(AllReachTest, AllDamageAppliesToEveryAliveEnemy) {
    auto gp = write_gd();
    {
        DbfBuilder b{{"ATT_ID", 'C', 16},    {"NAME_TXT", 'C', 20},  {"DESC_TXT", 'C', 20},
                     {"CLASS", 'N', 2},      {"SOURCE", 'N', 2},     {"REACH", 'N', 2},
                     {"INITIATIVE", 'N', 3}, {"QTY_DAM", 'N', 4},    {"QTY_HEAL", 'N', 3},
                     {"POWER", 'N', 3},      {"INFINITE", 'N', 1},   {"CRIT_HIT", 'N', 1},
                     {"WARD1", 'C', 40},     {"WARD2", 'C', 40},     {"WARD3", 'C', 40},
                     {"WARD4", 'C', 40},     {"ALT_ATTACK", 'C', 16}};
        b.add_record({"G000AT0099", "", "", "1", "1", "1", "50", "40", "0", "60", "1", "0", "", "",
                      "", "", ""});
        b.add_record({"G000AT0010", "", "", "1", "1", "2", "40", "25", "0", "50", "1", "0", "", "",
                      "", "", ""});
        b.write(gp / "Gattacks.dbf");
    }

    d2engine::GameDataRegistry gd(gp);
    AdventureWorldState        w;
    w.units.push_back({"AU", "g000uu0099", 1, {}, 0, "A", 0, {}, 70, 0});
    w.units.push_back({"EA", "g000uu0002", 1, {}, 0, "A", 0, {}, 100, 0});
    w.units.push_back({"EB", "g000uu0002", 1, {}, 0, "B", 0, {}, 80, 0});
    w.units.push_back({"EC", "g000uu0002", 1, {}, 0, "C", 0, {}, 30, 0});
    AdventureStack s1;
    s1.id = "S1";
    s1.owner = "O1";
    s1.leader_id = "AU";
    s1.group.members[0] = "AU";
    s1.group.positions[0] = 0;
    s1.group.cell_members[0] = 0;
    s1.position = {21, 23};
    AdventureStack s2;
    s2.id = "S2";
    s2.owner = "O2";
    s2.leader_id = "EA";
    s2.group.members[0] = "EA";
    s2.group.positions[0] = 1;
    s2.group.cell_members[1] = 0;
    s2.group.members[1] = "EB";
    s2.group.positions[1] = 3;
    s2.group.cell_members[3] = 1;
    s2.group.members[2] = "EC";
    s2.group.positions[2] = 5;
    s2.group.cell_members[5] = 2;
    s2.position = {24, 20};
    w.stacks.push_back(s1);
    w.stacks.push_back(s2);

    auto state = bootstrap_battle(s1, s2, w, gd);
    auto acts = valid_actions(state, gd);
    ASSERT_EQ(acts.size(), 1u);
    auto next = apply(state, acts[0], gd);

    EXPECT_EQ(next.find_unit("EA")->current_hp, 60);
    EXPECT_TRUE(next.find_unit("EA")->alive);
    EXPECT_EQ(next.find_unit("EB")->current_hp, 40);
    EXPECT_TRUE(next.find_unit("EB")->alive);
    EXPECT_EQ(next.find_unit("EC")->current_hp, 0);
    EXPECT_FALSE(next.find_unit("EC")->alive);
}

// ═══════════════════════════════════════════════════════════════════════════════
// All reach: dead enemy must not be affected
// ═══════════════════════════════════════════════════════════════════════════════

TEST(AllReachTest, AllDamageSkipsAlreadyDeadEnemies) {
    auto gp = write_gd();
    {
        DbfBuilder b{{"ATT_ID", 'C', 16},    {"NAME_TXT", 'C', 20},  {"DESC_TXT", 'C', 20},
                     {"CLASS", 'N', 2},      {"SOURCE", 'N', 2},     {"REACH", 'N', 2},
                     {"INITIATIVE", 'N', 3}, {"QTY_DAM", 'N', 4},    {"QTY_HEAL", 'N', 3},
                     {"POWER", 'N', 3},      {"INFINITE", 'N', 1},   {"CRIT_HIT", 'N', 1},
                     {"WARD1", 'C', 40},     {"WARD2", 'C', 40},     {"WARD3", 'C', 40},
                     {"WARD4", 'C', 40},     {"ALT_ATTACK", 'C', 16}};
        b.add_record({"G000AT0099", "", "", "1", "1", "1", "50", "40", "0", "60", "1", "0", "", "",
                      "", "", ""});
        b.add_record({"G000AT0010", "", "", "1", "1", "2", "40", "25", "0", "50", "1", "0", "", "",
                      "", "", ""});
        b.write(gp / "Gattacks.dbf");
    }

    d2engine::GameDataRegistry gd(gp);
    AdventureWorldState        w;
    w.units.push_back({"AU", "g000uu0099", 1, {}, 0, "A", 0, {}, 70, 0});
    w.units.push_back({"EA", "g000uu0002", 1, {}, 0, "A", 0, {}, 100, 0});
    w.units.push_back({"EB", "g000uu0002", 1, {}, 0, "B", 0, {}, 0, 0}); // already dead
    w.units.push_back({"EC", "g000uu0002", 1, {}, 0, "C", 0, {}, 50, 0});
    AdventureStack s1;
    s1.id = "S1";
    s1.owner = "O1";
    s1.leader_id = "AU";
    s1.group.members[0] = "AU";
    s1.group.positions[0] = 0;
    s1.group.cell_members[0] = 0;
    s1.position = {21, 23};
    AdventureStack s2;
    s2.id = "S2";
    s2.owner = "O2";
    s2.leader_id = "EA";
    s2.group.members[0] = "EA";
    s2.group.positions[0] = 1;
    s2.group.cell_members[1] = 0;
    s2.group.members[1] = "EB";
    s2.group.positions[1] = 3;
    s2.group.cell_members[3] = 1;
    s2.group.members[2] = "EC";
    s2.group.positions[2] = 5;
    s2.group.cell_members[5] = 2;
    s2.position = {24, 20};
    w.stacks.push_back(s1);
    w.stacks.push_back(s2);

    auto state = bootstrap_battle(s1, s2, w, gd);
    auto acts = valid_actions(state, gd);
    ASSERT_EQ(acts.size(), 1u);
    auto next = apply(state, acts[0], gd);

    EXPECT_EQ(next.find_unit("EA")->current_hp, 60);
    EXPECT_TRUE(next.find_unit("EA")->alive);
    EXPECT_EQ(next.find_unit("EB")->current_hp, 0);
    EXPECT_FALSE(next.find_unit("EB")->alive);
    EXPECT_EQ(next.find_unit("EC")->current_hp, 10);
    EXPECT_TRUE(next.find_unit("EC")->alive);
}

// ═══════════════════════════════════════════════════════════════════════════════
// All reach: large unit hit exactly once
// ═══════════════════════════════════════════════════════════════════════════════

TEST(AllReachTest, AllDamageHitsLargeUnitExactlyOnce) {
    auto gp = write_gd();
    {
        DbfBuilder b{{"ATT_ID", 'C', 16},    {"NAME_TXT", 'C', 20},  {"DESC_TXT", 'C', 20},
                     {"CLASS", 'N', 2},      {"SOURCE", 'N', 2},     {"REACH", 'N', 2},
                     {"INITIATIVE", 'N', 3}, {"QTY_DAM", 'N', 4},    {"QTY_HEAL", 'N', 3},
                     {"POWER", 'N', 3},      {"INFINITE", 'N', 1},   {"CRIT_HIT", 'N', 1},
                     {"WARD1", 'C', 40},     {"WARD2", 'C', 40},     {"WARD3", 'C', 40},
                     {"WARD4", 'C', 40},     {"ALT_ATTACK", 'C', 16}};
        b.add_record({"G000AT0099", "", "", "1", "1", "1", "50", "60", "0", "80", "1", "0", "", "",
                      "", "", ""});
        b.add_record({"G000AT0010", "", "", "1", "1", "2", "40", "25", "0", "50", "1", "0", "", "",
                      "", "", ""});
        b.write(gp / "Gattacks.dbf");
    }
    {
        DbfBuilder b{{"UNIT_ID", 'C', 16},       {"NAME_TXT", 'C', 20},   {"DESC_TXT", 'C', 20},
                     {"ABIL_TXT", 'C', 20},      {"RACE_ID", 'C', 16},    {"SUBRACE", 'N', 2},
                     {"UNIT_CAT", 'N', 2},       {"BRANCH", 'N', 2},      {"SIZE_SMALL", 'N', 1},
                     {"SEX_M", 'N', 1},          {"ATCK_TWICE", 'N', 1},  {"WATER_ONLY", 'N', 1},
                     {"LEVEL", 'N', 3},          {"HIT_POINT", 'N', 4},   {"ARMOR", 'N', 3},
                     {"REGEN", 'N', 3},          {"MOVE", 'N', 3},        {"SCOUT", 'N', 3},
                     {"LEADERSHIP", 'N', 3},     {"NEGOTIATE", 'N', 3},   {"XP_KILLED", 'N', 5},
                     {"XP_NEXT", 'N', 6},        {"ENROLL_C", 'C', 40},   {"ENROLL_B", 'C', 40},
                     {"REVIVE_C", 'C', 40},      {"HEAL_C", 'C', 40},     {"TRAINING_C", 'C', 40},
                     {"ATTACK_ID", 'C', 16},     {"ATTACK2_ID", 'C', 16}, {"PREV_ID", 'C', 16},
                     {"BASE_UNIT", 'C', 16},     {"UPGRADE_B", 'C', 16},  {"DYN_UPG1", 'C', 16},
                     {"DYN_UPG2", 'C', 16},      {"DYN_UPG_LV", 'N', 3},  {"DEATH_ANIM", 'N', 3},
                     {"DEATH_ANIM_LBL", 'C', 20}};
        b.add_record({"G000UU0099", "",  "",  "",           "G000SU0001", "0",   "1", "0",
                      "1",          "1", "0", "0",          "1",          "70",  "2", "0",
                      "3",          "1", "5", "1",          "50",         "100", "",  "",
                      "",           "",  "",  "G000AT0099", "",           "",    "",  "",
                      "",           "",  "",  "1",          "L_HUMAN"});
        b.add_record(
            {"G000UU0600", "",    "",  "",    "G000SU0001", "0", "1", "0",      "0", "1",
             "0",          "0",   "1", "600", "5",          "0", "3", "1",      "0", "1",
             "50",         "100", "",  "",    "",           "",  "",  "",       "",  "",
             "",           "",    "",  "",    "",           "",  "1", "L_HUMAN"}); // large unit, no
                                                                                   // attacks
                                                                                   // (defender)
        b.write(gp / "Gunits.dbf");
    }

    d2engine::GameDataRegistry gd(gp);
    AdventureWorldState        w;
    w.units.push_back({"AU", "g000uu0099", 1, {}, 0, "A", 0, {}, 70, 0});
    w.units.push_back({"LU", "g000uu0600", 1, {}, 0, "Large", 0, {}, 600, 0});
    AdventureStack s1;
    s1.id = "S1";
    s1.owner = "O1";
    s1.leader_id = "AU";
    s1.group.members[0] = "AU";
    s1.group.positions[0] = 0;
    s1.group.cell_members[0] = 0;
    s1.position = {21, 23};
    AdventureStack s2;
    s2.id = "S2";
    s2.owner = "O2";
    s2.leader_id = "LU";
    s2.group.members[0] = "LU";
    s2.group.positions[0] = 4; // anchor at front-bottom
    // large unit occupies two cells: front-bottom (4) and back-bottom (5)
    s2.group.cell_members[4] = 0;
    s2.group.cell_members[5] = 0;
    s2.position = {24, 20};
    w.stacks.push_back(s1);
    w.stacks.push_back(s2);

    auto state = bootstrap_battle(s1, s2, w, gd);
    auto acts = valid_actions(state, gd);
    ASSERT_EQ(acts.size(), 1u);
    auto next = apply(state, acts[0], gd);
    EXPECT_EQ(next.find_unit("LU")->current_hp, 540);
    EXPECT_TRUE(next.find_unit("LU")->alive);
}

// ═══════════════════════════════════════════════════════════════════════════════
// All reach: mass kill finishes battle atomically, no turn advance
// ═══════════════════════════════════════════════════════════════════════════════

TEST(AllReachTest, AllDamageCanFinishBattleWithoutTurnAdvance) {
    auto gp = write_gd();
    {
        DbfBuilder b{{"ATT_ID", 'C', 16},    {"NAME_TXT", 'C', 20},  {"DESC_TXT", 'C', 20},
                     {"CLASS", 'N', 2},      {"SOURCE", 'N', 2},     {"REACH", 'N', 2},
                     {"INITIATIVE", 'N', 3}, {"QTY_DAM", 'N', 4},    {"QTY_HEAL", 'N', 3},
                     {"POWER", 'N', 3},      {"INFINITE", 'N', 1},   {"CRIT_HIT", 'N', 1},
                     {"WARD1", 'C', 40},     {"WARD2", 'C', 40},     {"WARD3", 'C', 40},
                     {"WARD4", 'C', 40},     {"ALT_ATTACK", 'C', 16}};
        b.add_record({"G000AT0099", "", "", "1", "1", "1", "50", "100", "0", "80", "1", "0", "", "",
                      "", "", ""});
        b.add_record({"G000AT0010", "", "", "1", "1", "2", "40", "25", "0", "50", "1", "0", "", "",
                      "", "", ""});
        b.write(gp / "Gattacks.dbf");
    }

    d2engine::GameDataRegistry gd(gp);
    AdventureWorldState        w;
    w.units.push_back({"AU", "g000uu0099", 1, {}, 0, "A", 0, {}, 70, 0});
    w.units.push_back({"EA", "g000uu0002", 1, {}, 0, "A", 0, {}, 40, 0});
    w.units.push_back({"EB", "g000uu0002", 1, {}, 0, "B", 0, {}, 20, 0});
    w.units.push_back({"EC", "g000uu0002", 1, {}, 0, "C", 0, {}, 60, 0});
    AdventureStack s1;
    s1.id = "S1";
    s1.owner = "O1";
    s1.leader_id = "AU";
    s1.group.members[0] = "AU";
    s1.group.positions[0] = 0;
    s1.group.cell_members[0] = 0;
    s1.position = {21, 23};
    AdventureStack s2;
    s2.id = "S2";
    s2.owner = "O2";
    s2.leader_id = "EA";
    s2.group.members[0] = "EA";
    s2.group.positions[0] = 1;
    s2.group.cell_members[1] = 0;
    s2.group.members[1] = "EB";
    s2.group.positions[1] = 3;
    s2.group.cell_members[3] = 1;
    s2.group.members[2] = "EC";
    s2.group.positions[2] = 5;
    s2.group.cell_members[5] = 2;
    s2.position = {24, 20};
    w.stacks.push_back(s1);
    w.stacks.push_back(s2);

    auto state = bootstrap_battle(s1, s2, w, gd);
    auto before_round = state.round_state.round_number;
    auto before_turn = state.round_state.current_turn_index;

    auto acts = valid_actions(state, gd);
    ASSERT_EQ(acts.size(), 1u);
    auto next = apply(state, acts[0], gd);

    EXPECT_TRUE(next.is_terminal());
    ASSERT_TRUE(next.winner.has_value());
    EXPECT_EQ(*next.winner, BattleSide::Party1);
    EXPECT_FALSE(next.find_unit("EA")->alive);
    EXPECT_FALSE(next.find_unit("EB")->alive);
    EXPECT_FALSE(next.find_unit("EC")->alive);

    EXPECT_EQ(next.round_state.round_number, before_round);
    EXPECT_EQ(next.round_state.current_turn_index, before_turn);
}

// ═══════════════════════════════════════════════════════════════════════════════
// All reach: partial kill advances turn exactly once, next actor is specific
// ═══════════════════════════════════════════════════════════════════════════════

TEST(AllReachTest, AllDamagePartialKillAdvancesTurnExactlyOnce) {
    auto gp = write_gd();
    {
        DbfBuilder b{{"ATT_ID", 'C', 16},    {"NAME_TXT", 'C', 20},  {"DESC_TXT", 'C', 20},
                     {"CLASS", 'N', 2},      {"SOURCE", 'N', 2},     {"REACH", 'N', 2},
                     {"INITIATIVE", 'N', 3}, {"QTY_DAM", 'N', 4},    {"QTY_HEAL", 'N', 3},
                     {"POWER", 'N', 3},      {"INFINITE", 'N', 1},   {"CRIT_HIT", 'N', 1},
                     {"WARD1", 'C', 40},     {"WARD2", 'C', 40},     {"WARD3", 'C', 40},
                     {"WARD4", 'C', 40},     {"ALT_ATTACK", 'C', 16}};
        b.add_record({"G000AT0099", "", "", "1", "1", "1", "50", "40", "0", "60", "1", "0", "", "",
                      "", "", ""});
        b.add_record({"G000AT0010", "", "", "1", "1", "2", "25", "25", "0", "50", "1", "0", "", "",
                      "", "", ""});
        b.write(gp / "Gattacks.dbf");
    }

    d2engine::GameDataRegistry gd(gp);
    AdventureWorldState        w;
    w.units.push_back({"AU", "g000uu0099", 1, {}, 0, "A", 0, {}, 70, 0});  // All, init 50
    w.units.push_back({"EA", "g000uu0002", 1, {}, 0, "A", 0, {}, 100, 0}); // Any, init 25
    w.units.push_back({"EB", "g000uu0002", 1, {}, 0, "B", 0, {}, 80, 0});  // Any, init 25
    w.units.push_back({"EC", "g000uu0002", 1, {}, 0, "C", 0, {}, 30, 0});  // Any, init 25
    AdventureStack s1;
    s1.id = "S1";
    s1.owner = "O1";
    s1.leader_id = "AU";
    s1.group.members[0] = "AU";
    s1.group.positions[0] = 0;
    s1.group.cell_members[0] = 0;
    s1.position = {21, 23};
    AdventureStack s2;
    s2.id = "S2";
    s2.owner = "O2";
    s2.leader_id = "EA";
    s2.group.members[0] = "EA";
    s2.group.positions[0] = 1;
    s2.group.cell_members[1] = 0;
    s2.group.members[1] = "EB";
    s2.group.positions[1] = 3;
    s2.group.cell_members[3] = 1;
    s2.group.members[2] = "EC";
    s2.group.positions[2] = 5;
    s2.group.cell_members[5] = 2;
    s2.position = {24, 20};
    w.stacks.push_back(s1);
    w.stacks.push_back(s2);

    // Turn order: AU(50,P1), EA(25,P2), EB(25,P2), EC(25,P2)
    auto state = bootstrap_battle(s1, s2, w, gd);
    ASSERT_EQ(state.round_state.turn_order[0].unit_id, "AU");
    ASSERT_EQ(state.round_state.turn_order[1].unit_id, "EA");
    ASSERT_EQ(state.round_state.turn_order[2].unit_id, "EB");
    ASSERT_EQ(state.round_state.turn_order[3].unit_id, "EC");

    auto acts = valid_actions(state, gd);
    ASSERT_EQ(acts.size(), 1u);
    auto next = apply(state, acts[0], gd);

    EXPECT_EQ(next.status, BattleStatus::InProgress);
    // EC(30->0 dead), EA(100->60), EB(80->40)
    EXPECT_FALSE(next.find_unit("EC")->alive);
    EXPECT_TRUE(next.find_unit("EA")->alive);
    EXPECT_TRUE(next.find_unit("EB")->alive);
    // After AU acts, next actor should be EA (turn_order index 1)
    ASSERT_NE(next.current_actor(), nullptr);
    EXPECT_EQ(next.current_actor()->id, "EA");
    EXPECT_EQ(next.round_state.current_turn_index, 1u);
}

// ═══════════════════════════════════════════════════════════════════════════════
// All reach + UnitTarget -> TargetShapeMismatch
// ═══════════════════════════════════════════════════════════════════════════════

TEST(AllReachTest, AllReachRejectsUnitTarget) {
    auto                       gp = write_gd();
    d2engine::GameDataRegistry gd(gp);
    AdventureWorldState        w;
    w.units.push_back({"AU", "g000uu0099", 1, {}, 0, "A", 0, {}, 70, 0});
    w.units.push_back({"TU", "g000uu0002", 1, {}, 0, "T", 0, {}, 70, 0});
    AdventureStack s1;
    s1.id = "S1";
    s1.owner = "O1";
    s1.leader_id = "AU";
    s1.group.members[0] = "AU";
    s1.group.positions[0] = 0;
    s1.group.cell_members[0] = 0;
    s1.position = {21, 23};
    AdventureStack s2;
    s2.id = "S2";
    s2.owner = "O2";
    s2.leader_id = "TU";
    s2.group.members[0] = "TU";
    s2.group.positions[0] = 1;
    s2.group.cell_members[1] = 0;
    s2.position = {24, 20};
    w.stacks.push_back(s1);
    w.stacks.push_back(s2);

    auto s = bootstrap_battle(s1, s2, w, gd);
    EXPECT_EQ(
        validate_action(BattleAction{AttackAction{s.current_actor()->id, UnitTarget{"TU"}}}, s, gd),
        ActionValidationError::TargetShapeMismatch);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Any reach + AllEnemyUnitsTarget -> TargetShapeMismatch
// ═══════════════════════════════════════════════════════════════════════════════

TEST(AllReachTest, AnyReachRejectsAllEnemyUnitsTarget) {
    F    f;
    auto s = bootstrap_battle(f.w.stacks[0], f.w.stacks[1], f.w, f.gd);
    EXPECT_EQ(
        validate_action(BattleAction{AttackAction{s.current_actor()->id, AllEnemyUnitsTarget{}}}, s,
                        f.gd),
        ActionValidationError::TargetShapeMismatch);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Adjacent reach + AllEnemyUnitsTarget -> TargetShapeMismatch
// ═══════════════════════════════════════════════════════════════════════════════

TEST(AllReachTest, AdjacentReachRejectsAllEnemyUnitsTarget) {
    F f;
    f.w.units[0].type_id = "g000uu0006"; // Adjacent reach
    f.w.units[1].type_id = "g000uu0006";
    auto s = bootstrap_battle(f.w.stacks[0], f.w.stacks[1], f.w, f.gd);
    EXPECT_EQ(
        validate_action(BattleAction{AttackAction{s.current_actor()->id, AllEnemyUnitsTarget{}}}, s,
                        f.gd),
        ActionValidationError::TargetShapeMismatch);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Duplicate primary/secondary All attack emitted once
// ═══════════════════════════════════════════════════════════════════════════════

TEST(AllReachTest, DuplicatePrimarySecondaryAllAttackEmittedOnce) {
    auto gp = write_gd();
    {
        // Overwrite G000AT0099 with higher damage to make double application observable
        DbfBuilder b{{"ATT_ID", 'C', 16},    {"NAME_TXT", 'C', 20},  {"DESC_TXT", 'C', 20},
                     {"CLASS", 'N', 2},      {"SOURCE", 'N', 2},     {"REACH", 'N', 2},
                     {"INITIATIVE", 'N', 3}, {"QTY_DAM", 'N', 4},    {"QTY_HEAL", 'N', 3},
                     {"POWER", 'N', 3},      {"INFINITE", 'N', 1},   {"CRIT_HIT", 'N', 1},
                     {"WARD1", 'C', 40},     {"WARD2", 'C', 40},     {"WARD3", 'C', 40},
                     {"WARD4", 'C', 40},     {"ALT_ATTACK", 'C', 16}};
        b.add_record({"G000AT0099", "", "", "1", "1", "1", "50", "30", "0", "60", "1", "0", "", "",
                      "", "", ""}); // All, 30 damage
        b.add_record({"G000AT0010", "", "", "1", "1", "2", "40", "25", "0", "50", "1", "0", "", "",
                      "", "", ""});
        b.write(gp / "Gattacks.dbf");
    }
    d2engine::GameDataRegistry gd(gp);
    AdventureWorldState        w;
    w.units.push_back(
        {"AU", "g000uu0098", 1, {}, 0, "A", 0, {}, 65, 0}); // primary == secondary == All
    w.units.push_back({"E1", "g000uu0002", 1, {}, 0, "E1", 0, {}, 70, 0});
    w.units.push_back({"E2", "g000uu0002", 1, {}, 0, "E2", 0, {}, 80, 0});
    w.units.push_back({"E3", "g000uu0002", 1, {}, 0, "E3", 0, {}, 90, 0});
    AdventureStack s1;
    s1.id = "S1";
    s1.owner = "O1";
    s1.leader_id = "AU";
    s1.group.members[0] = "AU";
    s1.group.positions[0] = 0;
    s1.group.cell_members[0] = 0;
    s1.position = {21, 23};
    AdventureStack s2;
    s2.id = "S2";
    s2.owner = "O2";
    s2.leader_id = "E1";
    s2.group.members[0] = "E1";
    s2.group.positions[0] = 1;
    s2.group.cell_members[1] = 0;
    s2.group.members[1] = "E2";
    s2.group.positions[1] = 3;
    s2.group.cell_members[3] = 1;
    s2.group.members[2] = "E3";
    s2.group.positions[2] = 5;
    s2.group.cell_members[5] = 2;
    s2.position = {24, 20};
    w.stacks.push_back(s1);
    w.stacks.push_back(s2);

    auto s = bootstrap_battle(s1, s2, w, gd);
    auto acts = valid_actions(s, gd);
    ASSERT_EQ(acts.size(), 1u);
    const auto* atk = std::get_if<AttackAction>(&acts[0]);
    ASSERT_NE(atk, nullptr);
    EXPECT_EQ(atk->actor_id, "AU");
    EXPECT_TRUE(std::holds_alternative<AllEnemyUnitsTarget>(atk->target));

    // Both components apply: each enemy should take 30*2 = 60 damage
    auto next = apply(s, acts[0], gd);
    EXPECT_EQ(next.find_unit("E1")->current_hp, 10);
    EXPECT_EQ(next.find_unit("E2")->current_hp, 20);
    EXPECT_EQ(next.find_unit("E3")->current_hp, 30);
}

// ═══════════════════════════════════════════════════════════════════════════════
// valid_action_outcomes does not mutate input state
// ═══════════════════════════════════════════════════════════════════════════════

TEST(AllReachTest, ValidActionOutcomesDoesNotMutateAuthoritativeInputState) {
    auto                       gp = write_gd();
    d2engine::GameDataRegistry gd(gp);
    AdventureWorldState        w;
    w.units.push_back({"AU", "g000uu0001", 1, {}, 0, "A", 0, {}, 65, 0}); // Any, init 40
    w.units.push_back({"EA", "g000uu0002", 1, {}, 0, "A", 0, {}, 70, 0}); // Any, init 35
    w.units.push_back({"EB", "g000uu0002", 1, {}, 0, "B", 0, {}, 80, 0});
    w.units.push_back({"EC", "g000uu0002", 1, {}, 0, "C", 0, {}, 90, 0});
    AdventureStack s1;
    s1.id = "S1";
    s1.owner = "O1";
    s1.leader_id = "AU";
    s1.group.members[0] = "AU";
    s1.group.positions[0] = 0;
    s1.group.cell_members[0] = 0;
    s1.position = {21, 23};
    AdventureStack s2;
    s2.id = "S2";
    s2.owner = "O2";
    s2.leader_id = "EA";
    s2.group.members[0] = "EA";
    s2.group.positions[0] = 1;
    s2.group.cell_members[1] = 0;
    s2.group.members[1] = "EB";
    s2.group.positions[1] = 3;
    s2.group.cell_members[3] = 1;
    s2.group.members[2] = "EC";
    s2.group.positions[2] = 5;
    s2.group.cell_members[5] = 2;
    s2.position = {24, 20};
    w.stacks.push_back(s1);
    w.stacks.push_back(s2);

    auto state = bootstrap_battle(s1, s2, w, gd);
    auto fingerprint_before = compute_fingerprint(state);

    auto outcomes = valid_action_outcomes(state, gd);

    ASSERT_EQ(outcomes.size(), 3u);
    EXPECT_EQ(compute_fingerprint(state), fingerprint_before);

    EXPECT_NE(compute_fingerprint(outcomes[0].outcome), fingerprint_before);
    EXPECT_NE(compute_fingerprint(outcomes[1].outcome), fingerprint_before);
    EXPECT_NE(compute_fingerprint(outcomes[2].outcome), fingerprint_before);

    std::set<std::string> damaged;
    for (const auto& oc : outcomes) {
        EXPECT_EQ(compute_fingerprint(state), fingerprint_before);
        const auto* atk = std::get_if<AttackAction>(&oc.action);
        ASSERT_NE(atk, nullptr);
        auto* ut = std::get_if<UnitTarget>(&atk->target);
        ASSERT_NE(ut, nullptr);
        damaged.insert(ut->unit_id);
    }
    EXPECT_EQ(damaged.size(), 3u);
}

// ═══════════════════════════════════════════════════════════════════════════════
// DrainOverflow
// ═══════════════════════════════════════════════════════════════════════════════

namespace {

[[nodiscard]] std::filesystem::path write_gd_drain_overflow() {
    auto tmp = write_gd();
    {
        DbfBuilder b{{"ATT_ID", 'C', 16},    {"NAME_TXT", 'C', 20},  {"DESC_TXT", 'C', 20},
                     {"CLASS", 'N', 2},      {"SOURCE", 'N', 2},     {"REACH", 'N', 2},
                     {"INITIATIVE", 'N', 3}, {"QTY_DAM", 'N', 4},    {"QTY_HEAL", 'N', 3},
                     {"POWER", 'N', 3},      {"INFINITE", 'N', 1},   {"CRIT_HIT", 'N', 1},
                     {"WARD1", 'C', 40},     {"WARD2", 'C', 40},     {"WARD3", 'C', 40},
                     {"WARD4", 'C', 40},     {"ALT_ATTACK", 'C', 16}};
        b.add_record({"G000DO0001", "", "", "12", "1", "3", "50", "65", "0", "50", "1", "0", "", "",
                      "", "", ""}); // DrainOverflow Adjacent damage=65
        b.add_record({"G000DO0002", "", "", "12", "1", "2", "50", "65", "0", "50", "1", "0", "", "",
                      "", "", ""}); // DrainOverflow Any damage=65
        b.add_record({"G000DO0003", "", "", "12", "1", "1", "50", "60", "0", "50", "1", "0", "", "",
                      "", "", ""}); // DrainOverflow All damage=60
        b.write(tmp / "Gattacks.dbf");
    }
    {
        DbfBuilder b{{"UNIT_ID", 'C', 16},       {"NAME_TXT", 'C', 20},   {"DESC_TXT", 'C', 20},
                     {"ABIL_TXT", 'C', 20},      {"RACE_ID", 'C', 16},    {"SUBRACE", 'N', 2},
                     {"UNIT_CAT", 'N', 2},       {"BRANCH", 'N', 2},      {"SIZE_SMALL", 'N', 1},
                     {"SEX_M", 'N', 1},          {"ATCK_TWICE", 'N', 1},  {"WATER_ONLY", 'N', 1},
                     {"LEVEL", 'N', 3},          {"HIT_POINT", 'N', 4},   {"ARMOR", 'N', 3},
                     {"REGEN", 'N', 3},          {"MOVE", 'N', 3},        {"SCOUT", 'N', 3},
                     {"LEADERSHIP", 'N', 3},     {"NEGOTIATE", 'N', 3},   {"XP_KILLED", 'N', 5},
                     {"XP_NEXT", 'N', 6},        {"ENROLL_C", 'C', 40},   {"ENROLL_B", 'C', 40},
                     {"REVIVE_C", 'C', 40},      {"HEAL_C", 'C', 40},     {"TRAINING_C", 'C', 40},
                     {"ATTACK_ID", 'C', 16},     {"ATTACK2_ID", 'C', 16}, {"PREV_ID", 'C', 16},
                     {"BASE_UNIT", 'C', 16},     {"UPGRADE_B", 'C', 16},  {"DYN_UPG1", 'C', 16},
                     {"DYN_UPG2", 'C', 16},      {"DYN_UPG_LV", 'N', 3},  {"DEATH_ANIM", 'N', 3},
                     {"DEATH_ANIM_LBL", 'C', 20}};
        b.add_record({"G000UUD001", "",  "",  "",           "G000SU0001", "0",   "1", "0",
                      "1",          "1", "0", "0",          "1",          "400", "2", "0",
                      "3",          "1", "5", "1",          "50",         "100", "",  "",
                      "",           "",  "",  "G000DO0001", "",           "",    "",  "",
                      "",           "",  "",  "1",          "L_HUMAN"}); // DrainOverflow Adjacent,
                                                                         // 400 max HP
        b.add_record({"G000UUD002", "",  "",  "",           "G000SU0001", "0",   "1", "0",
                      "1",          "1", "0", "0",          "1",          "400", "2", "0",
                      "3",          "1", "5", "1",          "50",         "100", "",  "",
                      "",           "",  "",  "G000DO0002", "",           "",    "",  "",
                      "",           "",  "",  "1",          "L_HUMAN"}); // DrainOverflow Any,
                                                                         // 400 max HP
        b.add_record({"G000UUD003", "",  "",  "",           "G000SU0001", "0",   "1", "0",
                      "1",          "1", "0", "0",          "1",          "400", "2", "0",
                      "3",          "1", "5", "1",          "50",         "100", "",  "",
                      "",           "",  "",  "G000DO0003", "",           "",    "",  "",
                      "",           "",  "",  "1",          "L_HUMAN"}); // DrainOverflow All,
                                                                         // 400 max HP
        b.add_record({"G000UUD010", "",  "",  "",           "G000SU0001", "0",   "1", "0",
                      "1",          "1", "0", "0",          "1",          "150", "2", "0",
                      "3",          "1", "5", "1",          "50",         "100", "",  "",
                      "",           "",  "",  "G000DO0001", "",           "",    "",  "",
                      "",           "",  "",  "1",          "L_HUMAN"}); // Adjacent, 150 max HP
        b.add_record({"G000UUD020", "",    "",  "",   "G000SU0001", "0", "1",      "0", "1", "1",
                      "0",          "0",   "1", "70", "2",          "0", "3",      "1", "5", "1",
                      "50",         "100", "",  "",   "",           "",  "",       "",  "",  "",
                      "",           "",    "",  "",   "",           "1", "L_HUMAN"}); // no attacks
        b.add_record(
            {"G000UUD030", "",    "",  "",    "G000SU0001", "0", "1",      "0", "0", "1",
             "0",          "0",   "1", "600", "5",          "0", "3",      "1", "5", "1",
             "50",         "100", "",  "",    "",           "",  "",       "",  "",  "",
             "",           "",    "",  "",    "",           "1", "L_HUMAN"}); // large, 600 max HP,
                                                                              // no attacks
        b.write(tmp / "Gunits.dbf");
    }
    return tmp;
}

} // namespace

TEST(DrainOverflowTest, AdjacentDamagesTargetAndHealsActor) {
    auto                       gp = write_gd_drain_overflow();
    d2engine::GameDataRegistry gd(gp);
    AdventureWorldState        w;
    w.units.push_back({"AU", "g000uud001", 1, {}, 0, "Actor", 0, {}, 200, 0});
    w.units.push_back({"TU", "g000uud020", 1, {}, 0, "Target", 0, {}, 300, 0});
    AdventureStack s1;
    s1.id = "S1";
    s1.owner = "O1";
    s1.leader_id = "AU";
    s1.group.members[0] = "AU";
    s1.group.positions[0] = 2;
    s1.group.cell_members[2] = 0;
    s1.position = {21, 23};
    AdventureStack s2;
    s2.id = "S2";
    s2.owner = "O2";
    s2.leader_id = "TU";
    s2.group.members[0] = "TU";
    s2.group.positions[0] = 2;
    s2.group.cell_members[2] = 0;
    s2.position = {24, 20};
    w.stacks.push_back(s1);
    w.stacks.push_back(s2);

    auto state = bootstrap_battle(s1, s2, w, gd);
    auto acts = valid_actions(state, gd);
    ASSERT_EQ(acts.size(), 1u);
    auto next = apply(state, acts[0], gd);

    EXPECT_EQ(next.find_unit("TU")->current_hp, 235);
    EXPECT_EQ(next.find_unit("AU")->current_hp, 232);
}

TEST(DrainOverflowTest, UsesActualDamageForHealingBudget) {
    auto                       gp = write_gd_drain_overflow();
    d2engine::GameDataRegistry gd(gp);
    AdventureWorldState        w;
    w.units.push_back({"AU", "g000uud001", 1, {}, 0, "Actor", 0, {}, 100, 0});
    w.units.push_back({"TU", "g000uud020", 1, {}, 0, "Target", 0, {}, 15, 0});
    AdventureStack s1;
    s1.id = "S1";
    s1.owner = "O1";
    s1.leader_id = "AU";
    s1.group.members[0] = "AU";
    s1.group.positions[0] = 2;
    s1.group.cell_members[2] = 0;
    s1.position = {21, 23};
    AdventureStack s2;
    s2.id = "S2";
    s2.owner = "O2";
    s2.leader_id = "TU";
    s2.group.members[0] = "TU";
    s2.group.positions[0] = 2;
    s2.group.cell_members[2] = 0;
    s2.position = {24, 20};
    w.stacks.push_back(s1);
    w.stacks.push_back(s2);

    auto state = bootstrap_battle(s1, s2, w, gd);
    auto acts = valid_actions(state, gd);
    ASSERT_EQ(acts.size(), 1u);
    auto next = apply(state, acts[0], gd);

    EXPECT_EQ(next.find_unit("TU")->current_hp, 0);
    EXPECT_FALSE(next.find_unit("TU")->alive);
    EXPECT_EQ(next.find_unit("AU")->current_hp, 107);
}

TEST(DrainOverflowTest, HealsActorBeforeOverflowAllies) {
    auto                       gp = write_gd_drain_overflow();
    d2engine::GameDataRegistry gd(gp);
    AdventureWorldState        w;
    w.units.push_back({"AU", "g000uud001", 1, {}, 0, "Actor", 0, {}, 380, 0});
    w.units.push_back({"AL", "g000uud020", 1, {}, 0, "Ally", 0, {}, 20, 0});
    w.units.push_back({"TU", "g000uud020", 1, {}, 0, "Target", 0, {}, 300, 0});
    AdventureStack s1;
    s1.id = "S1";
    s1.owner = "O1";
    s1.leader_id = "AU";
    s1.group.members[0] = "AU";
    s1.group.positions[0] = 2;
    s1.group.cell_members[2] = 0;
    s1.group.members[1] = "AL";
    s1.group.positions[1] = 1;
    s1.group.cell_members[1] = 1;
    s1.position = {21, 23};
    AdventureStack s2;
    s2.id = "S2";
    s2.owner = "O2";
    s2.leader_id = "TU";
    s2.group.members[0] = "TU";
    s2.group.positions[0] = 2;
    s2.group.cell_members[2] = 0;
    s2.position = {24, 20};
    w.stacks.push_back(s1);
    w.stacks.push_back(s2);

    auto state = bootstrap_battle(s1, s2, w, gd);
    auto acts = valid_actions(state, gd);
    ASSERT_EQ(acts.size(), 1u);
    auto next = apply(state, acts[0], gd);

    EXPECT_EQ(next.find_unit("AU")->current_hp, 400);
    EXPECT_EQ(next.find_unit("AL")->current_hp, 32);
}

TEST(DrainOverflowTest, DistributesRemainderInCanonicalMemberOrder) {
    auto                       gp = write_gd_drain_overflow();
    d2engine::GameDataRegistry gd(gp);
    AdventureWorldState        w;
    w.units.push_back({"AU", "g000uud001", 1, {}, 0, "Actor", 0, {}, 400, 0});
    w.units.push_back({"AL", "g000uud020", 1, {}, 0, "A0", 0, {}, 60, 0});
    w.units.push_back({"A2", "g000uud020", 1, {}, 0, "A2", 0, {}, 55, 0});
    w.units.push_back({"A3", "g000uud020", 1, {}, 0, "A3", 0, {}, 10, 0});
    w.units.push_back({"TU", "g000uud020", 1, {}, 0, "Target", 0, {}, 300, 0});

    AdventureStack s1;
    s1.id = "S1";
    s1.owner = "O1";
    s1.leader_id = "AU";
    s1.group.members[0] = "AU";
    s1.group.positions[0] = 2;
    s1.group.cell_members[2] = 0;
    s1.group.members[1] = "AL";
    s1.group.positions[1] = 0;
    s1.group.cell_members[0] = 1;
    s1.group.members[2] = "A2";
    s1.group.positions[2] = 3;
    s1.group.cell_members[3] = 2;
    s1.group.members[3] = "A3";
    s1.group.positions[3] = 5;
    s1.group.cell_members[5] = 3;
    s1.position = {21, 23};
    AdventureStack s2;
    s2.id = "S2";
    s2.owner = "O2";
    s2.leader_id = "TU";
    s2.group.members[0] = "TU";
    s2.group.positions[0] = 2;
    s2.group.cell_members[2] = 0;
    s2.position = {24, 20};
    w.stacks.push_back(s1);
    w.stacks.push_back(s2);

    auto state = bootstrap_battle(s1, s2, w, gd);
    auto acts = valid_actions(state, gd);
    ASSERT_EQ(acts.size(), 1u);
    auto next = apply(state, acts[0], gd);

    EXPECT_EQ(next.find_unit("AL")->current_hp, 70);
    EXPECT_EQ(next.find_unit("A2")->current_hp, 70);
    EXPECT_EQ(next.find_unit("A3")->current_hp, 17);
}

TEST(DrainOverflowTest, DoesNotReviveDeadAllies) {
    auto                       gp = write_gd_drain_overflow();
    d2engine::GameDataRegistry gd(gp);
    AdventureWorldState        w;
    w.units.push_back({"AU", "g000uud001", 1, {}, 0, "Actor", 0, {}, 400, 0});
    w.units.push_back({"AL", "g000uud020", 1, {}, 0, "DeadAlly", 0, {}, 0, 0});
    w.units.push_back({"A1", "g000uud020", 1, {}, 0, "A1", 0, {}, 60, 0});
    w.units.push_back({"TU", "g000uud020", 1, {}, 0, "Target", 0, {}, 300, 0});
    AdventureStack s1;
    s1.id = "S1";
    s1.owner = "O1";
    s1.leader_id = "AU";
    s1.group.members[0] = "AU";
    s1.group.positions[0] = 2;
    s1.group.cell_members[2] = 0;
    s1.group.members[1] = "AL";
    s1.group.positions[1] = 1;
    s1.group.cell_members[1] = 1;
    s1.group.members[3] = "A1";
    s1.group.positions[3] = 3;
    s1.group.cell_members[3] = 3;
    s1.position = {21, 23};
    AdventureStack s2;
    s2.id = "S2";
    s2.owner = "O2";
    s2.leader_id = "TU";
    s2.group.members[0] = "TU";
    s2.group.positions[0] = 2;
    s2.group.cell_members[2] = 0;
    s2.position = {24, 20};
    w.stacks.push_back(s1);
    w.stacks.push_back(s2);

    auto state = bootstrap_battle(s1, s2, w, gd);
    auto acts = valid_actions(state, gd);
    ASSERT_EQ(acts.size(), 1u);
    auto next = apply(state, acts[0], gd);

    EXPECT_EQ(next.find_unit("AL")->current_hp, 0);
    EXPECT_FALSE(next.find_unit("AL")->alive);
    EXPECT_EQ(next.find_unit("A1")->current_hp, 70);
}

TEST(DrainOverflowTest, AllUsesHalfOfTotalActualDamage) {
    auto                       gp = write_gd_drain_overflow();
    d2engine::GameDataRegistry gd(gp);
    AdventureWorldState        w;
    w.units.push_back({"AU", "g000uud003", 1, {}, 0, "Actor", 0, {}, 400, 0});
    w.units.push_back({"E1", "g000uud020", 1, {}, 0, "E1", 0, {}, 100, 0});
    w.units.push_back({"E2", "g000uud020", 1, {}, 0, "E2", 0, {}, 20, 0});
    w.units.push_back({"E3", "g000uud020", 1, {}, 0, "E3", 0, {}, 80, 0});
    w.units.push_back({"A1", "g000uud020", 1, {}, 0, "A1", 0, {}, 20, 0});
    AdventureStack s1;
    s1.id = "S1";
    s1.owner = "O1";
    s1.leader_id = "AU";
    s1.group.members[0] = "AU";
    s1.group.positions[0] = 0;
    s1.group.cell_members[0] = 0;
    s1.group.members[1] = "A1";
    s1.group.positions[1] = 1;
    s1.group.cell_members[1] = 1;
    s1.position = {21, 23};
    AdventureStack s2;
    s2.id = "S2";
    s2.owner = "O2";
    s2.leader_id = "E1";
    s2.group.members[0] = "E1";
    s2.group.positions[0] = 1;
    s2.group.cell_members[1] = 0;
    s2.group.members[1] = "E2";
    s2.group.positions[1] = 3;
    s2.group.cell_members[3] = 1;
    s2.group.members[2] = "E3";
    s2.group.positions[2] = 5;
    s2.group.cell_members[5] = 2;
    s2.position = {24, 20};
    w.stacks.push_back(s1);
    w.stacks.push_back(s2);

    // damage=60 each. E1: 100->40 (actual 60), E2: 20->0 (actual 20), E3: 80->20 (actual 60)
    // total actual = 140, healing budget = 70
    // actor full (400/400), A1: 20/70 (missing 50) -> 20+50=70

    auto state = bootstrap_battle(s1, s2, w, gd);
    auto acts = valid_actions(state, gd);
    ASSERT_EQ(acts.size(), 1u);
    auto next = apply(state, acts[0], gd);

    EXPECT_EQ(next.find_unit("E1")->current_hp, 40);
    EXPECT_EQ(next.find_unit("E2")->current_hp, 0);
    EXPECT_EQ(next.find_unit("E3")->current_hp, 20);
    EXPECT_EQ(next.find_unit("A1")->current_hp, 70);
}

TEST(DrainOverflowTest, AllHitsLargeEnemyOnce) {
    auto gp = write_gd_drain_overflow();
    {
        DbfBuilder b{{"ATT_ID", 'C', 16},    {"NAME_TXT", 'C', 20},  {"DESC_TXT", 'C', 20},
                     {"CLASS", 'N', 2},      {"SOURCE", 'N', 2},     {"REACH", 'N', 2},
                     {"INITIATIVE", 'N', 3}, {"QTY_DAM", 'N', 4},    {"QTY_HEAL", 'N', 3},
                     {"POWER", 'N', 3},      {"INFINITE", 'N', 1},   {"CRIT_HIT", 'N', 1},
                     {"WARD1", 'C', 40},     {"WARD2", 'C', 40},     {"WARD3", 'C', 40},
                     {"WARD4", 'C', 40},     {"ALT_ATTACK", 'C', 16}};
        b.add_record({"G000DO0003", "", "", "12", "1", "1", "50", "60", "0", "80", "1", "0", "", "",
                      "", "", ""}); // DrainOverflow All damage=60
        b.write(gp / "Gattacks.dbf");
    }
    d2engine::GameDataRegistry gd(gp);
    AdventureWorldState        w;
    w.units.push_back({"AU", "g000uud003", 1, {}, 0, "Actor", 0, {}, 400, 0});
    w.units.push_back({"LU", "g000uud030", 1, {}, 0, "Large", 0, {}, 600, 0});
    AdventureStack s1;
    s1.id = "S1";
    s1.owner = "O1";
    s1.leader_id = "AU";
    s1.group.members[0] = "AU";
    s1.group.positions[0] = 0;
    s1.group.cell_members[0] = 0;
    s1.position = {21, 23};
    AdventureStack s2;
    s2.id = "S2";
    s2.owner = "O2";
    s2.leader_id = "LU";
    s2.group.members[0] = "LU";
    s2.group.positions[0] = 4;
    s2.group.cell_members[4] = 0;
    s2.group.cell_members[5] = 0;
    s2.position = {24, 20};
    w.stacks.push_back(s1);
    w.stacks.push_back(s2);

    auto state = bootstrap_battle(s1, s2, w, gd);
    auto acts = valid_actions(state, gd);
    ASSERT_EQ(acts.size(), 1u);
    auto next = apply(state, acts[0], gd);

    EXPECT_EQ(next.find_unit("LU")->current_hp, 540);
    EXPECT_TRUE(next.find_unit("LU")->alive);
}

TEST(DrainOverflowTest, FinishingAttackStillAppliesDrainHealing) {
    auto                       gp = write_gd_drain_overflow();
    d2engine::GameDataRegistry gd(gp);
    AdventureWorldState        w;
    w.units.push_back({"AU", "g000uud001", 1, {}, 0, "Actor", 0, {}, 200, 0});
    w.units.push_back({"TU", "g000uud020", 1, {}, 0, "Target", 0, {}, 40, 0});
    AdventureStack s1;
    s1.id = "S1";
    s1.owner = "O1";
    s1.leader_id = "AU";
    s1.group.members[0] = "AU";
    s1.group.positions[0] = 2;
    s1.group.cell_members[2] = 0;
    s1.position = {21, 23};
    AdventureStack s2;
    s2.id = "S2";
    s2.owner = "O2";
    s2.leader_id = "TU";
    s2.group.members[0] = "TU";
    s2.group.positions[0] = 2;
    s2.group.cell_members[2] = 0;
    s2.position = {24, 20};
    w.stacks.push_back(s1);
    w.stacks.push_back(s2);

    auto state = bootstrap_battle(s1, s2, w, gd);
    auto acts = valid_actions(state, gd);
    ASSERT_EQ(acts.size(), 1u);
    auto next = apply(state, acts[0], gd);

    EXPECT_FALSE(next.find_unit("TU")->alive);
    EXPECT_EQ(next.find_unit("TU")->current_hp, 0);
    EXPECT_TRUE(next.is_terminal());
    ASSERT_TRUE(next.winner.has_value());
    EXPECT_EQ(*next.winner, BattleSide::Party1);
    EXPECT_EQ(next.find_unit("AU")->current_hp, 220);
}

TEST(DrainOverflowTest, ValidActionsRespectsReach) {
    auto                       gp = write_gd_drain_overflow();
    d2engine::GameDataRegistry gd(gp);
    AdventureWorldState        w;
    w.units.push_back({"AU", "g000uud001", 1, {}, 0, "Actor", 0, {}, 200, 0});
    w.units.push_back({"T1", "g000uud020", 1, {}, 0, "T1", 0, {}, 70, 0});
    w.units.push_back({"T2", "g000uud020", 1, {}, 0, "T2", 0, {}, 70, 0});
    AdventureStack s1;
    s1.id = "S1";
    s1.owner = "O1";
    s1.leader_id = "AU";
    s1.group.members[0] = "AU";
    s1.group.positions[0] = 2;
    s1.group.cell_members[2] = 0;
    s1.position = {21, 23};
    AdventureStack s2;
    s2.id = "S2";
    s2.owner = "O2";
    s2.leader_id = "T1";
    s2.group.members[0] = "T1";
    s2.group.positions[0] = 2;
    s2.group.cell_members[2] = 0;
    s2.group.members[1] = "T2";
    s2.group.positions[1] = 3;
    s2.group.cell_members[3] = 1;
    s2.position = {24, 20};
    w.stacks.push_back(s1);
    w.stacks.push_back(s2);

    auto state = bootstrap_battle(s1, s2, w, gd);

    const auto before_fp = compute_fingerprint(state);
    auto       acts = valid_actions(state, gd);
    ASSERT_EQ(acts.size(), 1u);

    const auto* atk = std::get_if<AttackAction>(&acts[0]);
    ASSERT_NE(atk, nullptr);
    EXPECT_EQ(std::get<UnitTarget>(atk->target).unit_id, "T1");
    EXPECT_EQ(compute_fingerprint(state), before_fp);
}

TEST(DrainOverflowTest, HealsLargeAllyExactlyOnce) {
    auto gp = write_gd_drain_overflow();
    {
        DbfBuilder b{{"UNIT_ID", 'C', 16},       {"NAME_TXT", 'C', 20},   {"DESC_TXT", 'C', 20},
                     {"ABIL_TXT", 'C', 20},      {"RACE_ID", 'C', 16},    {"SUBRACE", 'N', 2},
                     {"UNIT_CAT", 'N', 2},       {"BRANCH", 'N', 2},      {"SIZE_SMALL", 'N', 1},
                     {"SEX_M", 'N', 1},          {"ATCK_TWICE", 'N', 1},  {"WATER_ONLY", 'N', 1},
                     {"LEVEL", 'N', 3},          {"HIT_POINT", 'N', 4},   {"ARMOR", 'N', 3},
                     {"REGEN", 'N', 3},          {"MOVE", 'N', 3},        {"SCOUT", 'N', 3},
                     {"LEADERSHIP", 'N', 3},     {"NEGOTIATE", 'N', 3},   {"XP_KILLED", 'N', 5},
                     {"XP_NEXT", 'N', 6},        {"ENROLL_C", 'C', 40},   {"ENROLL_B", 'C', 40},
                     {"REVIVE_C", 'C', 40},      {"HEAL_C", 'C', 40},     {"TRAINING_C", 'C', 40},
                     {"ATTACK_ID", 'C', 16},     {"ATTACK2_ID", 'C', 16}, {"PREV_ID", 'C', 16},
                     {"BASE_UNIT", 'C', 16},     {"UPGRADE_B", 'C', 16},  {"DYN_UPG1", 'C', 16},
                     {"DYN_UPG2", 'C', 16},      {"DYN_UPG_LV", 'N', 3},  {"DEATH_ANIM", 'N', 3},
                     {"DEATH_ANIM_LBL", 'C', 20}};
        b.add_record({"G000UUD001", "",  "",  "",           "G000SU0001", "0",   "1", "0",
                      "1",          "1", "0", "0",          "1",          "400", "2", "0",
                      "3",          "1", "5", "1",          "50",         "100", "",  "",
                      "",           "",  "",  "G000DO0001", "",           "",    "",  "",
                      "",           "",  "",  "1",          "L_HUMAN"}); // actor
        b.add_record({"G000UUD020", "",    "",  "",   "G000SU0001", "0", "1",      "0", "1", "1",
                      "0",          "0",   "1", "70", "2",          "0", "3",      "1", "5", "1",
                      "50",         "100", "",  "",   "",           "",  "",       "",  "",  "",
                      "",           "",    "",  "",   "",           "1", "L_HUMAN"}); // target
        b
            .add_record({"G000UUL001", "",  "",  "",  "G000SU0001", "0",   "1", "0",
                         "0",          "1", "0", "0", "1",          "600", "5", "0",
                         "3",          "1", "5", "1", "50",         "100", "",  "",
                         "",           "",  "",  "",  "",           "",    "",  "",
                         "",           "",  "",  "1", "L_HUMAN"}); // large ally, 600 max HP
        b.write(gp / "Gunits.dbf");
    }
    d2engine::GameDataRegistry gd(gp);
    AdventureWorldState        w;
    w.units.push_back({"AU", "g000uud001", 1, {}, 0, "Actor", 0, {}, 400, 0});
    w.units.push_back({"LA", "g000uul001", 1, {}, 0, "LargeAlly", 0, {}, 500, 0});
    w.units.push_back({"TU", "g000uud020", 1, {}, 0, "Target", 0, {}, 300, 0});
    AdventureStack s1;
    s1.id = "S1";
    s1.owner = "O1";
    s1.leader_id = "AU";
    s1.group.members[0] = "AU";
    s1.group.positions[0] = 0;
    s1.group.cell_members[0] = 0;
    s1.group.members[1] = "LA";
    s1.group.positions[1] = 4;
    s1.group.cell_members[4] = 1;
    s1.group.cell_members[5] = 1;
    s1.position = {21, 23};
    AdventureStack s2;
    s2.id = "S2";
    s2.owner = "O2";
    s2.leader_id = "TU";
    s2.group.members[0] = "TU";
    s2.group.positions[0] = 2;
    s2.group.cell_members[2] = 0;
    s2.position = {24, 20};
    w.stacks.push_back(s1);
    w.stacks.push_back(s2);

    auto state = bootstrap_battle(s1, s2, w, gd);
    auto acts = valid_actions(state, gd);
    ASSERT_EQ(acts.size(), 1u);
    auto next = apply(state, acts[0], gd);

    EXPECT_EQ(next.find_unit("LA")->current_hp, 532);
    EXPECT_EQ(next.find_unit("AU")->current_hp, 400);
}

TEST(DrainOverflowTest, FinishingAttackDoesNotAdvanceTurn) {
    auto                       gp = write_gd_drain_overflow();
    d2engine::GameDataRegistry gd(gp);
    AdventureWorldState        w;
    w.units.push_back({"AU", "g000uud001", 1, {}, 0, "Actor", 0, {}, 200, 0});
    w.units.push_back({"TU", "g000uud020", 1, {}, 0, "Target", 0, {}, 40, 0});
    AdventureStack s1;
    s1.id = "S1";
    s1.owner = "O1";
    s1.leader_id = "AU";
    s1.group.members[0] = "AU";
    s1.group.positions[0] = 2;
    s1.group.cell_members[2] = 0;
    s1.position = {21, 23};
    AdventureStack s2;
    s2.id = "S2";
    s2.owner = "O2";
    s2.leader_id = "TU";
    s2.group.members[0] = "TU";
    s2.group.positions[0] = 2;
    s2.group.cell_members[2] = 0;
    s2.position = {24, 20};
    w.stacks.push_back(s1);
    w.stacks.push_back(s2);

    auto state = bootstrap_battle(s1, s2, w, gd);
    auto round_before = state.round_state.round_number;
    auto turn_before = state.round_state.current_turn_index;

    auto acts = valid_actions(state, gd);
    ASSERT_EQ(acts.size(), 1u);
    auto next = apply(state, acts[0], gd);

    EXPECT_TRUE(next.is_terminal());
    EXPECT_EQ(next.round_state.round_number, round_before);
    EXPECT_EQ(next.round_state.current_turn_index, turn_before);
    EXPECT_EQ(next.find_unit("AU")->current_hp, 220);
}

TEST(DrainOverflowTest, DrainOverflowWithAnyReachWorksIdentically) {
    auto                       gp = write_gd_drain_overflow();
    d2engine::GameDataRegistry gd(gp);
    AdventureWorldState        w;
    w.units.push_back({"AU", "g000uud002", 1, {}, 0, "Actor", 0, {}, 200, 0});
    w.units.push_back({"TU", "g000uud020", 1, {}, 0, "Target", 0, {}, 300, 0});
    AdventureStack s1;
    s1.id = "S1";
    s1.owner = "O1";
    s1.leader_id = "AU";
    s1.group.members[0] = "AU";
    s1.group.positions[0] = 2;
    s1.group.cell_members[2] = 0;
    s1.position = {21, 23};
    AdventureStack s2;
    s2.id = "S2";
    s2.owner = "O2";
    s2.leader_id = "TU";
    s2.group.members[0] = "TU";
    s2.group.positions[0] = 2;
    s2.group.cell_members[2] = 0;
    s2.position = {24, 20};
    w.stacks.push_back(s1);
    w.stacks.push_back(s2);

    auto state = bootstrap_battle(s1, s2, w, gd);
    auto acts = valid_actions(state, gd);
    ASSERT_EQ(acts.size(), 1u);
    auto next = apply(state, acts[0], gd);

    EXPECT_EQ(next.find_unit("TU")->current_hp, 235);
    EXPECT_EQ(next.find_unit("AU")->current_hp, 232);
}

TEST(DrainOverflowTest, ResolveUnitMaxHpThrowsOnMissingUnitDef) {
    auto                       gp = write_gd_drain_overflow();
    d2engine::GameDataRegistry gd(gp);

    d2battle::BattleUnitState fake_unit;
    fake_unit.id = "FAKE";
    fake_unit.type_id = "nonexistent";

    EXPECT_THROW((void)d2battle::detail::resolve_unit_max_hp(fake_unit, gd), std::runtime_error);
}

TEST(DrainOverflowTest, DispatcherThrowsOnUnsupportedClass) {
    auto gp = write_gd_drain_overflow();
    {
        DbfBuilder b{{"ATT_ID", 'C', 16},    {"NAME_TXT", 'C', 20},  {"DESC_TXT", 'C', 20},
                     {"CLASS", 'N', 2},      {"SOURCE", 'N', 2},     {"REACH", 'N', 2},
                     {"INITIATIVE", 'N', 3}, {"QTY_DAM", 'N', 4},    {"QTY_HEAL", 'N', 3},
                     {"POWER", 'N', 3},      {"INFINITE", 'N', 1},   {"CRIT_HIT", 'N', 1},
                     {"WARD1", 'C', 40},     {"WARD2", 'C', 40},     {"WARD3", 'C', 40},
                     {"WARD4", 'C', 40},     {"ALT_ATTACK", 'C', 16}};
        b.add_record({"G000AT0010", "", "", "1", "1", "2", "40", "25", "0", "50", "1", "0", "", "",
                      "", "", ""});
        b.write(gp / "Gattacks.dbf");
    }
    {
        DbfBuilder b{{"ID", 'N', 2}, {"TEXT", 'C', 20}};
        b.add_record({"1", "L_DAMAGE"});
        b.add_record({"12", "L_DRAIN_OVERFLOW"});
        b.add_record({"13", "L_HEAL"});
        b.write(gp / "LattC.dbf");
    }

    d2engine::GameDataRegistry gd(gp);

    d2battle::BattleUnitState fake_unit;
    fake_unit.id = "FAKE";
    fake_unit.type_id = "g000uud001";

    d2engine::AttackDef poison_attack;
    poison_attack.attack_id = "G000POISON";
    poison_attack.attack_class = d2engine::AttackClass::Poison;
    poison_attack.damage = 10;

    d2battle::detail::ResolvedAttackContext ctx{"FAKE", std::ref(poison_attack), {"T1"}};

    d2battle::BattleState dummy;
    dummy.status = d2battle::BattleStatus::Finished;

    EXPECT_THROW(d2battle::detail::dispatch_attack_effect(dummy, ctx, gd), std::runtime_error);
}

TEST(DrainOverflowTest, EffectKindConsistency) {
    using d2battle::detail::AttackEffectKind;
    using d2battle::detail::effect_kind_for_attack_class;
    using d2battle::detail::attack_rules::is_attack_class_supported;

    d2engine::AttackDef dmg;
    dmg.attack_class = d2engine::AttackClass::Damage;
    EXPECT_TRUE(is_attack_class_supported(dmg));
    auto dmg_kind = effect_kind_for_attack_class(dmg.attack_class);
    ASSERT_TRUE(dmg_kind.has_value());
    EXPECT_EQ(*dmg_kind, AttackEffectKind::Damage);

    d2engine::AttackDef drn;
    drn.attack_class = d2engine::AttackClass::DrainOverflow;
    EXPECT_TRUE(is_attack_class_supported(drn));
    auto drn_kind = effect_kind_for_attack_class(drn.attack_class);
    ASSERT_TRUE(drn_kind.has_value());
    EXPECT_EQ(*drn_kind, AttackEffectKind::DrainOverflow);

    d2engine::AttackDef drain;
    drain.attack_class = d2engine::AttackClass::Drain;
    EXPECT_TRUE(is_attack_class_supported(drain));
    auto drain_kind = effect_kind_for_attack_class(drain.attack_class);
    ASSERT_TRUE(drain_kind.has_value());
    EXPECT_EQ(*drain_kind, AttackEffectKind::Drain);

    d2engine::AttackDef poison;
    poison.attack_class = d2engine::AttackClass::Poison;
    EXPECT_FALSE(is_attack_class_supported(poison));
    EXPECT_FALSE(effect_kind_for_attack_class(poison.attack_class).has_value());

    d2engine::AttackDef heal;
    heal.attack_class = d2engine::AttackClass::Heal;
    EXPECT_TRUE(is_attack_class_supported(heal));
    auto heal_kind = effect_kind_for_attack_class(heal.attack_class);
    ASSERT_TRUE(heal_kind.has_value());
    EXPECT_EQ(*heal_kind, AttackEffectKind::Heal);
}

TEST(DrainOverflowTest, EffectFailsIfResolvedActorIsMissing) {
    auto gp = write_gd_drain_overflow();
    {
        DbfBuilder b{{"UNIT_ID", 'C', 16},       {"NAME_TXT", 'C', 20},   {"DESC_TXT", 'C', 20},
                     {"ABIL_TXT", 'C', 20},      {"RACE_ID", 'C', 16},    {"SUBRACE", 'N', 2},
                     {"UNIT_CAT", 'N', 2},       {"BRANCH", 'N', 2},      {"SIZE_SMALL", 'N', 1},
                     {"SEX_M", 'N', 1},          {"ATCK_TWICE", 'N', 1},  {"WATER_ONLY", 'N', 1},
                     {"LEVEL", 'N', 3},          {"HIT_POINT", 'N', 4},   {"ARMOR", 'N', 3},
                     {"REGEN", 'N', 3},          {"MOVE", 'N', 3},        {"SCOUT", 'N', 3},
                     {"LEADERSHIP", 'N', 3},     {"NEGOTIATE", 'N', 3},   {"XP_KILLED", 'N', 5},
                     {"XP_NEXT", 'N', 6},        {"ENROLL_C", 'C', 40},   {"ENROLL_B", 'C', 40},
                     {"REVIVE_C", 'C', 40},      {"HEAL_C", 'C', 40},     {"TRAINING_C", 'C', 40},
                     {"ATTACK_ID", 'C', 16},     {"ATTACK2_ID", 'C', 16}, {"PREV_ID", 'C', 16},
                     {"BASE_UNIT", 'C', 16},     {"UPGRADE_B", 'C', 16},  {"DYN_UPG1", 'C', 16},
                     {"DYN_UPG2", 'C', 16},      {"DYN_UPG_LV", 'N', 3},  {"DEATH_ANIM", 'N', 3},
                     {"DEATH_ANIM_LBL", 'C', 20}};
        b.add_record({"G000UUD001", "",  "",  "",           "G000SU0001", "0",   "1", "0",
                      "1",          "1", "0", "0",          "1",          "400", "2", "0",
                      "3",          "1", "5", "1",          "50",         "100", "",  "",
                      "",           "",  "",  "G000DO0001", "",           "",    "",  "",
                      "",           "",  "",  "1",          "L_HUMAN"}); // actor
        b.add_record({"G000UUD020", "",    "",  "",   "G000SU0001", "0", "1",      "0", "1", "1",
                      "0",          "0",   "1", "70", "2",          "0", "3",      "1", "5", "1",
                      "50",         "100", "",  "",   "",           "",  "",       "",  "",  "",
                      "",           "",    "",  "",   "",           "1", "L_HUMAN"}); // target
        b.write(gp / "Gunits.dbf");
    }

    d2engine::GameDataRegistry gd(gp);
    d2battle::BattleState      state;
    state.status = d2battle::BattleStatus::Finished;

    auto actor_unit = d2battle::BattleUnitState{};
    actor_unit.id = "VALID";
    actor_unit.type_id = "g000uud020";
    actor_unit.current_hp = 70;
    actor_unit.alive = true;
    state.units.push_back(actor_unit);

    const auto& atk_def = gd.find_unit("g000uud001")->primary_attack; // DrainOverflow Adjacent
    ASSERT_NE(atk_def, nullptr);

    d2battle::detail::ResolvedAttackContext ctx{"MISSING_ACTOR", std::ref(*atk_def), {"VALID"}};

    bool threw = false;
    try {
        d2battle::detail::dispatch_attack_effect(state, ctx, gd);
    } catch (const std::runtime_error& e) {
        threw = true;
        EXPECT_NE(std::string(e.what()).find("MISSING_ACTOR"), std::string::npos);
    }
    EXPECT_TRUE(threw);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Drain (AttackClass::Drain)
// ═══════════════════════════════════════════════════════════════════════════════

namespace {

[[nodiscard]] std::filesystem::path write_gd_drain() {
    auto tmp = write_gd();
    {
        DbfBuilder b{{"ATT_ID", 'C', 16},    {"NAME_TXT", 'C', 20},  {"DESC_TXT", 'C', 20},
                     {"CLASS", 'N', 2},      {"SOURCE", 'N', 2},     {"REACH", 'N', 2},
                     {"INITIATIVE", 'N', 3}, {"QTY_DAM", 'N', 4},    {"QTY_HEAL", 'N', 3},
                     {"POWER", 'N', 3},      {"INFINITE", 'N', 1},   {"CRIT_HIT", 'N', 1},
                     {"WARD1", 'C', 40},     {"WARD2", 'C', 40},     {"WARD3", 'C', 40},
                     {"WARD4", 'C', 40},     {"ALT_ATTACK", 'C', 16}};
        b.add_record({"G000DR0001", "", "", "2", "1", "2", "50", "60", "0", "50", "1", "0", "", "",
                      "", "", ""}); // Drain Any damage=60
        b.add_record({"G000DR0002", "", "", "2", "1", "1", "50", "60", "0", "50", "1", "0", "", "",
                      "", "", ""}); // Drain All damage=60
        b.write(tmp / "Gattacks.dbf");
    }
    {
        DbfBuilder b{{"UNIT_ID", 'C', 16},       {"NAME_TXT", 'C', 20},   {"DESC_TXT", 'C', 20},
                     {"ABIL_TXT", 'C', 20},      {"RACE_ID", 'C', 16},    {"SUBRACE", 'N', 2},
                     {"UNIT_CAT", 'N', 2},       {"BRANCH", 'N', 2},      {"SIZE_SMALL", 'N', 1},
                     {"SEX_M", 'N', 1},          {"ATCK_TWICE", 'N', 1},  {"WATER_ONLY", 'N', 1},
                     {"LEVEL", 'N', 3},          {"HIT_POINT", 'N', 4},   {"ARMOR", 'N', 3},
                     {"REGEN", 'N', 3},          {"MOVE", 'N', 3},        {"SCOUT", 'N', 3},
                     {"LEADERSHIP", 'N', 3},     {"NEGOTIATE", 'N', 3},   {"XP_KILLED", 'N', 5},
                     {"XP_NEXT", 'N', 6},        {"ENROLL_C", 'C', 40},   {"ENROLL_B", 'C', 40},
                     {"REVIVE_C", 'C', 40},      {"HEAL_C", 'C', 40},     {"TRAINING_C", 'C', 40},
                     {"ATTACK_ID", 'C', 16},     {"ATTACK2_ID", 'C', 16}, {"PREV_ID", 'C', 16},
                     {"BASE_UNIT", 'C', 16},     {"UPGRADE_B", 'C', 16},  {"DYN_UPG1", 'C', 16},
                     {"DYN_UPG2", 'C', 16},      {"DYN_UPG_LV", 'N', 3},  {"DEATH_ANIM", 'N', 3},
                     {"DEATH_ANIM_LBL", 'C', 20}};
        b.add_record({"G000UUR001", "",  "",  "",           "G000SU0001", "0",   "1", "0",
                      "1",          "1", "0", "0",          "1",          "300", "2", "0",
                      "3",          "1", "5", "1",          "50",         "100", "",  "",
                      "",           "",  "",  "G000DR0001", "",           "",    "",  "",
                      "",           "",  "",  "1",          "L_HUMAN"}); // Drain Any, 300 max HP
        b.add_record({"G000UUR002", "",  "",  "",           "G000SU0001", "0",   "1", "0",
                      "1",          "1", "0", "0",          "1",          "300", "2", "0",
                      "3",          "1", "5", "1",          "50",         "100", "",  "",
                      "",           "",  "",  "G000DR0002", "",           "",    "",  "",
                      "",           "",  "",  "1",          "L_HUMAN"}); // Drain All, 300 max HP
        b.add_record({"G000UUR020", "",    "",  "",   "G000SU0001", "0", "1",      "0", "1", "1",
                      "0",          "0",   "1", "70", "2",          "0", "3",      "1", "5", "1",
                      "50",         "100", "",  "",   "",           "",  "",       "",  "",  "",
                      "",           "",    "",  "",   "",           "1", "L_HUMAN"}); // no attacks
        b.add_record(
            {"G000UUR030", "",    "",  "",    "G000SU0001", "0", "1",      "0", "0", "1",
             "0",          "0",   "1", "600", "5",          "0", "3",      "1", "5", "1",
             "50",         "100", "",  "",    "",           "",  "",       "",  "",  "",
             "",           "",    "",  "",    "",           "1", "L_HUMAN"}); // large, 600 max HP
        b.write(tmp / "Gunits.dbf");
    }
    return tmp;
}

} // namespace

TEST(DrainTest, DamagesTargetAndHealsActor) {
    auto                       gp = write_gd_drain();
    d2engine::GameDataRegistry gd(gp);
    AdventureWorldState        w;
    w.units.push_back({"AU", "g000uur001", 1, {}, 0, "Actor", 0, {}, 100, 0});
    w.units.push_back({"TU", "g000uur020", 1, {}, 0, "Target", 0, {}, 200, 0});
    AdventureStack s1;
    s1.id = "S1";
    s1.owner = "O1";
    s1.leader_id = "AU";
    s1.group.members[0] = "AU";
    s1.group.positions[0] = 2;
    s1.group.cell_members[2] = 0;
    s1.position = {21, 23};
    AdventureStack s2;
    s2.id = "S2";
    s2.owner = "O2";
    s2.leader_id = "TU";
    s2.group.members[0] = "TU";
    s2.group.positions[0] = 2;
    s2.group.cell_members[2] = 0;
    s2.position = {24, 20};
    w.stacks.push_back(s1);
    w.stacks.push_back(s2);

    auto state = bootstrap_battle(s1, s2, w, gd);
    auto acts = valid_actions(state, gd);
    ASSERT_EQ(acts.size(), 1u);
    auto next = apply(state, acts[0], gd);

    EXPECT_EQ(next.find_unit("TU")->current_hp, 140);
    EXPECT_EQ(next.find_unit("AU")->current_hp, 130);
}

TEST(DrainTest, UsesActualDamageAfterOverkill) {
    auto                       gp = write_gd_drain();
    d2engine::GameDataRegistry gd(gp);
    AdventureWorldState        w;
    w.units.push_back({"AU", "g000uur001", 1, {}, 0, "Actor", 0, {}, 100, 0});
    w.units.push_back({"TU", "g000uur020", 1, {}, 0, "Target", 0, {}, 15, 0});
    AdventureStack s1;
    s1.id = "S1";
    s1.owner = "O1";
    s1.leader_id = "AU";
    s1.group.members[0] = "AU";
    s1.group.positions[0] = 2;
    s1.group.cell_members[2] = 0;
    s1.position = {21, 23};
    AdventureStack s2;
    s2.id = "S2";
    s2.owner = "O2";
    s2.leader_id = "TU";
    s2.group.members[0] = "TU";
    s2.group.positions[0] = 2;
    s2.group.cell_members[2] = 0;
    s2.position = {24, 20};
    w.stacks.push_back(s1);
    w.stacks.push_back(s2);

    auto state = bootstrap_battle(s1, s2, w, gd);
    auto acts = valid_actions(state, gd);
    ASSERT_EQ(acts.size(), 1u);
    auto next = apply(state, acts[0], gd);

    EXPECT_EQ(next.find_unit("TU")->current_hp, 0);
    EXPECT_FALSE(next.find_unit("TU")->alive);
    EXPECT_EQ(next.find_unit("AU")->current_hp, 107);
}

TEST(DrainTest, HealingDoesNotExceedActorMaxHp) {
    auto                       gp = write_gd_drain();
    d2engine::GameDataRegistry gd(gp);
    AdventureWorldState        w;
    w.units.push_back({"AU", "g000uur001", 1, {}, 0, "Actor", 0, {}, 290, 0});
    w.units.push_back({"TU", "g000uur020", 1, {}, 0, "Target", 0, {}, 200, 0});
    AdventureStack s1;
    s1.id = "S1";
    s1.owner = "O1";
    s1.leader_id = "AU";
    s1.group.members[0] = "AU";
    s1.group.positions[0] = 2;
    s1.group.cell_members[2] = 0;
    s1.position = {21, 23};
    AdventureStack s2;
    s2.id = "S2";
    s2.owner = "O2";
    s2.leader_id = "TU";
    s2.group.members[0] = "TU";
    s2.group.positions[0] = 2;
    s2.group.cell_members[2] = 0;
    s2.position = {24, 20};
    w.stacks.push_back(s1);
    w.stacks.push_back(s2);

    auto state = bootstrap_battle(s1, s2, w, gd);
    auto acts = valid_actions(state, gd);
    ASSERT_EQ(acts.size(), 1u);
    auto next = apply(state, acts[0], gd);

    EXPECT_EQ(next.find_unit("AU")->current_hp, 300);
}

TEST(DrainTest, DiscardsUnusedHealingInsteadOfHealingAllies) {
    auto                       gp = write_gd_drain();
    d2engine::GameDataRegistry gd(gp);
    AdventureWorldState        w;
    w.units.push_back({"AU", "g000uur001", 1, {}, 0, "Actor", 0, {}, 290, 0});
    w.units.push_back({"AL", "g000uur020", 1, {}, 0, "Ally", 0, {}, 100, 0});
    w.units.push_back({"TU", "g000uur020", 1, {}, 0, "Target", 0, {}, 200, 0});
    AdventureStack s1;
    s1.id = "S1";
    s1.owner = "O1";
    s1.leader_id = "AU";
    s1.group.members[0] = "AU";
    s1.group.positions[0] = 2;
    s1.group.cell_members[2] = 0;
    s1.group.members[1] = "AL";
    s1.group.positions[1] = 1;
    s1.group.cell_members[1] = 1;
    s1.position = {21, 23};
    AdventureStack s2;
    s2.id = "S2";
    s2.owner = "O2";
    s2.leader_id = "TU";
    s2.group.members[0] = "TU";
    s2.group.positions[0] = 2;
    s2.group.cell_members[2] = 0;
    s2.position = {24, 20};
    w.stacks.push_back(s1);
    w.stacks.push_back(s2);

    auto state = bootstrap_battle(s1, s2, w, gd);
    auto acts = valid_actions(state, gd);
    ASSERT_EQ(acts.size(), 1u);
    auto next = apply(state, acts[0], gd);

    EXPECT_EQ(next.find_unit("AU")->current_hp, 300);
    EXPECT_EQ(next.find_unit("AL")->current_hp, 100);
}

TEST(DrainTest, WithFullActorDiscardsEntireHealingBudget) {
    auto                       gp = write_gd_drain();
    d2engine::GameDataRegistry gd(gp);
    AdventureWorldState        w;
    w.units.push_back({"AU", "g000uur001", 1, {}, 0, "Actor", 0, {}, 300, 0});
    w.units.push_back({"AL", "g000uur020", 1, {}, 0, "Ally", 0, {}, 100, 0});
    w.units.push_back({"TU", "g000uur020", 1, {}, 0, "Target", 0, {}, 200, 0});
    AdventureStack s1;
    s1.id = "S1";
    s1.owner = "O1";
    s1.leader_id = "AU";
    s1.group.members[0] = "AU";
    s1.group.positions[0] = 2;
    s1.group.cell_members[2] = 0;
    s1.group.members[1] = "AL";
    s1.group.positions[1] = 1;
    s1.group.cell_members[1] = 1;
    s1.position = {21, 23};
    AdventureStack s2;
    s2.id = "S2";
    s2.owner = "O2";
    s2.leader_id = "TU";
    s2.group.members[0] = "TU";
    s2.group.positions[0] = 2;
    s2.group.cell_members[2] = 0;
    s2.position = {24, 20};
    w.stacks.push_back(s1);
    w.stacks.push_back(s2);

    auto state = bootstrap_battle(s1, s2, w, gd);
    auto acts = valid_actions(state, gd);
    ASSERT_EQ(acts.size(), 1u);
    auto next = apply(state, acts[0], gd);

    EXPECT_EQ(next.find_unit("AU")->current_hp, 300);
    EXPECT_EQ(next.find_unit("AL")->current_hp, 100);
}

TEST(DrainTest, AllHealsFromHalfOfTotalActualDamage) {
    auto                       gp = write_gd_drain();
    d2engine::GameDataRegistry gd(gp);
    AdventureWorldState        w;
    w.units.push_back({"AU", "g000uur002", 1, {}, 0, "Actor", 0, {}, 100, 0});
    w.units.push_back({"E1", "g000uur020", 1, {}, 0, "E1", 0, {}, 100, 0});
    w.units.push_back({"E2", "g000uur020", 1, {}, 0, "E2", 0, {}, 20, 0});
    w.units.push_back({"E3", "g000uur020", 1, {}, 0, "E3", 0, {}, 80, 0});
    AdventureStack s1;
    s1.id = "S1";
    s1.owner = "O1";
    s1.leader_id = "AU";
    s1.group.members[0] = "AU";
    s1.group.positions[0] = 0;
    s1.group.cell_members[0] = 0;
    s1.position = {21, 23};
    AdventureStack s2;
    s2.id = "S2";
    s2.owner = "O2";
    s2.leader_id = "E1";
    s2.group.members[0] = "E1";
    s2.group.positions[0] = 1;
    s2.group.cell_members[1] = 0;
    s2.group.members[1] = "E2";
    s2.group.positions[1] = 3;
    s2.group.cell_members[3] = 1;
    s2.group.members[2] = "E3";
    s2.group.positions[2] = 5;
    s2.group.cell_members[5] = 2;
    s2.position = {24, 20};
    w.stacks.push_back(s1);
    w.stacks.push_back(s2);

    auto state = bootstrap_battle(s1, s2, w, gd);
    auto acts = valid_actions(state, gd);
    ASSERT_EQ(acts.size(), 1u);
    auto next = apply(state, acts[0], gd);

    EXPECT_EQ(next.find_unit("E1")->current_hp, 40);
    EXPECT_EQ(next.find_unit("E2")->current_hp, 0);
    EXPECT_EQ(next.find_unit("E3")->current_hp, 20);
    EXPECT_EQ(next.find_unit("AU")->current_hp, 170);
}

TEST(DrainTest, AllHitsLargeEnemyOnce) {
    auto gp = write_gd_drain();
    {
        DbfBuilder b{{"ATT_ID", 'C', 16},    {"NAME_TXT", 'C', 20},  {"DESC_TXT", 'C', 20},
                     {"CLASS", 'N', 2},      {"SOURCE", 'N', 2},     {"REACH", 'N', 2},
                     {"INITIATIVE", 'N', 3}, {"QTY_DAM", 'N', 4},    {"QTY_HEAL", 'N', 3},
                     {"POWER", 'N', 3},      {"INFINITE", 'N', 1},   {"CRIT_HIT", 'N', 1},
                     {"WARD1", 'C', 40},     {"WARD2", 'C', 40},     {"WARD3", 'C', 40},
                     {"WARD4", 'C', 40},     {"ALT_ATTACK", 'C', 16}};
        b.add_record({"G000DR0002", "", "", "2", "1", "1", "50", "60", "0", "80", "1", "0", "", "",
                      "", "", ""});
        b.write(gp / "Gattacks.dbf");
    }
    d2engine::GameDataRegistry gd(gp);
    AdventureWorldState        w;
    w.units.push_back({"AU", "g000uur002", 1, {}, 0, "Actor", 0, {}, 100, 0});
    w.units.push_back({"LU", "g000uur030", 1, {}, 0, "Large", 0, {}, 600, 0});
    AdventureStack s1;
    s1.id = "S1";
    s1.owner = "O1";
    s1.leader_id = "AU";
    s1.group.members[0] = "AU";
    s1.group.positions[0] = 0;
    s1.group.cell_members[0] = 0;
    s1.position = {21, 23};
    AdventureStack s2;
    s2.id = "S2";
    s2.owner = "O2";
    s2.leader_id = "LU";
    s2.group.members[0] = "LU";
    s2.group.positions[0] = 4;
    s2.group.cell_members[4] = 0;
    s2.group.cell_members[5] = 0;
    s2.position = {24, 20};
    w.stacks.push_back(s1);
    w.stacks.push_back(s2);

    auto state = bootstrap_battle(s1, s2, w, gd);
    auto acts = valid_actions(state, gd);
    ASSERT_EQ(acts.size(), 1u);
    auto next = apply(state, acts[0], gd);

    EXPECT_EQ(next.find_unit("LU")->current_hp, 540);
    EXPECT_TRUE(next.find_unit("LU")->alive);
    EXPECT_EQ(next.find_unit("AU")->current_hp, 130);
}

TEST(DrainTest, FinishingAttackStillHealsActor) {
    auto                       gp = write_gd_drain();
    d2engine::GameDataRegistry gd(gp);
    AdventureWorldState        w;
    w.units.push_back({"AU", "g000uur001", 1, {}, 0, "Actor", 0, {}, 100, 0});
    w.units.push_back({"TU", "g000uur020", 1, {}, 0, "Target", 0, {}, 40, 0});
    AdventureStack s1;
    s1.id = "S1";
    s1.owner = "O1";
    s1.leader_id = "AU";
    s1.group.members[0] = "AU";
    s1.group.positions[0] = 2;
    s1.group.cell_members[2] = 0;
    s1.position = {21, 23};
    AdventureStack s2;
    s2.id = "S2";
    s2.owner = "O2";
    s2.leader_id = "TU";
    s2.group.members[0] = "TU";
    s2.group.positions[0] = 2;
    s2.group.cell_members[2] = 0;
    s2.position = {24, 20};
    w.stacks.push_back(s1);
    w.stacks.push_back(s2);

    auto state = bootstrap_battle(s1, s2, w, gd);
    auto round_before = state.round_state.round_number;
    auto turn_before = state.round_state.current_turn_index;

    auto acts = valid_actions(state, gd);
    ASSERT_EQ(acts.size(), 1u);
    auto next = apply(state, acts[0], gd);

    EXPECT_FALSE(next.find_unit("TU")->alive);
    EXPECT_EQ(next.find_unit("TU")->current_hp, 0);
    EXPECT_TRUE(next.is_terminal());
    ASSERT_TRUE(next.winner.has_value());
    EXPECT_EQ(*next.winner, BattleSide::Party1);
    EXPECT_EQ(next.find_unit("AU")->current_hp, 120);
    EXPECT_EQ(next.round_state.round_number, round_before);
    EXPECT_EQ(next.round_state.current_turn_index, turn_before);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Petrify test fixture

[[nodiscard]] std::filesystem::path write_gd_petrify() {
    static std::atomic<unsigned> c{0};
    auto                         tmp =
        std::filesystem::temp_directory_path() /
        ("d2bt_petrify_" + std::to_string(test_support::process_id()) + "_" + std::to_string(c++));
    std::filesystem::remove_all(tmp);
    std::filesystem::create_directories(tmp);
    {
        DbfBuilder b{{"TXT_ID", 'C', 20}, {"TEXT", 'C', 50}};
        b.add_record({"L_HUMAN", "Human"});
        b.write(tmp / "Tglobal.dbf");
    }
    for (const auto& n : {"LunitB.dbf", "LunitC.dbf", "LDthAnim.dbf", "GDynUpgr.dbf", "GMabi.dbf",
                          "Gimmu.dbf", "GimmuC.dbf"}) {
        std::ofstream ofs(tmp / n, std::ios::binary);
        ofs.put(0x1a);
    }
    {
        DbfBuilder b{{"ID", 'N', 2}, {"TEXT", 'C', 20}};
        b.add_record({"1", "L_DAMAGE"});
        b.add_record({"7", "L_PETRIFY"});
        b.write(tmp / "LattC.dbf");
    }
    {
        DbfBuilder b{{"ID", 'N', 2}, {"TEXT", 'C', 20}};
        b.add_record({"1", "L_ALL"});
        b.add_record({"2", "L_ANY"});
        b.write(tmp / "LAttR.dbf");
    }
    {
        DbfBuilder b{{"ID", 'N', 2}, {"TEXT", 'C', 20}};
        b.add_record({"1", "L_WEAPON"});
        b.write(tmp / "LattS.dbf");
    }
    {
        DbfBuilder b{{"RACE_ID", 'C', 16},   {"NAME_TXT", 'C', 20},  {"RACE_TYPE", 'N', 3},
                     {"PLAYABLE", 'C', 1},   {"REGEN_H", 'N', 3},    {"GUARDIAN", 'C', 16},
                     {"LEADER_1", 'C', 16},  {"LEADER_2", 'C', 16},  {"LEADER_3", 'C', 16},
                     {"LEADER_4", 'C', 16},  {"SOLDIER_1", 'C', 16}, {"SOLDIER_2", 'C', 16},
                     {"SOLDIER_3", 'C', 16}, {"SOLDIER_4", 'C', 16}, {"SOLDIER_5", 'C', 16}};
        b.add_record({"G000SU0001", "L_HUMAN", "1", "T", "10", "G000PU0001", "", "", "", "", "", "",
                      "", ""});
        b.write(tmp / "Grace.dbf");
    }
    {
        DbfBuilder b{{"ATT_ID", 'C', 16},    {"NAME_TXT", 'C', 20},  {"DESC_TXT", 'C', 20},
                     {"CLASS", 'N', 2},      {"SOURCE", 'N', 2},     {"REACH", 'N', 2},
                     {"INITIATIVE", 'N', 3}, {"QTY_DAM", 'N', 4},    {"QTY_HEAL", 'N', 3},
                     {"POWER", 'N', 3},      {"INFINITE", 'N', 1},   {"CRIT_HIT", 'N', 1},
                     {"WARD1", 'C', 40},     {"WARD2", 'C', 40},     {"WARD3", 'C', 40},
                     {"WARD4", 'C', 40},     {"ALT_ATTACK", 'C', 16}};
        b.add_record({"G000PT0001", "", "", "7", "1", "1", "50", "0", "0", "60", "1", "0", "", "",
                      "", "", ""}); // Petrify, All
        b.write(tmp / "Gattacks.dbf");
    }
    {
        DbfBuilder b{{"UNIT_ID", 'C', 16},       {"NAME_TXT", 'C', 20},   {"DESC_TXT", 'C', 20},
                     {"ABIL_TXT", 'C', 20},      {"RACE_ID", 'C', 16},    {"SUBRACE", 'N', 2},
                     {"UNIT_CAT", 'N', 2},       {"BRANCH", 'N', 2},      {"SIZE_SMALL", 'N', 1},
                     {"SEX_M", 'N', 1},          {"ATCK_TWICE", 'N', 1},  {"WATER_ONLY", 'N', 1},
                     {"LEVEL", 'N', 3},          {"HIT_POINT", 'N', 4},   {"ARMOR", 'N', 3},
                     {"REGEN", 'N', 3},          {"MOVE", 'N', 3},        {"SCOUT", 'N', 3},
                     {"LEADERSHIP", 'N', 3},     {"NEGOTIATE", 'N', 3},   {"XP_KILLED", 'N', 5},
                     {"XP_NEXT", 'N', 6},        {"ENROLL_C", 'C', 40},   {"ENROLL_B", 'C', 40},
                     {"REVIVE_C", 'C', 40},      {"HEAL_C", 'C', 40},     {"TRAINING_C", 'C', 40},
                     {"ATTACK_ID", 'C', 16},     {"ATTACK2_ID", 'C', 16}, {"PREV_ID", 'C', 16},
                     {"BASE_UNIT", 'C', 16},     {"UPGRADE_B", 'C', 16},  {"DYN_UPG1", 'C', 16},
                     {"DYN_UPG2", 'C', 16},      {"DYN_UPG_LV", 'N', 3},  {"DEATH_ANIM", 'N', 3},
                     {"DEATH_ANIM_LBL", 'C', 20}};
        b.add_record({"G000PU0001", "",  "",  "",           "G000SU0001", "0",   "1", "0",
                      "1",          "1", "0", "0",          "1",          "300", "2", "0",
                      "3",          "1", "5", "1",          "50",         "100", "",  "",
                      "",           "",  "",  "G000PT0001", "",           "",    "",  "",
                      "",           "",  "",  "1",          "L_HUMAN"}); // actor, Petrify
        b.add_record({"G000PU0002", "",   "",  "",   "G000SU0001", "0", "1",      "0", "1", "1",
                      "0",          "0",  "1", "65", "2",          "0", "2",      "1", "0", "1",
                      "25",         "50", "",  "",   "",           "",  "",       "",  "",  "",
                      "",           "",   "",  "",   "",           "1", "L_HUMAN"}); // target
        b.write(tmp / "Gunits.dbf");
    }
    return tmp;
}

struct PetrifyF {
    std::filesystem::path      gp;
    d2engine::GameDataRegistry gd;
    AdventureWorldState        w;
    BattleState                state;

    PetrifyF() : gp(write_gd_petrify()), gd(gp) {
        w.units.push_back({"PU", "g000pu0001", 1, {}, 0, "Petrifier", 0, {}, 300, 0});
        w.units.push_back({"TU", "g000pu0002", 1, {}, 0, "Target", 0, {}, 65, 0});
        AdventureStack s1;
        s1.id = "S1";
        s1.owner = "O1";
        s1.subrace = "R1";
        s1.leader_id = "PU";
        s1.morale = 50;
        s1.group.members[0] = "PU";
        s1.group.positions[0] = 2;
        s1.group.cell_members[2] = 0;
        s1.banner = "B1";
        s1.position = {21, 23};
        AdventureStack s2;
        s2.id = "S2";
        s2.owner = "O2";
        s2.subrace = "R2";
        s2.leader_id = "TU";
        s2.morale = 40;
        s2.group.members[0] = "TU";
        s2.group.positions[0] = 1;
        s2.group.cell_members[1] = 0;
        s2.position = {24, 20};
        w.stacks.push_back(s1);
        w.stacks.push_back(s2);
        state = bootstrap_battle(s1, s2, w, gd);
    }
};

TEST(PetrifyTest, ApplyPetrifyEffect) {
    PetrifyF f;
    auto     acts = valid_actions(f.state, f.gd);
    ASSERT_EQ(acts.size(), 1u);
    auto* atk = std::get_if<AttackAction>(&acts[0]);
    ASSERT_NE(atk, nullptr);

    auto next = apply(f.state, acts[0], f.gd);

    const auto* target = next.find_unit("TU");
    ASSERT_NE(target, nullptr);
    ASSERT_EQ(target->effects.size(), 1u);
    ASSERT_TRUE(std::holds_alternative<PetrifiedEffect>(target->effects[0]));
    const auto& pet = std::get<PetrifiedEffect>(target->effects[0]);
    EXPECT_EQ(pet.source_actor_id, "PU");
    EXPECT_EQ(pet.source_attack_id, "g000pt0001");
    EXPECT_EQ(pet.remaining_activation_skips, 1u);
}

TEST(PetrifyTest, ValidationRejectsDeadUnitWithEffects) {
    PetrifyF f;
    detail::apply_or_refresh_petrified(*f.state.find_unit("TU"), "PU", "G000PT0001", 1);
    f.state.find_unit("TU")->alive = false;
    f.state.find_unit("TU")->current_hp = 0;

    EXPECT_THROW(validate_battle_state(f.state), std::runtime_error);
}

TEST(PetrifyTest, ValidationRejectsZeroRemainingSkips) {
    PetrifyF f;
    detail::apply_or_refresh_petrified(*f.state.find_unit("TU"), "PU", "G000PT0001", 1);
    auto& pet = std::get<PetrifiedEffect>(f.state.find_unit("TU")->effects[0]);
    pet.remaining_activation_skips = 0;

    EXPECT_THROW(validate_battle_state(f.state), std::runtime_error);
}

TEST(PetrifyTest, ValidationRejectsEmptySourceActorId) {
    PetrifyF f;
    auto*    tu = f.state.find_unit("TU");
    EXPECT_THROW(detail::apply_or_refresh_petrified(*tu, "", "G000PT0001", 1), std::runtime_error);
}

TEST(PetrifyTest, ValidationRejectsDuplicatePetrifiedEffect) {
    PetrifyF f;
    auto*    tu = f.state.find_unit("TU");
    detail::apply_or_refresh_petrified(*tu, "PU", "G000PT0001", 1);
    // NOLINTNEXTLINE(modernize-use-emplace)
    tu->effects.push_back(PetrifiedEffect{"PU", "G000PT0001", 1});

    EXPECT_THROW(validate_battle_state(f.state), std::runtime_error);
}

TEST(PetrifyTest, FingerprintIncludesEffects) {
    PetrifyF f;
    validate_battle_state(f.state);
    auto fp_empty = compute_fingerprint(f.state);

    detail::apply_or_refresh_petrified(*f.state.find_unit("TU"), "PU", "G000PT0001", 1);
    validate_battle_state(f.state);
    auto fp_with = compute_fingerprint(f.state);

    EXPECT_NE(fp_empty, fp_with);
}

TEST(PetrifyTest, ConsumeZeroSkipsThrows) {
    PetrifyF f;
    auto*    tu = f.state.find_unit("TU");
    detail::apply_or_refresh_petrified(*tu, "PU", "G000PT0001", 1);
    auto& pet = std::get<PetrifiedEffect>(tu->effects[0]);
    pet.remaining_activation_skips = 0;

    EXPECT_THROW(detail::consume_one_petrified_activation_skip(*tu), std::runtime_error);
}

TEST(PetrifyTest, ConsumeMissingEffectThrows) {
    PetrifyF f;
    EXPECT_THROW(detail::consume_one_petrified_activation_skip(*f.state.find_unit("TU")),
                 std::runtime_error);
}

// ── Multi-target Petrify fixture ──────────────────────────────────────────────

// Multi-target GD builder includes Petrify + Damage attacks,
// plus unit types: PU (Petrify), TU target (65hp), large (90hp),
// dead (0hp), later actor (Damage, 300hp), same-round target (Damage + init 20).

[[nodiscard]] std::filesystem::path write_gd_petrify_multi_term() {
    static std::atomic<unsigned> c{0};
    auto tmp = std::filesystem::temp_directory_path() /
               ("d2bt_petrify_multi_" + std::to_string(test_support::process_id()) + "_" +
                std::to_string(c++));
    std::filesystem::remove_all(tmp);
    std::filesystem::create_directories(tmp);
    {
        DbfBuilder b{{"TXT_ID", 'C', 20}, {"TEXT", 'C', 50}};
        b.add_record({"L_HUMAN", "Human"});
        b.write(tmp / "Tglobal.dbf");
    }
    for (const auto& n : {"LunitB.dbf", "LunitC.dbf", "LDthAnim.dbf", "GDynUpgr.dbf", "GMabi.dbf",
                          "Gimmu.dbf", "GimmuC.dbf"}) {
        std::ofstream ofs(tmp / n, std::ios::binary);
        ofs.put(0x1a);
    }
    {
        DbfBuilder b{{"ID", 'N', 2}, {"TEXT", 'C', 20}};
        b.add_record({"1", "L_DAMAGE"});
        b.add_record({"7", "L_PETRIFY"});
        b.write(tmp / "LattC.dbf");
    }
    {
        DbfBuilder b{{"ID", 'N', 2}, {"TEXT", 'C', 20}};
        b.add_record({"1", "L_ALL"});
        b.add_record({"2", "L_ANY"});
        b.write(tmp / "LAttR.dbf");
    }
    {
        DbfBuilder b{{"ID", 'N', 2}, {"TEXT", 'C', 20}};
        b.add_record({"1", "L_WEAPON"});
        b.write(tmp / "LattS.dbf");
    }
    {
        DbfBuilder b{{"RACE_ID", 'C', 16},   {"NAME_TXT", 'C', 20},  {"RACE_TYPE", 'N', 3},
                     {"PLAYABLE", 'C', 1},   {"REGEN_H", 'N', 3},    {"GUARDIAN", 'C', 16},
                     {"LEADER_1", 'C', 16},  {"LEADER_2", 'C', 16},  {"LEADER_3", 'C', 16},
                     {"LEADER_4", 'C', 16},  {"SOLDIER_1", 'C', 16}, {"SOLDIER_2", 'C', 16},
                     {"SOLDIER_3", 'C', 16}, {"SOLDIER_4", 'C', 16}, {"SOLDIER_5", 'C', 16}};
        b.add_record({"G000SU0001", "L_HUMAN", "1", "T", "10", "G000PU0001", "", "", "", "", "", "",
                      "", ""});
        b.write(tmp / "Grace.dbf");
    }
    {
        DbfBuilder b{{"ATT_ID", 'C', 16},    {"NAME_TXT", 'C', 20},  {"DESC_TXT", 'C', 20},
                     {"CLASS", 'N', 2},      {"SOURCE", 'N', 2},     {"REACH", 'N', 2},
                     {"INITIATIVE", 'N', 3}, {"QTY_DAM", 'N', 4},    {"QTY_HEAL", 'N', 3},
                     {"POWER", 'N', 3},      {"INFINITE", 'N', 1},   {"CRIT_HIT", 'N', 1},
                     {"WARD1", 'C', 40},     {"WARD2", 'C', 40},     {"WARD3", 'C', 40},
                     {"WARD4", 'C', 40},     {"ALT_ATTACK", 'C', 16}};
        b.add_record({"G000PT0001", "", "", "7", "1", "1", "50", "0", "0", "60", "1", "0", "", "",
                      "", "", ""}); // Petrify All
        b.add_record({"G000DM0001", "", "", "1", "1", "2", "30", "200", "0", "100", "1", "0", "",
                      "", "", "", ""}); // Damage Any, 200 dmg, init 30
        b.add_record({"G000DM0002", "", "", "1", "1", "2", "20", "0", "0", "100", "1", "0", "", "",
                      "", "", ""}); // Dummy Damage Any, 0 dmg, init 20
        b.write(tmp / "Gattacks.dbf");
    }
    {
        DbfBuilder b{{"UNIT_ID", 'C', 16},       {"NAME_TXT", 'C', 20},   {"DESC_TXT", 'C', 20},
                     {"ABIL_TXT", 'C', 20},      {"RACE_ID", 'C', 16},    {"SUBRACE", 'N', 2},
                     {"UNIT_CAT", 'N', 2},       {"BRANCH", 'N', 2},      {"SIZE_SMALL", 'N', 1},
                     {"SEX_M", 'N', 1},          {"ATCK_TWICE", 'N', 1},  {"WATER_ONLY", 'N', 1},
                     {"LEVEL", 'N', 3},          {"HIT_POINT", 'N', 4},   {"ARMOR", 'N', 3},
                     {"REGEN", 'N', 3},          {"MOVE", 'N', 3},        {"SCOUT", 'N', 3},
                     {"LEADERSHIP", 'N', 3},     {"NEGOTIATE", 'N', 3},   {"XP_KILLED", 'N', 5},
                     {"XP_NEXT", 'N', 6},        {"ENROLL_C", 'C', 40},   {"ENROLL_B", 'C', 40},
                     {"REVIVE_C", 'C', 40},      {"HEAL_C", 'C', 40},     {"TRAINING_C", 'C', 40},
                     {"ATTACK_ID", 'C', 16},     {"ATTACK2_ID", 'C', 16}, {"PREV_ID", 'C', 16},
                     {"BASE_UNIT", 'C', 16},     {"UPGRADE_B", 'C', 16},  {"DYN_UPG1", 'C', 16},
                     {"DYN_UPG2", 'C', 16},      {"DYN_UPG_LV", 'N', 3},  {"DEATH_ANIM", 'N', 3},
                     {"DEATH_ANIM_LBL", 'C', 20}};
        b.add_record({"G000PU0001", "",  "",  "",           "G000SU0001", "0",   "1", "0",
                      "1",          "1", "0", "0",          "1",          "300", "2", "0",
                      "3",          "1", "5", "1",          "50",         "100", "",  "",
                      "",           "",  "",  "G000PT0001", "",           "",    "",  "",
                      "",           "",  "",  "1",          "L_HUMAN"}); // Petrifier
        b.add_record(
            {"G000PU0002", "",   "",  "",   "G000SU0001", "0", "1",      "0", "1", "1",
             "0",          "0",  "1", "65", "2",          "0", "2",      "1", "0", "1",
             "25",         "50", "",  "",   "",           "",  "",       "",  "",  "",
             "",           "",   "",  "",   "",           "1", "L_HUMAN"}); // target no init
        b.add_record({"G000PU0004", "",   "",  "",   "G000SU0001", "0", "1",      "0", "0", "1",
                      "0",          "0",  "1", "90", "2",          "0", "2",      "1", "0", "1",
                      "25",         "50", "",  "",   "",           "",  "",       "",  "",  "",
                      "",           "",   "",  "",   "",           "1", "L_HUMAN"}); // large
        b.add_record(
            {"G000PU0010", "",    "",  "",    "G000SU0001", "0", "1",      "0", "1", "1",
             "0",          "0",   "1", "300", "2",          "0", "3",      "1", "5", "1",
             "50",         "100", "",  "",    "",           "",  "",       "",  "",  "",
             "",           "",    "",  "",    "",           "1", "L_HUMAN"}); // later (Damage)
        b.write(tmp / "Gunits.dbf");
    }
    return tmp;
}

struct PetrifyMultiF {
    std::filesystem::path      gp;
    d2engine::GameDataRegistry gd;
    AdventureWorldState        w;
    BattleState                state;

    PetrifyMultiF() : gp(write_gd_petrify_multi_term()), gd(gp) {
        w.units.push_back({"PU", "g000pu0001", 1, {{}}, 0, "Petrifier", 0, {}, 300, 0});
        w.units.push_back({"EA", "g000pu0002", 1, {{}}, 0, "EnemyA", 0, {}, 65, 0});
        w.units.push_back({"EB", "g000pu0002", 1, {{}}, 0, "EnemyB", 0, {}, 65, 0});
        w.units.push_back({"EC", "g000pu0004", 1, {{}}, 0, "LargeC", 0, {}, 90, 0});
        w.units.push_back({"ED", "g000pu0002", 1, {{}}, 0, "DeadD", 0, {}, 0, 0});

        AdventureStack s1;
        s1.id = "S1";
        s1.owner = "O1";
        s1.subrace = "R1";
        s1.leader_id = "PU";
        s1.morale = 50;
        s1.group.members[0] = "PU";
        s1.group.positions[0] = 2;
        s1.group.cell_members[2] = 0;
        s1.banner = "B1";
        s1.position = {21, 23};

        AdventureStack s2;
        s2.id = "S2";
        s2.owner = "O2";
        s2.subrace = "R2";
        s2.leader_id = "EA";
        s2.morale = 40;
        s2.group.members[0] = "EA";
        s2.group.members[1] = "EB";
        s2.group.members[2] = "EC";
        s2.group.members[3] = "ED";
        s2.group.positions[0] = 0;
        s2.group.positions[1] = 1;
        s2.group.positions[2] = 4;
        s2.group.positions[3] = 3;
        s2.group.cell_members[0] = 0;
        s2.group.cell_members[1] = 1;
        s2.group.cell_members[3] = 3;
        s2.group.cell_members[4] = 2;
        s2.group.cell_members[5] = 2;
        s2.position = {24, 20};

        w.stacks.push_back(s1);
        w.stacks.push_back(s2);
        state = bootstrap_battle(s1, s2, w, gd);
        validate_battle_state(state);
        // Mark ED dead
        {
            auto* dd = state.find_unit("ED");
            if (!dd)
                throw std::runtime_error("PetrifyMultiF: ED unit not found");
            dd->alive = false;
            dd->current_hp = 0;
            validate_battle_state(state);
        }
    }
};

TEST(PetrifyTest, PetrifyAllEmitsSingleAllEnemyUnitsAction) {
    PetrifyMultiF f;
    auto          actions = valid_actions(f.state, f.gd);
    ASSERT_EQ(actions.size(), 1u);
    const auto& attack = std::get<AttackAction>(actions[0]);
    EXPECT_TRUE(std::holds_alternative<AllEnemyUnitsTarget>(attack.target));
}

TEST(PetrifyTest, PetrifyAllAppliesToEveryAliveEnemyExactlyOnce) {
    PetrifyMultiF f;
    auto          acts = valid_actions(f.state, f.gd);
    ASSERT_EQ(acts.size(), 1u);

    auto next = apply(f.state, acts[0], f.gd);

    // PU actor unaffected
    const auto* pu = next.find_unit("PU");
    ASSERT_NE(pu, nullptr);
    EXPECT_EQ(pu->current_hp, 300);
    EXPECT_TRUE(pu->effects.empty());

    // EA alive → got PetrifiedEffect
    auto* ea = next.find_unit("EA");
    ASSERT_NE(ea, nullptr);
    EXPECT_TRUE(ea->alive);
    ASSERT_EQ(ea->effects.size(), 1u);
    EXPECT_TRUE(std::holds_alternative<PetrifiedEffect>(ea->effects[0]));
    EXPECT_EQ(ea->current_hp, 65);

    // EB alive → got PetrifiedEffect
    auto* eb = next.find_unit("EB");
    ASSERT_NE(eb, nullptr);
    EXPECT_TRUE(eb->alive);
    ASSERT_EQ(eb->effects.size(), 1u);
    EXPECT_TRUE(std::holds_alternative<PetrifiedEffect>(eb->effects[0]));
    EXPECT_EQ(eb->current_hp, 65);

    // EC large (2 cells) → exactly one PetrifiedEffect
    auto* ec = next.find_unit("EC");
    ASSERT_NE(ec, nullptr);
    EXPECT_TRUE(ec->alive);
    ASSERT_EQ(ec->effects.size(), 1u);
    EXPECT_TRUE(std::holds_alternative<PetrifiedEffect>(ec->effects[0]));
    EXPECT_EQ(ec->current_hp, 90);

    // ED dead → no effect, HP 0
    auto* ed = next.find_unit("ED");
    ASSERT_NE(ed, nullptr);
    EXPECT_FALSE(ed->alive);
    EXPECT_TRUE(ed->effects.empty());
    EXPECT_EQ(ed->current_hp, 0);
}

TEST(PetrifyTest, PetrifyAttackRefreshesExistingEffectWithoutStacking) {
    PetrifyF f;

    // Add a PetrifiedEffect with different source_attack_id so Petrify All will refresh it
    detail::apply_or_refresh_petrified(*f.state.find_unit("TU"), "PU", "G000PT0099", 1);
    ASSERT_TRUE(detail::is_petrified(*f.state.find_unit("TU")));
    ASSERT_EQ(f.state.current_actor()->id, "PU");
    validate_battle_state(f.state);

    auto acts = valid_actions(f.state, f.gd);
    ASSERT_EQ(acts.size(), 1u);

    auto next = apply(f.state, acts[0], f.gd);

    // TU has exactly one PetrifiedEffect — refreshed, not stacked
    const auto* tu = next.find_unit("TU");
    ASSERT_NE(tu, nullptr);
    ASSERT_EQ(tu->effects.size(), 1u);
    EXPECT_TRUE(std::holds_alternative<PetrifiedEffect>(tu->effects[0]));
    const auto& pet = std::get<PetrifiedEffect>(tu->effects[0]);
    EXPECT_EQ(pet.source_actor_id, "PU");
    EXPECT_EQ(pet.source_attack_id, "g000pt0001");
    EXPECT_EQ(pet.remaining_activation_skips, 1u);
    EXPECT_EQ(tu->current_hp, 65);
    EXPECT_TRUE(tu->alive);
}

TEST(PetrifyTest, ForcedPetrifySkipConsumesEffectAndAdvancesExactlyOnce) {
    // Same-round advance: after PU petrifies all enemies, EA (first enemy in turn
    // order) must skip. The turn advances to EB (next alive actor) within the same round.
    PetrifyMultiF f;

    // Get PU's Petrify action
    auto acts1 = valid_actions(f.state, f.gd);
    ASSERT_GE(acts1.size(), 1u);
    auto after_atk = apply(f.state, acts1[0], f.gd);

    // EA is the first petrified enemy in turn order
    const auto* ea = after_atk.find_unit("EA");
    ASSERT_NE(ea, nullptr);
    ASSERT_TRUE(ea->alive);
    ASSERT_TRUE(detail::is_petrified(*ea));
    ASSERT_EQ(after_atk.current_actor()->id, "EA");

    auto round_before = after_atk.round_state.round_number;
    auto turn_before = after_atk.round_state.current_turn_index;
    auto hp_before = ea->current_hp;

    // Get EA's forced Skip action
    auto acts2 = valid_actions(after_atk, f.gd);
    ASSERT_EQ(acts2.size(), 1u);
    ASSERT_TRUE(std::holds_alternative<SkipActivationAction>(acts2[0]));
    EXPECT_EQ(std::get<SkipActivationAction>(acts2[0]).actor_id, "EA");

    auto after_skip = apply(after_atk, acts2[0], f.gd);

    // EA unchanged HP, alive, effect consumed
    const auto* ea_after = after_skip.find_unit("EA");
    ASSERT_NE(ea_after, nullptr);
    EXPECT_EQ(ea_after->current_hp, hp_before);
    EXPECT_TRUE(ea_after->alive);
    EXPECT_TRUE(ea_after->effects.empty());

    // Same-round advance: next actor is EB
    EXPECT_EQ(after_skip.round_state.round_number, round_before);
    EXPECT_EQ(after_skip.round_state.current_turn_index, turn_before + 1);
    ASSERT_NE(after_skip.current_actor(), nullptr);
    EXPECT_EQ(after_skip.current_actor()->id, "EB");
    EXPECT_EQ(after_skip.status, BattleStatus::InProgress);
    EXPECT_FALSE(after_skip.winner.has_value());
}

TEST(PetrifyTest, ForcedPetrifySkipAtRoundEndStartsNextRound) {
    PetrifyF f;
    auto     acts1 = valid_actions(f.state, f.gd);
    ASSERT_EQ(acts1.size(), 1u);

    auto after_atk = apply(f.state, acts1[0], f.gd);
    ASSERT_TRUE(after_atk.find_unit("TU")->alive);
    ASSERT_EQ(after_atk.find_unit("TU")->effects.size(), 1u);

    const auto round_before = after_atk.round_state.round_number;
    const auto hp_before = after_atk.find_unit("TU")->current_hp;

    // TU is last in turn order; apply forced skip → new round
    auto acts2 = valid_actions(after_atk, f.gd);
    ASSERT_EQ(acts2.size(), 1u);
    ASSERT_TRUE(std::holds_alternative<SkipActivationAction>(acts2[0]));

    auto after_skip = apply(after_atk, acts2[0], f.gd);

    // TU unchanged HP, alive, effect consumed
    const auto* tu = after_skip.find_unit("TU");
    ASSERT_NE(tu, nullptr);
    EXPECT_EQ(tu->current_hp, hp_before);
    EXPECT_TRUE(tu->alive);
    EXPECT_TRUE(tu->effects.empty());

    // New round: round_number incremented, turn_index reset to 0
    EXPECT_EQ(after_skip.round_state.round_number, round_before + 1);
    EXPECT_EQ(after_skip.round_state.current_turn_index, 0u);

    // Next actor is PU (first in turn order)
    ASSERT_NE(after_skip.current_actor(), nullptr);
    EXPECT_EQ(after_skip.current_actor()->id, "PU");

    // Battle still in progress
    EXPECT_EQ(after_skip.status, BattleStatus::InProgress);
    EXPECT_FALSE(after_skip.winner.has_value());
}

TEST(PetrifyTest, PetrifiedUnitStillCountsAsAliveForBattleStatus) {
    auto                       gp = write_gd_petrify();
    d2engine::GameDataRegistry gd(gp);

    d2runtime::AdventureWorldState w;
    w.units.push_back({"PU", "g000pu0001", 1, {{}}, 0, "Petrifier", 0, {}, 300, 0});
    w.units.push_back({"TU", "g000pu0002", 1, {{}}, 0, "Target", 0, {}, 65, 0});

    d2runtime::AdventureStack s1;
    s1.id = "S1";
    s1.owner = "O1";
    s1.leader_id = "PU";
    s1.group.members[0] = "PU";
    s1.group.positions[0] = 2;
    s1.group.cell_members[2] = 0;
    s1.position = {21, 23};

    d2runtime::AdventureStack s2;
    s2.id = "S2";
    s2.owner = "O2";
    s2.leader_id = "TU";
    s2.group.members[0] = "TU";
    s2.group.positions[0] = 1;
    s2.group.cell_members[1] = 0;
    s2.position = {24, 20};

    w.stacks.push_back(s1);
    w.stacks.push_back(s2);

    auto state = bootstrap_battle(s1, s2, w, gd);
    auto acts = valid_actions(state, gd);
    ASSERT_EQ(acts.size(), 1u);

    auto  after = apply(state, acts[0], gd);
    auto* tu = after.find_unit("TU");
    ASSERT_NE(tu, nullptr);
    EXPECT_TRUE(tu->alive);
    EXPECT_EQ(tu->effects.size(), 1u);

    EXPECT_EQ(after.status, BattleStatus::InProgress);
    EXPECT_FALSE(after.winner.has_value());
}

// GD builder for lethal damage test: PU has Damage/Any primary + Petrify/Any secondary
[[nodiscard]] std::filesystem::path write_gd_composite_lethal() {
    static std::atomic<unsigned> c{0};
    auto tmp = std::filesystem::temp_directory_path() /
               ("d2bt_composite_lethal_" + std::to_string(test_support::process_id()) + "_" +
                std::to_string(c++));
    std::filesystem::remove_all(tmp);
    std::filesystem::create_directories(tmp);
    {
        DbfBuilder b{{"TXT_ID", 'C', 20}, {"TEXT", 'C', 50}};
        b.add_record({"L_HUMAN", "Human"});
        b.write(tmp / "Tglobal.dbf");
    }
    for (const auto& n : {"LunitB.dbf", "LunitC.dbf", "LDthAnim.dbf", "GDynUpgr.dbf", "GMabi.dbf",
                          "Gimmu.dbf", "GimmuC.dbf"}) {
        std::ofstream ofs(tmp / n, std::ios::binary);
        ofs.put(0x1a);
    }
    {
        DbfBuilder b{{"ID", 'N', 2}, {"TEXT", 'C', 20}};
        b.add_record({"1", "L_DAMAGE"});
        b.add_record({"7", "L_PETRIFY"});
        b.write(tmp / "LattC.dbf");
    }
    {
        DbfBuilder b{{"ID", 'N', 2}, {"TEXT", 'C', 20}};
        b.add_record({"1", "L_ALL"});
        b.add_record({"2", "L_ANY"});
        b.write(tmp / "LAttR.dbf");
    }
    {
        DbfBuilder b{{"ID", 'N', 2}, {"TEXT", 'C', 20}};
        b.add_record({"1", "L_WEAPON"});
        b.write(tmp / "LattS.dbf");
    }
    {
        DbfBuilder b{{"RACE_ID", 'C', 16},   {"NAME_TXT", 'C', 20},  {"RACE_TYPE", 'N', 3},
                     {"PLAYABLE", 'C', 1},   {"REGEN_H", 'N', 3},    {"GUARDIAN", 'C', 16},
                     {"LEADER_1", 'C', 16},  {"LEADER_2", 'C', 16},  {"LEADER_3", 'C', 16},
                     {"LEADER_4", 'C', 16},  {"SOLDIER_1", 'C', 16}, {"SOLDIER_2", 'C', 16},
                     {"SOLDIER_3", 'C', 16}, {"SOLDIER_4", 'C', 16}, {"SOLDIER_5", 'C', 16}};
        b.add_record({"G000SU0001", "L_HUMAN", "1", "T", "10", "G000PU0001", "", "", "", "", "", "",
                      "", ""});
        b.write(tmp / "Grace.dbf");
    }
    {
        DbfBuilder b{{"ATT_ID", 'C', 16},    {"NAME_TXT", 'C', 20},  {"DESC_TXT", 'C', 20},
                     {"CLASS", 'N', 2},      {"SOURCE", 'N', 2},     {"REACH", 'N', 2},
                     {"INITIATIVE", 'N', 3}, {"QTY_DAM", 'N', 4},    {"QTY_HEAL", 'N', 3},
                     {"POWER", 'N', 3},      {"INFINITE", 'N', 1},   {"CRIT_HIT", 'N', 1},
                     {"WARD1", 'C', 40},     {"WARD2", 'C', 40},     {"WARD3", 'C', 40},
                     {"WARD4", 'C', 40},     {"ALT_ATTACK", 'C', 16}};
        b.add_record({"G000DM0001", "", "", "1", "1", "2", "30", "200", "0", "100", "1", "0", "",
                      "", "", "", ""}); // Damage Any 200 dmg
        b.add_record({"G000PT0001", "", "", "7", "1", "2", "50", "0", "0", "60", "1", "0", "", "",
                      "", "", ""}); // Petrify Any
        b.write(tmp / "Gattacks.dbf");
    }
    {
        DbfBuilder b{{"UNIT_ID", 'C', 16},       {"NAME_TXT", 'C', 20},   {"DESC_TXT", 'C', 20},
                     {"ABIL_TXT", 'C', 20},      {"RACE_ID", 'C', 16},    {"SUBRACE", 'N', 2},
                     {"UNIT_CAT", 'N', 2},       {"BRANCH", 'N', 2},      {"SIZE_SMALL", 'N', 1},
                     {"SEX_M", 'N', 1},          {"ATCK_TWICE", 'N', 1},  {"WATER_ONLY", 'N', 1},
                     {"LEVEL", 'N', 3},          {"HIT_POINT", 'N', 4},   {"ARMOR", 'N', 3},
                     {"REGEN", 'N', 3},          {"MOVE", 'N', 3},        {"SCOUT", 'N', 3},
                     {"LEADERSHIP", 'N', 3},     {"NEGOTIATE", 'N', 3},   {"XP_KILLED", 'N', 5},
                     {"XP_NEXT", 'N', 6},        {"ENROLL_C", 'C', 40},   {"ENROLL_B", 'C', 40},
                     {"REVIVE_C", 'C', 40},      {"HEAL_C", 'C', 40},     {"TRAINING_C", 'C', 40},
                     {"ATTACK_ID", 'C', 16},     {"ATTACK2_ID", 'C', 16}, {"PREV_ID", 'C', 16},
                     {"BASE_UNIT", 'C', 16},     {"UPGRADE_B", 'C', 16},  {"DYN_UPG1", 'C', 16},
                     {"DYN_UPG2", 'C', 16},      {"DYN_UPG_LV", 'N', 3},  {"DEATH_ANIM", 'N', 3},
                     {"DEATH_ANIM_LBL", 'C', 20}};
        b.add_record({"G000PU0001", "",  "",  "",           "G000SU0001", "0",   "1", "0",
                      "1",          "1", "0", "0",          "1",          "300", "2", "0",
                      "3",          "1", "5", "1",          "50",         "100", "",  "",
                      "",           "",  "",  "G000DM0001", "G000PT0001", "",    "",  "",
                      "",           "",  "",  "1",          "L_HUMAN"}); // Damage + Petrify
        b.add_record({"G000PU0002", "",   "",  "",   "G000SU0001", "0", "1",      "0", "1", "1",
                      "0",          "0",  "1", "65", "2",          "0", "2",      "1", "0", "1",
                      "25",         "50", "",  "",   "",           "",  "",       "",  "",  "",
                      "",           "",   "",  "",   "",           "1", "L_HUMAN"}); // target
        b.write(tmp / "Gunits.dbf");
    }
    return tmp;
}

TEST(PetrifyTest, SecondaryComponentDropsTargetsKilledByPrimary) {
    auto                       gp = write_gd_composite_lethal();
    d2engine::GameDataRegistry gd(gp);

    d2runtime::AdventureWorldState w;
    w.units.push_back({"PU", "g000pu0001", 1, {}, 0, "Attacker", 0, {}, 300, 0});
    w.units.push_back({"TU", "g000pu0002", 1, {}, 0, "Target", 0, {}, 65, 0});

    d2runtime::AdventureStack s1;
    s1.id = "S1";
    s1.owner = "O1";
    s1.subrace = "R1";
    s1.leader_id = "PU";
    s1.group.members[0] = "PU";
    s1.group.positions[0] = 2;
    s1.group.cell_members[2] = 0;
    s1.banner = "B1";
    s1.position = {21, 23};

    d2runtime::AdventureStack s2;
    s2.id = "S2";
    s2.owner = "O2";
    s2.subrace = "R2";
    s2.leader_id = "TU";
    s2.group.members[0] = "TU";
    s2.group.positions[0] = 1;
    s2.group.cell_members[1] = 0;
    s2.position = {24, 20};

    w.stacks.push_back(s1);
    w.stacks.push_back(s2);

    auto state = bootstrap_battle(s1, s2, w, gd);

    ASSERT_TRUE(state.current_actor() != nullptr);
    ASSERT_EQ(state.current_actor()->id, "PU");

    auto fingerprint_before = compute_fingerprint(state);

    // valid_actions must contain exactly one UnitTarget action on the target
    auto acts = valid_actions(state, gd);
    ASSERT_EQ(acts.size(), 1u);
    const auto* atk = std::get_if<AttackAction>(&acts[0]);
    ASSERT_NE(atk, nullptr);
    EXPECT_TRUE(std::holds_alternative<UnitTarget>(atk->target));
    EXPECT_EQ(std::get<UnitTarget>(atk->target).unit_id, "TU");

    // Public apply
    auto next = apply(state, acts[0], gd);

    const auto* tu = next.find_unit("TU");
    ASSERT_NE(tu, nullptr);
    EXPECT_EQ(tu->current_hp, 0);
    EXPECT_FALSE(tu->alive);

    // PetrifiedEffect must NOT be present (target was killed by primary damage)
    EXPECT_FALSE(detail::is_petrified(*tu));

    // Input fingerprint unchanged
    EXPECT_EQ(compute_fingerprint(state), fingerprint_before);

    // Turn advanced exactly once if there's a next live actor
    if (next.status == BattleStatus::InProgress && next.current_actor()) {
        EXPECT_EQ(next.round_state.current_turn_index, state.round_state.current_turn_index + 1);
    }
}

TEST(PetrifyTest, PetrifiedActorCannotUseAttackAction) {
    PetrifyF f;
    auto     acts = valid_actions(f.state, f.gd);
    ASSERT_EQ(acts.size(), 1u);
    auto after = apply(f.state, acts[0], f.gd);

    ASSERT_NE(after.current_actor(), nullptr);
    EXPECT_EQ(after.current_actor()->id, "TU");

    auto err = validate_action(BattleAction{AttackAction{"TU", UnitTarget{"PU"}}}, after, f.gd);
    EXPECT_EQ(err, ActionValidationError::ActorIncapacitated);
}

TEST(PetrifyTest, NonPetrifiedActorCannotUseSkipAction) {
    PetrifyF f;
    // PU is current actor but not petrified → SkipNotRequired
    auto err = validate_action(
        BattleAction{SkipActivationAction{"PU", SkipActivationReason::Petrified}}, f.state, f.gd);
    EXPECT_EQ(err, ActionValidationError::SkipNotRequired);
}

TEST(PetrifyTest, SkipActorMustBeCurrentActor) {
    PetrifyF f;
    // Apply petrify so TU becomes petrified current actor
    auto acts = valid_actions(f.state, f.gd);
    ASSERT_EQ(acts.size(), 1u);
    auto after = apply(f.state, acts[0], f.gd);
    ASSERT_EQ(after.current_actor()->id, "TU");
    ASSERT_TRUE(detail::is_petrified(*after.current_actor()));

    // TU is current and petrified, but SkipActivationAction specifies PU (non-current) →
    // ActorNotCurrent
    auto err = validate_action(
        BattleAction{SkipActivationAction{"PU", SkipActivationReason::Petrified}}, after, f.gd);
    EXPECT_EQ(err, ActionValidationError::ActorNotCurrent);
}

TEST(PetrifyTest, SkipReasonMustMatchActiveEffect) {
    PetrifyF f;
    auto     acts = valid_actions(f.state, f.gd);
    ASSERT_EQ(acts.size(), 1u);
    auto after = apply(f.state, acts[0], f.gd);
    ASSERT_EQ(after.current_actor()->id, "TU");
    ASSERT_TRUE(detail::is_petrified(*after.current_actor()));

    auto invalid_reason = static_cast<SkipActivationReason>(255);
    auto err =
        validate_action(BattleAction{SkipActivationAction{"TU", invalid_reason}}, after, f.gd);
    EXPECT_EQ(err, ActionValidationError::SkipReasonMismatch);
}

TEST(PetrifyTest, ForcedSkipOutcomeDoesNotMutateAuthoritativeState) {
    PetrifyF f;
    auto     acts1 = valid_actions(f.state, f.gd);
    ASSERT_EQ(acts1.size(), 1u);

    auto after_atk = apply(f.state, acts1[0], f.gd);
    auto fp_before = compute_fingerprint(after_atk);

    auto outcomes = valid_action_outcomes(after_atk, f.gd);
    ASSERT_EQ(outcomes.size(), 1u);
    ASSERT_TRUE(std::holds_alternative<SkipActivationAction>(outcomes[0].action));

    // Input state fingerprint unchanged
    auto fp_after = compute_fingerprint(after_atk);
    EXPECT_EQ(fp_before, fp_after);

    // Input state still has PetrifiedEffect
    const auto* tu = after_atk.find_unit("TU");
    ASSERT_NE(tu, nullptr);
    EXPECT_TRUE(detail::is_petrified(*tu));

    // Successor fingerprint differs
    auto fp_outcome = compute_fingerprint(outcomes[0].outcome);
    EXPECT_NE(fp_before, fp_outcome);

    // Successor: PetrifiedEffect consumed, turn advanced
    const auto* tu_out = outcomes[0].outcome.find_unit("TU");
    ASSERT_NE(tu_out, nullptr);
    EXPECT_TRUE(tu_out->effects.empty());
    EXPECT_TRUE(tu_out->alive);
    EXPECT_EQ(tu_out->current_hp, 65);

    // TU was last in order → new round starts with PU
    EXPECT_EQ(outcomes[0].outcome.round_state.round_number, after_atk.round_state.round_number + 1);
    ASSERT_NE(outcomes[0].outcome.current_actor(), nullptr);
    EXPECT_EQ(outcomes[0].outcome.current_actor()->id, "PU");
}

TEST(PetrifyTest, PetrifyFingerprintChangesWithSourceActor) {
    PetrifyF f;
    // Use existing units "PU" and "TU" as source actors (both exist in state)
    validate_battle_state(f.state);

    detail::apply_or_refresh_petrified(*f.state.find_unit("TU"), "PU", "G000PT0001", 1);
    auto fp1 = compute_fingerprint(f.state);

    f.state.find_unit("TU")->effects.clear();
    detail::apply_or_refresh_petrified(*f.state.find_unit("TU"), "TU", "G000PT0001", 1);
    auto fp2 = compute_fingerprint(f.state);

    EXPECT_NE(fp1, fp2);
}

TEST(PetrifyTest, PetrifyFingerprintChangesWithSourceAttack) {
    PetrifyF f;
    validate_battle_state(f.state);

    detail::apply_or_refresh_petrified(*f.state.find_unit("TU"), "PU", "G000PT0001", 1);
    auto fp1 = compute_fingerprint(f.state);

    f.state.find_unit("TU")->effects.clear();
    detail::apply_or_refresh_petrified(*f.state.find_unit("TU"), "PU", "G000PT0002", 1);
    auto fp2 = compute_fingerprint(f.state);

    EXPECT_NE(fp1, fp2);
}

TEST(PetrifyTest, PetrifyFingerprintChangesWithRemainingSkips) {
    PetrifyF f;
    validate_battle_state(f.state);

    detail::apply_or_refresh_petrified(*f.state.find_unit("TU"), "PU", "G000PT0001", 1);
    auto fp1 = compute_fingerprint(f.state);

    f.state.find_unit("TU")->effects.clear();
    detail::apply_or_refresh_petrified(*f.state.find_unit("TU"), "PU", "G000PT0001", 2);
    auto fp2 = compute_fingerprint(f.state);

    EXPECT_NE(fp1, fp2);
}

TEST(PetrifyTest, ApplyPetrifiedRejectsZeroActivationSkips) {
    PetrifyF f;
    auto*    tu = f.state.find_unit("TU");
    EXPECT_THROW(detail::apply_or_refresh_petrified(*tu, "PU", "G000PT0001", 0),
                 std::runtime_error);
    EXPECT_TRUE(tu->effects.empty());
}

TEST(PetrifyTest, ApplyPetrifiedRejectsDeadTarget) {
    PetrifyF f;
    auto*    tu = f.state.find_unit("TU");
    tu->alive = false;
    tu->current_hp = 0;
    EXPECT_THROW(detail::apply_or_refresh_petrified(*tu, "PU", "G000PT0001", 1),
                 std::runtime_error);
}

TEST(PetrifyTest, ApplyPetrifiedRejectsEmptySourceActor) {
    PetrifyF f;
    auto*    tu = f.state.find_unit("TU");
    EXPECT_THROW(detail::apply_or_refresh_petrified(*tu, "", "G000PT0001", 1), std::runtime_error);
    EXPECT_TRUE(tu->effects.empty());
}

TEST(PetrifyTest, ApplyPetrifiedRejectsEmptySourceAttack) {
    PetrifyF f;
    auto*    tu = f.state.find_unit("TU");
    EXPECT_THROW(detail::apply_or_refresh_petrified(*tu, "PU", "", 1), std::runtime_error);
    EXPECT_TRUE(tu->effects.empty());
}
