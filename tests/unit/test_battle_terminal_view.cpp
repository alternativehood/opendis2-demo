#include <gtest/gtest.h>

#include <d2battle_rules/battle_action.hpp>
#include <d2battle_rules/battle_action_validate.hpp>
#include <d2battle_rules/battle_apply.hpp>
#include <d2battle_rules/battle_bootstrap.hpp>
#include <d2battle_rules/battle_effect.hpp>
#include <d2battle_rules/detail/unit_effects.hpp>
#include <d2battle_rules/battle_outcomes.hpp>
#include <d2battle_rules/battle_state.hpp>
#include <d2battle_rules/battle_valid_actions.hpp>
#include <d2battle_rules/battle_validate.hpp>
#include <d2engine/assets/game_data_registry.hpp>
#include <d2runtime/AdventureWorldState.hpp>
#include <opendis2_battle/attack_presentation_labels.hpp>
#include <opendis2_battle/terminal_view.hpp>

#include "tests/test_dbf_builder.hpp"

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "tests/test_process.hpp"

namespace {

using namespace d2battle;
using namespace test_dbf;

[[nodiscard]] std::filesystem::path write_gd_drain_with_ally() {
    static std::atomic<unsigned> c{0};
    auto tmp = std::filesystem::temp_directory_path() /
               ("d2bt_drain_ally_" + std::to_string(test_support::process_id()) + "_" +
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
        b.add_record({"G000DR0002", "", "", "2", "1", "1", "50", "60", "0", "50", "1", "0", "", "",
                      "", "", ""}); // Drain All
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
        b
            .add_record(
                {"G000UUR002", "",  "",  "",           "G000SU0001", "0",   "1", "0",
                 "1",          "1", "0", "0",          "1",          "300", "2", "0",
                 "3",          "1", "5", "1",          "50",         "100", "",  "",
                 "",           "",  "",  "G000DR0002", "",           "",    "",  "",
                 "",           "",  "",  "1",          "L_HUMAN"}); // actor, Drain All, 300 max
        b.add_record(
            {"G000UUR020", "",    "",  "",    "G000SU0001", "0", "1",      "0", "1", "1",
             "0",          "0",   "1", "300", "2",          "0", "3",      "1", "5", "1",
             "50",         "100", "",  "",    "",           "",  "",       "",  "",  "",
             "",           "",    "",  "",    "",           "1", "L_HUMAN"}); // ally / target
        b.write(tmp / "Gunits.dbf");
    }
    return tmp;
}

[[nodiscard]] std::filesystem::path write_gd_damage() {
    static std::atomic<unsigned> c{0};
    auto tmp = std::filesystem::temp_directory_path() /
               ("d2bt_damage_pres_" + std::to_string(test_support::process_id()) + "_" +
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
        b.add_record({"2", "L_DRAIN"});
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
        b.add_record({"G000DM0001", "", "", "1", "1", "2", "50", "25", "0", "100", "1", "0", "", "",
                      "", "", ""}); // Damage Any, 25 dmg
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
        b.add_record({"G000UUA001", "",  "",  "",           "G000SU0001", "0",   "1", "0",
                      "1",          "1", "0", "0",          "1",          "300", "2", "0",
                      "3",          "1", "5", "1",          "50",         "100", "",  "",
                      "",           "",  "",  "G000DM0001", "",           "",    "",  "",
                      "",           "",  "",  "1",          "L_HUMAN"}); // damage actor
        b.add_record(
            {"G000UUA002", "",    "",  "",    "G000SU0001", "0", "1",      "0", "1", "1",
             "0",          "0",   "1", "300", "2",          "0", "3",      "1", "5", "1",
             "50",         "100", "",  "",    "",           "",  "",       "",  "",  "",
             "",           "",    "",  "",    "",           "1", "L_HUMAN"}); // unchanged ally
        b.add_record(
            {"G000UUA003", "",    "",  "",    "G000SU0001", "0", "1",      "0", "1", "1",
             "0",          "0",   "1", "100", "2",          "0", "3",      "1", "5", "1",
             "50",         "100", "",  "",    "",           "",  "",       "",  "",  "",
             "",           "",    "",  "",    "",           "1", "L_HUMAN"}); // target, 100 max
        b.write(tmp / "Gunits.dbf");
    }
    return tmp;
}

[[nodiscard]] std::size_t count_occurrences(std::string_view text, std::string_view needle) {
    std::size_t count = 0;
    std::size_t pos = 0;
    while ((pos = text.find(needle, pos)) != std::string_view::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

[[nodiscard]] std::filesystem::path write_gd_damage_all() {
    static std::atomic<unsigned> c{0};
    auto tmp = std::filesystem::temp_directory_path() /
               ("d2bt_damage_all_" + std::to_string(test_support::process_id()) + "_" +
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
        b.add_record({"G000DM0002", "", "", "1", "1", "1", "50", "25", "0", "100", "1", "0", "", "",
                      "", "", ""}); // Damage All, 25 dmg
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
        b.add_record({"G000UUA001", "",  "",  "",           "G000SU0001", "0",   "1", "0",
                      "1",          "1", "0", "0",          "1",          "300", "2", "0",
                      "3",          "1", "5", "1",          "50",         "100", "",  "",
                      "",           "",  "",  "G000DM0002", "",           "",    "",  "",
                      "",           "",  "",  "1",          "L_HUMAN"}); // actor, Damage All
        b.add_record(
            {"G000UUA002", "",    "",  "",    "G000SU0001", "0", "1",      "0", "1", "1",
             "0",          "0",   "1", "300", "2",          "0", "3",      "1", "5", "1",
             "50",         "100", "",  "",    "",           "",  "",       "",  "",  "",
             "",           "",    "",  "",    "",           "1", "L_HUMAN"}); // unchanged ally
        b.add_record(
            {"G000UUA003", "",    "",  "",    "G000SU0001", "0", "1",      "0", "1", "1",
             "0",          "0",   "1", "100", "2",          "0", "3",      "1", "5", "1",
             "50",         "100", "",  "",    "",           "",  "",       "",  "",  "",
             "",           "",    "",  "",    "",           "1", "L_HUMAN"}); // enemy A, 100 max
        b.add_record(
            {"G000UUA004", "",    "",  "",   "G000SU0001", "0", "1",      "0", "1", "1",
             "0",          "0",   "1", "80", "2",          "0", "3",      "1", "5", "1",
             "50",         "100", "",  "",   "",           "",  "",       "",  "",  "",
             "",           "",    "",  "",   "",           "1", "L_HUMAN"}); // enemy B, 80 max
        b.add_record(
            {"G000UUA005", "",    "",  "",    "G000SU0001", "0", "1",      "0", "0", "1",
             "0",          "0",   "1", "120", "2",          "0", "3",      "1", "5", "1",
             "50",         "100", "",  "",    "",           "",  "",       "",  "",  "",
             "",           "",    "",  "",    "",           "1", "L_HUMAN"}); // large, 120 max
        b.write(tmp / "Gunits.dbf");
    }
    return tmp;
}

struct CaptureCout {
    std::streambuf*    orig;
    std::ostringstream stream;

    CaptureCout() : orig(std::cout.rdbuf()) { std::cout.rdbuf(stream.rdbuf()); }

    ~CaptureCout() { std::cout.rdbuf(orig); }

    CaptureCout(const CaptureCout&) = delete;
    CaptureCout& operator=(const CaptureCout&) = delete;
    CaptureCout(CaptureCout&&) = delete;
    CaptureCout& operator=(CaptureCout&&) = delete;

    [[nodiscard]] std::string str() const { return stream.str(); }
};

[[nodiscard]] BattleState drain_fixture(d2engine::GameDataRegistry& gd) {
    using d2runtime::AdventureStack;
    using d2runtime::AdventureWorldState;

    AdventureWorldState w;
    w.units.push_back({"AU", "g000uur002", 1, {}, 0, "Actor", 0, {}, 100, 0});
    w.units.push_back({"UA", "g000uur020", 1, {}, 0, "Ally", 0, {}, 250, 0});
    w.units.push_back({"TU", "g000uur020", 1, {}, 0, "Target", 0, {}, 200, 0});

    AdventureStack s1;
    s1.id = "S1";
    s1.owner = "O1";
    s1.leader_id = "AU";
    s1.group.members[0] = "AU";
    s1.group.members[1] = "UA";
    s1.group.positions[0] = 0;
    s1.group.positions[1] = 1;
    s1.group.cell_members[0] = 0;
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

    return bootstrap_battle(s1, s2, w, gd);
}

[[nodiscard]] BattleState damage_fixture(d2engine::GameDataRegistry& gd) {
    using d2runtime::AdventureStack;
    using d2runtime::AdventureWorldState;

    AdventureWorldState w;
    w.units.push_back({"DA", "g000uua001", 1, {}, 0, "Damager", 0, {}, 300, 0});
    w.units.push_back({"UA", "g000uua002", 1, {}, 0, "Ally", 0, {}, 300, 0});
    w.units.push_back({"TU", "g000uua003", 1, {}, 0, "Target", 0, {}, 100, 0});

    AdventureStack s1;
    s1.id = "S1";
    s1.owner = "O1";
    s1.leader_id = "DA";
    s1.group.members[0] = "DA";
    s1.group.members[1] = "UA";
    s1.group.positions[0] = 0;
    s1.group.positions[1] = 1;
    s1.group.cell_members[0] = 0;
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

    return bootstrap_battle(s1, s2, w, gd);
}

[[nodiscard]] BattleState all_damage_fixture(d2engine::GameDataRegistry& gd) {
    using d2runtime::AdventureStack;
    using d2runtime::AdventureWorldState;

    AdventureWorldState w;
    w.units.push_back({"DA", "g000uua001", 1, {}, 0, "Damager", 0, {}, 300, 0});
    w.units.push_back({"UA", "g000uua002", 1, {}, 0, "Ally", 0, {}, 300, 0});
    w.units.push_back({"EA", "g000uua003", 1, {}, 0, "EnemyA", 0, {}, 100, 0});
    w.units.push_back({"EB", "g000uua004", 1, {}, 0, "EnemyB", 0, {}, 80, 0});
    w.units.push_back({"EL", "g000uua005", 1, {}, 0, "Large", 0, {}, 120, 0});

    AdventureStack s1;
    s1.id = "S1";
    s1.owner = "O1";
    s1.leader_id = "DA";
    s1.group.members[0] = "DA";
    s1.group.members[1] = "UA";
    s1.group.positions[0] = 0;
    s1.group.positions[1] = 1;
    s1.group.cell_members[0] = 0;
    s1.group.cell_members[1] = 1;
    s1.position = {21, 23};

    AdventureStack s2;
    s2.id = "S2";
    s2.owner = "O2";
    s2.leader_id = "EA";
    s2.group.members[0] = "EA";
    s2.group.members[1] = "EB";
    s2.group.members[2] = "EL";
    s2.group.positions[0] = 0;
    s2.group.positions[1] = 2;
    s2.group.positions[2] = 4;
    s2.group.cell_members[0] = 0;
    s2.group.cell_members[2] = 1;
    s2.group.cell_members[4] = 2;
    s2.group.cell_members[5] = 2;
    s2.position = {24, 20};

    w.stacks.push_back(s1);
    w.stacks.push_back(s2);

    return bootstrap_battle(s1, s2, w, gd);
}

} // namespace

TEST(AttackPresentationLabelsTest, AllAttackClassesHaveNonEmptyLabels) {
    using d2battle_terminal::attack_class_label;
    using d2engine::AttackClass;

    for (const auto& value :
         {AttackClass::Damage,        AttackClass::Drain,          AttackClass::Paralyze,
          AttackClass::Heal,          AttackClass::Fear,           AttackClass::BoostDamage,
          AttackClass::Petrify,       AttackClass::LowerDamage,    AttackClass::LowerInitiative,
          AttackClass::Poison,        AttackClass::Frostbite,      AttackClass::Revive,
          AttackClass::DrainOverflow, AttackClass::Cure,           AttackClass::Summon,
          AttackClass::DrainLevel,    AttackClass::GiveAttack,     AttackClass::Doppelganger,
          AttackClass::TransformSelf, AttackClass::TransformOther, AttackClass::Blister,
          AttackClass::BestowWards,   AttackClass::Shatter,        AttackClass::Unknown}) {
        EXPECT_FALSE(attack_class_label(value).empty())
            << "Empty label for AttackClass=" << static_cast<int>(value);
    }
}

TEST(AttackPresentationLabelsTest, KnownMappingsAreExact) {
    EXPECT_EQ(d2battle_terminal::attack_class_label(d2engine::AttackClass::Damage), "Damage");
    EXPECT_EQ(d2battle_terminal::attack_class_label(d2engine::AttackClass::Drain), "Drain");
    EXPECT_EQ(d2battle_terminal::attack_class_label(d2engine::AttackClass::DrainOverflow),
              "DrainOverflow");
    EXPECT_EQ(d2battle_terminal::attack_class_label(d2engine::AttackClass::Poison), "Poison");
    EXPECT_EQ(d2battle_terminal::attack_class_label(d2engine::AttackClass::Unknown), "Unknown");
}

TEST(AttackPresentationLabelsTest, AllAttackReachesHaveNonEmptyLabels) {
    using d2battle_terminal::attack_reach_label;
    using d2engine::AttackReach;

    for (const auto& value :
         {AttackReach::All, AttackReach::Any, AttackReach::Adjacent, AttackReach::Unknown}) {
        EXPECT_FALSE(attack_reach_label(value).empty());
    }
}

TEST(DrainTerminalPresentationTest, DrainActionMenuShowsOnlyActualDamageAndHealingDeltas) {
    auto                       gp = write_gd_drain_with_ally();
    d2engine::GameDataRegistry gd(gp);

    auto state = drain_fixture(gd);
    auto outcomes = valid_action_outcomes(state, gd);
    ASSERT_EQ(outcomes.size(), 1u);

    CaptureCout cap;
    print_actions_menu(std::cout, state, outcomes, gd);
    auto output = cap.str();

    EXPECT_NE(output.find("    attack=[Drain/All]\n"), std::string::npos);

    EXPECT_NE(output.find("DAMAGE:"), std::string::npos);
    EXPECT_NE(output.find("HP 200 -> 140"), std::string::npos);

    EXPECT_NE(output.find("HEAL:"), std::string::npos);
    EXPECT_NE(output.find("HP 100 -> 130"), std::string::npos);

    EXPECT_EQ(output.find("HP 250 -> 250"), std::string::npos);
}

TEST(DrainTerminalPresentationTest, DrainSelectedActionShowsOnlyActualDamageAndHealingDeltas) {
    auto                       gp = write_gd_drain_with_ally();
    d2engine::GameDataRegistry gd(gp);

    auto state = drain_fixture(gd);
    auto outcomes = valid_action_outcomes(state, gd);
    ASSERT_EQ(outcomes.size(), 1u);

    auto next = apply(state, outcomes[0].action, gd);

    CaptureCout cap;
    print_selected_action(std::cout, state, outcomes[0].action, next, gd);
    auto output = cap.str();

    EXPECT_NE(output.find("uses [Drain/All]\n"), std::string::npos);

    EXPECT_NE(output.find("DAMAGE:"), std::string::npos);
    EXPECT_NE(output.find("HP 200 -> 140"), std::string::npos);

    EXPECT_NE(output.find("HEAL:"), std::string::npos);
    EXPECT_NE(output.find("HP 100 -> 130"), std::string::npos);

    EXPECT_EQ(output.find("HP 250 -> 250"), std::string::npos);
}

TEST(DrainTerminalPresentationTest, DamageActionPreviewShowsEnemyDamageAndNoUnchangedAllies) {
    auto                       gp = write_gd_damage();
    d2engine::GameDataRegistry gd(gp);

    auto state = damage_fixture(gd);
    auto outcomes = valid_action_outcomes(state, gd);
    ASSERT_EQ(outcomes.size(), 1u);

    CaptureCout cap;
    print_actions_menu(std::cout, state, outcomes, gd);
    auto output = cap.str();

    EXPECT_NE(output.find("DAMAGE:"), std::string::npos);
    EXPECT_NE(output.find("HP 100 -> 75"), std::string::npos);

    EXPECT_EQ(output.find("HEAL:"), std::string::npos);

    EXPECT_EQ(output.find("HP 300 -> 300"), std::string::npos);
}

TEST(DrainTerminalPresentationTest, DamageSelectedActionShowsEnemyDamageAndNoUnchangedAllies) {
    auto                       gp = write_gd_damage();
    d2engine::GameDataRegistry gd(gp);

    auto state = damage_fixture(gd);
    auto outcomes = valid_action_outcomes(state, gd);
    ASSERT_EQ(outcomes.size(), 1u);

    auto next = apply(state, outcomes[0].action, gd);

    CaptureCout cap;
    print_selected_action(std::cout, state, outcomes[0].action, next, gd);
    auto output = cap.str();

    EXPECT_NE(output.find("DAMAGE:"), std::string::npos);
    EXPECT_NE(output.find("HP 100 -> 75"), std::string::npos);

    EXPECT_EQ(output.find("HEAL:"), std::string::npos);

    EXPECT_EQ(output.find("HP 300 -> 300"), std::string::npos);
}

TEST(TerminalHpDeltaFailsIfUnitDisappearsFromSuccessorState, Basic) {
    auto                       gp = write_gd_damage();
    d2engine::GameDataRegistry gd(gp);

    auto state = damage_fixture(gd);
    auto outcomes = valid_action_outcomes(state, gd);
    ASSERT_EQ(outcomes.size(), 1u);

    auto after = apply(state, outcomes[0].action, gd);

    auto it = std::remove_if(after.units.begin(), after.units.end(),
                             [](const auto& u) { return u.id == "TU"; });
    after.units.erase(it, after.units.end());

    std::string msg;
    try {
        CaptureCout cap;
        print_selected_action(std::cout, state, outcomes[0].action, after, gd);
        FAIL() << "Expected std::runtime_error";
    } catch (const std::runtime_error& e) {
        msg = e.what();
    }
    EXPECT_NE(msg.find("TU"), std::string::npos);
}

TEST(AllDamagePresentationTest, AllDamagePreviewShowsEveryChangedEnemyExactlyOnce) {
    auto                       gp = write_gd_damage_all();
    d2engine::GameDataRegistry gd(gp);

    auto state = all_damage_fixture(gd);
    auto outcomes = valid_action_outcomes(state, gd);
    ASSERT_EQ(outcomes.size(), 1u);

    CaptureCout cap;
    print_actions_menu(std::cout, state, outcomes, gd);
    auto output = cap.str();

    EXPECT_NE(output.find("    attack=[Damage/All]\n"), std::string::npos);
    EXPECT_NE(output.find("DAMAGE:"), std::string::npos);

    EXPECT_EQ(count_occurrences(output, "HP 100 -> 75"), 1u);
    EXPECT_EQ(count_occurrences(output, "HP 80 -> 55"), 1u);
    EXPECT_EQ(count_occurrences(output, "HP 120 -> 95"), 1u);

    EXPECT_EQ(output.find("HP 300 -> 300"), std::string::npos);
    EXPECT_EQ(output.find("HEAL:"), std::string::npos);
}

TEST(AllDamagePresentationTest, AllDamageSelectedActionShowsEveryChangedEnemyExactlyOnce) {
    auto                       gp = write_gd_damage_all();
    d2engine::GameDataRegistry gd(gp);

    auto state = all_damage_fixture(gd);
    auto outcomes = valid_action_outcomes(state, gd);
    ASSERT_EQ(outcomes.size(), 1u);

    auto after = apply(state, outcomes[0].action, gd);

    CaptureCout cap;
    print_selected_action(std::cout, state, outcomes[0].action, after, gd);
    auto output = cap.str();

    EXPECT_NE(output.find("uses [Damage/All]\n"), std::string::npos);
    EXPECT_NE(output.find("DAMAGE:"), std::string::npos);

    EXPECT_EQ(count_occurrences(output, "HP 100 -> 75"), 1u);
    EXPECT_EQ(count_occurrences(output, "HP 80 -> 55"), 1u);
    EXPECT_EQ(count_occurrences(output, "HP 120 -> 95"), 1u);

    EXPECT_EQ(output.find("HP 300 -> 300"), std::string::npos);
    EXPECT_EQ(output.find("HEAL:"), std::string::npos);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Petrify terminal presentation

[[nodiscard]] std::filesystem::path write_gd_petrify_term() {
    static std::atomic<unsigned> c{0};
    auto tmp = std::filesystem::temp_directory_path() /
               ("d2bt_petrify_term_" + std::to_string(test_support::process_id()) + "_" +
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
                      "", "", ""});
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
                      "",           "",  "",  "1",          "L_HUMAN"});
        b.add_record({"G000PU0002", "",   "",  "",   "G000SU0001", "0", "1",      "0", "1", "1",
                      "0",          "0",  "1", "65", "2",          "0", "2",      "1", "0", "1",
                      "25",         "50", "",  "",   "",           "",  "",       "",  "",  "",
                      "",           "",   "",  "",   "",           "1", "L_HUMAN"});
        b.write(tmp / "Gunits.dbf");
    }
    return tmp;
}

