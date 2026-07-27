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
#include <d2battle_rules/detail/battle_attack_rules.hpp>
#include <d2battle_rules/detail/battle_derived.hpp>
#include <d2battle_rules/detail/battle_status.hpp>
#include <d2battle_rules/detail/battle_turn.hpp>
#include <d2battle_rules/detail/heal.hpp>
#include <d2battle_rules/detail/cure.hpp>
#include <d2battle_rules/detail/revive.hpp>
#include <d2battle_rules/detail/unit_effects.hpp>
#include <d2engine/assets/attack_def.hpp>
#include <d2engine/assets/game_data_registry.hpp>
#include <d2runtime/AdventureWorldState.hpp>
#include <opendis2_battle/terminal_view.hpp>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

using namespace d2battle;

// ── Common infrastructure helpers ───────────────────────────────────────────

void write_common_tables(const std::filesystem::path& dir) {
    // Tglobal
    {
        test_dbf::DbfBuilder b{{"TXT_ID", 'C', 20}, {"TEXT", 'C', 50}};
        b.add_record({"L_HUMAN", "Human"});
        b.write(dir / "Tglobal.dbf");
    }
    // Empty stubs
    for (const auto* n : {"LunitB.dbf", "LunitC.dbf", "LDthAnim.dbf", "GDynUpgr.dbf", "GMabi.dbf",
                          "Gimmu.dbf", "GimmuC.dbf"}) {
        std::ofstream ofs(dir / n, std::ios::binary);
        ofs.put(0x1a);
    }
    // LattC — IDs match AttackClass enum ordinals
    {
        test_dbf::DbfBuilder b{{"ID", 'N', 2}, {"TEXT", 'C', 20}};
        b.add_record({"0", "L_DAMAGE"});
        b.add_record({"1", "L_DRAIN"});
        b.add_record({"2", "L_PARALYZE"});
        b.add_record({"3", "L_HEAL"});
        b.add_record({"4", "L_FEAR"});
        b.add_record({"5", "L_BOOST_DAMAGE"});
        b.add_record({"6", "L_PETRIFY"});
        b.add_record({"7", "L_LOWER_DAMAGE"});
        b.add_record({"8", "L_LOWER_INITIATIVE"});
        b.add_record({"9", "L_POISON"});
        b.add_record({"10", "L_FROSTBITE"});
        b.add_record({"21", "L_REVIVE"});
        b.add_record({"22", "L_DRAIN_OVERFLOW"});
        b.add_record({"23", "L_CURE"});
        b.add_record({"30", "L_BLISTER"});
        b.add_record({"32", "L_SHATTER"});
        b.write(dir / "LattC.dbf");
    }
    // LAttR (reach labels)
    {
        test_dbf::DbfBuilder b{{"ID", 'N', 2}, {"TEXT", 'C', 20}};
        b.add_record({"1", "L_ALL"});
        b.add_record({"2", "L_ANY"});
        b.add_record({"3", "L_ADJACENT"});
        b.write(dir / "LAttR.dbf");
    }
    // LattS (source labels)
    {
        test_dbf::DbfBuilder b{{"ID", 'N', 2}, {"TEXT", 'C', 20}};
        b.add_record({"1", "L_WEAPON"});
        b.write(dir / "LattS.dbf");
    }
    // Grace (race)
    {
        test_dbf::DbfBuilder b{
            {"RACE_ID", 'C', 16},   {"NAME_TXT", 'C', 20},  {"RACE_TYPE", 'N', 3},
            {"PLAYABLE", 'C', 1},   {"REGEN_H", 'N', 3},    {"GUARDIAN", 'C', 16},
            {"LEADER_1", 'C', 16},  {"LEADER_2", 'C', 16},  {"LEADER_3", 'C', 16},
            {"LEADER_4", 'C', 16},  {"SOLDIER_1", 'C', 16}, {"SOLDIER_2", 'C', 16},
            {"SOLDIER_3", 'C', 16}, {"SOLDIER_4", 'C', 16}, {"SOLDIER_5", 'C', 16}};
        b.add_record({"G000SU0001", "L_HUMAN", "1", "T", "10", "G000UH0100", "", "", "", "", "", "",
                      "", "", ""});
        b.write(dir / "Grace.dbf");
    }
}

enum class ReachSpec { All = 1, Any = 2 };

test_dbf::DbfBuilder make_base_gattacks_builder() {
    return test_dbf::DbfBuilder{
        {"ATT_ID", 'C', 16},    {"NAME_TXT", 'C', 20}, {"DESC_TXT", 'C', 20},  {"CLASS", 'N', 2},
        {"SOURCE", 'N', 2},     {"REACH", 'N', 2},     {"INITIATIVE", 'N', 3}, {"QTY_DAM", 'N', 4},
        {"QTY_HEAL", 'N', 3},   {"POWER", 'N', 3},     {"INFINITE", 'N', 1},   {"CRIT_HIT", 'N', 1},
        {"WARD1", 'C', 40},     {"WARD2", 'C', 40},    {"WARD3", 'C', 40},     {"WARD4", 'C', 40},
        {"ALT_ATTACK", 'C', 16}};
}

test_dbf::DbfBuilder make_base_gunits_builder() {
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
    b.add_record({"G_CA_BASE", "",    "",  "",    "G000SU0001", "0", "1",      "0", "1", "1",
                  "0",         "0",   "1", "200", "2",          "0", "1",      "1", "0", "0",
                  "75",        "150", "",  "",    "",           "",  "",       "",  "",  "",
                  "",          "",    "",  "",    "",           "1", "L_HUMAN"});
    b.add_record({"G_CA_LRGE", "",    "",  "",    "G000SU0001", "0", "1",      "0", "0", "1",
                  "0",         "0",   "1", "300", "4",          "0", "1",      "1", "0", "0",
                  "75",        "150", "",  "",    "",           "",  "",       "",  "",  "",
                  "",          "",    "",  "",    "",           "1", "L_HUMAN"});
    return b;
}

template <typename F>
d2engine::GameDataRegistry make_gd_impl(const char* tag, std::atomic<unsigned>& counter,
                                        F&& write_extra) {
    auto dir =
        std::filesystem::temp_directory_path() / (std::string(tag) + std::to_string(counter++));
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    write_common_tables(dir);
    write_extra(dir);
    return d2engine::GameDataRegistry(dir);
}

// ── State builder helpers ───────────────────────────────────────────────────

d2battle::BattleState make_unsupported_state(const d2engine::GameDataRegistry& gd,
                                             const std::string&                actor_type_id) {
    d2runtime::AdventureWorldState w;
    w.units.push_back({"U0", actor_type_id, 1, {}, 0, "Actor", 0, {}, 200, 0});
    w.units.push_back({"E0", "G_CA_BASE", 1, {}, 0, "Enemy", 0, {}, 200, 0});

    d2runtime::AdventureStack s1;
    s1.id = "S1";
    s1.owner = "P1";
    s1.leader_id = "U0";
    s1.leader_alive = 1;
    s1.group.members[0] = "U0";
    s1.group.positions[0] = 0;
    s1.group.cell_members[0] = 0;
    s1.position = {21, 23};

    d2runtime::AdventureStack s2;
    s2.id = "S2";
    s2.owner = "P2";
    s2.leader_id = "E0";
    s2.leader_alive = 1;
    s2.group.members[0] = "E0";
    s2.group.positions[0] = 0;
    s2.group.cell_members[0] = 0;
    s2.position = {24, 20};

    return bootstrap_battle(s1, s2, w, gd);
}

