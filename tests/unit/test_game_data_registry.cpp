#include <gtest/gtest.h>

#include "tests/test_dbf_builder.hpp"

#include "d2engine/assets/asset_runtime.hpp"
#include "d2engine/assets/game_data_registry.hpp"
#include "d2engine/assets/unit_def.hpp"
#include "d2engine/battle_adapters/raw_ff_animation_catalog.hpp"

#include <d2battle_rules/attack_support.hpp>

#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace d2engine {

using test_dbf::DbfBuilder;

// ── parse_resource_cost ───────────────────────────────────────────────────────

TEST(ParseResourceCost, FullString) {
    const ResourceCost c = parse_resource_cost("g0050:r0010:y0025:e0005:w0003");
    EXPECT_EQ(c.gold, 50);
    EXPECT_EQ(c.runic, 10);
    EXPECT_EQ(c.yellow, 25);
    EXPECT_EQ(c.infernal, 5);
    EXPECT_EQ(c.grove, 3);
}

TEST(ParseResourceCost, EmptyString) {
    const ResourceCost c = parse_resource_cost("");
    EXPECT_EQ(c.gold, 0);
    EXPECT_EQ(c.runic, 0);
    EXPECT_EQ(c.yellow, 0);
    EXPECT_EQ(c.infernal, 0);
    EXPECT_EQ(c.grove, 0);
}

TEST(ParseResourceCost, PartialString) {
    const ResourceCost c = parse_resource_cost("g0050:r0000:y0025:e0000:w0000");
    EXPECT_EQ(c.gold, 50);
    EXPECT_EQ(c.yellow, 25);
    EXPECT_EQ(c.runic, 0);
}

// ── death_battle_ff_name static function ─────────────────────────────────────

TEST(DeathBattleFfName, AllKnownMappings) {
    EXPECT_EQ(GameDataRegistry::death_battle_ff_name("L_HUMAN", true), "DEATH_HUMAN_S13");
    EXPECT_EQ(GameDataRegistry::death_battle_ff_name("L_HERETIC", true), "DEATH_HERETIC_S13");
    EXPECT_EQ(GameDataRegistry::death_battle_ff_name("L_DWARF", true), "DEATH_DWARF_S15");
    EXPECT_EQ(GameDataRegistry::death_battle_ff_name("L_UNDEAD", true), "DEATH_UNDEAD_S15");
    EXPECT_EQ(GameDataRegistry::death_battle_ff_name("L_NEUTRAL", true), "DEATH_NEUTRAL_S10");
    EXPECT_EQ(GameDataRegistry::death_battle_ff_name("L_DRAGON", false), "DEATH_DRAGON_S15");
    EXPECT_EQ(GameDataRegistry::death_battle_ff_name("L_GHOST", true), "DEATH_GHOST_S15");
    EXPECT_EQ(GameDataRegistry::death_battle_ff_name("L_ELF", true), "DEATH_ELF_S15");
    EXPECT_EQ(GameDataRegistry::death_battle_ff_name("L_ELF", false), "DEATH_ELF_L15");
}

TEST(DeathBattleFfName, UnknownDefaultsToHuman) {
    EXPECT_EQ(GameDataRegistry::death_battle_ff_name("L_UNKNOWN", true), "DEATH_HUMAN_S13");
}

// ── bones_sprite_base static function ────────────────────────────────────────

TEST(BonesSpriteBase, SmallReturnsSABase) {
    EXPECT_EQ(GameDataRegistry::bones_sprite_base("L_HUMAN", true), "DEAD_HUMAN_SA");
}

TEST(BonesSpriteBase, LargeReturnsLABase) {
    EXPECT_EQ(GameDataRegistry::bones_sprite_base("L_HUMAN", false), "DEAD_HUMAN_LA");
}

TEST(BonesSpriteBase, UnknownLabelFallsBackToSmallSA) {
    EXPECT_EQ(GameDataRegistry::bones_sprite_base("L_DRAGON", true), "DEAD_HUMAN_SA");
}

TEST(BonesSpriteBase, UnknownLabelFallsBackToLargeLA) {
    EXPECT_EQ(GameDataRegistry::bones_sprite_base("L_DRAGON", false), "DEAD_HUMAN_LA");
}

TEST(MissingFileRegistryTest, DoesNotThrow) {
    EXPECT_NO_THROW({
        GameDataRegistry reg{std::filesystem::path("/nonexistent/path/that/doesnt/exist")};
        EXPECT_TRUE(reg.all_units().empty());
    });
}