[[nodiscard]] BattleState petrify_fixture(d2engine::GameDataRegistry& gd) {
    using d2runtime::AdventureStack;
    using d2runtime::AdventureWorldState;

    AdventureWorldState w;
    w.units.push_back({"PU", "g000pu0001", 1, {}, 0, "Petrifier", 0, {}, 300, 0});
    w.units.push_back({"TU", "g000pu0002", 1, {}, 0, "Target", 0, {}, 65, 0});

    AdventureStack s1;
    s1.id = "S1";
    s1.owner = "O1";
    s1.leader_id = "PU";
    s1.group.members[0] = "PU";
    s1.group.positions[0] = 2;
    s1.group.cell_members[2] = 0;
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
    return bootstrap_battle(s1, s2, w, gd);
}

TEST(PetrifyTerminalPresentationTest, ActionMenuShowsEffectsApplied) {
    auto                       gp = write_gd_petrify_term();
    d2engine::GameDataRegistry gd(gp);

    auto state = petrify_fixture(gd);
    auto outcomes = valid_action_outcomes(state, gd);
    ASSERT_EQ(outcomes.size(), 1u);

    CaptureCout cap;
    print_actions_menu(std::cout, state, outcomes, gd);
    auto output = cap.str();

    EXPECT_NE(output.find("    attack=[Petrify/All]\n"), std::string::npos);
    EXPECT_NE(output.find("EFFECTS APPLIED:"), std::string::npos);
    EXPECT_NE(output.find("PETRIFIED"), std::string::npos);
}

