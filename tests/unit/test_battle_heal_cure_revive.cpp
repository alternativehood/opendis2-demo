#include <gtest/gtest.h>

#include "tests/test_dbf_builder.hpp"

#include <d2battle_rules/attack_support.hpp>
#include <d2battle_rules/battle_action.hpp>
#include <d2battle_rules/battle_action_validate.hpp>
#include <d2battle_rules/battle_apply.hpp>
#include <d2battle_rules/battle_bootstrap.hpp>
#include <d2battle_rules/battle_fingerprint.hpp>
#include <d2battle_rules/battle_outcomes.hpp>
#include <d2battle_rules/battle_state.hpp>
#include <d2battle_rules/battle_valid_actions.hpp>
#include <d2battle_rules/detail/attack_target_enumeration.hpp>
#include <d2battle_rules/detail/battle_action_validate_internal.hpp>
#include <d2battle_rules/detail/battle_attack_rules.hpp>
#include <d2battle_rules/detail/battle_derived.hpp>
#include <d2battle_rules/detail/battle_valid_actions_internal.hpp>
#include <d2battle_rules/detail/battle_status.hpp>
#include <d2battle_rules/detail/battle_turn.hpp>
#include <d2battle_rules/detail/cure.hpp>
#include <d2battle_rules/detail/heal.hpp>
#include <d2battle_rules/detail/healing_primitive.hpp>
#include <d2battle_rules/detail/revive.hpp>
#include <d2battle_rules/detail/unit_effects.hpp>
#include <d2engine/assets/attack_def.hpp>
#include <d2engine/assets/game_data_registry.hpp>
#include <opendis2_battle/terminal_view.hpp>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "tests/test_process.hpp"