namespace {

using test_dbf::DbfBuilder;

std::filesystem::path write_attack_link_fixture_globals(const std::string& secondary_attack_id) {
    static std::atomic<unsigned> counter{0};
    auto tmp = std::filesystem::temp_directory_path() /
               ("d2_attack_link_registry_test_" + std::to_string(counter++));
    std::filesystem::remove_all(tmp);
    std::filesystem::create_directories(tmp);

    {
        DbfBuilder builder{{"TXT_ID", 'C', 20}, {"TEXT", 'C', 50}};
        builder.add_record({"L_HUMAN", "Human"});
        builder.write(tmp / "Tglobal.dbf");
    }

    for (const auto& name : {"LunitB.dbf", "LunitC.dbf", "LDthAnim.dbf", "GDynUpgr.dbf",
                             "GMabi.dbf", "Gimmu.dbf", "GimmuC.dbf"}) {
        std::ofstream ofs(tmp / name, std::ios::binary);
        ofs.put(0x1a);
    }

    {
        DbfBuilder builder{{"ID", 'N', 2}, {"TEXT", 'C', 20}};
        builder.add_record({"0", "L_DAMAGE"});
        builder.add_record({"3", "L_HEAL"});
        builder.write(tmp / "LattC.dbf");
    }

    {
        DbfBuilder builder{{"ID", 'N', 2}, {"TEXT", 'C', 20}};
        builder.add_record({"1", "L_ALL"});
        builder.add_record({"2", "L_ANY"});
        builder.write(tmp / "LAttR.dbf");
    }

    {
        DbfBuilder builder{{"ID", 'N', 2}, {"TEXT", 'C', 20}};
        builder.add_record({"1", "L_WEAPON"});
        builder.write(tmp / "LattS.dbf");
    }

    {
        DbfBuilder builder{{"RACE_ID", 'C', 16},   {"NAME_TXT", 'C', 20},  {"RACE_TYPE", 'N', 3},
                           {"PLAYABLE", 'C', 1},   {"REGEN_H", 'N', 3},    {"GUARDIAN", 'C', 16},
                           {"LEADER_1", 'C', 16},  {"LEADER_2", 'C', 16},  {"LEADER_3", 'C', 16},
                           {"LEADER_4", 'C', 16},  {"SOLDIER_1", 'C', 16}, {"SOLDIER_2", 'C', 16},
                           {"SOLDIER_3", 'C', 16}, {"SOLDIER_4", 'C', 16}, {"SOLDIER_5", 'C', 16}};
        builder.add_record({"G000SU0001", "L_HUMAN", "1", "T", "10", "G000UU0001", "", "", "", "",
                            "", "", "", "", ""});
        builder.write(tmp / "Grace.dbf");
    }

    {
        DbfBuilder builder{{"ATT_ID", 'C', 16},    {"NAME_TXT", 'C', 20},  {"DESC_TXT", 'C', 20},
                           {"CLASS", 'N', 2},      {"SOURCE", 'N', 2},     {"REACH", 'N', 2},
                           {"INITIATIVE", 'N', 3}, {"QTY_DAM", 'N', 4},    {"QTY_HEAL", 'N', 3},
                           {"POWER", 'N', 3},      {"INFINITE", 'N', 1},   {"CRIT_HIT", 'N', 1},
                           {"WARD1", 'C', 40},     {"WARD2", 'C', 40},     {"WARD3", 'C', 40},
                           {"WARD4", 'C', 40},     {"ALT_ATTACK", 'C', 16}};
        builder.add_record({"G000AT0001", "", "", "0", "1", "2", "40", "50", "0", "60", "1", "0",
                            "", "", "", "", ""});
        builder.write(tmp / "Gattacks.dbf");
    }

    {
        DbfBuilder builder{
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
        builder.add_record(
            {"G000UU0001", "",  "",  "",  "G000SU0001", "0", "1", "0",          "1",
             "1",          "0", "0", "1", "100",        "2", "0", "2",          "1",
             "0",          "0", "0", "0", "",           "",  "",  "G000AT0001", secondary_attack_id,
             "",           "",  "",  "",  "",           "",  "1", "L_HUMAN"});
        builder.write(tmp / "Gunits.dbf");
    }

    return tmp;
}

} // namespace

TEST(GameDataRegistryAttackLinks, CanonicalizesNullAttackSentinelToEmptySecondaryLink) {
    const auto       path = write_attack_link_fixture_globals("G000000000");
    GameDataRegistry registry(path);

    const auto* unit = registry.find_unit("g000uu0001");
    ASSERT_NE(unit, nullptr);
    EXPECT_EQ(unit->primary_attack_id, "g000at0001");
    ASSERT_NE(unit->primary_attack, nullptr);
    EXPECT_TRUE(unit->secondary_attack_id.empty());
    EXPECT_EQ(unit->secondary_attack, nullptr);

    auto support = d2battle::analyze_attack_bundle(*unit);
    EXPECT_TRUE(support.supported);

    std::filesystem::remove_all(path);
}

TEST(GameDataRegistryAttackLinks, PreservesUnresolvedNonSentinelSecondaryLink) {
    const auto       path = write_attack_link_fixture_globals("G000AT0002");
    GameDataRegistry registry(path);

    const auto* unit = registry.find_unit("g000uu0001");
    ASSERT_NE(unit, nullptr);
    EXPECT_FALSE(unit->secondary_attack_id.empty());
    EXPECT_EQ(unit->secondary_attack, nullptr);

    auto support = d2battle::analyze_attack_bundle(*unit);
    EXPECT_FALSE(support.supported);
    EXPECT_EQ(support.error, d2battle::AttackBundleSupportError::MissingSecondaryDefinition);

    std::filesystem::remove_all(path);
}

// ── find_race_by_type with synthetic fixture ────────────────────────────

std::filesystem::path write_race_fixture_globals() {
    static std::atomic<unsigned> counter{0};
    auto                         tmp = std::filesystem::temp_directory_path() /
                                       ("d2_race_lookup_test_" + std::to_string(counter++));
    std::filesystem::remove_all(tmp);
    std::filesystem::create_directories(tmp);

    {
        DbfBuilder builder{{"TXT_ID", 'C', 20}, {"TEXT", 'C', 50}};
        builder.add_record({"L_HUMAN", "Human"});
        builder.add_record({"L_ELF", "Elf"});
        builder.write(tmp / "Tglobal.dbf");
    }

    // Minimal empty DBF files for remaining tables
    for (const auto& name :
         {"LattC.dbf", "LattS.dbf", "LAttR.dbf", "LunitB.dbf", "LunitC.dbf", "LDthAnim.dbf",
          "Gattacks.dbf", "GDynUpgr.dbf", "Gunits.dbf", "GMabi.dbf", "Gimmu.dbf", "GimmuC.dbf"}) {
        auto           path = tmp / name;
        std::ofstream  ofs(path, std::ios::binary);
        constexpr char eof_marker = 0x1a;
        ofs.write(&eof_marker, 1);
        ofs.close();
    }

    {
        DbfBuilder builder{{"RACE_ID", 'C', 16},   {"NAME_TXT", 'C', 20},  {"RACE_TYPE", 'N', 3},
                           {"PLAYABLE", 'C', 1},   {"REGEN_H", 'N', 3},    {"GUARDIAN", 'C', 16},
                           {"LEADER_1", 'C', 16},  {"LEADER_2", 'C', 16},  {"LEADER_3", 'C', 16},
                           {"LEADER_4", 'C', 16},  {"SOLDIER_1", 'C', 16}, {"SOLDIER_2", 'C', 16},
                           {"SOLDIER_3", 'C', 16}, {"SOLDIER_4", 'C', 16}, {"SOLDIER_5", 'C', 16}};
        builder.add_record({"G000SU0001", "L_HUMAN", "1", "T", "10", "G000UU0001", "", "", "", "",
                            "", "", "", "", ""});
        builder.add_record({"G000SU0002", "L_ELF", "2", "T", "8", "G000UU0002", "", "", "", "", "",
                            "", "", "", ""});
        builder.add_record(
            {"G000SU0003", "", "99", "F", "0", "", "", "", "", "", "", "", "", "", ""});
        builder.write(tmp / "Grace.dbf");
    }

    return tmp;
}

TEST(FindRaceByType, ReturnsRaceForKnownType) {
    const auto       globals_path = write_race_fixture_globals();
    GameDataRegistry registry(globals_path);
    std::filesystem::remove_all(globals_path);

    const auto* race = registry.find_race_by_type(1);
    ASSERT_NE(race, nullptr);
    EXPECT_EQ(race->race_type, 1);
    EXPECT_EQ(race->name, "Human");

    const auto* race2 = registry.find_race_by_type(2);
    ASSERT_NE(race2, nullptr);
    EXPECT_EQ(race2->race_type, 2);
    EXPECT_EQ(race2->name, "Elf");
}

TEST(FindRaceByType, ReturnsNullForUnknownType) {
    const auto       globals_path = write_race_fixture_globals();
    GameDataRegistry registry(globals_path);
    std::filesystem::remove_all(globals_path);

    EXPECT_EQ(registry.find_race_by_type(42), nullptr);
    EXPECT_EQ(registry.find_race_by_type(0), nullptr);
    EXPECT_EQ(registry.find_race_by_type(-1), nullptr);
}

} // namespace d2engine