TEST(PetrifyTerminalPresentationTest, SelectedActionShowsForcedSkip) {
    auto                       gp = write_gd_petrify_term();
    d2engine::GameDataRegistry gd(gp);

    auto state = petrify_fixture(gd);
    auto outcomes = valid_action_outcomes(state, gd);
    ASSERT_EQ(outcomes.size(), 1u);

    auto after_atk = apply(state, outcomes[0].action, gd);

    auto acts2 = valid_action_outcomes(after_atk, gd);
    ASSERT_EQ(acts2.size(), 1u);
    ASSERT_TRUE(std::holds_alternative<SkipActivationAction>(acts2[0].action));

    auto after_skip = apply(after_atk, acts2[0].action, gd);

    CaptureCout cap;
    print_selected_action(std::cout, after_atk, acts2[0].action, after_skip, gd);
    auto output = cap.str();

    EXPECT_NE(output.find(">>> FORCED ACTION"), std::string::npos);
    EXPECT_NE(output.find("skips activation: PETRIFIED"), std::string::npos);
    EXPECT_NE(output.find("EFFECTS CONSUMED:"), std::string::npos);
    EXPECT_NE(output.find("PETRIFIED"), std::string::npos);
}

TEST(PetrifyTerminalPresentationTest, UnitLegendShowsPetrifiedMarker) {
    auto                       gp = write_gd_petrify_term();
    d2engine::GameDataRegistry gd(gp);

    auto state = petrify_fixture(gd);
    auto outcomes = valid_action_outcomes(state, gd);
    ASSERT_EQ(outcomes.size(), 1u);

    auto after = apply(state, outcomes[0].action, gd);

    CaptureCout cap;
    print_unit_legend(std::cout, after, gd);
    auto output = cap.str();
    EXPECT_NE(output.find("PETRIFIED"), std::string::npos);
}

