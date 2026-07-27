#include <gtest/gtest.h>

#include <opendis2_battle_sweep/stack_catalog.hpp>
#include <opendis2_battle_sweep/stack_pair_generator.hpp>
#include <opendis2_battle_sweep/random_action_policy.hpp>
#include <opendis2_battle_sweep/battle_simulator.hpp>
#include <opendis2_battle_sweep/battle_log_writer.hpp>
#include <opendis2_battle_sweep/sweep_runner.hpp>

#include <d2battle_rules/attack_support.hpp>
#include <d2battle_rules/battle_action.hpp>
#include <d2battle_rules/battle_bootstrap.hpp>
#include <d2battle_rules/battle_state.hpp>
#include <d2battle_rules/battle_outcomes.hpp>
#include <d2battle_rules/battle_valid_actions.hpp>
#include <d2battle_rules/battle_fingerprint.hpp>
#include <d2battle_rules/battle_validate.hpp>
#include <d2battle_rules/detail/battle_derived.hpp>
#include <d2battle_rules/detail/battle_status.hpp>
#include <d2battle_rules/detail/battle_turn.hpp>
#include <d2engine/assets/attack_def.hpp>
#include <d2engine/assets/game_data_registry.hpp>
#include <d2runtime/AdventureWorldState.hpp>
#include <opendis2_battle/terminal_view.hpp>

#include "tests/test_dbf_builder.hpp"

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace d2battle_sweep;

std::filesystem::path write_minimal_gd(const std::string& suffix = "") {
    static std::atomic<unsigned> c{0};
    auto tmp = std::filesystem::temp_directory_path() / ("d2sweep_" + std::to_string(c++) + suffix);
    std::filesystem::remove_all(tmp);
    std::filesystem::create_directories(tmp);
    {
        test_dbf::DbfBuilder b{{"TXT_ID", 'C', 20}, {"TEXT", 'C', 50}};
        b.add_record({"L_HUMAN", "Human"});
        b.add_record({"L_DOPPELGANGER", "Doppelganger"});
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
        b.add_record({"12", "L_DRAIN_OVERFLOW"});
        b.add_record({"13", "L_HEAL"});
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
        b.write(tmp / "LattS.dbf");
    }
    {
        test_dbf::DbfBuilder b{
            {"RACE_ID", 'C', 16},   {"NAME_TXT", 'C', 20},  {"RACE_TYPE", 'N', 3},
            {"PLAYABLE", 'C', 1},   {"REGEN_H", 'N', 3},    {"GUARDIAN", 'C', 16},
            {"LEADER_1", 'C', 16},  {"LEADER_2", 'C', 16},  {"LEADER_3", 'C', 16},
            {"LEADER_4", 'C', 16},  {"SOLDIER_1", 'C', 16}, {"SOLDIER_2", 'C', 16},
            {"SOLDIER_3", 'C', 16}, {"SOLDIER_4", 'C', 16}, {"SOLDIER_5", 'C', 16}};
        b.add_record({"G000SU0001", "L_HUMAN", "1", "T", "10", "G000UU0001", "", "", "", "", "", "",
                      "", "", ""});
        b.write(tmp / "Grace.dbf");
    }
    {
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
        b.add_record({"G000UU0001", "L_DOPPELGANGER",
                      "",           "",
                      "G000SU0001", "0",
                      "1",          "0",
                      "1",          "1",
                      "0",          "0",
                      "1",          "65",
                      "2",          "0",
                      "0",          "0",
                      "0",          "50",
                      "100",        "",
                      "",           "",
                      "",           "",
                      "",           "",
                      "",           "",
                      "0",          "1",
                      "L_HUMAN"});
        b.write(tmp / "Gunits.dbf");
    }
    {
        test_dbf::DbfBuilder b{
            {"ATT_ID", 'C', 16},    {"NAME_TXT", 'C', 20},  {"DESC_TXT", 'C', 20},
            {"CLASS", 'N', 2},      {"SOURCE", 'N', 2},     {"REACH", 'N', 2},
            {"INITIATIVE", 'N', 3}, {"QTY_DAM", 'N', 4},    {"QTY_HEAL", 'N', 3},
            {"POWER", 'N', 3},      {"INFINITE", 'N', 1},   {"CRIT_HIT", 'N', 1},
            {"WARD1", 'C', 40},     {"WARD2", 'C', 40},     {"WARD3", 'C', 40},
            {"WARD4", 'C', 40},     {"ALT_ATTACK", 'C', 16}};
        b.write(tmp / "Gattacks.dbf");
    }
    {
        test_dbf::DbfBuilder b{
            {"UPGRD_ID", 'C', 16}, {"NAME_TXT", 'C', 20}, {"DESC_TXT", 'C', 20},
            {"HIT_POINT", 'N', 4}, {"ARMOR", 'N', 3},     {"REGEN", 'N', 3},
            {"DAMAGE", 'N', 4},    {"HEAL", 'N', 4},      {"POWER", 'N', 4},
            {"XP_KILLED", 'N', 5}, {"XP_NEXT", 'N', 6},   {"INITIATIVE", 'N', 4},
            {"MOVE", 'N', 3},      {"NEGOTIATE", 'N', 3}, {"ENROLL_C", 'C', 40},
            {"REVIVE_C", 'C', 40}, {"HEAL_C", 'C', 40},   {"TRAINING_C", 'C', 40}};
        b.write(tmp / "GDynUpgr.dbf");
    }
    return tmp;
}

d2engine::GameDataRegistry make_minimal_gd(const std::string& suffix = "") {
    return d2engine::GameDataRegistry(write_minimal_gd(suffix));
}