d2battle::BattleState make_healcure_all_state(const d2engine::GameDataRegistry& gd) {
    d2runtime::AdventureWorldState w;
    w.units.push_back({"U0", "G_CA_HCAL", 1, {}, 0, "Actor", 0, {}, 200, 0});
    w.units.push_back({"U1", "G_CA_BASE", 1, {}, 0, "U1", 0, {}, 40, 0});
    w.units.push_back({"U2", "G_CA_BASE", 1, {}, 0, "U2", 0, {}, 200, 0});
    w.units.push_back({"U3", "G_CA_BASE", 1, {}, 0, "U3", 0, {}, 0, 0});
    w.units.push_back({"E0", "G_CA_BASE", 1, {}, 0, "Enemy", 0, {}, 100, 0});

    d2runtime::AdventureStack s1;
    s1.id = "S1";
    s1.owner = "P1";
    s1.leader_id = "U0";
    s1.leader_alive = 1;
    s1.group.members[0] = "U0";
    s1.group.members[1] = "U1";
    s1.group.members[2] = "U2";
    s1.group.members[3] = "U3";
    s1.group.positions[0] = 0;
    s1.group.positions[1] = 1;
    s1.group.positions[2] = 2;
    s1.group.positions[3] = 3;
    s1.group.cell_members[0] = 0;
    s1.group.cell_members[1] = 1;
    s1.group.cell_members[2] = 2;
    s1.group.cell_members[3] = 3;
    s1.position = {21, 23};

    d2runtime::AdventureStack s2;
    s2.id = "S2";
    s2.owner = "P2";
    s2.leader_id = "E0";
    s2.leader_alive = 1;
    s2.group.members[0] = "E0";
    s2.group.positions[0] = 0;
    s2.group.cell_members[0] = 0;
    s2.position = {24, 20};

    return bootstrap_battle(s1, s2, w, gd);
}

d2battle::BattleState make_healcure_any_state(const d2engine::GameDataRegistry& gd) {
    d2runtime::AdventureWorldState w;
    w.units.push_back({"U0", "G_CA_HCHC", 1, {}, 0, "Actor", 0, {}, 200, 0});
    w.units.push_back({"U1", "G_CA_BASE", 1, {}, 0, "U1", 0, {}, 40, 0});
    w.units.push_back({"U2", "G_CA_BASE", 1, {}, 0, "U2", 0, {}, 200, 0});
    w.units.push_back({"U3", "G_CA_BASE", 1, {}, 0, "U3", 0, {}, 0, 0});
    w.units.push_back({"E0", "G_CA_BASE", 1, {}, 0, "Enemy", 0, {}, 100, 0});

    d2runtime::AdventureStack s1;
    s1.id = "S1";
    s1.owner = "P1";
    s1.leader_id = "U0";
    s1.leader_alive = 1;
    s1.group.members[0] = "U0";
    s1.group.members[1] = "U1";
    s1.group.members[2] = "U2";
    s1.group.members[3] = "U3";
    s1.group.positions[0] = 0;
    s1.group.positions[1] = 1;
    s1.group.positions[2] = 2;
    s1.group.positions[3] = 3;
    s1.group.cell_members[0] = 0;
    s1.group.cell_members[1] = 1;
    s1.group.cell_members[2] = 2;
    s1.group.cell_members[3] = 3;
    s1.position = {21, 23};

    d2runtime::AdventureStack s2;
    s2.id = "S2";
    s2.owner = "P2";
    s2.leader_id = "E0";
    s2.leader_alive = 1;
    s2.group.members[0] = "E0";
    s2.group.positions[0] = 0;
    s2.group.cell_members[0] = 0;
    s2.position = {24, 20};

    return bootstrap_battle(s1, s2, w, gd);
}

d2battle::BattleState make_healcure_large_state(const d2engine::GameDataRegistry& gd) {
    d2runtime::AdventureWorldState w;
    w.units.push_back({"U0", "G_CA_HCAL", 1, {}, 0, "Actor", 0, {}, 200, 0});
    w.units.push_back({"U1", "G_CA_LRGE", 1, {}, 0, "Large", 0, {}, 100, 0});
    w.units.push_back({"E0", "G_CA_BASE", 1, {}, 0, "Enemy", 0, {}, 100, 0});

    d2runtime::AdventureStack s1;
    s1.id = "S1";
    s1.owner = "P1";
    s1.leader_id = "U0";
    s1.leader_alive = 1;
    s1.group.members[0] = "U0";
    s1.group.members[1] = "U1";
    s1.group.positions[0] = 0;
    s1.group.positions[1] = 1;
    s1.group.cell_members[0] = 0;
    s1.group.cell_members[1] = 1;
    s1.group.cell_members[2] = 1;
    s1.position = {21, 23};

    d2runtime::AdventureStack s2;
    s2.id = "S2";
    s2.owner = "P2";
    s2.leader_id = "E0";
    s2.leader_alive = 1;
    s2.group.members[0] = "E0";
    s2.group.positions[0] = 0;
    s2.group.cell_members[0] = 0;
    s2.position = {24, 20};

    auto state = bootstrap_battle(s1, s2, w, gd);
    state.find_unit("U1")->effects.push_back(PetrifiedEffect{"E0", "G_PET_ATK", 1});
    return state;
}

d2battle::BattleState make_healrevive_large_state(const d2engine::GameDataRegistry& gd,
                                                  int                               current_hp) {
    d2runtime::AdventureWorldState w;
    w.units.push_back({"U0", "G_CA_HRAN", 1, {}, 0, "Hierophant", 0, {}, 200, 0});
    w.units.push_back({"U1", "G_CA_LRGE", 1, {}, 0, "Large", 0, {}, current_hp, 0});
    w.units.push_back({"E0", "G_CA_BASE", 1, {}, 0, "Enemy", 0, {}, 100, 0});

    d2runtime::AdventureStack s1;
    s1.id = "S1";
    s1.owner = "P1";
    s1.leader_id = "U0";
    s1.leader_alive = 1;
    s1.group.members[0] = "U0";
    s1.group.members[1] = "U1";
    s1.group.positions[0] = 0;
    s1.group.positions[1] = 1;
    s1.group.cell_members[0] = 0;
    s1.group.cell_members[1] = 1;
    s1.group.cell_members[2] = 1;
    s1.position = {21, 23};

    d2runtime::AdventureStack s2;
    s2.id = "S2";
    s2.owner = "P2";
    s2.leader_id = "E0";
    s2.leader_alive = 1;
    s2.group.members[0] = "E0";
    s2.group.positions[0] = 0;
    s2.group.cell_members[0] = 0;
    s2.position = {24, 20};

    return bootstrap_battle(s1, s2, w, gd);
}

d2battle::BattleState make_healrevive_state(const d2engine::GameDataRegistry& gd) {
    d2runtime::AdventureWorldState w;
    w.units.push_back({"U0", "G_CA_HRAN", 1, {}, 0, "Hierophant", 0, {}, 200, 0});
    w.units.push_back({"U1", "G_CA_BASE", 1, {}, 0, "U1", 0, {}, 40, 0});
    w.units.push_back({"U2", "G_CA_BASE", 1, {}, 0, "U2", 0, {}, 200, 0});
    w.units.push_back({"U3", "G_CA_BASE", 1, {}, 0, "U3", 0, {}, 0, 0});
    w.units.push_back({"U4", "G_CA_BASE", 1, {}, 0, "U4", 0, {}, 0, 0});
    w.units.push_back({"U5", "G_CA_BASE", 1, {}, 0, "U5", 0, {}, 100, 0});
    w.units.push_back({"E0", "G_CA_BASE", 1, {}, 0, "Enemy", 0, {}, 100, 0});

    d2runtime::AdventureStack s1;
    s1.id = "S1";
    s1.owner = "P1";
    s1.leader_id = "U0";
    s1.leader_alive = 1;
    s1.group.members[0] = "U0";
    s1.group.members[1] = "U1";
    s1.group.members[2] = "U2";
    s1.group.members[3] = "U3";
    s1.group.members[4] = "U4";
    s1.group.members[5] = "U5";
    s1.group.positions[0] = 0;
    s1.group.positions[1] = 1;
    s1.group.positions[2] = 2;
    s1.group.positions[3] = 3;
    s1.group.positions[4] = 4;
    s1.group.positions[5] = 5;
    s1.group.cell_members[0] = 0;
    s1.group.cell_members[1] = 1;
    s1.group.cell_members[2] = 2;
    s1.group.cell_members[3] = 3;
    s1.group.cell_members[4] = 4;
    s1.group.cell_members[5] = 5;
    s1.position = {21, 23};

    d2runtime::AdventureStack s2;
    s2.id = "S2";
    s2.owner = "P2";
    s2.leader_id = "E0";
    s2.leader_alive = 1;
    s2.group.members[0] = "E0";
    s2.group.positions[0] = 0;
    s2.group.cell_members[0] = 0;
    s2.position = {24, 20};

    return bootstrap_battle(s1, s2, w, gd);
}