// Multi-target petrify fixture

[[nodiscard]] std::filesystem::path write_gd_petrify_multi() {
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
                      "", "", ""});
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
                      "",           "",  "",  "1",          "L_HUMAN"});
        b.add_record({"G000PU0002", "",   "",  "",   "G000SU0001", "0", "1",      "0", "1", "1",
                      "0",          "0",  "1", "65", "2",          "0", "2",      "1", "0", "1",
                      "25",         "50", "",  "",   "",           "",  "",       "",  "",  "",
                      "",           "",   "",  "",   "",           "1", "L_HUMAN"});
        b.add_record({"G000PU0003", "",   "",  "",   "G000SU0001", "0", "1",      "0", "1", "1",
                      "0",          "0",  "1", "70", "2",          "0", "2",      "1", "0", "1",
                      "25",         "50", "",  "",   "",           "",  "",       "",  "",  "",
                      "",           "",   "",  "",   "",           "1", "L_HUMAN"});
        b.add_record({"G000PU0004", "",   "",  "",   "G000SU0001", "0", "1",      "0", "0", "1",
                      "0",          "0",  "1", "90", "2",          "0", "2",      "1", "0", "1",
                      "25",         "50", "",  "",   "",           "",  "",       "",  "",  "",
                      "",           "",   "",  "",   "",           "1", "L_HUMAN"});
        b.add_record({"G000PU0005", "",   "",  "",   "G000SU0001", "0", "1",      "0", "1", "1",
                      "0",          "0",  "1", "60", "2",          "0", "2",      "1", "0", "1",
                      "25",         "50", "",  "",   "",           "",  "",       "",  "",  "",
                      "",           "",   "",  "",   "",           "1", "L_HUMAN"});
        b.write(tmp / "Gunits.dbf");
    }
    return tmp;
}