// Fixture with Doppelganger-class attack for no-valid-actions test
std::filesystem::path write_doppelganger_gd() {
    static std::atomic<unsigned> c{0};
    auto tmp = std::filesystem::temp_directory_path() / ("d2sweep_dopp_" + std::to_string(c++));
    std::filesystem::remove_all(tmp);
    std::filesystem::create_directories(tmp);
    {
        test_dbf::DbfBuilder b{{"TXT_ID", 'C', 20}, {"TEXT", 'C', 50}};
        b.add_record({"L_HUMAN", "Human"});
        b.add_record({"L_DOPPELGANGER", "Doppelganger"});
        b.add_record({"L_WEAPON", "Weapon"});
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
        b.add_record({"14", "L_DOPPELGANGER"});
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
        b.write(tmp / "LattS.dbf");
    }
    {
        test_dbf::DbfBuilder b{
            {"RACE_ID", 'C', 16},   {"NAME_TXT", 'C', 20},  {"RACE_TYPE", 'N', 3},
            {"PLAYABLE", 'C', 1},   {"REGEN_H", 'N', 3},    {"GUARDIAN", 'C', 16},
            {"LEADER_1", 'C', 16},  {"LEADER_2", 'C', 16},  {"LEADER_3", 'C', 16},
            {"LEADER_4", 'C', 16},  {"SOLDIER_1", 'C', 16}, {"SOLDIER_2", 'C', 16},
            {"SOLDIER_3", 'C', 16}, {"SOLDIER_4", 'C', 16}, {"SOLDIER_5", 'C', 16}};
        b.add_record({"G000SU0001", "L_HUMAN", "1", "T", "10", "G000UU0001", "", "", "", "", "", "",
                      "", "", ""});
        b.write(tmp / "Grace.dbf");
    }
    {
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
        b.add_record({"G000UD0001", "L_DOPPELGANGER",
                      "",           "",
                      "G000SU0001", "0",
                      "1",          "0",
                      "1",          "1",
                      "0",          "0",
                      "1",          "65",
                      "2",          "0",
                      "0",          "0",
                      "0",          "0",
                      "50",         "100",
                      "",           "",
                      "",           "",
                      "",           "G000AT0098",
                      "",           "",
                      "",           "",
                      "",           "",
                      "",           "0",
                      "1",          "L_HUMAN"});
        b.write(tmp / "Gunits.dbf");
    }
    {
        test_dbf::DbfBuilder b{
            {"ATT_ID", 'C', 16},    {"NAME_TXT", 'C', 20},  {"DESC_TXT", 'C', 20},
            {"CLASS", 'N', 2},      {"SOURCE", 'N', 2},     {"REACH", 'N', 2},
            {"INITIATIVE", 'N', 3}, {"QTY_DAM", 'N', 4},    {"QTY_HEAL", 'N', 3},
            {"POWER", 'N', 3},      {"INFINITE", 'N', 1},   {"CRIT_HIT", 'N', 1},
            {"WARD1", 'C', 40},     {"WARD2", 'C', 40},     {"WARD3", 'C', 40},
            {"WARD4", 'C', 40},     {"ALT_ATTACK", 'C', 16}};
        b.add_record({"G000AT0098", "", "", "14", "1", "2", "50", "30", "0", "60", "1", "0", "", "",
                      "", "", ""});
        b.write(tmp / "Gattacks.dbf");
    }
    {
        test_dbf::DbfBuilder b{
            {"UPGRD_ID", 'C', 16}, {"NAME_TXT", 'C', 20}, {"DESC_TXT", 'C', 20},
            {"HIT_POINT", 'N', 4}, {"ARMOR", 'N', 3},     {"REGEN", 'N', 3},
            {"DAMAGE", 'N', 4},    {"HEAL", 'N', 4},      {"POWER", 'N', 4},
            {"XP_KILLED", 'N', 5}, {"XP_NEXT", 'N', 6},   {"INITIATIVE", 'N', 4},
            {"MOVE", 'N', 3},      {"NEGOTIATE", 'N', 3}, {"ENROLL_C", 'C', 40},
            {"REVIVE_C", 'C', 40}, {"HEAL_C", 'C', 40},   {"TRAINING_C", 'C', 40}};
        b.write(tmp / "GDynUpgr.dbf");
    }
    return tmp;
}

d2engine::GameDataRegistry make_doppelganger_gd() {
    return d2engine::GameDataRegistry(write_doppelganger_gd());
}

// Fixture with Damage-class attack for heading tests
std::filesystem::path write_heading_gd() {
    static std::atomic<unsigned> c{0};
    auto tmp = std::filesystem::temp_directory_path() / ("d2sweep_hdg_" + std::to_string(c++));
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
        b.add_record({"G000SU0001", "L_HUMAN", "1", "T", "10", "G000UH0001", "", "", "", "", "", "",
                      "", "", ""});
        b.write(tmp / "Grace.dbf");
    }
    {
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
        b.add_record({"G000UH0001", "",  "",  "",           "G000SU0001", "0",      "1", "0",
                      "1",          "1", "0", "0",          "1",          "150",    "5", "0",
                      "1",          "1", "0", "0",          "75",         "150",    "",  "",
                      "",           "",  "",  "G000AT0010", "",           "",       "",  "",
                      "",           "",  "",  "",           "1",          "L_HUMAN"});
        b.add_record({"G000UH0002", "",  "",  "",           "G000SU0001", "0",      "1", "0",
                      "1",          "1", "0", "0",          "1",          "150",    "5", "0",
                      "1",          "1", "0", "0",          "75",         "150",    "",  "",
                      "",           "",  "",  "G000AT0010", "",           "",       "",  "",
                      "",           "",  "",  "",           "1",          "L_HUMAN"});
        b.write(tmp / "Gunits.dbf");
    }
    {
        test_dbf::DbfBuilder b{
            {"ATT_ID", 'C', 16},    {"NAME_TXT", 'C', 20},  {"DESC_TXT", 'C', 20},
            {"CLASS", 'N', 2},      {"SOURCE", 'N', 2},     {"REACH", 'N', 2},
            {"INITIATIVE", 'N', 3}, {"QTY_DAM", 'N', 4},    {"QTY_HEAL", 'N', 3},
            {"POWER", 'N', 3},      {"INFINITE", 'N', 1},   {"CRIT_HIT", 'N', 1},
            {"WARD1", 'C', 40},     {"WARD2", 'C', 40},     {"WARD3", 'C', 40},
            {"WARD4", 'C', 40},     {"ALT_ATTACK", 'C', 16}};
        b.add_record({"G000AT0010", "", "", "1", "1", "2", "40", "25", "0", "50", "1", "0", "", "",
                      "", "", ""});
        b.write(tmp / "Gattacks.dbf");
    }
    {
        test_dbf::DbfBuilder b{
            {"UPGRD_ID", 'C', 16}, {"NAME_TXT", 'C', 20}, {"DESC_TXT", 'C', 20},
            {"HIT_POINT", 'N', 4}, {"ARMOR", 'N', 3},     {"REGEN", 'N', 3},
            {"DAMAGE", 'N', 4},    {"HEAL", 'N', 4},      {"POWER", 'N', 4},
            {"XP_KILLED", 'N', 5}, {"XP_NEXT", 'N', 6},   {"INITIATIVE", 'N', 4},
            {"MOVE", 'N', 3},      {"NEGOTIATE", 'N', 3}, {"ENROLL_C", 'C', 40},
            {"REVIVE_C", 'C', 40}, {"HEAL_C", 'C', 40},   {"TRAINING_C", 'C', 40}};
        b.write(tmp / "GDynUpgr.dbf");
    }
    return tmp;
}