d2battle::BattleState make_damage_petrify_state(const d2engine::GameDataRegistry& gd,
                                                int                               enemy_hp = 200) {
    d2runtime::AdventureWorldState w;
    w.units.push_back({"U0", "G_CA_DPAN", 1, {}, 0, "Actor", 0, {}, 200, 0});
    w.units.push_back({"E0", "G_CA_BASE", 1, {}, 0, "Enemy", 0, {}, enemy_hp, 0});

    d2runtime::AdventureStack s1;
    s1.id = "S1";
    s1.owner = "P1";
    s1.leader_id = "U0";
    s1.leader_alive = 1;
    s1.group.members[0] = "U0";
    s1.group.positions[0] = 0;
    s1.group.cell_members[0] = 0;
    s1.position = {21, 23};

    d2runtime::AdventureStack s2;
    s2.id = "S2";
    s2.owner = "P2";
    s2.leader_id = "E0";
    s2.leader_alive = 1;
    s2.group.members[0] = "E0";
    s2.group.positions[0] = 0;
    s2.group.cell_members[0] = 0;
    s2.position = {24, 20};

    return bootstrap_battle(s1, s2, w, gd);
}

d2battle::BattleState make_damage_petrify_dual_enemy_state(const d2engine::GameDataRegistry& gd,
                                                           int enemy_hp = 200) {
    d2runtime::AdventureWorldState w;
    w.units.push_back({"U0", "G_CA_DPAN", 1, {}, 0, "Actor", 0, {}, 200, 0});
    w.units.push_back({"E0", "G_CA_BASE", 1, {}, 0, "Enemy", 0, {}, enemy_hp, 0});
    w.units.push_back({"E1", "G_CA_BASE", 1, {}, 0, "Enemy2", 0, {}, 100, 0});

    d2runtime::AdventureStack s1;
    s1.id = "S1";
    s1.owner = "P1";
    s1.leader_id = "U0";
    s1.leader_alive = 1;
    s1.group.members[0] = "U0";
    s1.group.positions[0] = 0;
    s1.group.cell_members[0] = 0;
    s1.position = {21, 23};

    d2runtime::AdventureStack s2;
    s2.id = "S2";
    s2.owner = "P2";
    s2.leader_id = "E0";
    s2.leader_alive = 1;
    s2.group.members[0] = "E0";
    s2.group.positions[0] = 0;
    s2.group.cell_members[0] = 0;
    s2.group.members[1] = "E1";
    s2.group.positions[1] = 1;
    s2.group.cell_members[1] = 1;
    s2.position = {24, 20};

    return bootstrap_battle(s1, s2, w, gd);
}

// ── Helper to add a combat-unit record to a Gunits builder ──────────────────

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

[[nodiscard]] std::vector<std::string>
collect_attack_target_ids(const std::vector<d2battle::BattleAction>& actions) {
    std::vector<std::string> targets;
    for (const auto& action : actions) {
        if (auto* atk = std::get_if<d2battle::AttackAction>(&action)) {
            if (auto* ut = std::get_if<d2battle::UnitTarget>(&atk->target)) {
                if (std::find(targets.begin(), targets.end(), ut->unit_id) == targets.end()) {
                    targets.push_back(ut->unit_id);
                }
            }
        }
    }
    return targets;
}

[[nodiscard]] const d2battle::BattleAction*
find_attack_action_for_target(const std::vector<d2battle::BattleAction>& actions,
                              const std::string&                         target_id) {
    for (const auto& action : actions) {
        if (auto* atk = std::get_if<d2battle::AttackAction>(&action)) {
            if (auto* ut = std::get_if<d2battle::UnitTarget>(&atk->target);
                ut && ut->unit_id == target_id) {
                return &action;
            }
        }
    }
    return nullptr;
}

void add_combat_unit(test_dbf::DbfBuilder& b, const std::string& id, const std::string& atk1,
                     const std::string& atk2) {
    b.add_record({id,   "",    "",  "",    "G000SU0001", "0", "1",      "0",  "1",  "1",
                  "0",  "0",   "1", "200", "2",          "0", "1",      "1",  "0",  "0",
                  "75", "150", "",  "",    "",           "",  "",       atk1, atk2, "",
                  "",   "",    "",  "",    "",           "1", "L_HUMAN"});
}

// ═══════════════════════════════════════════════════════════════════════════════
// Test 1: SecondaryAttackNeverEmitsStandaloneAction
// ═══════════════════════════════════════════════════════════════════════════════

TEST(CompositeAttackTest, SecondaryAttackNeverEmitsStandaloneAction) {
    static std::atomic<unsigned> c{0};
    auto                         gd = make_gd_impl("t01", c, [](const std::filesystem::path& dir) {
        auto batt = make_base_gattacks_builder();
        batt.add_record({"G_AT_DM", "", "", "0", "1", "2", "40", "50", "0", "60", "1", "0", "", "",
                         "", "", ""});
        batt.add_record(
            {"G_AT_PT", "", "", "6", "1", "2", "40", "0", "0", "0", "1", "0", "", "", "", "", ""});
        batt.write(dir / "Gattacks.dbf");

        auto gunits = make_base_gunits_builder();
        add_combat_unit(gunits, "G_CA_DPAN", "G_AT_DM", "G_AT_PT");
        gunits.write(dir / "Gunits.dbf");
    });

    auto       state = make_damage_petrify_state(gd, 200);
    const auto before_fp = d2battle::compute_fingerprint(state);
    auto       actions = d2battle::valid_actions(state, gd);
    ASSERT_EQ(actions.size(), 1u);
    const auto* atk = std::get_if<d2battle::AttackAction>(&actions[0]);
    ASSERT_NE(atk, nullptr);
    ASSERT_TRUE(std::holds_alternative<d2battle::UnitTarget>(atk->target));
    EXPECT_EQ(std::get<d2battle::UnitTarget>(atk->target).unit_id, "E0");

    auto next = d2battle::apply(state, actions[0], gd);
    EXPECT_EQ(d2battle::compute_fingerprint(state), before_fp);
    EXPECT_EQ(next.round_state.current_turn_index, state.round_state.current_turn_index + 1);
    const auto* enemy = next.find_unit("E0");
    ASSERT_NE(enemy, nullptr);
    EXPECT_EQ(enemy->current_hp, 150);
    EXPECT_FALSE(enemy->effects.empty());
    EXPECT_TRUE(std::holds_alternative<d2battle::PetrifiedEffect>(enemy->effects.front()));
}

// ═══════════════════════════════════════════════════════════════════════════════
// Test 2: UnsupportedPrimaryBlocksSupportedSecondary
// ═══════════════════════════════════════════════════════════════════════════════

TEST(CompositeAttackTest, UnsupportedPrimaryBlocksSupportedSecondary) {
    static std::atomic<unsigned> c{0};
    auto                         gd = make_gd_impl("t02", c, [](const std::filesystem::path& dir) {
        auto batt = make_base_gattacks_builder();
        batt.add_record(
            {"G_AT_BD", "", "", "5", "1", "2", "40", "0", "0", "0", "1", "0", "", "", "", "", ""});
        batt.add_record(
            {"G_AT_CR", "", "", "23", "1", "2", "40", "0", "0", "0", "1", "0", "", "", "", "", ""});
        batt.write(dir / "Gattacks.dbf");

        auto gunits = make_base_gunits_builder();
        add_combat_unit(gunits, "G_CA_BCAN", "G_AT_BD", "G_AT_CR");
        gunits.write(dir / "Gunits.dbf");
    });

    auto        state = make_unsupported_state(gd, "G_CA_BCAN");
    const auto* udef = gd.find_unit("G_CA_BCAN");
    ASSERT_NE(udef, nullptr);
    const auto before_fp = d2battle::compute_fingerprint(state);
    auto       support = d2battle::analyze_attack_bundle(*udef);
    EXPECT_FALSE(support.supported);
    EXPECT_EQ(support.error, AttackBundleSupportError::UnsupportedPrimaryClass);

    d2battle::AttackAction candidate{"U0", d2battle::UnitTarget{"E0"}};
    EXPECT_EQ(d2battle::validate_action(d2battle::BattleAction{candidate}, state, gd),
              d2battle::ActionValidationError::UnsupportedBundle);
    EXPECT_EQ(d2battle::compute_fingerprint(state), before_fp);

    auto actions = d2battle::valid_actions(state, gd);
    EXPECT_TRUE(actions.empty());
}