[[nodiscard]] BattleState petrify_multi_fixture(d2engine::GameDataRegistry& gd) {
    using d2runtime::AdventureStack;
    using d2runtime::AdventureWorldState;

    AdventureWorldState w;
    w.units.push_back({"PU", "g000pu0001", 1, {}, 0, "Petrifier", 0, {}, 300, 0});
    w.units.push_back({"EA", "g000pu0002", 1, {}, 0, "EnemyA", 0, {}, 65, 0});
    w.units.push_back({"EB", "g000pu0003", 1, {}, 0, "EnemyB", 0, {}, 70, 0});
    w.units.push_back({"EC", "g000pu0004", 1, {}, 0, "LargeC", 0, {}, 90, 0});
    w.units.push_back({"ED", "g000pu0005", 1, {}, 0, "DeadD", 0, {}, 0, 0});

    AdventureStack s1;
    s1.id = "S1";
    s1.owner = "O1";
    s1.leader_id = "PU";
    s1.group.members[0] = "PU";
    s1.group.positions[0] = 2;
    s1.group.cell_members[2] = 0;
    s1.position = {21, 23};

    AdventureStack s2;
    s2.id = "S2";
    s2.owner = "O2";
    s2.leader_id = "EA";
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

    auto state = bootstrap_battle(s1, s2, w, gd);
    // Mark DeadD as dead
    auto* dd = state.find_unit("ED");
    if (dd) {
        dd->alive = false;
        dd->current_hp = 0;
    }
    return state;
}