d2engine::GameDataRegistry make_heading_gd() {
    return d2engine::GameDataRegistry(write_heading_gd());
}

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

[[nodiscard]] std::string read_log(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

static std::atomic<unsigned> g_diag_counter{0};

[[nodiscard]] std::filesystem::path make_diag_log_path() {
    auto tmpdir = std::filesystem::temp_directory_path() /
                  ("d2sweep_diag_" + std::to_string(g_diag_counter.fetch_add(1)));
    std::filesystem::create_directories(tmpdir);
    return tmpdir / "test.log";
}

[[nodiscard]] std::filesystem::path
write_bundle_diag_gd(const std::string& suffix, const std::string& primary_attack_id,
                     int primary_class, const std::string& secondary_attack_id, int secondary_class,
                     bool include_secondary) {
    static std::atomic<unsigned> counter{0};
    auto tmp = std::filesystem::temp_directory_path() /
               ("d2sweep_bundle_diag_" + std::to_string(counter++) + suffix);
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
        b.add_record({"0", "L_DAMAGE"});
        b.add_record({"5", "L_BOOST_DAMAGE"});
        b.add_record({"23", "L_CURE"});
        b.write(tmp / "LattC.dbf");
    }
    {
        test_dbf::DbfBuilder b{{"ID", 'N', 2}, {"TEXT", 'C', 20}};
        b.add_record({"1", "L_ALL"});
        b.add_record({"2", "L_ANY"});
        b.write(tmp / "LAttR.dbf");
    }
    {
        test_dbf::DbfBuilder b{{"ID", 'N', 2}, {"TEXT", 'C', 20}};
        b.add_record({"1", "L_WEAPON"});
        b.write(tmp / "LattS.dbf");
    }
    {
        test_dbf::DbfBuilder b{
            {"RACE_ID", 'C', 16},   {"NAME_TXT", 'C', 20},  {"RACE_TYPE", 'N', 3},
            {"PLAYABLE", 'C', 1},   {"REGEN_H", 'N', 3},    {"GUARDIAN", 'C', 16},
            {"LEADER_1", 'C', 16},  {"LEADER_2", 'C', 16},  {"LEADER_3", 'C', 16},
            {"LEADER_4", 'C', 16},  {"SOLDIER_1", 'C', 16}, {"SOLDIER_2", 'C', 16},
            {"SOLDIER_3", 'C', 16}, {"SOLDIER_4", 'C', 16}, {"SOLDIER_5", 'C', 16}};
        b.add_record({"G000SU0001", "L_HUMAN", "1", "T", "10", "G000UD0001", "", "", "", "", "", "",
                      "", "", ""});
        b.write(tmp / "Grace.dbf");
    }
    {
        test_dbf::DbfBuilder b{
            {"ATT_ID", 'C', 16},    {"NAME_TXT", 'C', 20},  {"DESC_TXT", 'C', 20},
            {"CLASS", 'N', 2},      {"SOURCE", 'N', 2},     {"REACH", 'N', 2},
            {"INITIATIVE", 'N', 3}, {"QTY_DAM", 'N', 4},    {"QTY_HEAL", 'N', 3},
            {"POWER", 'N', 3},      {"INFINITE", 'N', 1},   {"CRIT_HIT", 'N', 1},
            {"WARD1", 'C', 40},     {"WARD2", 'C', 40},     {"WARD3", 'C', 40},
            {"WARD4", 'C', 40},     {"ALT_ATTACK", 'C', 16}};
        b.add_record({primary_attack_id, "", "", std::to_string(primary_class), "1", "2", "40",
                      "50", "0", "60", "1", "0", "", "", "", "", ""});
        if (include_secondary) {
            b.add_record({secondary_attack_id, "", "", std::to_string(secondary_class), "1", "2",
                          "40", "0", "0", "0", "1", "0", "", "", "", "", ""});
        }
        b.write(tmp / "Gattacks.dbf");
    }
    {
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
        b.add_record({"G000UD0001",
                      "",
                      "",
                      "",
                      "G000SU0001",
                      "0",
                      "1",
                      "0",
                      "1",
                      "1",
                      "0",
                      "0",
                      "1",
                      "100",
                      "2",
                      "0",
                      "2",
                      "1",
                      "0",
                      "0",
                      "0",
                      "0",
                      "",
                      "",
                      "",
                      primary_attack_id,
                      secondary_attack_id,
                      "",
                      "",
                      "",
                      "",
                      "",
                      "",
                      "1",
                      "L_HUMAN"});
        b.add_record({"G000UD0002", "",  "",  "",  "G000SU0001", "0", "1", "0",      "1",
                      "1",          "0", "0", "1", "100",        "2", "0", "2",      "1",
                      "0",          "0", "0", "0", "",           "",  "",  "",       "",
                      "",           "",  "",  "",  "",           "",  "1", "L_HUMAN"});
        b.write(tmp / "Gunits.dbf");
    }

    return tmp;
}

[[nodiscard]] std::string attack_bundle_block_from_log(const std::string& log) {
    const auto start = log.find("attack_bundle:\n");
    if (start == std::string::npos) {
        return {};
    }
    const auto end = log.find("state_fingerprint=", start);
    if (end == std::string::npos) {
        return log.substr(start);
    }
    return log.substr(start, end - start);
}

// ════════════════════════════════════════════════════════
// RandomPolicy, StableHash, PairGenerator, StackCatalog,
// CatalogDescriptor, BattleFilename, SequenceFormatter
// ════════════════════════════════════════════════════════

TEST(RandomPolicyTest, IsDeterministicForSameSeed) {
    RandomActionPolicy                         p1(42);
    RandomActionPolicy                         p2(42);
    std::vector<d2battle::BattleActionOutcome> fake(10);
    for (std::size_t i = 0; i < 20; ++i)
        EXPECT_EQ(p1.choose_index(fake), p2.choose_index(fake));
}

TEST(RandomPolicyTest, DiffersForDifferentSeeds) {
    RandomActionPolicy                         p1(42);
    RandomActionPolicy                         p2(99);
    std::vector<d2battle::BattleActionOutcome> fake(10);
    bool                                       all_same = true;
    for (std::size_t i = 0; i < 20; ++i) {
        if (p1.choose_index(fake) != p2.choose_index(fake))
            all_same = false;
    }
    EXPECT_FALSE(all_same);
}