// ═══════════════════════════════════════════════════════════════════════════════
// Test 3: UnsupportedSecondaryBlocksSupportedPrimary
// ═══════════════════════════════════════════════════════════════════════════════

TEST(CompositeAttackTest, UnsupportedSecondaryBlocksSupportedPrimary) {
    static std::atomic<unsigned> c{0};
    auto                         gd = make_gd_impl("t03", c, [](const std::filesystem::path& dir) {
        auto batt = make_base_gattacks_builder();
        batt.add_record({"G_AT_DM", "", "", "0", "1", "2", "40", "50", "0", "60", "1", "0", "", "",
                         "", "", ""});
        batt.add_record(
            {"G_AT_PA", "", "", "2", "1", "2", "40", "0", "0", "0", "1", "0", "", "", "", "", ""});
        batt.write(dir / "Gattacks.dbf");

        auto gunits = make_base_gunits_builder();
        add_combat_unit(gunits, "G_CA_DMPA", "G_AT_DM", "G_AT_PA");
        gunits.write(dir / "Gunits.dbf");
    });

    auto        state = make_unsupported_state(gd, "G_CA_DMPA");
    const auto* udef = gd.find_unit("G_CA_DMPA");
    ASSERT_NE(udef, nullptr);
    const auto before_fp = d2battle::compute_fingerprint(state);
    auto       support = d2battle::analyze_attack_bundle(*udef);
    EXPECT_FALSE(support.supported);
    EXPECT_EQ(support.error, AttackBundleSupportError::UnsupportedSecondaryClass);

    d2battle::AttackAction candidate{"U0", d2battle::UnitTarget{"E0"}};
    EXPECT_EQ(d2battle::validate_action(d2battle::BattleAction{candidate}, state, gd),
              d2battle::ActionValidationError::UnsupportedBundle);
    EXPECT_EQ(d2battle::compute_fingerprint(state), before_fp);

    auto actions = d2battle::valid_actions(state, gd);
    EXPECT_TRUE(actions.empty());
}

// ═══════════════════════════════════════════════════════════════════════════════
// Test 4: UnsupportedSecondaryMatrixBlocksPrimaryExecution
// ═══════════════════════════════════════════════════════════════════════════════