TEST(PetrifyTerminalPresentationTest, PetrifyAllPreviewShowsEveryAppliedEffectExactlyOnce) {
    auto                       gp = write_gd_petrify_multi();
    d2engine::GameDataRegistry gd(gp);

    auto state = petrify_multi_fixture(gd);
    auto outcomes = valid_action_outcomes(state, gd);
    ASSERT_EQ(outcomes.size(), 1u);

    CaptureCout cap;
    print_actions_menu(std::cout, state, outcomes, gd);
    auto output = cap.str();

    EXPECT_NE(output.find("[Petrify/All]"), std::string::npos);
    EXPECT_NE(output.find("EFFECTS APPLIED:"), std::string::npos);
    EXPECT_NE(output.find("PETRIFIED"), std::string::npos);

    EXPECT_EQ(output.find("DAMAGE:"), std::string::npos);
    EXPECT_EQ(output.find("HEAL:"), std::string::npos);

    // Each alive enemy appears exactly once with PETRIFIED label
    std::string ea_line = "EnemyA PETRIFIED";
    std::string eb_line = "EnemyB PETRIFIED";
    std::string ec_line = "LargeC PETRIFIED";
    EXPECT_EQ(count_occurrences(output, ea_line), 1u) << ea_line;
    EXPECT_EQ(count_occurrences(output, eb_line), 1u) << eb_line;
    EXPECT_EQ(count_occurrences(output, ec_line), 1u) << ec_line;

    // DeadD must not appear
    EXPECT_EQ(output.find("DeadD PETRIFIED"), std::string::npos);
}