TEST(RandomPolicyTest, NeverReturnsOutOfRange) {
    RandomActionPolicy p(42);
    for (std::size_t n = 1; n <= 20; ++n) {
        std::vector<d2battle::BattleActionOutcome> fake(n);
        for (std::size_t i = 0; i < 100; ++i) {
            std::size_t idx = p.choose_index(fake);
            EXPECT_LT(idx, n);
        }
    }
}

TEST(RandomPolicyTest, EmptyOutcomesReturnsZero) {
    RandomActionPolicy                         p(42);
    std::vector<d2battle::BattleActionOutcome> empty;
    EXPECT_EQ(p.choose_index(empty), 0);
}

TEST(StableHashTest, IsDeterministic) {
    EXPECT_EQ(stable_hash_64(12345, "stack_A", "stack_B"),
              stable_hash_64(12345, "stack_A", "stack_B"));
}

TEST(StableHashTest, DiffersForDifferentSeed) {
    EXPECT_NE(stable_hash_64(12345, "stack_A", "stack_B"),
              stable_hash_64(99999, "stack_A", "stack_B"));
}

TEST(StableHashTest, DiffersForDifferentStackIds) {
    EXPECT_NE(stable_hash_64(12345, "stack_A", "stack_B"),
              stable_hash_64(12345, "stack_C", "stack_D"));
}

TEST(StableHashTest, OrderMatters) {
    EXPECT_NE(stable_hash_64(12345, "A", "B"), stable_hash_64(12345, "B", "A"));
}

TEST(PairGeneratorTest, ThreeStacksGenerateSixDirectedPairs) {
    auto pairs = generate_directed_pairs({{"SA", "O1", "L1", {"U1"}, {}, "A"},
                                          {"SB", "O2", "L2", {"U2"}, {}, "B"},
                                          {"SC", "O3", "L3", {"U3"}, {}, "C"}});
    EXPECT_EQ(pairs.size(), 6);
}

TEST(PairGeneratorTest, PairGenerationNeverCreatesSelfBattle) {
    auto pairs =
        generate_directed_pairs({{"SA", "", "", {"U1"}, {}, "A"}, {"SB", "", "", {"U2"}, {}, "B"}});
    for (const auto& p : pairs)
        EXPECT_NE(p.party1_stack_id, p.party2_stack_id);
}

TEST(PairGeneratorTest, PairGenerationOrderIsDeterministic) {
    auto pairs = generate_directed_pairs(
        {{"A", "", "", {}, {}, ""}, {"B", "", "", {}, {}, ""}, {"C", "", "", {}, {}, ""}});
    ASSERT_EQ(pairs.size(), 6);
    EXPECT_EQ(pairs[0].party1_stack_id, "A");
    EXPECT_EQ(pairs[1].party1_stack_id, "A");
    EXPECT_EQ(pairs[2].party1_stack_id, "B");
    EXPECT_EQ(pairs[3].party1_stack_id, "B");
    EXPECT_EQ(pairs[4].party1_stack_id, "C");
    EXPECT_EQ(pairs[5].party1_stack_id, "C");
}

TEST(PairGeneratorTest, BothOrientationsAreGenerated) {
    auto pairs = generate_directed_pairs({{"SA", "", "", {}, {}, ""}, {"SB", "", "", {}, {}, ""}});
    ASSERT_EQ(pairs.size(), 2);
    bool found_ab = false, found_ba = false;
    for (const auto& p : pairs) {
        if (p.party1_stack_id == "SA" && p.party2_stack_id == "SB")
            found_ab = true;
        if (p.party1_stack_id == "SB" && p.party2_stack_id == "SA")
            found_ba = true;
    }
    EXPECT_TRUE(found_ab);
    EXPECT_TRUE(found_ba);
}

TEST(StackCatalogTest, CatalogSkipsEmptyStacks) {
    d2runtime::AdventureWorldState world;
    d2runtime::AdventureStack      stack;
    stack.id = "S1";
    stack.inside = "";
    stack.position = {10, 10};
    world.stacks.push_back(stack);
    EXPECT_EQ(build_stack_catalog(world, make_minimal_gd("_emp")).entries.size(), 0);
}

TEST(StackCatalogTest, CatalogSkipsStacksWithoutAliveMembers) {
    d2runtime::AdventureWorldState   world;
    d2runtime::AdventureUnitInstance unit;
    unit.id = "U1";
    unit.current_hp = 0;
    world.units.push_back(unit);
    d2runtime::AdventureStack stack;
    stack.id = "S1";
    stack.inside = "";
    stack.position = {10, 10};
    stack.group.members[0] = "U1";
    world.stacks.push_back(stack);
    EXPECT_EQ(build_stack_catalog(world, make_minimal_gd("_ded")).entries.size(), 0);
}

TEST(StackCatalogTest, CatalogContainsEligibleStack) {
    d2runtime::AdventureWorldState   world;
    d2runtime::AdventureUnitInstance unit;
    unit.id = "U1";
    unit.current_hp = 100;
    unit.name = "Hero";
    world.units.push_back(unit);
    d2runtime::AdventureStack stack;
    stack.id = "S_T";
    stack.inside = "";
    stack.position = {10, 10};
    stack.group.members[0] = "U1";
    stack.leader_id = "U1";
    world.stacks.push_back(stack);
    auto r = build_stack_catalog(world, make_minimal_gd("_el"));
    ASSERT_EQ(r.entries.size(), 1);
    EXPECT_FALSE(r.entries[0].human_descriptor.empty());
}

TEST(StackCatalogTest, CatalogSkipsStacksInsideCity) {
    d2runtime::AdventureWorldState   world;
    d2runtime::AdventureUnitInstance unit;
    unit.id = "U1";
    unit.current_hp = 100;
    world.units.push_back(unit);
    d2runtime::AdventureStack stack;
    stack.id = "S1";
    stack.inside = "G000G00001";
    stack.position = {10, 10};
    stack.group.members[0] = "U1";
    world.stacks.push_back(stack);
    EXPECT_EQ(build_stack_catalog(world, make_minimal_gd("_ct")).entries.size(), 0);
}