namespace {

using namespace d2battle;

[[nodiscard]] std::size_t count_occurrences(const std::string& haystack,
                                            const std::string& needle) {
    std::size_t count = 0;
    std::size_t pos = 0;
    while ((pos = haystack.find(needle, pos)) != std::string::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

std::filesystem::path write_hcr_gd() {
    static std::atomic<unsigned> c{0};
    auto tmp = std::filesystem::temp_directory_path() /
               ("d2hcr_" + std::to_string(test_support::process_id()) + "_" + std::to_string(c++));
    std::filesystem::remove_all(tmp);
    std::filesystem::create_directories(tmp);
    {
        test_dbf::DbfBuilder b{{"TXT_ID", 'C', 20}, {"TEXT", 'C', 50}};
        b.add_record({"L_HUMAN", "Human"});
        b.write(tmp / "Tglobal.dbf");
    }
    for (const auto& n : {"LunitB.dbf", "LunitC.dbf", "LDthAnim.dbf", "GDynUpgr.dbf", "GMabi.dbf",
                          "Gimmu.dbf", "GimmuC.dbf"}) {
        std::ofstream ofs(tmp / n, std::ios::binary);
        ofs.put(0x1a);
    }
    {
        test_dbf::DbfBuilder b{{"ID", 'N', 2}, {"TEXT", 'C', 20}};
        b.add_record({"1", "L_DAMAGE"});
        b.add_record({"2", "L_DRAIN"});
        b.add_record({"6", "L_PETRIFY"});
        b.add_record({"12", "L_DRAIN_OVERFLOW"});
        b.add_record({"13", "L_HEAL"});
        b.add_record({"21", "L_REVIVE"});
        b.add_record({"23", "L_CURE"});
        b.write(tmp / "LattC.dbf");
    }
    {
        test_dbf::DbfBuilder b{{"ID", 'N', 2}, {"TEXT", 'C', 20}};
        b.add_record({"1", "L_ALL"});
        b.add_record({"2", "L_ANY"});
        b.add_record({"3", "L_ADJACENT"});
        b.write(tmp / "LAttR.dbf");
    }
    {
        test_dbf::DbfBuilder b{{"ID", 'N', 2}, {"TEXT", 'C', 20}};
        b.add_record({"1", "L_WEAPON"});
        b.add_record({"2", "L_MIND"});
        b.add_record({"3", "L_LIFE"});
        b.add_record({"4", "L_DEATH"});
        b.add_record({"5", "L_FIRE"});
        b.add_record({"6", "L_WATER"});
        b.add_record({"7", "L_EARTH"});
        b.add_record({"8", "L_AIR"});
        b.write(tmp / "LattS.dbf");
    }
    {
        test_dbf::DbfBuilder b{
            {"RACE_ID", 'C', 16},   {"NAME_TXT", 'C', 20},  {"RACE_TYPE", 'N', 3},
            {"PLAYABLE", 'C', 1},   {"REGEN_H", 'N', 3},    {"GUARDIAN", 'C', 16},
            {"LEADER_1", 'C', 16},  {"LEADER_2", 'C', 16},  {"LEADER_3", 'C', 16},
            {"LEADER_4", 'C', 16},  {"SOLDIER_1", 'C', 16}, {"SOLDIER_2", 'C', 16},
            {"SOLDIER_3", 'C', 16}, {"SOLDIER_4", 'C', 16}, {"SOLDIER_5", 'C', 16}};
        b.add_record({"G000SU0001", "L_HUMAN", "1", "T", "10", "G000UH0100", "", "", "", "", "", "",
                      "", "", ""});
        b.write(tmp / "Grace.dbf");
    }
    {
        auto write_gunits = [&](const std::vector<std::vector<std::string>>& rows) {
            test_dbf::DbfBuilder b{
                {"UNIT_ID", 'C', 16},       {"NAME_TXT", 'C', 20},   {"DESC_TXT", 'C', 20},
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
            for (const auto& r : rows)
                b.add_record(r);
            b.write(tmp / "Gunits.dbf");
        };
        write_gunits({
            {"G000UH0100", "",    "",  "",    "G000SU0001", "0", "1",      "0",       "1", "1",
             "0",          "0",   "1", "200", "2",          "0", "1",      "1",       "0", "0",
             "75",         "150", "",  "",    "",           "",  "",       "G_HL_AN", "",  "",
             "",           "",    "",  "",    "",           "1", "L_HUMAN"},
            {"G000UH0105", "",    "",  "",    "G000SU0001", "0", "1",      "0",       "1", "1",
             "0",          "0",   "1", "200", "2",          "0", "1",      "1",       "0", "0",
             "75",         "150", "",  "",    "",           "",  "",       "G_HL_AL", "",  "",
             "",           "",    "",  "",    "",           "1", "L_HUMAN"},
            {"G000UH0101", "",    "",  "",    "G000SU0001", "0", "1",      "0",       "1", "1",
             "0",          "0",   "1", "200", "2",          "0", "1",      "1",       "0", "0",
             "75",         "150", "",  "",    "",           "",  "",       "G_CR_AN", "",  "",
             "",           "",    "",  "",    "",           "1", "L_HUMAN"},
            {"G000UH0106", "",    "",  "",    "G000SU0001", "0", "1",      "0",       "1", "1",
             "0",          "0",   "1", "200", "2",          "0", "1",      "1",       "0", "0",
             "75",         "150", "",  "",    "",           "",  "",       "G_CR_AL", "",  "",
             "",           "",    "",  "",    "",           "1", "L_HUMAN"},
            {"G000UH0102", "",    "",  "",    "G000SU0001", "0", "1",      "0",       "1", "1",
             "0",          "0",   "1", "200", "2",          "0", "1",      "1",       "0", "0",
             "75",         "150", "",  "",    "",           "",  "",       "G_RV_AN", "",  "",
             "",           "",    "",  "",    "",           "1", "L_HUMAN"},
            {"G000UH0107", "",    "",  "",    "G000SU0001", "0", "1",      "0",       "1", "1",
             "0",          "0",   "1", "200", "2",          "0", "1",      "1",       "0", "0",
             "75",         "150", "",  "",    "",           "",  "",       "G_RV_AL", "",  "",
             "",           "",    "",  "",    "",           "1", "L_HUMAN"},
            {"G000UH0103", "",    "",  "",    "G000SU0001", "0", "1",      "0", "1", "1",
             "0",          "0",   "1", "100", "2",          "0", "1",      "1", "0", "0",
             "75",         "150", "",  "",    "",           "",  "",       "",  "",  "",
             "",           "",    "",  "",    "",           "1", "L_HUMAN"},
            {"G000UH0104", "",    "",  "",    "G000SU0001", "0", "1",      "0", "2", "1",
             "0",          "0",   "1", "300", "4",          "0", "1",      "1", "0", "0",
             "75",         "150", "",  "",    "",           "",  "",       "",  "",  "",
             "",           "",    "",  "",    "",           "1", "L_HUMAN"},
        });
    }
    {
        test_dbf::DbfBuilder b{
            {"ATT_ID", 'C', 16},    {"NAME_TXT", 'C', 20},  {"DESC_TXT", 'C', 20},
            {"CLASS", 'N', 2},      {"SOURCE", 'N', 2},     {"REACH", 'N', 2},
            {"INITIATIVE", 'N', 3}, {"QTY_DAM", 'N', 4},    {"QTY_HEAL", 'N', 3},
            {"POWER", 'N', 3},      {"INFINITE", 'N', 1},   {"CRIT_HIT", 'N', 1},
            {"WARD1", 'C', 40},     {"WARD2", 'C', 40},     {"WARD3", 'C', 40},
            {"WARD4", 'C', 40},     {"ALT_ATTACK", 'C', 16}};
        b.add_record({"G_HL_AN", "", "", "13", "1", "2", "40", "0", "30", "0", "1", "0", "", "", "",
                      "", ""});
        b.add_record({"G_HL_AL", "", "", "13", "1", "1", "40", "0", "30", "0", "1", "0", "", "", "",
                      "", ""});
        b.add_record(
            {"G_CR_AN", "", "", "23", "1", "2", "40", "0", "0", "0", "1", "0", "", "", "", "", ""});
        b.add_record(
            {"G_CR_AL", "", "", "23", "1", "1", "40", "0", "0", "0", "1", "0", "", "", "", "", ""});
        b.add_record({"G_RV_AN", "", "", "21", "1", "2", "40", "0", "60", "0", "1", "0", "", "", "",
                      "", ""});
        b.add_record({"G_RV_AL", "", "", "21", "1", "1", "40", "0", "60", "0", "1", "0", "", "", "",
                      "", ""});
        b.write(tmp / "Gattacks.dbf");
    }
    return tmp;
}

d2engine::GameDataRegistry make_hcr_gd() {
    return d2engine::GameDataRegistry(write_hcr_gd());
}

d2battle::BattleState make_heal_state(const d2engine::GameDataRegistry& gd) {
    d2battle::BattleState state;
    state.status = d2battle::BattleStatus::InProgress;

    state.party1.source_stack_id = "S1";
    state.party1.owner = "P1";
    state.party1.leader_id = "U0";
    state.party1.leader_alive = 1;
    state.party1.members = {
        std::optional<std::string>{"U0"}, {"U1"}, {"U2"}, {"U3"}, {"U4"}, std::nullopt};
    state.party1.cell_members = {0, 1, 2, 3, 4, -1};
    state.party1.positions = {0, 1, 2, 3, 4, -1};

    state.party2.source_stack_id = "S2";
    state.party2.owner = "P2";
    state.party2.leader_id = "E0";
    state.party2.leader_alive = 1;
    state.party2.members = {std::optional<std::string>{"E0"},
                            std::nullopt,
                            std::nullopt,
                            std::nullopt,
                            std::nullopt,
                            std::nullopt};
    state.party2.cell_members = {0, -1, -1, -1, -1, -1};
    state.party2.positions = {0, -1, -1, -1, -1, -1};

    d2battle::BattleUnitState actor;
    actor.id = "U0";
    actor.type_id = "G000UH0100";
    actor.side = d2battle::BattleSide::Party1;
    actor.member_index = 0;
    actor.current_hp = 150;
    actor.alive = true;
    actor.formation_cell = 0;
    state.units.push_back(actor);

    d2battle::BattleUnitState a;
    a.id = "U1";
    a.type_id = "G000UH0103";
    a.side = d2battle::BattleSide::Party1;
    a.member_index = 1;
    a.current_hp = 40;
    a.alive = true;
    a.formation_cell = 1;
    state.units.push_back(a);

    d2battle::BattleUnitState b;
    b.id = "U2";
    b.type_id = "G000UH0103";
    b.side = d2battle::BattleSide::Party1;
    b.member_index = 2;
    b.current_hp = 100;
    b.alive = true;
    b.formation_cell = 2;
    state.units.push_back(b);

    d2battle::BattleUnitState c;
    c.id = "U3";
    c.type_id = "G000UH0104";
    c.side = d2battle::BattleSide::Party1;
    c.member_index = 3;
    c.current_hp = 150;
    c.alive = true;
    c.formation_cell = 3;
    state.units.push_back(c);

    d2battle::BattleUnitState d;
    d.id = "U4";
    d.type_id = "G000UH0103";
    d.side = d2battle::BattleSide::Party1;
    d.member_index = 4;
    d.current_hp = 0;
    d.alive = false;
    d.formation_cell = 4;
    state.units.push_back(d);

    d2battle::BattleUnitState e;
    e.id = "E0";
    e.type_id = "G000UH0103";
    e.side = d2battle::BattleSide::Party2;
    e.member_index = 0;
    e.current_hp = 100;
    e.alive = true;
    e.formation_cell = 0;
    state.units.push_back(e);

    d2battle::detail::normalize_derived_side_state(state);
    d2battle::detail::normalize_battle_status(state);
    d2battle::detail::begin_round(state, 1, gd);
    return state;
}

d2battle::BattleState make_cure_state(const d2engine::GameDataRegistry& gd) {
    d2battle::BattleState state;
    state.status = d2battle::BattleStatus::InProgress;

    state.party1.source_stack_id = "S1";
    state.party1.owner = "P1";
    state.party1.leader_id = "U0";
    state.party1.leader_alive = 1;
    state.party1.members = {
        std::optional<std::string>{"U0"}, {"U1"}, {"U2"}, {"U3"}, {"U4"}, std::nullopt};
    state.party1.cell_members = {0, 1, 2, 3, 4, -1};
    state.party1.positions = {0, 1, 2, 3, 4, -1};

    state.party2.source_stack_id = "S2";
    state.party2.owner = "P2";
    state.party2.leader_id = "E0";
    state.party2.leader_alive = 1;
    state.party2.members = {std::optional<std::string>{"E0"},
                            std::nullopt,
                            std::nullopt,
                            std::nullopt,
                            std::nullopt,
                            std::nullopt};
    state.party2.cell_members = {0, -1, -1, -1, -1, -1};
    state.party2.positions = {0, -1, -1, -1, -1, -1};

    d2battle::BattleUnitState actor;
    actor.id = "U0";
    actor.type_id = "G000UH0101";
    actor.side = d2battle::BattleSide::Party1;
    actor.member_index = 0;
    actor.current_hp = 200;
    actor.alive = true;
    actor.formation_cell = 0;
    state.units.push_back(actor);

    d2battle::BattleUnitState a;
    a.id = "U1";
    a.type_id = "G000UH0103";
    a.side = d2battle::BattleSide::Party1;
    a.member_index = 1;
    a.current_hp = 80;
    a.alive = true;
    a.formation_cell = 1;
    a.effects.emplace_back(d2battle::PetrifiedEffect{"E0", "G_PET_ATK", 1});
    state.units.push_back(a);

    d2battle::BattleUnitState b;
    b.id = "U2";
    b.type_id = "G000UH0103";
    b.side = d2battle::BattleSide::Party1;
    b.member_index = 2;
    b.current_hp = 100;
    b.alive = true;
    b.formation_cell = 2;
    state.units.push_back(b);

    d2battle::BattleUnitState c;
    c.id = "U3";
    c.type_id = "G000UH0104";
    c.side = d2battle::BattleSide::Party1;
    c.member_index = 3;
    c.current_hp = 300;
    c.alive = true;
    c.formation_cell = 3;
    c.effects.emplace_back(d2battle::PetrifiedEffect{"E0", "G_PET_ATK", 2});
    state.units.push_back(c);

    d2battle::BattleUnitState d;
    d.id = "U4";
    d.type_id = "G000UH0103";
    d.side = d2battle::BattleSide::Party1;
    d.member_index = 4;
    d.current_hp = 0;
    d.alive = false;
    d.formation_cell = 4;
    state.units.push_back(d);

    d2battle::BattleUnitState e;
    e.id = "E0";
    e.type_id = "G000UH0103";
    e.side = d2battle::BattleSide::Party2;
    e.member_index = 0;
    e.current_hp = 80;
    e.alive = true;
    e.formation_cell = 0;
    e.effects.emplace_back(d2battle::PetrifiedEffect{"U0", "G_PET_ATK", 1});
    state.units.push_back(e);

    d2battle::detail::normalize_derived_side_state(state);
    d2battle::detail::normalize_battle_status(state);
    d2battle::detail::begin_round(state, 1, gd);
    return state;
}

d2battle::BattleState make_revive_state(const d2engine::GameDataRegistry& gd) {
    d2battle::BattleState state;
    state.status = d2battle::BattleStatus::InProgress;

    state.party1.source_stack_id = "S1";
    state.party1.owner = "P1";
    state.party1.leader_id = "U0";
    state.party1.leader_alive = 1;
    state.party1.members = {
        std::optional<std::string>{"U0"}, {"U1"}, {"U2"}, {"U3"}, {"U4"}, std::nullopt};
    state.party1.cell_members = {0, 1, 2, 3, 4, -1};
    state.party1.positions = {0, 1, 2, 3, 4, -1};

    state.party2.source_stack_id = "S2";
    state.party2.owner = "P2";
    state.party2.leader_id = "E0";
    state.party2.leader_alive = 1;
    state.party2.members = {std::optional<std::string>{"E0"},
                            {"E1"},
                            std::nullopt,
                            std::nullopt,
                            std::nullopt,
                            std::nullopt};
    state.party2.cell_members = {0, 1, -1, -1, -1, -1};
    state.party2.positions = {0, 1, -1, -1, -1, -1};

    d2battle::BattleUnitState actor;
    actor.id = "U0";
    actor.type_id = "G000UH0102";
    actor.side = d2battle::BattleSide::Party1;
    actor.member_index = 0;
    actor.current_hp = 200;
    actor.alive = true;
    actor.formation_cell = 0;
    state.units.push_back(actor);

    d2battle::BattleUnitState a;
    a.id = "U1";
    a.type_id = "G000UH0103";
    a.side = d2battle::BattleSide::Party1;
    a.member_index = 1;
    a.current_hp = 80;
    a.alive = true;
    a.formation_cell = 1;
    state.units.push_back(a);

    d2battle::BattleUnitState b;
    b.id = "U2";
    b.type_id = "G000UH0103";
    b.side = d2battle::BattleSide::Party1;
    b.member_index = 2;
    b.current_hp = 0;
    b.alive = false;
    b.formation_cell = 2;
    state.units.push_back(b);

    d2battle::BattleUnitState c;
    c.id = "U3";
    c.type_id = "G000UH0104";
    c.side = d2battle::BattleSide::Party1;
    c.member_index = 3;
    c.current_hp = 0;
    c.alive = false;
    c.formation_cell = 3;
    state.units.push_back(c);

    d2battle::BattleUnitState d;
    d.id = "U4";
    d.type_id = "G000UH0103";
    d.side = d2battle::BattleSide::Party1;
    d.member_index = 4;
    d.current_hp = 0;
    d.alive = false;
    d.formation_cell = 4;
    state.units.push_back(d);

    d2battle::BattleUnitState ed;
    ed.id = "E0";
    ed.type_id = "G000UH0103";
    ed.side = d2battle::BattleSide::Party2;
    ed.member_index = 0;
    ed.current_hp = 0;
    ed.alive = false;
    ed.formation_cell = 0;
    state.units.push_back(ed);

    d2battle::BattleUnitState ea;
    ea.id = "E1";
    ea.type_id = "G000UH0103";
    ea.side = d2battle::BattleSide::Party2;
    ea.member_index = 1;
    ea.current_hp = 100;
    ea.alive = true;
    ea.formation_cell = 1;
    state.units.push_back(ea);

    d2battle::detail::normalize_derived_side_state(state);
    d2battle::detail::normalize_battle_status(state);
    d2battle::detail::begin_round(state, 1, gd);
    return state;
}

// ════════════════════════════════════════════════════════
// Heal terminal tests
// ════════════════════════════════════════════════════════

TEST(BattlePreviewTest, HealPreviewShowsActualHealHeadingExactlyOnce) {
    auto gd = make_hcr_gd();
    auto state = make_heal_state(gd);
    auto outcomes = d2battle::valid_action_outcomes(state, gd);
    ASSERT_GE(outcomes.size(), 1u);
    std::ostringstream oss;
    print_actions_menu(oss, state, outcomes, gd);
    std::string output = oss.str();
    EXPECT_EQ(count_occurrences(output, "HEAL:\n"), 3u);
    EXPECT_EQ(count_occurrences(output, "REVIVED:\n"), 0u);
    EXPECT_EQ(count_occurrences(output, "EFFECTS REMOVED:\n"), 0u);
}

TEST(BattleSelectedTest, HealSelectedActionShowsActualHealHeadingExactlyOnce) {
    auto gd = make_hcr_gd();
    auto state = make_heal_state(gd);
    auto outcomes = d2battle::valid_action_outcomes(state, gd);
    ASSERT_GE(outcomes.size(), 1u);
    const d2battle::BattleActionOutcome* found = nullptr;
    for (const auto& o : outcomes) {
        if (auto* atk = std::get_if<d2battle::AttackAction>(&o.action)) {
            const auto* actor = state.find_unit(atk->actor_id);
            if (actor && actor->type_id == "G000UH0100") {
                found = &o;
                break;
            }
        }
    }
    ASSERT_NE(found, nullptr);
    std::ostringstream oss;
    print_selected_action(oss, state, found->action, found->outcome, gd);
    std::string output = oss.str();
    EXPECT_EQ(count_occurrences(output, "HEAL:\n"), 1u);
    EXPECT_EQ(count_occurrences(output, "REVIVED:\n"), 0u);
}

TEST(BattlePreviewTest, HealNoOpShowsNoFakeHealDelta) {
    auto                  gd = make_hcr_gd();
    d2battle::BattleState state;
    state.status = d2battle::BattleStatus::InProgress;
    state.party1 = {};
    state.party1.source_stack_id = "S1";
    state.party1.owner = "P1";
    state.party1.leader_id = "U0";
    state.party1.leader_alive = 1;
    state.party1.members = {std::optional<std::string>{"U0"},
                            std::nullopt,
                            std::nullopt,
                            std::nullopt,
                            std::nullopt,
                            std::nullopt};
    state.party1.cell_members = {0, -1, -1, -1, -1, -1};
    state.party1.positions = {0, -1, -1, -1, -1, -1};
    state.party2 = {};
    state.party2.source_stack_id = "S2";
    state.party2.owner = "P2";
    state.party2.leader_id = "E0";
    state.party2.leader_alive = 1;
    state.party2.members = {std::optional<std::string>{"E0"},
                            std::nullopt,
                            std::nullopt,
                            std::nullopt,
                            std::nullopt,
                            std::nullopt};
    state.party2.cell_members = {0, -1, -1, -1, -1, -1};
    state.party2.positions = {0, -1, -1, -1, -1, -1};
    d2battle::BattleUnitState actor;
    actor.id = "U0";
    actor.type_id = "G000UH0100";
    actor.side = d2battle::BattleSide::Party1;
    actor.member_index = 0;
    actor.current_hp = 200;
    actor.alive = true;
    actor.formation_cell = 0;
    state.units.push_back(actor);
    d2battle::BattleUnitState enemy;
    enemy.id = "E0";
    enemy.type_id = "G000UH0103";
    enemy.side = d2battle::BattleSide::Party2;
    enemy.member_index = 0;
    enemy.current_hp = 100;
    enemy.alive = true;
    enemy.formation_cell = 0;
    state.units.push_back(enemy);
    d2battle::detail::normalize_derived_side_state(state);
    d2battle::detail::normalize_battle_status(state);
    d2battle::detail::begin_round(state, 1, gd);
    auto outcomes = d2battle::valid_action_outcomes(state, gd);
    ASSERT_GE(outcomes.size(), 1u);
    std::ostringstream oss;
    print_actions_menu(oss, state, outcomes, gd);
    std::string output = oss.str();
    EXPECT_EQ(count_occurrences(output, "HEAL:\n"), 0u);
}

// ════════════════════════════════════════════════════════
// Heal targeting fixture
// ════════════════════════════════════════════════════════

TEST(HealTargetingFixture, HealAnyTargetsEveryAliveAllyIncludingActor) {
    auto                  gd = make_hcr_gd();
    auto                  state = make_heal_state(gd);
    auto                  actions = d2battle::detail::valid_actions_on_valid_state(state, gd);
    std::set<std::string> heal_targets;
    for (const auto& a : actions) {
        auto* atk = std::get_if<d2battle::AttackAction>(&a);
        if (!atk)
            continue;
        auto* ut = std::get_if<d2battle::UnitTarget>(&atk->target);
        if (!ut)
            continue;
        const auto* actor = state.find_unit(atk->actor_id);
        if (!actor || actor->type_id != "G000UH0100")
            continue;
        heal_targets.insert(ut->unit_id);
    }
    EXPECT_TRUE(heal_targets.contains("U0"));
    EXPECT_TRUE(heal_targets.contains("U1"));
    EXPECT_TRUE(heal_targets.contains("U2"));
    EXPECT_TRUE(heal_targets.contains("U3"));
    EXPECT_FALSE(heal_targets.contains("U4"));
    EXPECT_FALSE(heal_targets.contains("E0"));
    EXPECT_EQ(heal_targets.size(), 4u);
}

TEST(HealTargetingFixture, HealAnyRejectsEnemyTargetWithHostileTarget) {
    auto                   gd = make_hcr_gd();
    auto                   state = make_heal_state(gd);
    d2battle::AttackAction enemy_heal{"U0", d2battle::UnitTarget{"E0"}};
    auto err = d2battle::detail::validate_action_on_valid_state(d2battle::BattleAction{enemy_heal},
                                                                state, gd);
    EXPECT_EQ(err, d2battle::ActionValidationError::HostileTarget);
}

TEST(HealTargetingFixture, HealAnyRejectsDeadAllyWithTargetDead) {
    auto                   gd = make_hcr_gd();
    auto                   state = make_heal_state(gd);
    d2battle::AttackAction dead_heal{"U0", d2battle::UnitTarget{"U4"}};
    auto err = d2battle::detail::validate_action_on_valid_state(d2battle::BattleAction{dead_heal},
                                                                state, gd);
    EXPECT_EQ(err, d2battle::ActionValidationError::NoEligibleTargets);
}

TEST(HealTargetingFixture, HealDoesNotReviveDeadAlly) {
    auto                   gd = make_hcr_gd();
    auto                   state = make_heal_state(gd);
    d2battle::AttackAction dead_heal{"U0", d2battle::UnitTarget{"U4"}};
    auto err = d2battle::detail::validate_action_on_valid_state(d2battle::BattleAction{dead_heal},
                                                                state, gd);
    EXPECT_EQ(err, d2battle::ActionValidationError::NoEligibleTargets);
    auto* u4 = state.find_unit("U4");
    ASSERT_NE(u4, nullptr);
    EXPECT_FALSE(u4->alive);
}

TEST(HealTargetingFixture, HealFullHpTargetRemainsValidNoOp) {
    auto                   gd = make_hcr_gd();
    auto                   state = make_heal_state(gd);
    d2battle::AttackAction full_heal{"U0", d2battle::UnitTarget{"U2"}};
    auto err = d2battle::detail::validate_action_on_valid_state(d2battle::BattleAction{full_heal},
                                                                state, gd);
    EXPECT_EQ(err, d2battle::ActionValidationError::None);
    auto ctx = d2battle::detail::resolve_validated_attack_bundle_context(state, full_heal, gd);
    auto before_hp = state.find_unit("U2")->current_hp;
    d2battle::detail::resolve_heal_effect(state, ctx.primary, gd);
    EXPECT_EQ(state.find_unit("U2")->current_hp, before_hp);
}

TEST(HealTargetingFixture, HealAdjacentIsUnsupported) {
    auto rule = d2battle::detail::attack_rule_for_class(d2engine::AttackClass::Heal);
    ASSERT_TRUE(rule.has_value());
    EXPECT_FALSE(d2battle::detail::is_reach_supported(*rule, d2engine::AttackReach::Adjacent));
}

TEST(HealTargetingFixture, HealAllEmitsSingleAllAlliedUnitsAction) {
    auto gd = make_hcr_gd();
    auto state = make_heal_state(gd);
    state.find_unit("U0")->type_id = "G000UH0105";
    state.round_state = {};
    state.status = d2battle::BattleStatus::InProgress;
    d2battle::detail::normalize_derived_side_state(state);
    d2battle::detail::normalize_battle_status(state);
    d2battle::detail::begin_round(state, 1, gd);
    auto actions = d2battle::detail::valid_actions_on_valid_state(state, gd);
    int  all_count = 0;
    for (const auto& a : actions) {
        if (auto* atk = std::get_if<d2battle::AttackAction>(&a)) {
            if (std::holds_alternative<d2battle::AllAlliedUnitsTarget>(atk->target)) {
                ++all_count;
            }
        }
    }
    EXPECT_EQ(all_count, 1);
}

TEST(HealTargetingFixture, HealAllHealsEveryAliveAllyIndependently) {
    auto gd = make_hcr_gd();
    auto state = make_heal_state(gd);
    state.find_unit("U0")->type_id = "G000UH0105";
    state.round_state = {};
    state.status = d2battle::BattleStatus::InProgress;
    d2battle::detail::normalize_derived_side_state(state);
    d2battle::detail::normalize_battle_status(state);
    d2battle::detail::begin_round(state, 1, gd);
    d2battle::AttackAction all_heal{"U0", d2battle::AllAlliedUnitsTarget{}};
    auto err = d2battle::detail::validate_action_on_valid_state(d2battle::BattleAction{all_heal},
                                                                state, gd);
    ASSERT_EQ(err, d2battle::ActionValidationError::None);
    auto ctx = d2battle::detail::resolve_validated_attack_bundle_context(state, all_heal, gd);
    ASSERT_EQ(ctx.primary.target_unit_ids.size(), 4u);
    EXPECT_TRUE(std::find(ctx.primary.target_unit_ids.begin(), ctx.primary.target_unit_ids.end(),
                          "U0") != ctx.primary.target_unit_ids.end());
    EXPECT_TRUE(std::find(ctx.primary.target_unit_ids.begin(), ctx.primary.target_unit_ids.end(),
                          "U1") != ctx.primary.target_unit_ids.end());
    EXPECT_TRUE(std::find(ctx.primary.target_unit_ids.begin(), ctx.primary.target_unit_ids.end(),
                          "U2") != ctx.primary.target_unit_ids.end());
    EXPECT_TRUE(std::find(ctx.primary.target_unit_ids.begin(), ctx.primary.target_unit_ids.end(),
                          "U3") != ctx.primary.target_unit_ids.end());
    EXPECT_FALSE(std::find(ctx.primary.target_unit_ids.begin(), ctx.primary.target_unit_ids.end(),
                           "U4") != ctx.primary.target_unit_ids.end());
    EXPECT_FALSE(std::find(ctx.primary.target_unit_ids.begin(), ctx.primary.target_unit_ids.end(),
                           "E0") != ctx.primary.target_unit_ids.end());
}

TEST(HealTargetingFixture, HealAllCapsEachTargetAtOwnMaximumHp) {
    auto gd = make_hcr_gd();
    auto state = make_heal_state(gd);
    state.find_unit("U0")->type_id = "G000UH0105";
    state.round_state = {};
    state.status = d2battle::BattleStatus::InProgress;
    d2battle::detail::normalize_derived_side_state(state);
    d2battle::detail::normalize_battle_status(state);
    d2battle::detail::begin_round(state, 1, gd);
    d2battle::AttackAction all_heal{"U0", d2battle::AllAlliedUnitsTarget{}};
    d2battle::detail::resolve_heal_effect(
        state,
        d2battle::detail::resolve_validated_attack_bundle_context(state, all_heal, gd).primary, gd);
    auto* u0 = state.find_unit("U0");
    ASSERT_NE(u0, nullptr);
    EXPECT_EQ(u0->current_hp, 180);
    auto* u1 = state.find_unit("U1");
    ASSERT_NE(u1, nullptr);
    EXPECT_EQ(u1->current_hp, 70);
    auto* u2 = state.find_unit("U2");
    ASSERT_NE(u2, nullptr);
    EXPECT_EQ(u2->current_hp, 100);
    auto* u3 = state.find_unit("U3");
    ASSERT_NE(u3, nullptr);
    EXPECT_EQ(u3->current_hp, 180);
    auto* u4 = state.find_unit("U4");
    ASSERT_NE(u4, nullptr);
    EXPECT_EQ(u4->current_hp, 0);
    EXPECT_FALSE(u4->alive);
}

TEST(HealTargetingFixture, HealAllProcessesLargeAllyExactlyOnce) {
    auto gd = make_hcr_gd();
    auto state = make_heal_state(gd);
    state.find_unit("U0")->type_id = "G000UH0105";
    state.round_state = {};
    state.status = d2battle::BattleStatus::InProgress;
    d2battle::detail::normalize_derived_side_state(state);
    d2battle::detail::normalize_battle_status(state);
    d2battle::detail::begin_round(state, 1, gd);
    d2battle::AttackAction all_heal{"U0", d2battle::AllAlliedUnitsTarget{}};
    d2battle::detail::resolve_heal_effect(
        state,
        d2battle::detail::resolve_validated_attack_bundle_context(state, all_heal, gd).primary, gd);
    auto* u3 = state.find_unit("U3");
    ASSERT_NE(u3, nullptr);
    EXPECT_EQ(u3->current_hp, 180);
}

// ════════════════════════════════════════════════════════
// Cure targeting/effect fixture
// ════════════════════════════════════════════════════════

TEST(CureTargetingFixture, CureAnyTargetsEveryAliveAllyIncludingActor) {
    auto                  gd = make_hcr_gd();
    auto                  state = make_cure_state(gd);
    auto                  actions = d2battle::detail::valid_actions_on_valid_state(state, gd);
    std::set<std::string> cure_targets;
    for (const auto& a : actions) {
        auto* atk = std::get_if<d2battle::AttackAction>(&a);
        if (!atk)
            continue;
        auto* ut = std::get_if<d2battle::UnitTarget>(&atk->target);
        if (!ut)
            continue;
        const auto* actor = state.find_unit(atk->actor_id);
        if (!actor || actor->type_id != "G000UH0101")
            continue;
        cure_targets.insert(ut->unit_id);
    }
    EXPECT_TRUE(cure_targets.contains("U0"));
    EXPECT_TRUE(cure_targets.contains("U1"));
    EXPECT_TRUE(cure_targets.contains("U2"));
    EXPECT_TRUE(cure_targets.contains("U3"));
    EXPECT_FALSE(cure_targets.contains("U4"));
    EXPECT_FALSE(cure_targets.contains("E0"));
    EXPECT_EQ(cure_targets.size(), 4u);
}

TEST(CureTargetingFixture, CureAnyRejectsEnemyTargetWithHostileTarget) {
    auto                   gd = make_hcr_gd();
    auto                   state = make_cure_state(gd);
    d2battle::AttackAction enemy_cure{"U0", d2battle::UnitTarget{"E0"}};
    auto err = d2battle::detail::validate_action_on_valid_state(d2battle::BattleAction{enemy_cure},
                                                                state, gd);
    EXPECT_EQ(err, d2battle::ActionValidationError::HostileTarget);
}

TEST(CureTargetingFixture, CureAnyRejectsDeadAllyWithTargetDead) {
    auto                   gd = make_hcr_gd();
    auto                   state = make_cure_state(gd);
    d2battle::AttackAction dead_cure{"U0", d2battle::UnitTarget{"U4"}};
    auto err = d2battle::detail::validate_action_on_valid_state(d2battle::BattleAction{dead_cure},
                                                                state, gd);
    EXPECT_EQ(err, d2battle::ActionValidationError::NoEligibleTargets);
}

TEST(CureTargetingFixture, CureAllEmitsSingleAllAlliedUnitsAction) {
    auto gd = make_hcr_gd();
    auto state = make_cure_state(gd);
    state.find_unit("U0")->type_id = "G000UH0106";
    state.round_state = {};
    state.status = d2battle::BattleStatus::InProgress;
    d2battle::detail::normalize_derived_side_state(state);
    d2battle::detail::normalize_battle_status(state);
    d2battle::detail::begin_round(state, 1, gd);
    auto actions = d2battle::detail::valid_actions_on_valid_state(state, gd);
    int  all_count = 0;
    for (const auto& a : actions) {
        if (auto* atk = std::get_if<d2battle::AttackAction>(&a)) {
            if (std::holds_alternative<d2battle::AllAlliedUnitsTarget>(atk->target)) {
                ++all_count;
            }
        }
    }
    EXPECT_EQ(all_count, 1);
}

TEST(CureTargetingFixture, CureAllRemovesPetrifiedFromEveryAffectedAlly) {
    auto gd = make_hcr_gd();
    auto state = make_cure_state(gd);
    state.find_unit("U0")->type_id = "G000UH0106";
    state.round_state = {};
    state.status = d2battle::BattleStatus::InProgress;
    d2battle::detail::normalize_derived_side_state(state);
    d2battle::detail::normalize_battle_status(state);
    d2battle::detail::begin_round(state, 1, gd);
    d2battle::AttackAction all_cure{"U0", d2battle::AllAlliedUnitsTarget{}};
    auto err = d2battle::detail::validate_action_on_valid_state(d2battle::BattleAction{all_cure},
                                                                state, gd);
    ASSERT_EQ(err, d2battle::ActionValidationError::None);
    auto ctx = d2battle::detail::resolve_validated_attack_bundle_context(state, all_cure, gd);
    d2battle::detail::resolve_cure_effect(state, ctx.primary);
    auto* u1 = state.find_unit("U1");
    ASSERT_NE(u1, nullptr);
    EXPECT_EQ(u1->effects.size(), 0u);
    auto* u2 = state.find_unit("U2");
    ASSERT_NE(u2, nullptr);
    EXPECT_EQ(u2->effects.size(), 0u);
    auto* u3 = state.find_unit("U3");
    ASSERT_NE(u3, nullptr);
    EXPECT_EQ(u3->effects.size(), 0u);
    auto* u4 = state.find_unit("U4");
    ASSERT_NE(u4, nullptr);
    EXPECT_FALSE(u4->alive);
    auto* e0 = state.find_unit("E0");
    ASSERT_NE(e0, nullptr);
    EXPECT_EQ(e0->effects.size(), 1u);
}

TEST(CureTargetingFixture, CureAllProcessesLargeAllyExactlyOnce) {
    auto gd = make_hcr_gd();
    auto state = make_cure_state(gd);
    state.find_unit("U0")->type_id = "G000UH0106";
    state.round_state = {};
    state.status = d2battle::BattleStatus::InProgress;
    d2battle::detail::normalize_derived_side_state(state);
    d2battle::detail::normalize_battle_status(state);
    d2battle::detail::begin_round(state, 1, gd);
    d2battle::AttackAction all_cure{"U0", d2battle::AllAlliedUnitsTarget{}};
    auto ctx = d2battle::detail::resolve_validated_attack_bundle_context(state, all_cure, gd);
    EXPECT_EQ(
        std::count(ctx.primary.target_unit_ids.begin(), ctx.primary.target_unit_ids.end(), "U3"),
        1);
}

TEST(CureTargetingFixture, CureCleanTargetsRemainValidNoOp) {
    auto                   gd = make_hcr_gd();
    auto                   state = make_cure_state(gd);
    d2battle::AttackAction clean_cure{"U0", d2battle::UnitTarget{"U2"}};
    auto err = d2battle::detail::validate_action_on_valid_state(d2battle::BattleAction{clean_cure},
                                                                state, gd);
    EXPECT_EQ(err, d2battle::ActionValidationError::None);
}

TEST(CureTargetingFixture, CureDoesNotChangeHp) {
    auto gd = make_hcr_gd();
    auto state = make_cure_state(gd);
    state.find_unit("U0")->type_id = "G000UH0106";
    state.round_state = {};
    state.status = d2battle::BattleStatus::InProgress;
    d2battle::detail::normalize_derived_side_state(state);
    d2battle::detail::normalize_battle_status(state);
    d2battle::detail::begin_round(state, 1, gd);
    d2battle::AttackAction all_cure{"U0", d2battle::AllAlliedUnitsTarget{}};
    auto                   before_hp = std::vector<int>();
    for (const auto& u : state.units)
        before_hp.push_back(u.current_hp);
    auto ctx = d2battle::detail::resolve_validated_attack_bundle_context(state, all_cure, gd);
    d2battle::detail::resolve_cure_effect(state, ctx.primary);
    for (std::size_t i = 0; i < state.units.size(); ++i)
        EXPECT_EQ(state.units[i].current_hp, before_hp[i]);
}

TEST(CureTargetingFixture, CureDoesNotRemoveEnemyEffects) {
    auto gd = make_hcr_gd();
    auto state = make_cure_state(gd);
    state.find_unit("U0")->type_id = "G000UH0106";
    state.round_state = {};
    state.status = d2battle::BattleStatus::InProgress;
    d2battle::detail::normalize_derived_side_state(state);
    d2battle::detail::normalize_battle_status(state);
    d2battle::detail::begin_round(state, 1, gd);
    d2battle::AttackAction all_cure{"U0", d2battle::AllAlliedUnitsTarget{}};
    auto ctx = d2battle::detail::resolve_validated_attack_bundle_context(state, all_cure, gd);
    d2battle::detail::resolve_cure_effect(state, ctx.primary);
    auto* e0 = state.find_unit("E0");
    ASSERT_NE(e0, nullptr);
    EXPECT_TRUE(d2battle::detail::is_petrified(*e0));
}

TEST(CureTargetingFixture, CureAdjacentIsUnsupported) {
    auto rule = d2battle::detail::attack_rule_for_class(d2engine::AttackClass::Cure);
    ASSERT_TRUE(rule.has_value());
    EXPECT_FALSE(d2battle::detail::is_reach_supported(*rule, d2engine::AttackReach::Adjacent));
}

// ════════════════════════════════════════════════════════
// Revive targeting fixture
// ════════════════════════════════════════════════════════

TEST(ReviveTargetingFixture, ReviveAnyEnumeratesDeadAlliesOnly) {
    auto                  gd = make_hcr_gd();
    auto                  state = make_revive_state(gd);
    auto                  actions = d2battle::detail::valid_actions_on_valid_state(state, gd);
    std::set<std::string> revive_targets;
    for (const auto& a : actions) {
        auto* atk = std::get_if<d2battle::AttackAction>(&a);
        if (!atk)
            continue;
        auto* ut = std::get_if<d2battle::UnitTarget>(&atk->target);
        if (!ut)
            continue;
        const auto* actor = state.find_unit(atk->actor_id);
        if (!actor || actor->type_id != "G000UH0102")
            continue;
        revive_targets.insert(ut->unit_id);
    }
    EXPECT_FALSE(revive_targets.contains("U0"));
    EXPECT_FALSE(revive_targets.contains("U1"));
    EXPECT_TRUE(revive_targets.contains("U2"));
    EXPECT_TRUE(revive_targets.contains("U3"));
    EXPECT_TRUE(revive_targets.contains("U4"));
    EXPECT_FALSE(revive_targets.contains("E0"));
    EXPECT_FALSE(revive_targets.contains("E1"));
    EXPECT_EQ(revive_targets.size(), 3u);
}

TEST(ReviveTargetingFixture, ReviveRejectsEnemyTargetWithHostileTarget) {
    auto                   gd = make_hcr_gd();
    auto                   state = make_revive_state(gd);
    d2battle::AttackAction enemy_revive{"U0", d2battle::UnitTarget{"E0"}};
    auto                   err = d2battle::detail::validate_action_on_valid_state(
        d2battle::BattleAction{enemy_revive}, state, gd);
    EXPECT_EQ(err, d2battle::ActionValidationError::HostileTarget);
}

TEST(ReviveTargetingFixture, ReviveRejectsAliveAllyWithTargetAlive) {
    auto                   gd = make_hcr_gd();
    auto                   state = make_revive_state(gd);
    d2battle::AttackAction alive_revive{"U0", d2battle::UnitTarget{"U1"}};
    auto                   err = d2battle::detail::validate_action_on_valid_state(
        d2battle::BattleAction{alive_revive}, state, gd);
    EXPECT_EQ(err, d2battle::ActionValidationError::NoEligibleTargets);
}

TEST(ReviveTargetingFixture, ReviveAllEmitsSingleActionWhenDeadAlliesExist) {
    auto gd = make_hcr_gd();
    auto state = make_revive_state(gd);
    state.find_unit("U0")->type_id = "G000UH0107";
    state.round_state = {};
    state.status = d2battle::BattleStatus::InProgress;
    d2battle::detail::normalize_derived_side_state(state);
    d2battle::detail::normalize_battle_status(state);
    d2battle::detail::begin_round(state, 1, gd);
    auto actions = d2battle::detail::valid_actions_on_valid_state(state, gd);
    int  all_count = 0;
    for (const auto& a : actions) {
        if (auto* atk = std::get_if<d2battle::AttackAction>(&a)) {
            if (std::holds_alternative<d2battle::AllAlliedUnitsTarget>(atk->target)) {
                ++all_count;
            }
        }
    }
    EXPECT_EQ(all_count, 1);
}

TEST(ReviveTargetingFixture, ReviveAllEmitsNoActionWithoutDeadAllies) {
    auto                  gd = make_hcr_gd();
    d2battle::BattleState state;
    state.status = d2battle::BattleStatus::InProgress;
    state.party1 = {};
    state.party1.source_stack_id = "S1";
    state.party1.owner = "P1";
    state.party1.leader_id = "U0";
    state.party1.leader_alive = 1;
    state.party1.members = {std::optional<std::string>{"U0"},
                            std::nullopt,
                            std::nullopt,
                            std::nullopt,
                            std::nullopt,
                            std::nullopt};
    state.party1.cell_members = {0, -1, -1, -1, -1, -1};
    state.party1.positions = {0, -1, -1, -1, -1, -1};
    state.party2 = {};
    state.party2.source_stack_id = "S2";
    state.party2.owner = "P2";
    state.party2.leader_id = "E0";
    state.party2.leader_alive = 1;
    state.party2.members = {std::optional<std::string>{"E0"},
                            std::nullopt,
                            std::nullopt,
                            std::nullopt,
                            std::nullopt,
                            std::nullopt};
    state.party2.cell_members = {0, -1, -1, -1, -1, -1};
    state.party2.positions = {0, -1, -1, -1, -1, -1};
    d2battle::BattleUnitState actor;
    actor.id = "U0";
    actor.type_id = "G000UH0107";
    actor.side = d2battle::BattleSide::Party1;
    actor.member_index = 0;
    actor.current_hp = 200;
    actor.alive = true;
    actor.formation_cell = 0;
    state.units.push_back(actor);
    d2battle::BattleUnitState enemy;
    enemy.id = "E0";
    enemy.type_id = "G000UH0103";
    enemy.side = d2battle::BattleSide::Party2;
    enemy.member_index = 0;
    enemy.current_hp = 100;
    enemy.alive = true;
    enemy.formation_cell = 0;
    state.units.push_back(enemy);
    d2battle::detail::normalize_derived_side_state(state);
    d2battle::detail::normalize_battle_status(state);
    d2battle::detail::begin_round(state, 1, gd);
    auto actions = d2battle::detail::valid_actions_on_valid_state(state, gd);
    for (const auto& a : actions) {
        if (auto* atk = std::get_if<d2battle::AttackAction>(&a)) {
            EXPECT_FALSE(std::holds_alternative<d2battle::AllAlliedUnitsTarget>(atk->target))
                << "Revive All should not be emitted without dead allies";
        }
    }
}

TEST(ReviveTargetingFixture, CraftedReviveAllWithoutDeadAlliesReturnsNoEligibleTargets) {
    auto                  gd = make_hcr_gd();
    d2battle::BattleState state;
    state.status = d2battle::BattleStatus::InProgress;
    state.party1 = {};
    state.party1.source_stack_id = "S1";
    state.party1.members = {std::optional<std::string>{"U0"},
                            std::nullopt,
                            std::nullopt,
                            std::nullopt,
                            std::nullopt,
                            std::nullopt};
    state.party2.members = {std::optional<std::string>{"E0"},
                            std::nullopt,
                            std::nullopt,
                            std::nullopt,
                            std::nullopt,
                            std::nullopt};
    d2battle::BattleUnitState actor;
    actor.id = "U0";
    actor.type_id = "G000UH0107";
    actor.side = d2battle::BattleSide::Party1;
    actor.member_index = 0;
    actor.current_hp = 200;
    actor.alive = true;
    actor.formation_cell = 0;
    state.units.push_back(actor);
    d2battle::BattleUnitState enemy;
    enemy.id = "E0";
    enemy.type_id = "G000UH0103";
    enemy.side = d2battle::BattleSide::Party2;
    enemy.member_index = 0;
    enemy.current_hp = 100;
    enemy.alive = true;
    enemy.formation_cell = 0;
    state.units.push_back(enemy);
    d2battle::detail::normalize_derived_side_state(state);
    d2battle::detail::normalize_battle_status(state);
    d2battle::detail::begin_round(state, 1, gd);
    d2battle::AttackAction all_revive{"U0", d2battle::AllAlliedUnitsTarget{}};
    auto err = d2battle::detail::validate_action_on_valid_state(d2battle::BattleAction{all_revive},
                                                                state, gd);
    EXPECT_EQ(err, d2battle::ActionValidationError::NoEligibleTargets);
}

TEST(ReviveTargetingFixture, ReviveRestoresAttackHealHp) {
    auto                   gd = make_hcr_gd();
    auto                   state = make_revive_state(gd);
    d2battle::AttackAction revive_b{"U0", d2battle::UnitTarget{"U2"}};
    auto err = d2battle::detail::validate_action_on_valid_state(d2battle::BattleAction{revive_b},
                                                                state, gd);
    ASSERT_EQ(err, d2battle::ActionValidationError::None);
    auto ctx = d2battle::detail::resolve_validated_attack_bundle_context(state, revive_b, gd);
    d2battle::detail::resolve_revive_effect(state, ctx.primary, gd);
    auto* u2 = state.find_unit("U2");
    ASSERT_NE(u2, nullptr);
    EXPECT_TRUE(u2->alive);
    EXPECT_EQ(u2->current_hp, 60);
}

TEST(ReviveTargetingFixture, ReviveCapsHpAtTargetMaximum) {
    auto                   gd = make_hcr_gd();
    auto                   state = make_revive_state(gd);
    d2battle::AttackAction revive_c{"U0", d2battle::UnitTarget{"U3"}};
    auto ctx = d2battle::detail::resolve_validated_attack_bundle_context(state, revive_c, gd);
    d2battle::detail::resolve_revive_effect(state, ctx.primary, gd);
    auto* u3 = state.find_unit("U3");
    ASSERT_NE(u3, nullptr);
    EXPECT_TRUE(u3->alive);
    EXPECT_EQ(u3->current_hp, 60);
}

TEST(ReviveTargetingFixture, ReviveProcessesLargeUnitExactlyOnce) {
    auto gd = make_hcr_gd();
    auto state = make_revive_state(gd);
    state.find_unit("U0")->type_id = "G000UH0107";
    state.round_state = {};
    state.status = d2battle::BattleStatus::InProgress;
    d2battle::detail::normalize_derived_side_state(state);
    d2battle::detail::normalize_battle_status(state);
    d2battle::detail::begin_round(state, 1, gd);
    d2battle::AttackAction all_revive{"U0", d2battle::AllAlliedUnitsTarget{}};
    auto ctx = d2battle::detail::resolve_validated_attack_bundle_context(state, all_revive, gd);
    EXPECT_EQ(
        std::count(ctx.primary.target_unit_ids.begin(), ctx.primary.target_unit_ids.end(), "U3"),
        1);
}

TEST(ReviveTargetingFixture, RevivePreservesIdentityXpTypeAndFormation) {
    auto  gd = make_hcr_gd();
    auto  state = make_revive_state(gd);
    auto* b_before = state.find_unit("U2");
    ASSERT_NE(b_before, nullptr);
    std::string            orig_type = b_before->type_id;
    int                    orig_member = b_before->member_index;
    int                    orig_cell = b_before->formation_cell;
    auto                   orig_side = b_before->side;
    d2battle::AttackAction revive_b{"U0", d2battle::UnitTarget{"U2"}};
    auto ctx = d2battle::detail::resolve_validated_attack_bundle_context(state, revive_b, gd);
    d2battle::detail::resolve_revive_effect(state, ctx.primary, gd);
    auto* b_after = state.find_unit("U2");
    ASSERT_NE(b_after, nullptr);
    EXPECT_EQ(b_after->type_id, orig_type);
    EXPECT_EQ(b_after->member_index, orig_member);
    EXPECT_EQ(b_after->formation_cell, orig_cell);
    EXPECT_EQ(b_after->side, orig_side);
    EXPECT_EQ(b_after->effects.size(), 0u);
}

TEST(ReviveTargetingFixture, ReviveLeaderRestoresLeaderAliveFlag) {
    auto gd = make_hcr_gd();
    auto state = make_revive_state(gd);
    state.party1.leader_id = "U4";
    state.party1.leader_alive = 0;
    d2battle::AttackAction revive_d{"U0", d2battle::UnitTarget{"U4"}};
    auto ctx = d2battle::detail::resolve_validated_attack_bundle_context(state, revive_d, gd);
    d2battle::detail::resolve_revive_effect(state, ctx.primary, gd);
    EXPECT_EQ(state.party1.leader_alive, 1);
}

TEST(ReviveTargetingFixture, ReviveRequiresPositiveHealAmount) {
    auto                gd = make_hcr_gd();
    auto                state = make_revive_state(gd);
    d2engine::AttackDef zero_heal;
    zero_heal.heal = 0;
    d2battle::detail::ResolvedAttackContext bad_ctx{"U0", zero_heal, {"U2"}};
    EXPECT_THROW(d2battle::detail::resolve_revive_effect(state, bad_ctx, gd), std::runtime_error);
}

TEST(ReviveTargetingFixture, ReviveAdjacentIsUnsupported) {
    auto rule = d2battle::detail::attack_rule_for_class(d2engine::AttackClass::Revive);
    ASSERT_TRUE(rule.has_value());
    EXPECT_FALSE(d2battle::detail::is_reach_supported(*rule, d2engine::AttackReach::Adjacent));
}

// ════════════════════════════════════════════════════════
// Revive round order tests
// ════════════════════════════════════════════════════════

TEST(ReviveRoundOrder, RevivedUnitAbsentFromCurrentRoundOrderWaitsUntilNextRound) {
    auto                   gd = make_hcr_gd();
    auto                   state = make_revive_state(gd);
    d2battle::AttackAction revive_b{"U0", d2battle::UnitTarget{"U2"}};
    auto ctx = d2battle::detail::resolve_validated_attack_bundle_context(state, revive_b, gd);
    d2battle::detail::resolve_revive_effect(state, ctx.primary, gd);
    auto* u2 = state.find_unit("U2");
    ASSERT_NE(u2, nullptr);
    EXPECT_TRUE(u2->alive);
    for (const auto& entry : state.round_state.turn_order)
        EXPECT_NE(entry.unit_id, "U2") << "revived unit must not be in current turn_order";
}

TEST(ReviveRoundOrder, ReviveAdvancesCasterTurnExactlyOnce) {
    auto                   gd = make_hcr_gd();
    auto                   state = make_revive_state(gd);
    auto                   before_idx = state.round_state.current_turn_index;
    d2battle::AttackAction revive_b{"U0", d2battle::UnitTarget{"U2"}};
    auto                   next = d2battle::apply(state, d2battle::BattleAction{revive_b}, gd);
    EXPECT_EQ(next.round_state.current_turn_index, before_idx + 1);
}

// ════════════════════════════════════════════════════════
// Heal/Cure/Revive primitive tests
// ════════════════════════════════════════════════════════

TEST(HealPrimitiveTest, HealPrimitiveRejectsNegativeAmount) {
    auto                      gd = make_hcr_gd();
    d2battle::BattleState     state;
    d2battle::BattleUnitState u;
    u.id = "U0";
    u.type_id = "G000UH0103";
    u.alive = true;
    u.current_hp = 50;
    state.units.push_back(u);
    EXPECT_THROW(
        ([&] { return d2battle::detail::heal_alive_unit_up_to_max(state, "U0", -5, gd); }()),
        std::runtime_error);
}

TEST(HealPrimitiveTest, HealPrimitiveRejectsDeadUnit) {
    auto                      gd = make_hcr_gd();
    d2battle::BattleState     state;
    d2battle::BattleUnitState u;
    u.id = "U0";
    u.type_id = "G000UH0103";
    u.alive = false;
    u.current_hp = 0;
    state.units.push_back(u);
    EXPECT_THROW(
        ([&] { return d2battle::detail::heal_alive_unit_up_to_max(state, "U0", 10, gd); }()),
        std::runtime_error);
}

TEST(HealPrimitiveTest, HealPrimitiveAllowsFullHpNoOp) {
    auto                      gd = make_hcr_gd();
    d2battle::BattleState     state;
    d2battle::BattleUnitState u;
    u.id = "U0";
    u.type_id = "G000UH0103";
    u.alive = true;
    u.current_hp = 100;
    state.units.push_back(u);
    auto r = d2battle::detail::heal_alive_unit_up_to_max(state, "U0", 10, gd);
    EXPECT_EQ(r.applied, 0);
    EXPECT_EQ(r.unused, 10);
    EXPECT_EQ(u.current_hp, 100);
}

TEST(RevivePrimitiveTest, RevivePrimitiveRejectsZeroHpAmount) {
    auto                      gd = make_hcr_gd();
    d2battle::BattleState     state;
    d2battle::BattleUnitState u;
    u.id = "U0";
    u.type_id = "G000UH0103";
    u.alive = false;
    u.current_hp = 0;
    state.units.push_back(u);
    EXPECT_THROW(([&] { return d2battle::detail::revive_dead_unit_with_hp(state, "U0", 0, gd); }()),
                 std::runtime_error);
}

TEST(RevivePrimitiveTest, RevivePrimitiveRejectsAliveUnit) {
    auto                      gd = make_hcr_gd();
    d2battle::BattleState     state;
    d2battle::BattleUnitState u;
    u.id = "U0";
    u.type_id = "G000UH0103";
    u.alive = true;
    u.current_hp = 100;
    state.units.push_back(u);
    EXPECT_THROW(
        ([&] { return d2battle::detail::revive_dead_unit_with_hp(state, "U0", 10, gd); }()),
        std::runtime_error);
}

TEST(RevivePrimitiveTest, RevivePrimitiveRejectsNonzeroHpDeadUnit) {
    auto                      gd = make_hcr_gd();
    d2battle::BattleState     state;
    d2battle::BattleUnitState u;
    u.id = "U0";
    u.type_id = "G000UH0103";
    u.alive = false;
    u.current_hp = 30;
    state.units.push_back(u);
    EXPECT_THROW(
        ([&] { return d2battle::detail::revive_dead_unit_with_hp(state, "U0", 10, gd); }()),
        std::runtime_error);
}

TEST(RevivePrimitiveTest, RevivePrimitiveRejectsDeadUnitWithEffects) {
    auto                      gd = make_hcr_gd();
    d2battle::BattleState     state;
    d2battle::BattleUnitState u;
    u.id = "U0";
    u.type_id = "G000UH0103";
    u.alive = false;
    u.current_hp = 0;
    u.effects.emplace_back(d2battle::PetrifiedEffect{"X", "Y", 1});
    state.units.push_back(u);
    EXPECT_THROW(
        ([&] { return d2battle::detail::revive_dead_unit_with_hp(state, "U0", 10, gd); }()),
        std::runtime_error);
}

TEST(RevivePrimitiveTest, RevivePrimitiveCapsAtMaximumHp) {
    auto                      gd = make_hcr_gd();
    d2battle::BattleState     state;
    d2battle::BattleUnitState u;
    u.id = "U0";
    u.type_id = "G000UH0103";
    u.alive = false;
    u.current_hp = 0;
    state.units.push_back(u);
    auto r = d2battle::detail::revive_dead_unit_with_hp(state, "U0", 999, gd);
    EXPECT_EQ(r.applied, 100);
    EXPECT_EQ(r.hp_after, 100);
    auto* revived = state.find_unit("U0");
    ASSERT_NE(revived, nullptr);
    EXPECT_TRUE(revived->alive);
}

TEST(CurePrimitiveTest, CurePrimitiveRejectsDeadUnit) {
    d2battle::BattleUnitState u;
    u.id = "U0";
    u.alive = false;
    EXPECT_THROW(d2battle::detail::remove_curable_negative_effects(u), std::runtime_error);
}

TEST(CurePrimitiveTest, CurePrimitiveRemovesOnlyCurableNegativeEffects) {
    d2battle::BattleUnitState u;
    u.id = "U0";
    u.alive = true;
    u.effects.emplace_back(d2battle::PetrifiedEffect{"X", "Y", 1});
    auto removed = d2battle::detail::remove_curable_negative_effects(u);
    EXPECT_EQ(removed, 1u);
    EXPECT_EQ(u.effects.size(), 0u);
}

TEST(CurePrimitiveTest, CurePrimitiveAllowsCleanNoOp) {
    d2battle::BattleUnitState u;
    u.id = "U0";
    u.alive = true;
    auto removed = d2battle::detail::remove_curable_negative_effects(u);
    EXPECT_EQ(removed, 0u);
    EXPECT_EQ(u.effects.size(), 0u);
}

// ════════════════════════════════════════════════════════
// Pure branching tests
// ════════════════════════════════════════════════════════

TEST(HealCureReviveBranching, HealOutcomeDoesNotMutateAuthoritativeState) {
    auto gd = make_hcr_gd();
    auto state = make_heal_state(gd);
    auto before_fp = d2battle::compute_fingerprint(state);
    auto outcomes = d2battle::valid_action_outcomes(state, gd);
    ASSERT_FALSE(outcomes.empty());
    const d2battle::BattleActionOutcome* heal_outcome = nullptr;
    for (const auto& o : outcomes) {
        if (auto* atk = std::get_if<d2battle::AttackAction>(&o.action)) {
            const auto* actor = state.find_unit(atk->actor_id);
            if (actor && actor->type_id == "G000UH0100") {
                heal_outcome = &o;
                break;
            }
        }
    }
    ASSERT_NE(heal_outcome, nullptr);
    EXPECT_EQ(d2battle::compute_fingerprint(state), before_fp);
    auto after_fp = d2battle::compute_fingerprint(heal_outcome->outcome);
    EXPECT_NE(after_fp, before_fp);
    EXPECT_EQ(state.round_state.current_turn_index + 1,
              heal_outcome->outcome.round_state.current_turn_index);
}

TEST(HealCureReviveBranching, CureOutcomeDoesNotMutateAuthoritativeState) {
    auto gd = make_hcr_gd();
    auto state = make_cure_state(gd);
    auto before_fp = d2battle::compute_fingerprint(state);
    auto outcomes = d2battle::valid_action_outcomes(state, gd);
    ASSERT_FALSE(outcomes.empty());
    const d2battle::BattleActionOutcome* cure_outcome = nullptr;
    for (const auto& o : outcomes) {
        if (auto* atk = std::get_if<d2battle::AttackAction>(&o.action)) {
            const auto* actor = state.find_unit(atk->actor_id);
            if (actor && actor->type_id == "G000UH0101") {
                cure_outcome = &o;
                break;
            }
        }
    }
    ASSERT_NE(cure_outcome, nullptr);
    EXPECT_EQ(d2battle::compute_fingerprint(state), before_fp);
    auto after_fp = d2battle::compute_fingerprint(cure_outcome->outcome);
    EXPECT_NE(after_fp, before_fp);
}

TEST(HealCureReviveBranching, ReviveOutcomeDoesNotMutateAuthoritativeState) {
    auto gd = make_hcr_gd();
    auto state = make_revive_state(gd);
    auto before_fp = d2battle::compute_fingerprint(state);
    auto outcomes = d2battle::valid_action_outcomes(state, gd);
    ASSERT_FALSE(outcomes.empty());
    const d2battle::BattleActionOutcome* revive_outcome = nullptr;
    for (const auto& o : outcomes) {
        if (auto* atk = std::get_if<d2battle::AttackAction>(&o.action)) {
            const auto* actor = state.find_unit(atk->actor_id);
            if (actor && actor->type_id == "G000UH0102") {
                revive_outcome = &o;
                break;
            }
        }
    }
    ASSERT_NE(revive_outcome, nullptr);
    EXPECT_EQ(d2battle::compute_fingerprint(state), before_fp);
    auto after_fp = d2battle::compute_fingerprint(revive_outcome->outcome);
    EXPECT_NE(after_fp, before_fp);
}

// ════════════════════════════════════════════════════════
// Cure/Revive terminal presentation tests
// ════════════════════════════════════════════════════════

TEST(CureTerminalPresentation, CurePreviewShowsEffectsRemoved) {
    auto gd = make_hcr_gd();
    auto state = make_cure_state(gd);
    auto outcomes = d2battle::valid_action_outcomes(state, gd);
    ASSERT_GE(outcomes.size(), 1u);
    std::ostringstream oss;
    print_actions_menu(oss, state, outcomes, gd);
    std::string output = oss.str();
    EXPECT_EQ(count_occurrences(output, "EFFECTS REMOVED:\n"), 2u);
    EXPECT_EQ(count_occurrences(output, "DAMAGE:\n"), 0u);
    EXPECT_EQ(count_occurrences(output, "HEAL:\n"), 0u);
    EXPECT_EQ(count_occurrences(output, "REVIVED:\n"), 0u);
}

TEST(CureTerminalPresentation, CureSelectedActionShowsEffectsRemoved) {
    auto gd = make_hcr_gd();
    auto state = make_cure_state(gd);
    auto outcomes = d2battle::valid_action_outcomes(state, gd);
    ASSERT_GE(outcomes.size(), 1u);
    const d2battle::BattleActionOutcome* found = nullptr;
    for (const auto& o : outcomes) {
        if (auto* atk = std::get_if<d2battle::AttackAction>(&o.action)) {
            if (auto* ut = std::get_if<d2battle::UnitTarget>(&atk->target)) {
                if (ut->unit_id == "U1") {
                    found = &o;
                    break;
                }
            }
        }
    }
    ASSERT_NE(found, nullptr);
    std::ostringstream oss;
    print_selected_action(oss, state, found->action, found->outcome, gd);
    std::string output = oss.str();
    EXPECT_EQ(count_occurrences(output, "EFFECTS REMOVED:\n"), 1u);
    EXPECT_EQ(count_occurrences(output, "EFFECTS CONSUMED:\n"), 0u);
}

TEST(CureTerminalPresentation, CureNoOpShowsNoFakeEffectDelta) {
    auto                  gd = make_hcr_gd();
    d2battle::BattleState state;
    state.status = d2battle::BattleStatus::InProgress;
    state.party1 = {};
    state.party1.source_stack_id = "S1";
    state.party1.owner = "P1";
    state.party1.leader_id = "U0";
    state.party1.leader_alive = 1;
    state.party1.members = {std::optional<std::string>{"U0"},
                            std::nullopt,
                            std::nullopt,
                            std::nullopt,
                            std::nullopt,
                            std::nullopt};
    state.party1.cell_members = {0, -1, -1, -1, -1, -1};
    state.party1.positions = {0, -1, -1, -1, -1, -1};
    state.party2 = {};
    state.party2.source_stack_id = "S2";
    state.party2.owner = "P2";
    state.party2.leader_id = "E0";
    state.party2.leader_alive = 1;
    state.party2.members = {std::optional<std::string>{"E0"},
                            std::nullopt,
                            std::nullopt,
                            std::nullopt,
                            std::nullopt,
                            std::nullopt};
    state.party2.cell_members = {0, -1, -1, -1, -1, -1};
    state.party2.positions = {0, -1, -1, -1, -1, -1};
    d2battle::BattleUnitState actor;
    actor.id = "U0";
    actor.type_id = "G000UH0101";
    actor.side = d2battle::BattleSide::Party1;
    actor.member_index = 0;
    actor.current_hp = 200;
    actor.alive = true;
    actor.formation_cell = 0;
    state.units.push_back(actor);
    d2battle::BattleUnitState enemy;
    enemy.id = "E0";
    enemy.type_id = "G000UH0103";
    enemy.side = d2battle::BattleSide::Party2;
    enemy.member_index = 0;
    enemy.current_hp = 100;
    enemy.alive = true;
    enemy.formation_cell = 0;
    state.units.push_back(enemy);
    d2battle::detail::normalize_derived_side_state(state);
    d2battle::detail::normalize_battle_status(state);
    d2battle::detail::begin_round(state, 1, gd);
    auto outcomes = d2battle::valid_action_outcomes(state, gd);
    ASSERT_GE(outcomes.size(), 1u);
    const d2battle::BattleActionOutcome* cure_noop = nullptr;
    for (const auto& o : outcomes) {
        if (auto* atk = std::get_if<d2battle::AttackAction>(&o.action)) {
            if (auto* ut = std::get_if<d2battle::UnitTarget>(&atk->target)) {
                if (ut->unit_id == "U0") {
                    cure_noop = &o;
                    break;
                }
            }
        }
    }
    std::ostringstream oss;
    if (cure_noop) {
        print_selected_action(oss, state, cure_noop->action, cure_noop->outcome, gd);
    } else {
        print_actions_menu(oss, state, outcomes, gd);
    }
    std::string output = oss.str();
    EXPECT_EQ(count_occurrences(output, "PETRIFIED"), 0u);
}

TEST(ReviveTerminalPresentation, RevivePreviewShowsAllAllies) {
    auto gd = make_hcr_gd();
    auto state = make_revive_state(gd);
    auto outcomes = d2battle::valid_action_outcomes(state, gd);
    ASSERT_GE(outcomes.size(), 1u);
    std::ostringstream oss;
    print_actions_menu(oss, state, outcomes, gd);
    std::string output = oss.str();
    EXPECT_EQ(count_occurrences(output, "ALL ALLIES\n"), 0u);
}

TEST(ReviveTerminalPresentation, ReviveSelectedActionShowsRevivedHeading) {
    auto gd = make_hcr_gd();
    auto state = make_revive_state(gd);
    auto outcomes = d2battle::valid_action_outcomes(state, gd);
    ASSERT_GE(outcomes.size(), 1u);
    const d2battle::BattleActionOutcome* found = nullptr;
    for (const auto& o : outcomes) {
        if (auto* atk = std::get_if<d2battle::AttackAction>(&o.action)) {
            const auto* actor = state.find_unit(atk->actor_id);
            if (actor && actor->type_id == "G000UH0102") {
                found = &o;
                break;
            }
        }
    }
    ASSERT_NE(found, nullptr);
    std::ostringstream oss;
    print_selected_action(oss, state, found->action, found->outcome, gd);
    if (auto* atk = std::get_if<d2battle::AttackAction>(&found->action)) {
        if (auto* ut = std::get_if<d2battle::UnitTarget>(&atk->target)) {
            auto* target = state.find_unit(ut->unit_id);
            if (target && !target->alive)
                EXPECT_EQ(count_occurrences(oss.str(), "REVIVED:\n"), 1u);
        }
    }
}

TEST(ReviveTerminalPresentation, RevivePreviewExcludesAliveUnits) {
    auto gd = make_hcr_gd();
    auto state = make_revive_state(gd);
    auto actions = d2battle::detail::valid_actions_on_valid_state(state, gd);
    for (const auto& a : actions) {
        if (auto* atk = std::get_if<d2battle::AttackAction>(&a)) {
            if (auto* ut = std::get_if<d2battle::UnitTarget>(&atk->target)) {
                EXPECT_NE(ut->unit_id, "U1") << "Revive should not target alive ally U1";
            }
        }
    }
}

} // namespace