TEST(PetrifyTerminalPresentationTest, ForcedPetrifySkipPresentationShowsOnlyConsumedEffect) {
    auto                       gp = write_gd_petrify_term();
    d2engine::GameDataRegistry gd(gp);

    auto state = petrify_fixture(gd);
    auto outcomes = valid_action_outcomes(state, gd);
    ASSERT_EQ(outcomes.size(), 1u);

    auto after_atk = apply(state, outcomes[0].action, gd);

    auto acts2 = valid_action_outcomes(after_atk, gd);
    ASSERT_EQ(acts2.size(), 1u);
    ASSERT_TRUE(std::holds_alternative<SkipActivationAction>(acts2[0].action));

    auto after_skip = apply(after_atk, acts2[0].action, gd);

    CaptureCout cap;
    print_selected_action(std::cout, after_atk, acts2[0].action, after_skip, gd);
    auto output = cap.str();

    EXPECT_NE(output.find(">>> FORCED ACTION"), std::string::npos);
    EXPECT_NE(output.find("skips activation: PETRIFIED"), std::string::npos);
    EXPECT_NE(output.find("EFFECTS CONSUMED:"), std::string::npos);
    EXPECT_NE(output.find("PETRIFIED"), std::string::npos);

    EXPECT_EQ(output.find("attack="), std::string::npos);
    EXPECT_EQ(output.find("reach="), std::string::npos);
    EXPECT_EQ(output.find("enemies"), std::string::npos);
    EXPECT_EQ(output.find("DAMAGE:"), std::string::npos);
    EXPECT_EQ(output.find("HEAL:"), std::string::npos);
}