TEST(StackCatalogTest, CatalogOrderIsCanonicalByStackId) {
    d2runtime::AdventureWorldState world;
    for (int i = 2; i >= 0; --i) {
        d2runtime::AdventureUnitInstance unit;
        unit.id = "U" + std::to_string(i);
        unit.current_hp = 100;
        world.units.push_back(unit);
        d2runtime::AdventureStack stack;
        stack.id = "S" + std::to_string(i);
        stack.inside = "";
        stack.position = {10, i};
        stack.group.members[0] = "U" + std::to_string(i);
        world.stacks.push_back(stack);
    }
    auto r = build_stack_catalog(world, make_minimal_gd("_or"));
    ASSERT_EQ(r.entries.size(), 3);
    EXPECT_EQ(r.entries[0].stack_id, "S0");
    EXPECT_EQ(r.entries[1].stack_id, "S1");
    EXPECT_EQ(r.entries[2].stack_id, "S2");
}

TEST(StackCatalogTest, LargeUnitFormationIsPreserved) {
    d2runtime::AdventureWorldState   world;
    d2runtime::AdventureUnitInstance unit;
    unit.id = "U_B";
    unit.current_hp = 200;
    unit.name = "Dragon";
    world.units.push_back(unit);
    d2runtime::AdventureStack stack;
    stack.id = "S_D";
    stack.inside = "";
    stack.position = {10, 10};
    stack.group.members[0] = "U_B";
    stack.group.cell_members = {0, 0, -1, -1, -1, -1};
    world.stacks.push_back(stack);
    auto r = build_stack_catalog(world, make_minimal_gd("_bg"));
    ASSERT_EQ(r.entries.size(), 1);
    EXPECT_EQ(r.entries[0].cell_members[0], 0);
    EXPECT_EQ(r.entries[0].cell_members[1], 0);
}

TEST(CatalogDescriptorTest, UsesUnitDefNameWhenDisplayNameIsEmpty) {
    d2runtime::AdventureWorldState   world;
    d2runtime::AdventureUnitInstance unit;
    unit.id = "U1";
    unit.current_hp = 100;
    unit.name = "";
    unit.type_id = "G000UU0001";
    world.units.push_back(unit);
    d2runtime::AdventureStack stack;
    stack.id = "S1";
    stack.inside = "";
    stack.position = {0, 0};
    stack.group.members[0] = "U1";
    world.stacks.push_back(stack);
    auto r = build_stack_catalog(world, make_minimal_gd("_ud"));
    ASSERT_EQ(r.entries.size(), 1);
    EXPECT_EQ(r.entries[0].human_descriptor, "Doppelganger");
    auto fn = make_battle_filename(1, "S1", r.entries[0].human_descriptor, "S2", "B");
    EXPECT_EQ(fn.substr(0, 6), "000001");
    EXPECT_NE(fn.find("Doppelganger"), std::string::npos);
}

TEST(CatalogDescriptorTest, PrefersSgDisplayName) {
    d2runtime::AdventureWorldState   world;
    d2runtime::AdventureUnitInstance unit;
    unit.id = "U1";
    unit.current_hp = 100;
    unit.name = "SirHero";
    unit.type_id = "G000UU0001";
    world.units.push_back(unit);
    d2runtime::AdventureStack stack;
    stack.id = "S1";
    stack.inside = "";
    stack.position = {0, 0};
    stack.group.members[0] = "U1";
    world.stacks.push_back(stack);
    auto r = build_stack_catalog(world, make_minimal_gd("_sp"));
    ASSERT_EQ(r.entries.size(), 1);
    EXPECT_EQ(r.entries[0].human_descriptor, "SirHero");
    auto fn = make_battle_filename(1, "S1", r.entries[0].human_descriptor, "S2", "B");
    EXPECT_NE(fn.find("SirHero"), std::string::npos);
}

TEST(SequenceFormatterTest, FormatsSixDigitZeroPadded) {
    EXPECT_EQ(format_battle_sequence(1), "000001");
    EXPECT_EQ(format_battle_sequence(17), "000017");
}

TEST(BattleFilenameTest, SequenceNumberSixDigitZeroPadded) {
    EXPECT_EQ(make_battle_filename(17, "S1", "A", "S2", "B").substr(0, 6), "000017");
}

TEST(AttackSupportTest, ExhaustiveClassSupport) {
    using AC = d2engine::AttackClass;
    std::vector<std::pair<AC, bool>> cs = {{AC::Damage, true},
                                           {AC::Drain, true},
                                           {AC::Paralyze, false},
                                           {AC::Heal, true},
                                           {AC::Fear, false},
                                           {AC::BoostDamage, false},
                                           {AC::Petrify, true},
                                           {AC::LowerDamage, false},
                                           {AC::LowerInitiative, false},
                                           {AC::Poison, false},
                                           {AC::Frostbite, false},
                                           {AC::Revive, true},
                                           {AC::DrainOverflow, true},
                                           {AC::Cure, true},
                                           {AC::Summon, false},
                                           {AC::DrainLevel, false},
                                           {AC::GiveAttack, false},
                                           {AC::Doppelganger, false},
                                           {AC::TransformSelf, false},
                                           {AC::TransformOther, false},
                                           {AC::Blister, false},
                                           {AC::BestowWards, false},
                                           {AC::Shatter, false},
                                           {AC::Unknown, false}};
    for (const auto& [cls, exp] : cs) {
        d2engine::AttackDef atk;
        atk.attack_class = cls;
        EXPECT_EQ(d2battle::is_attack_class_supported(atk), exp);
    }
}

TEST(AttackSupportTest, ExhaustiveReachSupport) {
    using AR = d2engine::AttackReach;
    using AC = d2engine::AttackClass;
    auto chk = [](AC cls, AR reach, bool exp) {
        d2engine::AttackDef atk;
        atk.attack_class = cls;
        atk.reach = reach;
        EXPECT_EQ(d2battle::is_attack_reach_supported(atk), exp)
            << " class=" << static_cast<int>(cls) << " reach=" << static_cast<int>(reach);
    };
    auto all = [&](AC cls, bool adj, bool any, bool allv) {
        chk(cls, AR::Adjacent, adj);
        chk(cls, AR::Any, any);
        chk(cls, AR::All, allv);
    };
    // Damage/Drain/DrainOverflow/Petrify support Adjacent+Any+All
    all(AC::Damage, true, true, true);
    all(AC::Drain, true, true, true);
    all(AC::DrainOverflow, true, true, true);
    all(AC::Petrify, true, true, true);
    // Heal/Cure support Any+All, NOT Adjacent
    all(AC::Heal, false, true, true);
    all(AC::Cure, false, true, true);
    // Revive supports Any+All, NOT Adjacent
    all(AC::Revive, false, true, true);
    // Unknown supports nothing
    all(AC::Unknown, false, false, false);
    chk(AC::Unknown, AR::Unknown, false);
}