TEST(CompositeAttackTest, UnsupportedSecondaryMatrixBlocksPrimaryExecution) {
    static std::atomic<unsigned> c{0};
    auto                         gd = make_gd_impl("t04", c, [](const std::filesystem::path& dir) {
        auto batt = make_base_gattacks_builder();
        batt.add_record({"G_AT_DM", "", "", "0", "1", "2", "40", "50", "0", "60", "1", "0", "", "",
                         "", "", ""});
        batt.add_record(
            {"G_AT_PO", "", "", "9", "1", "2", "40", "0", "0", "0", "1", "0", "", "", "", "", ""});
        batt.add_record(
            {"G_AT_PA", "", "", "2", "1", "2", "40", "0", "0", "0", "1", "0", "", "", "", "", ""});
        batt.add_record(
            {"G_AT_SH", "", "", "32", "1", "2", "40", "0", "0", "0", "1", "0", "", "", "", "", ""});
        batt.add_record(
            {"G_AT_FR", "", "", "10", "1", "2", "40", "0", "0", "0", "1", "0", "", "", "", "", ""});
        batt.add_record(
            {"G_AT_BL", "", "", "30", "1", "2", "40", "0", "0", "0", "1", "0", "", "", "", "", ""});
        batt.add_record(
            {"G_AT_FE", "", "", "4", "1", "2", "40", "0", "0", "0", "1", "0", "", "", "", "", ""});
        batt.write(dir / "Gattacks.dbf");

        auto gunits = make_base_gunits_builder();
        add_combat_unit(gunits, "G_CA_DMPO", "G_AT_DM", "G_AT_PO");
        add_combat_unit(gunits, "G_CA_DMPA", "G_AT_DM", "G_AT_PA");
        add_combat_unit(gunits, "G_CA_DMSH", "G_AT_DM", "G_AT_SH");
        add_combat_unit(gunits, "G_CA_DMFR", "G_AT_DM", "G_AT_FR");
        add_combat_unit(gunits, "G_CA_DMBL", "G_AT_DM", "G_AT_BL");
        add_combat_unit(gunits, "G_CA_DMFE", "G_AT_DM", "G_AT_FE");
        gunits.write(dir / "Gunits.dbf");
    });

    struct Row {
        std::string unit_type;
        std::string label;
    };
    Row rows[] = {
        {"G_CA_DMPO", "Poison"},    {"G_CA_DMPA", "Paralyze"}, {"G_CA_DMSH", "Shatter"},
        {"G_CA_DMFR", "Frostbite"}, {"G_CA_DMBL", "Blister"},  {"G_CA_DMFE", "Fear"},
    };
    for (const auto& row : rows) {
        const auto* udef = gd.find_unit(row.unit_type);
        ASSERT_NE(udef, nullptr) << "row=" << row.label;
        auto       state = make_unsupported_state(gd, row.unit_type);
        const auto before_fp = d2battle::compute_fingerprint(state);
        auto       support = d2battle::analyze_attack_bundle(*udef);
        EXPECT_FALSE(support.supported) << "row=" << row.label;
        EXPECT_EQ(support.error, AttackBundleSupportError::UnsupportedSecondaryClass)
            << "row=" << row.label;

        d2battle::AttackAction candidate{"U0", d2battle::UnitTarget{"E0"}};
        EXPECT_EQ(d2battle::validate_action(d2battle::BattleAction{candidate}, state, gd),
                  d2battle::ActionValidationError::UnsupportedBundle)
            << "row=" << row.label;
        EXPECT_EQ(d2battle::compute_fingerprint(state), before_fp) << "row=" << row.label;

        auto actions = d2battle::valid_actions(state, gd);
        EXPECT_TRUE(actions.empty()) << "row=" << row.label;
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Test 5: HealCureBundleEmitsOneCompositeAction
// ═══════════════════════════════════════════════════════════════════════════════

TEST(CompositeAttackTest, HealCureBundleEmitsOneCompositeAction) {
    static std::atomic<unsigned> c{0};
    auto                         gd = make_gd_impl("t05", c, [](const std::filesystem::path& dir) {
        auto batt = make_base_gattacks_builder();
        batt.add_record({"G_AT_HL_A", "", "", "3", "1", "1", "40", "0", "30", "0", "1", "0", "", "",
                         "", "", ""});
        batt.add_record({"G_AT_CR_A", "", "", "23", "1", "1", "40", "0", "0", "0", "1", "0", "", "",
                         "", "", ""});
        batt.write(dir / "Gattacks.dbf");

        auto gunits = make_base_gunits_builder();
        add_combat_unit(gunits, "G_CA_HCAL", "G_AT_HL_A", "G_AT_CR_A");
        gunits.write(dir / "Gunits.dbf");
    });

    auto state = make_healcure_all_state(gd);
    auto actions = d2battle::valid_actions(state, gd);
    ASSERT_EQ(actions.size(), 1u);
    const auto* atk = std::get_if<d2battle::AttackAction>(&actions[0]);
    ASSERT_NE(atk, nullptr);
    EXPECT_TRUE(std::holds_alternative<d2battle::AllAlliedUnitsTarget>(atk->target));
}

// ═══════════════════════════════════════════════════════════════════════════════
// Test 6: HealCureBundleExecutesBothComponentsInOneSuccessor
// ═══════════════════════════════════════════════════════════════════════════════

TEST(CompositeAttackTest, HealCureBundleExecutesBothComponentsInOneSuccessor) {
    static std::atomic<unsigned> c{0};
    auto                         gd = make_gd_impl("t06", c, [](const std::filesystem::path& dir) {
        auto batt = make_base_gattacks_builder();
        batt.add_record({"G_AT_HL_A", "", "", "3", "1", "1", "40", "0", "30", "0", "1", "0", "", "",
                         "", "", ""});
        batt.add_record({"G_AT_CR_A", "", "", "23", "1", "1", "40", "0", "0", "0", "1", "0", "", "",
                         "", "", ""});
        batt.write(dir / "Gattacks.dbf");

        auto gunits = make_base_gunits_builder();
        add_combat_unit(gunits, "G_CA_HCAL", "G_AT_HL_A", "G_AT_CR_A");
        gunits.write(dir / "Gunits.dbf");
    });

    auto state = make_healcure_all_state(gd);
    auto actions = d2battle::valid_actions(state, gd);
    ASSERT_EQ(actions.size(), 1u);
    auto next = d2battle::apply(state, actions[0], gd);

    auto* u1 = next.find_unit("U1");
    ASSERT_NE(u1, nullptr);
    EXPECT_EQ(u1->current_hp, 70);
    EXPECT_TRUE(u1->effects.empty());

    auto* u2 = next.find_unit("U2");
    ASSERT_NE(u2, nullptr);
    EXPECT_EQ(u2->current_hp, 200);

    auto* u3 = next.find_unit("U3");
    ASSERT_NE(u3, nullptr);
    EXPECT_FALSE(u3->alive);
    EXPECT_EQ(u3->current_hp, 0);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Test 7: HealCureBundleAdvancesTurnExactlyOnce
// ═══════════════════════════════════════════════════════════════════════════════

TEST(CompositeAttackTest, HealCureBundleAdvancesTurnExactlyOnce) {
    static std::atomic<unsigned> c{0};
    auto                         gd = make_gd_impl("t07", c, [](const std::filesystem::path& dir) {
        auto batt = make_base_gattacks_builder();
        batt.add_record({"G_AT_HL_A", "", "", "3", "1", "1", "40", "0", "30", "0", "1", "0", "", "",
                         "", "", ""});
        batt.add_record({"G_AT_CR_A", "", "", "23", "1", "1", "40", "0", "0", "0", "1", "0", "", "",
                         "", "", ""});
        batt.write(dir / "Gattacks.dbf");

        auto gunits = make_base_gunits_builder();
        add_combat_unit(gunits, "G_CA_HCAL", "G_AT_HL_A", "G_AT_CR_A");
        gunits.write(dir / "Gunits.dbf");
    });

    auto state = make_healcure_all_state(gd);
    auto before_turn = state.round_state.current_turn_index;
    auto actions = d2battle::valid_actions(state, gd);
    ASSERT_EQ(actions.size(), 1u);
    auto next = d2battle::apply(state, actions[0], gd);
    EXPECT_EQ(next.round_state.current_turn_index, before_turn + 1);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Test 8: HealCureBundleProcessesLargeAllyOnce
// ═══════════════════════════════════════════════════════════════════════════════

TEST(CompositeAttackTest, HealCureBundleProcessesLargeAllyOnce) {
    static std::atomic<unsigned> c{0};
    auto                         gd = make_gd_impl("t08", c, [](const std::filesystem::path& dir) {
        auto batt = make_base_gattacks_builder();
        batt.add_record({"G_AT_HL_A", "", "", "3", "1", "1", "40", "0", "30", "0", "1", "0", "", "",
                         "", "", ""});
        batt.add_record({"G_AT_CR_A", "", "", "23", "1", "1", "40", "0", "0", "0", "1", "0", "", "",
                         "", "", ""});
        batt.write(dir / "Gattacks.dbf");

        auto gunits = make_base_gunits_builder();
        add_combat_unit(gunits, "G_CA_HCAL", "G_AT_HL_A", "G_AT_CR_A");
        gunits.write(dir / "Gunits.dbf");
    });

    auto       state = make_healcure_large_state(gd);
    const auto before_fp = d2battle::compute_fingerprint(state);
    auto       actions = d2battle::valid_actions(state, gd);
    ASSERT_EQ(actions.size(), 1u);
    const auto* atk = std::get_if<d2battle::AttackAction>(&actions[0]);
    ASSERT_NE(atk, nullptr);
    ASSERT_TRUE(std::holds_alternative<d2battle::AllAlliedUnitsTarget>(atk->target));

    std::ostringstream oss;
    auto               next = d2battle::apply(state, actions[0], gd);
    print_selected_action(oss, state, actions[0], next, gd);
    auto output = oss.str();

    EXPECT_EQ(d2battle::compute_fingerprint(state), before_fp);
    EXPECT_EQ(next.round_state.current_turn_index, state.round_state.current_turn_index + 1);

    auto* u1 = next.find_unit("U1");
    ASSERT_NE(u1, nullptr);
    EXPECT_TRUE(u1->alive);
    EXPECT_EQ(u1->current_hp, 130);
    EXPECT_TRUE(u1->effects.empty());

    EXPECT_EQ(count_occurrences(output, "HEAL:\n"), 1u);
    EXPECT_EQ(count_occurrences(output, "HP 100 -> 130"), 1u);
    EXPECT_EQ(count_occurrences(output, "PETRIFIED"), 1u);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Test 9: HealReviveBundleEnumeratesAliveAndDeadAllies
// ═══════════════════════════════════════════════════════════════════════════════

TEST(CompositeAttackTest, HealReviveBundleEnumeratesAliveAndDeadAllies) {
    static std::atomic<unsigned> c{0};
    auto                         gd = make_gd_impl("t09", c, [](const std::filesystem::path& dir) {
        auto batt = make_base_gattacks_builder();
        batt.add_record(
            {"G_AT_HL", "", "", "3", "1", "2", "40", "0", "30", "0", "1", "0", "", "", "", "", ""});
        batt.add_record({"G_AT_RV", "", "", "21", "1", "2", "40", "0", "60", "0", "1", "0", "", "",
                         "", "", ""});
        batt.write(dir / "Gattacks.dbf");

        auto gunits = make_base_gunits_builder();
        add_combat_unit(gunits, "G_CA_HRAN", "G_AT_HL", "G_AT_RV");
        gunits.write(dir / "Gunits.dbf");
    });

    auto       state = make_healrevive_state(gd);
    const auto before_fp = d2battle::compute_fingerprint(state);
    auto       actions = d2battle::valid_actions(state, gd);

    const auto targets = collect_attack_target_ids(actions);
    EXPECT_EQ(targets, (std::vector<std::string>{"U0", "U1", "U2", "U5", "U3", "U4"}));
    EXPECT_EQ(std::count(targets.begin(), targets.end(), "U0"), 1u);
    EXPECT_EQ(std::count(targets.begin(), targets.end(), "U1"), 1u);
    EXPECT_EQ(std::count(targets.begin(), targets.end(), "U2"), 1u);
    EXPECT_EQ(std::count(targets.begin(), targets.end(), "U3"), 1u);
    EXPECT_EQ(std::count(targets.begin(), targets.end(), "U4"), 1u);
    EXPECT_EQ(std::count(targets.begin(), targets.end(), "U5"), 1u);
    EXPECT_EQ(std::count(targets.begin(), targets.end(), "E0"), 0u);
    EXPECT_EQ(std::count(targets.begin(), targets.end(), "E1"), 0u);
    EXPECT_EQ(d2battle::compute_fingerprint(state), before_fp);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Test 10: HealReviveAliveTargetExecutesHealOnly
// ═══════════════════════════════════════════════════════════════════════════════

TEST(CompositeAttackTest, HealReviveAliveTargetExecutesHealOnly) {
    static std::atomic<unsigned> c{0};
    auto                         gd = make_gd_impl("t10", c, [](const std::filesystem::path& dir) {
        auto batt = make_base_gattacks_builder();
        batt.add_record(
            {"G_AT_HL", "", "", "3", "1", "2", "40", "0", "30", "0", "1", "0", "", "", "", "", ""});
        batt.add_record({"G_AT_RV", "", "", "21", "1", "2", "40", "0", "60", "0", "1", "0", "", "",
                         "", "", ""});
        batt.write(dir / "Gattacks.dbf");

        auto gunits = make_base_gunits_builder();
        add_combat_unit(gunits, "G_CA_HRAN", "G_AT_HL", "G_AT_RV");
        gunits.write(dir / "Gunits.dbf");
    });

    auto       state = make_healrevive_state(gd);
    const auto before_fp = d2battle::compute_fingerprint(state);
    const auto actions = d2battle::valid_actions(state, gd);
    ASSERT_EQ(actions.size(), 6u);
    const auto target_ids = collect_attack_target_ids(actions);
    EXPECT_EQ(std::count(target_ids.begin(), target_ids.end(), "U1"), 1u);
    const auto* selected = find_attack_action_for_target(actions, "U1");
    ASSERT_NE(selected, nullptr);

    auto next = d2battle::apply(state, *selected, gd);

    auto* u1 = next.find_unit("U1");
    ASSERT_NE(u1, nullptr);
    EXPECT_EQ(u1->current_hp, 70);
    EXPECT_TRUE(u1->alive);
    EXPECT_EQ(d2battle::compute_fingerprint(state), before_fp);
    EXPECT_EQ(next.round_state.current_turn_index, state.round_state.current_turn_index + 1);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Test 11: HealReviveDeadTargetExecutesReviveOnly
// ═══════════════════════════════════════════════════════════════════════════════

TEST(CompositeAttackTest, HealReviveDeadTargetExecutesReviveOnly) {
    static std::atomic<unsigned> c{0};
    auto                         gd = make_gd_impl("t11", c, [](const std::filesystem::path& dir) {
        auto batt = make_base_gattacks_builder();
        batt.add_record(
            {"G_AT_HL", "", "", "3", "1", "2", "40", "0", "30", "0", "1", "0", "", "", "", "", ""});
        batt.add_record({"G_AT_RV", "", "", "21", "1", "2", "40", "0", "60", "0", "1", "0", "", "",
                         "", "", ""});
        batt.write(dir / "Gattacks.dbf");

        auto gunits = make_base_gunits_builder();
        add_combat_unit(gunits, "G_CA_HRAN", "G_AT_HL", "G_AT_RV");
        gunits.write(dir / "Gunits.dbf");
    });

    auto        state = make_healrevive_state(gd);
    const auto  before_fp = d2battle::compute_fingerprint(state);
    const auto* before_u3 = state.find_unit("U3");
    ASSERT_NE(before_u3, nullptr);
    const auto before_party1 = state.party1;

    auto                          actions = d2battle::valid_actions(state, gd);
    const d2battle::BattleAction* selected = nullptr;
    for (const auto& a : actions) {
        if (auto* atk = std::get_if<d2battle::AttackAction>(&a)) {
            if (auto* ut = std::get_if<d2battle::UnitTarget>(&atk->target)) {
                if (ut->unit_id == "U3") {
                    selected = &a;
                    break;
                }
            }
        }
    }
    ASSERT_NE(selected, nullptr);

    auto next = d2battle::apply(state, *selected, gd);

    auto* u3 = next.find_unit("U3");
    ASSERT_NE(u3, nullptr);
    EXPECT_EQ(u3->id, before_u3->id);
    EXPECT_EQ(u3->type_id, before_u3->type_id);
    EXPECT_EQ(u3->xp, before_u3->xp);
    EXPECT_EQ(u3->member_index, before_u3->member_index);
    EXPECT_EQ(u3->formation_cell, before_u3->formation_cell);
    EXPECT_EQ(next.party1.leader_id, before_party1.leader_id);
    EXPECT_EQ(next.party1.leader_alive, before_party1.leader_alive);
    EXPECT_EQ(next.party1.members[3], before_party1.members[3]);
    EXPECT_EQ(next.party1.cell_members[3], before_party1.cell_members[3]);
    EXPECT_TRUE(u3->alive);
    EXPECT_EQ(u3->current_hp, 60);
    EXPECT_EQ(d2battle::compute_fingerprint(state), before_fp);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Test 12: HealReviveBundleAdvancesTurnExactlyOnce
// ═══════════════════════════════════════════════════════════════════════════════

TEST(CompositeAttackTest, HealReviveBundleAdvancesTurnExactlyOnce) {
    static std::atomic<unsigned> c{0};
    auto                         gd = make_gd_impl("t12", c, [](const std::filesystem::path& dir) {
        auto batt = make_base_gattacks_builder();
        batt.add_record(
            {"G_AT_HL", "", "", "3", "1", "2", "40", "0", "30", "0", "1", "0", "", "", "", "", ""});
        batt.add_record({"G_AT_RV", "", "", "21", "1", "2", "40", "0", "60", "0", "1", "0", "", "",
                         "", "", ""});
        batt.write(dir / "Gattacks.dbf");

        auto gunits = make_base_gunits_builder();
        add_combat_unit(gunits, "G_CA_HRAN", "G_AT_HL", "G_AT_RV");
        gunits.write(dir / "Gunits.dbf");
    });

    auto       state = make_healrevive_state(gd);
    const auto before_fp = d2battle::compute_fingerprint(state);
    const auto before_turn = state.round_state.current_turn_index;
    const auto actions = d2battle::valid_actions(state, gd);
    ASSERT_EQ(actions.size(), 6u);
    const auto target_ids = collect_attack_target_ids(actions);
    EXPECT_EQ(std::count(target_ids.begin(), target_ids.end(), "U1"), 1u);
    const auto* selected = find_attack_action_for_target(actions, "U1");
    ASSERT_NE(selected, nullptr);

    auto next = d2battle::apply(state, *selected, gd);
    EXPECT_FALSE(next.is_terminal());
    EXPECT_EQ(next.round_state.current_turn_index, before_turn + 1);
    EXPECT_EQ(d2battle::compute_fingerprint(state), before_fp);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Test 13: HealReviveBundleProcessesLargeTargetOnce
// ═══════════════════════════════════════════════════════════════════════════════

TEST(CompositeAttackTest, HealReviveBundleProcessesLargeTargetOnce) {
    static std::atomic<unsigned> c{0};
    auto                         gd = make_gd_impl("t13", c, [](const std::filesystem::path& dir) {
        auto batt = make_base_gattacks_builder();
        batt.add_record(
            {"G_AT_HL", "", "", "3", "1", "2", "40", "0", "30", "0", "1", "0", "", "", "", "", ""});
        batt.add_record({"G_AT_RV", "", "", "21", "1", "2", "40", "0", "60", "0", "1", "0", "", "",
                         "", "", ""});
        batt.write(dir / "Gattacks.dbf");

        auto gunits = make_base_gunits_builder();
        add_combat_unit(gunits, "G_CA_HRAN", "G_AT_HL", "G_AT_RV");
        gunits.write(dir / "Gunits.dbf");
    });

    auto       alive_state = make_healrevive_large_state(gd, 100);
    const auto alive_fp = d2battle::compute_fingerprint(alive_state);
    const auto alive_before_party1 = alive_state.party1;
    const auto alive_before_large = *alive_state.find_unit("U1");

    auto alive_actions = d2battle::valid_actions(alive_state, gd);
    ASSERT_EQ(alive_actions.size(), 2u);
    const auto target_ids = collect_attack_target_ids(alive_actions);
    EXPECT_EQ(target_ids, (std::vector<std::string>{"U0", "U1"}));
    EXPECT_EQ(std::count(target_ids.begin(), target_ids.end(), "U1"), 1u);
    const auto* alive_selected = find_attack_action_for_target(alive_actions, "U1");
    ASSERT_NE(alive_selected, nullptr);

    auto        alive_next = d2battle::apply(alive_state, *alive_selected, gd);
    const auto* alive_u1 = alive_next.find_unit("U1");
    ASSERT_NE(alive_u1, nullptr);
    EXPECT_EQ(alive_next.round_state.current_turn_index,
              alive_state.round_state.current_turn_index + 1);
    EXPECT_EQ(d2battle::compute_fingerprint(alive_state), alive_fp);
    EXPECT_TRUE(alive_u1->alive);
    EXPECT_EQ(alive_u1->current_hp, 130);
    EXPECT_EQ(alive_u1->id, alive_before_large.id);
    EXPECT_EQ(alive_u1->type_id, alive_before_large.type_id);
    EXPECT_EQ(alive_u1->xp, alive_before_large.xp);
    EXPECT_EQ(alive_u1->member_index, alive_before_large.member_index);
    EXPECT_EQ(alive_u1->formation_cell, alive_before_large.formation_cell);
    EXPECT_EQ(alive_next.party1.members, alive_before_party1.members);
    EXPECT_EQ(alive_next.party1.positions, alive_before_party1.positions);
    EXPECT_EQ(alive_next.party1.cell_members, alive_before_party1.cell_members);
    EXPECT_EQ(alive_next.party1.cell_members[1], alive_next.party1.cell_members[2]);

    auto       dead_state = make_healrevive_large_state(gd, 0);
    const auto dead_fp = d2battle::compute_fingerprint(dead_state);
    const auto dead_before_party1 = dead_state.party1;
    const auto dead_before_large = *dead_state.find_unit("U1");

    auto       dead_actions = d2battle::valid_actions(dead_state, gd);
    const auto dead_targets = collect_attack_target_ids(dead_actions);
    EXPECT_EQ(dead_targets, (std::vector<std::string>{"U0", "U1"}));
    EXPECT_EQ(std::count(dead_targets.begin(), dead_targets.end(), "U1"), 1u);
    const auto* dead_selected = find_attack_action_for_target(dead_actions, "U1");
    ASSERT_NE(dead_selected, nullptr);

    auto        dead_next = d2battle::apply(dead_state, *dead_selected, gd);
    const auto* dead_u1 = dead_next.find_unit("U1");
    ASSERT_NE(dead_u1, nullptr);
    EXPECT_EQ(dead_next.round_state.current_turn_index,
              dead_state.round_state.current_turn_index + 1);
    EXPECT_EQ(d2battle::compute_fingerprint(dead_state), dead_fp);
    EXPECT_TRUE(dead_u1->alive);
    EXPECT_EQ(dead_u1->current_hp, 60);
    EXPECT_EQ(dead_u1->id, dead_before_large.id);
    EXPECT_EQ(dead_u1->type_id, dead_before_large.type_id);
    EXPECT_EQ(dead_u1->xp, dead_before_large.xp);
    EXPECT_EQ(dead_u1->member_index, dead_before_large.member_index);
    EXPECT_EQ(dead_u1->formation_cell, dead_before_large.formation_cell);
    EXPECT_EQ(dead_next.party1.members, dead_before_party1.members);
    EXPECT_EQ(dead_next.party1.positions, dead_before_party1.positions);
    EXPECT_EQ(dead_next.party1.cell_members, dead_before_party1.cell_members);
    EXPECT_EQ(dead_next.party1.cell_members[1], dead_next.party1.cell_members[2]);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Test 14: CompositeActionRejectsWrongTargetRelation
// ═══════════════════════════════════════════════════════════════════════════════

TEST(CompositeAttackTest, CompositeActionRejectsWrongTargetRelation) {
    static std::atomic<unsigned> c{0};
    auto                         gd = make_gd_impl("t14", c, [](const std::filesystem::path& dir) {
        auto batt = make_base_gattacks_builder();
        batt.add_record(
            {"G_AT_HL", "", "", "3", "1", "2", "40", "0", "30", "0", "1", "0", "", "", "", "", ""});
        batt.add_record({"G_AT_DM", "", "", "0", "1", "2", "40", "50", "0", "60", "1", "0", "", "",
                         "", "", ""});
        batt.write(dir / "Gattacks.dbf");

        auto gunits = make_base_gunits_builder();
        add_combat_unit(gunits, "G_CA_HLDM", "G_AT_HL", "G_AT_DM");
        gunits.write(dir / "Gunits.dbf");
    });

    const auto* udef = gd.find_unit("G_CA_HLDM");
    ASSERT_NE(udef, nullptr);
    auto support = d2battle::analyze_attack_bundle(*udef);
    EXPECT_FALSE(support.supported);
    EXPECT_EQ(support.error, AttackBundleSupportError::IncompatibleTargetRelation);

    auto state = make_unsupported_state(gd, "G_CA_HLDM");
    auto actions = d2battle::valid_actions(state, gd);
    EXPECT_TRUE(actions.empty());
}

// ═══════════════════════════════════════════════════════════════════════════════
// Test 15: CompositeActionRejectsIncompatibleReach
// ═══════════════════════════════════════════════════════════════════════════════

TEST(CompositeAttackTest, CompositeActionRejectsIncompatibleReach) {
    static std::atomic<unsigned> c{0};
    auto                         gd = make_gd_impl("t15", c, [](const std::filesystem::path& dir) {
        auto batt = make_base_gattacks_builder();
        batt.add_record(
            {"G_AT_HL", "", "", "3", "1", "2", "40", "0", "30", "0", "1", "0", "", "", "", "", ""});
        batt.add_record({"G_AT_CR_A", "", "", "23", "1", "1", "40", "0", "0", "0", "1", "0", "", "",
                         "", "", ""});
        batt.write(dir / "Gattacks.dbf");

        auto gunits = make_base_gunits_builder();
        add_combat_unit(gunits, "G_CA_HACR", "G_AT_HL", "G_AT_CR_A");
        gunits.write(dir / "Gunits.dbf");
    });

    const auto* udef = gd.find_unit("G_CA_HACR");
    ASSERT_NE(udef, nullptr);
    auto support = d2battle::analyze_attack_bundle(*udef);
    EXPECT_FALSE(support.supported);
    EXPECT_EQ(support.error, AttackBundleSupportError::IncompatibleReach);

    auto state = make_unsupported_state(gd, "G_CA_HACR");
    auto actions = d2battle::valid_actions(state, gd);
    EXPECT_TRUE(actions.empty());
}

// ═══════════════════════════════════════════════════════════════════════════════
// Test 16: CompositeActionRequiresAtLeastOneApplicableComponent
// ═══════════════════════════════════════════════════════════════════════════════

TEST(CompositeAttackTest, CompositeActionRequiresAtLeastOneApplicableComponent) {
    static std::atomic<unsigned> c{0};
    auto                         gd = make_gd_impl("t16", c, [](const std::filesystem::path& dir) {
        auto batt = make_base_gattacks_builder();
        batt.add_record(
            {"G_AT_HL", "", "", "3", "1", "2", "40", "0", "30", "0", "1", "0", "", "", "", "", ""});
        batt.add_record(
            {"G_AT_CR", "", "", "23", "1", "2", "40", "0", "0", "0", "1", "0", "", "", "", "", ""});
        batt.write(dir / "Gattacks.dbf");

        auto gunits = make_base_gunits_builder();
        add_combat_unit(gunits, "G_CA_HCHC", "G_AT_HL", "G_AT_CR");
        gunits.write(dir / "Gunits.dbf");
    });

    auto                   state = make_healcure_any_state(gd);
    d2battle::AttackAction dead_target{"U0", d2battle::UnitTarget{"U3"}};
    auto err = d2battle::validate_action(d2battle::BattleAction{dead_target}, state, gd);
    EXPECT_EQ(err, d2battle::ActionValidationError::NoEligibleTargets);

    auto actions = d2battle::valid_actions(state, gd);
    for (const auto& a : actions) {
        if (auto* atk = std::get_if<d2battle::AttackAction>(&a)) {
            if (auto* ut = std::get_if<d2battle::UnitTarget>(&atk->target))
                EXPECT_NE(ut->unit_id, "U3");
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Test 17: CompositeActionPresentationShowsBothComponents
// ═══════════════════════════════════════════════════════════════════════════════

TEST(CompositeAttackTest, CompositeActionPresentationShowsBothComponents) {
    // Heal/All + Cure/All
    {
        static std::atomic<unsigned> c{0};
        auto gd = make_gd_impl("t17a", c, [](const std::filesystem::path& dir) {
            auto batt = make_base_gattacks_builder();
            batt.add_record({"G_AT_HL_A", "", "", "3", "1", "1", "40", "0", "30", "0", "1", "0", "",
                             "", "", "", ""});
            batt.add_record({"G_AT_CR_A", "", "", "23", "1", "1", "40", "0", "0", "0", "1", "0", "",
                             "", "", "", ""});
            batt.write(dir / "Gattacks.dbf");

            auto gunits = make_base_gunits_builder();
            add_combat_unit(gunits, "G_CA_HCAL", "G_AT_HL_A", "G_AT_CR_A");
            gunits.write(dir / "Gunits.dbf");
        });

        auto state = make_healcure_all_state(gd);

        auto outcomes = d2battle::valid_action_outcomes(state, gd);
        ASSERT_FALSE(outcomes.empty());

        std::ostringstream oss;
        print_selected_action(oss, state, outcomes[0].action, outcomes[0].outcome, gd);
        std::istringstream lines(oss.str());
        std::string        line;
        bool               found = false;
        while (std::getline(lines, line)) {
            if (line.rfind("uses ", 0) == 0) {
                EXPECT_EQ(line, "uses [Heal/All + Cure/All]");
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found);
    }

    // Heal/Any + Revive/Any
    {
        static std::atomic<unsigned> c{0};
        auto gd = make_gd_impl("t17b", c, [](const std::filesystem::path& dir) {
            auto batt = make_base_gattacks_builder();
            batt.add_record({"G_AT_HL", "", "", "3", "1", "2", "40", "0", "30", "0", "1", "0", "",
                             "", "", "", ""});
            batt.add_record({"G_AT_RV", "", "", "21", "1", "2", "40", "0", "60", "0", "1", "0", "",
                             "", "", "", ""});
            batt.write(dir / "Gattacks.dbf");

            auto gunits = make_base_gunits_builder();
            add_combat_unit(gunits, "G_CA_HRAN", "G_AT_HL", "G_AT_RV");
            gunits.write(dir / "Gunits.dbf");
        });

        auto state = make_healrevive_state(gd);
        auto outcomes = d2battle::valid_action_outcomes(state, gd);
        ASSERT_FALSE(outcomes.empty());

        std::ostringstream oss;
        print_selected_action(oss, state, outcomes[0].action, outcomes[0].outcome, gd);
        std::istringstream lines(oss.str());
        std::string        line;
        bool               found = false;
        while (std::getline(lines, line)) {
            if (line.rfind("uses ", 0) == 0) {
                EXPECT_EQ(line, "uses [Heal/Any + Revive/Any]");
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Test 18: SecondaryComponentDropsTargetsKilledByPrimary
// ═══════════════════════════════════════════════════════════════════════════════

TEST(CompositeAttackTest, SecondaryComponentDropsTargetsKilledByPrimary) {
    static std::atomic<unsigned> c{0};
    auto                         gd = make_gd_impl("t18", c, [](const std::filesystem::path& dir) {
        auto batt = make_base_gattacks_builder();
        batt.add_record({"G_AT_DM", "", "", "0", "1", "2", "40", "50", "0", "60", "1", "0", "", "",
                         "", "", ""});
        batt.add_record(
            {"G_AT_PT", "", "", "6", "1", "2", "40", "0", "0", "0", "1", "0", "", "", "", "", ""});
        batt.write(dir / "Gattacks.dbf");

        auto gunits = make_base_gunits_builder();
        add_combat_unit(gunits, "G_CA_DPAN", "G_AT_DM", "G_AT_PT");
        gunits.write(dir / "Gunits.dbf");
    });

    auto state = make_damage_petrify_dual_enemy_state(gd, 50);

    const auto before_fp = d2battle::compute_fingerprint(state);
    auto       actions = d2battle::valid_actions(state, gd);
    ASSERT_FALSE(actions.empty());

    // Find the action targeting E0
    const d2battle::BattleAction* action_e0 = nullptr;
    for (const auto& a : actions) {
        if (auto* atk = std::get_if<d2battle::AttackAction>(&a)) {
            if (auto* ut = std::get_if<d2battle::UnitTarget>(&atk->target)) {
                if (ut->unit_id == "E0") {
                    action_e0 = &a;
                    break;
                }
            }
        }
    }
    ASSERT_NE(action_e0, nullptr);

    auto next = d2battle::apply(state, *action_e0, gd);

    auto* enemy = next.find_unit("E0");
    ASSERT_NE(enemy, nullptr);
    EXPECT_EQ(enemy->current_hp, 0);
    EXPECT_FALSE(enemy->alive);
    EXPECT_FALSE(d2battle::detail::is_petrified(*enemy));

    EXPECT_EQ(d2battle::compute_fingerprint(state), before_fp);
    EXPECT_EQ(next.round_state.current_turn_index, 2u);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Test 19: CompositeActionRejectsWrongTargetShape
// ═══════════════════════════════════════════════════════════════════════════════

TEST(CompositeAttackTest, CompositeActionRejectsWrongTargetShape) {
    // Heal/All + Cure/All with UnitTarget -> TargetShapeMismatch
    {
        static std::atomic<unsigned> c{0};
        auto gd = make_gd_impl("t19a", c, [](const std::filesystem::path& dir) {
            auto batt = make_base_gattacks_builder();
            batt.add_record({"G_AT_HL_A", "", "", "3", "1", "1", "40", "0", "30", "0", "1", "0", "",
                             "", "", "", ""});
            batt.add_record({"G_AT_CR_A", "", "", "23", "1", "1", "40", "0", "0", "0", "1", "0", "",
                             "", "", "", ""});
            batt.write(dir / "Gattacks.dbf");

            auto gunits = make_base_gunits_builder();
            add_combat_unit(gunits, "G_CA_HCAL", "G_AT_HL_A", "G_AT_CR_A");
            gunits.write(dir / "Gunits.dbf");
        });

        auto state = make_healcure_all_state(gd);

        d2battle::AttackAction unit_target{"U0", d2battle::UnitTarget{"E0"}};
        auto err = d2battle::validate_action(d2battle::BattleAction{unit_target}, state, gd);
        EXPECT_EQ(err, d2battle::ActionValidationError::TargetShapeMismatch);
    }

    // Heal/Any + Revive/Any with AllAlliedUnitsTarget -> TargetShapeMismatch
    {
        static std::atomic<unsigned> c{0};
        auto gd = make_gd_impl("t19b", c, [](const std::filesystem::path& dir) {
            auto batt = make_base_gattacks_builder();
            batt.add_record({"G_AT_HL", "", "", "3", "1", "2", "40", "0", "30", "0", "1", "0", "",
                             "", "", "", ""});
            batt.add_record({"G_AT_RV", "", "", "21", "1", "2", "40", "0", "60", "0", "1", "0", "",
                             "", "", "", ""});
            batt.write(dir / "Gattacks.dbf");

            auto gunits = make_base_gunits_builder();
            add_combat_unit(gunits, "G_CA_HRAN", "G_AT_HL", "G_AT_RV");
            gunits.write(dir / "Gunits.dbf");
        });

        auto                   state = make_healrevive_state(gd);
        d2battle::AttackAction all_target{"U0", d2battle::AllAlliedUnitsTarget{}};
        auto err = d2battle::validate_action(d2battle::BattleAction{all_target}, state, gd);
        EXPECT_EQ(err, d2battle::ActionValidationError::TargetShapeMismatch);
    }
}

} // namespace