TEST(PetrifyTerminalPresentationTest, PetrifyRefreshPreviewShowsRefreshedEffect) {
    auto                       gp = write_gd_petrify_term();
    d2engine::GameDataRegistry gd(gp);

    auto state = petrify_fixture(gd);

    // Add a distinct PetrifiedEffect to TU so Petrify All will refresh it
    detail::apply_or_refresh_petrified(*state.find_unit("TU"), "PU", "G000PT0099", 1);
    ASSERT_TRUE(detail::is_petrified(*state.find_unit("TU")));
    ASSERT_EQ(state.current_actor()->id, "PU");
    validate_battle_state(state);

    auto outcomes = valid_action_outcomes(state, gd);
    ASSERT_EQ(outcomes.size(), 1u);

    CaptureCout cap;
    print_actions_menu(std::cout, state, outcomes, gd);
    auto output = cap.str();

    EXPECT_NE(output.find("[Petrify/All]"), std::string::npos);

    EXPECT_NE(output.find("EFFECTS REFRESHED:"), std::string::npos);
    EXPECT_NE(output.find("PETRIFIED"), std::string::npos);

    EXPECT_EQ(output.find("EFFECTS APPLIED:"), std::string::npos);
    EXPECT_EQ(output.find("EFFECTS CONSUMED:"), std::string::npos);
    EXPECT_EQ(output.find("DAMAGE:"), std::string::npos);
    EXPECT_EQ(output.find("HEAL:"), std::string::npos);

    EXPECT_EQ(count_occurrences(output, "Target PETRIFIED"), 1u);
}

TEST(PetrifyTerminalPresentationTest, PetrifyRefreshSelectedActionShowsRefreshedEffect) {
    auto                       gp = write_gd_petrify_term();
    d2engine::GameDataRegistry gd(gp);

    auto state = petrify_fixture(gd);

    // Add a distinct PetrifiedEffect to TU so Petrify All will refresh it
    detail::apply_or_refresh_petrified(*state.find_unit("TU"), "PU", "G000PT0099", 1);
    ASSERT_TRUE(detail::is_petrified(*state.find_unit("TU")));
    ASSERT_EQ(state.current_actor()->id, "PU");
    validate_battle_state(state);

    auto outcomes = valid_action_outcomes(state, gd);
    ASSERT_EQ(outcomes.size(), 1u);

    CaptureCout cap;
    print_selected_action(std::cout, state, outcomes[0].action, outcomes[0].outcome, gd);
    auto output = cap.str();

    EXPECT_NE(output.find("[Petrify/All]"), std::string::npos);
    EXPECT_NE(output.find("EFFECTS REFRESHED:"), std::string::npos);
    EXPECT_NE(output.find("PETRIFIED"), std::string::npos);

    EXPECT_EQ(output.find("EFFECTS APPLIED:"), std::string::npos);
    EXPECT_EQ(output.find("EFFECTS CONSUMED:"), std::string::npos);
    EXPECT_EQ(output.find("DAMAGE:"), std::string::npos);
    EXPECT_EQ(output.find("HEAL:"), std::string::npos);
}

TEST(PetrifyTerminalPresentationTest, ForcedActionMenuFailsWhenActorIsMissing) {
    std::vector<d2battle::BattleActionOutcome> outcomes;
    outcomes.push_back({d2battle::BattleAction{d2battle::SkipActivationAction{
                            "MISSING_ACTOR", d2battle::SkipActivationReason::Petrified}},
                        d2battle::BattleState{}});

    d2battle::BattleState      dummy_state;
    d2engine::GameDataRegistry dummy_gd{""};
    try {
        print_actions_menu(std::cout, dummy_state, outcomes, dummy_gd);
        FAIL() << "expected runtime_error for missing forced actor";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find("MISSING_ACTOR"), std::string::npos);
    }
}

TEST(PetrifyTerminalPresentationTest, ForcedSelectedActionFailsWhenActorIsMissing) {
    d2battle::BattleAction action{
        d2battle::SkipActivationAction{"MISSING_ACTOR", d2battle::SkipActivationReason::Petrified}};
    d2battle::BattleState dummy_state;
    d2battle::BattleState successor;

    d2engine::GameDataRegistry dummy_gd{""};
    try {
        print_selected_action(std::cout, dummy_state, action, successor, dummy_gd);
        FAIL() << "expected runtime_error for missing forced actor";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find("MISSING_ACTOR"), std::string::npos);
    }
}