// ════════════════════════════════════════════════════════
// Heading regression tests
// ════════════════════════════════════════════════════════

TEST(BattlePreviewTest, DamageHeadingAppearsExactlyOnce) {
    auto                  gd = make_heading_gd();
    d2battle::BattleState state;
    state.status = d2battle::BattleStatus::InProgress;
    state.party1.source_stack_id = "S1";
    state.party1.owner = "P1";
    state.party1.leader_id = "U1";
    state.party1.leader_alive = 1;
    state.party1.members[0] = "U1";
    state.party1.cell_members = {0, -1, -1, -1, -1, -1};
    state.party1.positions = {0, -1, -1, -1, -1, -1};
    state.party2.source_stack_id = "S2";
    state.party2.owner = "P2";
    state.party2.leader_id = "U2";
    state.party2.leader_alive = 1;
    state.party2.members[0] = "U2";
    state.party2.cell_members = {-1, -1, 0, -1, -1, -1};
    state.party2.positions = {2, -1, -1, -1, -1, -1};
    d2battle::BattleUnitState u1;
    u1.id = "U1";
    u1.type_id = "G000UH0001";
    u1.side = d2battle::BattleSide::Party1;
    u1.member_index = 0;
    u1.current_hp = 100;
    u1.alive = true;
    u1.formation_cell = 0;
    state.units.push_back(u1);
    d2battle::BattleUnitState u2;
    u2.id = "U2";
    u2.type_id = "G000UH0002";
    u2.side = d2battle::BattleSide::Party2;
    u2.member_index = 0;
    u2.current_hp = 100;
    u2.alive = true;
    u2.formation_cell = 2;
    state.units.push_back(u2);
    d2battle::detail::normalize_derived_side_state(state);
    d2battle::detail::normalize_battle_status(state);
    d2battle::detail::begin_round(state, 1, gd);
    auto outcomes = d2battle::valid_action_outcomes(state, gd);
    ASSERT_GE(outcomes.size(), 1u);
    std::ostringstream oss;
    print_actions_menu(oss, state, outcomes, gd);
    std::string output = oss.str();
    EXPECT_EQ(count_occurrences(output, "DAMAGE:\n"), 1u);
    EXPECT_EQ(count_occurrences(output, "DAMAGE:\nDAMAGE:\n"), 0u);
}

TEST(BattleSelectedTest, DamageHeadingAppearsExactlyOnce) {
    auto                  gd = make_heading_gd();
    d2battle::BattleState state;
    state.status = d2battle::BattleStatus::InProgress;
    state.party1.source_stack_id = "S1";
    state.party1.owner = "P1";
    state.party1.leader_id = "U1";
    state.party1.leader_alive = 1;
    state.party1.members[0] = "U1";
    state.party1.cell_members = {0, -1, -1, -1, -1, -1};
    state.party1.positions = {0, -1, -1, -1, -1, -1};
    state.party2.source_stack_id = "S2";
    state.party2.owner = "P2";
    state.party2.leader_id = "U2";
    state.party2.leader_alive = 1;
    state.party2.members[0] = "U2";
    state.party2.cell_members = {-1, -1, 0, -1, -1, -1};
    state.party2.positions = {2, -1, -1, -1, -1, -1};
    d2battle::BattleUnitState u1;
    u1.id = "U1";
    u1.type_id = "G000UH0001";
    u1.side = d2battle::BattleSide::Party1;
    u1.member_index = 0;
    u1.current_hp = 100;
    u1.alive = true;
    u1.formation_cell = 0;
    state.units.push_back(u1);
    d2battle::BattleUnitState u2;
    u2.id = "U2";
    u2.type_id = "G000UH0002";
    u2.side = d2battle::BattleSide::Party2;
    u2.member_index = 0;
    u2.current_hp = 100;
    u2.alive = true;
    u2.formation_cell = 2;
    state.units.push_back(u2);
    d2battle::detail::normalize_derived_side_state(state);
    d2battle::detail::normalize_battle_status(state);
    d2battle::detail::begin_round(state, 1, gd);
    auto outcomes = d2battle::valid_action_outcomes(state, gd);
    ASSERT_GE(outcomes.size(), 1u);
    ASSERT_TRUE(std::holds_alternative<d2battle::AttackAction>(outcomes[0].action));
    std::ostringstream oss;
    print_selected_action(oss, state, outcomes[0].action, outcomes[0].outcome, gd);
    std::string output = oss.str();
    EXPECT_EQ(count_occurrences(output, "DAMAGE:\n"), 1u);
    EXPECT_EQ(count_occurrences(output, "DAMAGE:\nDAMAGE:\n"), 0u);
}

// ════════════════════════════════════════════════════════
// Simulator integration tests
// ════════════════════════════════════════════════════════

d2battle::BattleState make_doppelganger_state(const d2engine::GameDataRegistry& gd) {
    d2battle::BattleState state;
    state.status = d2battle::BattleStatus::InProgress;
    state.party1.source_stack_id = "S1";
    state.party1.owner = "P1";
    state.party1.leader_id = "UD";
    state.party1.leader_alive = 1;
    state.party1.members[0] = "UD";
    state.party1.cell_members = {0, -1, -1, -1, -1, -1};
    state.party1.positions = {0, -1, -1, -1, -1, -1};
    state.party2.source_stack_id = "S2";
    state.party2.owner = "P2";
    state.party2.leader_id = "UE";
    state.party2.leader_alive = 1;
    state.party2.members[0] = "UE";
    state.party2.cell_members = {-1, -1, 0, -1, -1, -1};
    state.party2.positions = {2, -1, -1, -1, -1, -1};
    d2battle::BattleUnitState ud;
    ud.id = "UD";
    ud.type_id = "G000UD0001";
    ud.side = d2battle::BattleSide::Party1;
    ud.member_index = 0;
    ud.current_hp = 65;
    ud.alive = true;
    ud.formation_cell = 0;
    state.units.push_back(ud);
    d2battle::BattleUnitState ue;
    ue.id = "UE";
    ue.type_id = "G000UD0001";
    ue.side = d2battle::BattleSide::Party2;
    ue.member_index = 0;
    ue.current_hp = 65;
    ue.alive = true;
    ue.formation_cell = 2;
    state.units.push_back(ue);
    d2battle::detail::normalize_derived_side_state(state);
    d2battle::detail::normalize_battle_status(state);
    d2battle::detail::begin_round(state, 1, gd);
    return state;
}

TEST(NoValidActionsTest, FooterContainsDiagnosticViaSimulator) {
    auto        gd = make_doppelganger_gd();
    const auto* unit = gd.find_unit("G000UD0001");
    ASSERT_NE(unit, nullptr);
    ASSERT_NE(unit->primary_attack, nullptr);
    EXPECT_EQ(unit->primary_attack->attack_class, d2engine::AttackClass::Doppelganger);
    auto state = make_doppelganger_state(gd);
    auto outcomes = d2battle::valid_action_outcomes(state, gd);
    ASSERT_TRUE(outcomes.empty());
    std::string init_fp = d2battle::compute_fingerprint(state);
    const auto* actor = state.current_actor();
    ASSERT_NE(actor, nullptr);
    auto path = make_diag_log_path();
    {
        BattleLogWriter    w(path);
        RandomActionPolicy p(42);
        SimulatorConfig    cfg;
        cfg.max_actions = 1000;
        cfg.max_rounds = 100;
        auto result = run_one_battle(state, gd, p, w, cfg);
        EXPECT_EQ(result.status, BattleRunStatus::AbortedNoValidActions);
        EXPECT_EQ(result.diagnostic, "no valid actions for live current actor");
        EXPECT_EQ(result.current_actor_id, actor->id);
        EXPECT_FALSE(result.current_actor_label.empty());
        EXPECT_EQ(result.final_fingerprint, init_fp);
    }
    auto log = read_log(path);
    EXPECT_EQ(count_occurrences(log, "diagnostic=no valid actions for live current actor\n"), 1u);
    EXPECT_EQ(count_occurrences(log, "status=ABORTED_NO_VALID_ACTIONS\n"), 1u);
    EXPECT_GE(count_occurrences(log, "reason=NO_VALID_ACTIONS_FOR_LIVE_CURRENT_ACTOR\n"), 1u);
    EXPECT_NE(log.find("FATAL BATTLE INVARIANT"), std::string::npos);
}

TEST(RuleExceptionTest, FooterContainsDiagnosticViaSimulator) {
    auto                  gd = make_minimal_gd("_rexc");
    d2battle::BattleState state;
    state.status = d2battle::BattleStatus::InProgress;
    state.party1.members[0] = "U1";
    state.party1.leader_id = "U1";
    state.party1.leader_alive = 1;
    state.party1.cell_members = {0, -1, -1, -1, -1, -1};
    state.party1.positions = {0, -1, -1, -1, -1, -1};
    state.party2.members[0] = "U2";
    state.party2.leader_id = "U2";
    state.party2.leader_alive = 1;
    state.party2.cell_members = {0, -1, -1, -1, -1, -1};
    state.party2.positions = {2, -1, -1, -1, -1, -1};
    d2battle::BattleUnitState u1;
    u1.id = "U1";
    u1.type_id = "G000UU0001";
    u1.side = d2battle::BattleSide::Party1;
    u1.member_index = 0;
    u1.current_hp = 65;
    u1.alive = true;
    u1.formation_cell = 0;
    state.units.push_back(u1);
    d2battle::BattleUnitState u2;
    u2.id = "U2";
    u2.type_id = "G000UU0001";
    u2.side = d2battle::BattleSide::Party2;
    u2.member_index = 0;
    u2.current_hp = 65;
    u2.alive = true;
    u2.formation_cell = 2;
    state.units.push_back(u2);
    for (auto& bs : {&state.party1, &state.party2}) {
        for (int c = 0; c < 6; ++c)
            bs->cell_members[static_cast<std::size_t>(c)] = -1;
        for (auto& member : bs->members)
            member = std::nullopt;
    }
    d2battle::detail::normalize_derived_side_state(state);
    state.round_state.round_number = 1;
    state.round_state.current_turn_index = 0;
    auto path = make_diag_log_path();
    {
        BattleLogWriter    w(path);
        RandomActionPolicy p(42);
        SimulatorConfig    cfg;
        auto               result = run_one_battle(state, gd, p, w, cfg);
        EXPECT_EQ(result.status, BattleRunStatus::AbortedRuleException);
        EXPECT_TRUE(result.diagnostic.find("exception: ") == 0);
    }
    auto log = read_log(path);
    EXPECT_EQ(count_occurrences(log, "status=ABORTED_RULE_EXCEPTION\n"), 1u);
    EXPECT_EQ(count_occurrences(log, "reason=RULE_EXCEPTION\n"), 1u);
    EXPECT_NE(log.find("diagnostic=exception: "), std::string::npos);
}

TEST(LimitFooterTest, ActionLimitFooterPreservesCurrentActor) {
    auto        gd = make_doppelganger_gd();
    auto        state = make_doppelganger_state(gd);
    const auto* actor = state.current_actor();
    ASSERT_NE(actor, nullptr);
    auto path = make_diag_log_path();
    {
        BattleLogWriter    w(path);
        RandomActionPolicy p(42);
        SimulatorConfig    cfg;
        cfg.max_actions = 0;
        auto result = run_one_battle(state, gd, p, w, cfg);
        EXPECT_EQ(result.status, BattleRunStatus::AbortedActionLimit);
        EXPECT_EQ(result.current_actor_id, actor->id);
        EXPECT_FALSE(result.current_actor_label.empty());
        EXPECT_EQ(result.diagnostic, "action limit exceeded");
    }
    auto log = read_log(path);
    EXPECT_GE(count_occurrences(log, "current_actor=" + actor->id + "\n"), 1u);
    EXPECT_EQ(count_occurrences(log, "current_actor=<limit-reached>\n"), 0u);
    EXPECT_EQ(count_occurrences(log, "current_actor=<missing-current-actor>\n"), 0u);
    EXPECT_EQ(count_occurrences(log, "diagnostic=action limit exceeded\n"), 1u);
}

TEST(LimitFooterTest, RoundLimitFooterPreservesCurrentActor) {
    auto        gd = make_doppelganger_gd();
    auto        state = make_doppelganger_state(gd);
    const auto* actor = state.current_actor();
    ASSERT_NE(actor, nullptr);
    auto path = make_diag_log_path();
    {
        BattleLogWriter    w(path);
        RandomActionPolicy p(42);
        SimulatorConfig    cfg;
        cfg.max_rounds = 0;
        auto result = run_one_battle(state, gd, p, w, cfg);
        EXPECT_EQ(result.status, BattleRunStatus::AbortedRoundLimit);
        EXPECT_EQ(result.current_actor_id, actor->id);
        EXPECT_FALSE(result.current_actor_label.empty());
        EXPECT_EQ(result.diagnostic, "round limit exceeded");
    }
    auto log = read_log(path);
    EXPECT_GE(count_occurrences(log, "current_actor=" + actor->id + "\n"), 1u);
    EXPECT_EQ(count_occurrences(log, "current_actor=<limit-reached>\n"), 0u);
    EXPECT_EQ(count_occurrences(log, "diagnostic=round limit exceeded\n"), 1u);
}

// ════════════════════════════════════════════════════════
// Writer-level diagnostic tests
// ════════════════════════════════════════════════════════

TEST(AbortedFooterTest, DiagnosticFieldAppearsEvenWhenEmpty) {
    auto path = make_diag_log_path();
    {
        BattleLogWriter w(path);
        w.write_aborted(BattleRunStatus::AbortedNoValidActions,
                        "NO_VALID_ACTIONS_FOR_LIVE_CURRENT_ACTOR", "", 1, 0, "actor1",
                        "P1#0 unit (type)", "fp");
    }
    auto log = read_log(path);
    EXPECT_EQ(count_occurrences(log, "diagnostic=<none>\n"), 1u);
}

TEST(AbortedFooterTest, DiagnosticFieldContainsExactMessage) {
    auto path = make_diag_log_path();
    {
        BattleLogWriter w(path);
        w.write_aborted(BattleRunStatus::AbortedRuleException, "RULE_EXCEPTION",
                        "exception: bad thing", 0, 0, "actor1", "", "");
    }
    auto log = read_log(path);
    EXPECT_EQ(count_occurrences(log, "diagnostic=exception: bad thing\n"), 1u);
}

TEST(NoActionsDiagnosticReportsWholeBundleFailure, ExactBundleBlocks) {
    {
        auto gd_path = write_bundle_diag_gd("_unsupported", "G_AT_BD", 5, "G_AT_CR", 23, true);
        d2engine::GameDataRegistry gd(gd_path);

        d2runtime::AdventureWorldState w;
        w.units.push_back({"A0", "g000ud0001", 1, {}, 0, "Actor", 0, {}, 100, 0});
        w.units.push_back({"B0", "g000ud0002", 1, {}, 0, "Enemy", 0, {}, 100, 0});
        d2runtime::AdventureStack s1;
        s1.id = "S1";
        s1.owner = "O1";
        s1.leader_id = "A0";
        s1.group.members[0] = "A0";
        s1.group.positions[0] = 0;
        s1.group.cell_members[0] = 0;
        d2runtime::AdventureStack s2;
        s2.id = "S2";
        s2.owner = "O2";
        s2.leader_id = "B0";
        s2.group.members[0] = "B0";
        s2.group.positions[0] = 0;
        s2.group.cell_members[0] = 0;
        w.stacks.push_back(s1);
        w.stacks.push_back(s2);
        auto state = d2battle::bootstrap_battle(s1, s2, w, gd);

        auto path = make_diag_log_path();
        {
            BattleLogWriter writer(path);
            writer.write_fatal_invariant("NO_ACTIONS", "S1", "S2", state, gd);
        }

        auto              log = read_log(path);
        auto              block = attack_bundle_block_from_log(log);
        const std::string expected = "attack_bundle:\n"
                                     "  support=UNSUPPORTED\n"
                                     "  error=unsupported_primary_class\n"
                                     "  primary:\n"
                                     "    present=yes\n"
                                     "    id=g_at_bd\n"
                                     "    definition_status=FOUND\n"
                                     "    class=BoostDamage\n"
                                     "    reach=Any\n"
                                     "    class_supported=no\n"
                                     "    reach_supported=yes\n"
                                     "  secondary:\n"
                                     "    present=yes\n"
                                     "    id=g_at_cr\n"
                                     "    definition_status=FOUND\n"
                                     "    class=Cure\n"
                                     "    reach=Any\n"
                                     "    class_supported=yes\n"
                                     "    reach_supported=yes\n";
        EXPECT_EQ(block, expected);
    }

    {
        auto gd_path = write_bundle_diag_gd("_primary_only", "G_AT_DM", 0, "G000000000", 23, false);
        d2engine::GameDataRegistry gd(gd_path);

        d2runtime::AdventureWorldState w;
        w.units.push_back({"A0", "g000ud0001", 1, {}, 0, "Actor", 0, {}, 100, 0});
        w.units.push_back({"B0", "g000ud0002", 1, {}, 0, "Enemy", 0, {}, 100, 0});
        d2runtime::AdventureStack s1;
        s1.id = "S1";
        s1.owner = "O1";
        s1.leader_id = "A0";
        s1.group.members[0] = "A0";
        s1.group.positions[0] = 0;
        s1.group.cell_members[0] = 0;
        d2runtime::AdventureStack s2;
        s2.id = "S2";
        s2.owner = "O2";
        s2.leader_id = "B0";
        s2.group.members[0] = "B0";
        s2.group.positions[0] = 0;
        s2.group.cell_members[0] = 0;
        w.stacks.push_back(s1);
        w.stacks.push_back(s2);
        auto state = d2battle::bootstrap_battle(s1, s2, w, gd);

        auto path = make_diag_log_path();
        {
            BattleLogWriter writer(path);
            writer.write_fatal_invariant("NO_ACTIONS", "S1", "S2", state, gd);
        }

        auto              log = read_log(path);
        auto              block = attack_bundle_block_from_log(log);
        const std::string expected = "attack_bundle:\n"
                                     "  support=SUPPORTED\n"
                                     "  error=none\n"
                                     "  primary:\n"
                                     "    present=yes\n"
                                     "    id=g_at_dm\n"
                                     "    definition_status=FOUND\n"
                                     "    class=Damage\n"
                                     "    reach=Any\n"
                                     "    class_supported=yes\n"
                                     "    reach_supported=yes\n"
                                     "  secondary:\n"
                                     "    present=no\n"
                                     "    id=\n"
                                     "    definition_status=NOT_PRESENT\n"
                                     "    class=\n"
                                     "    reach=\n"
                                     "    class_supported=no\n"
                                     "    reach_supported=no\n";
        EXPECT_EQ(block, expected);
    }
}

} // namespace
