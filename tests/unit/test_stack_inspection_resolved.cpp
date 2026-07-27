#include <gtest/gtest.h>

#include "tests/test_dbf_builder.hpp"

#include "d2engine/app/stack_inspection.hpp"
#include "d2engine/app/stack_info_asset_plan.hpp"
#include "d2engine/assets/canonical_containers.hpp"
#include "d2engine/assets/game_data_registry.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace d2engine {

namespace {

using test_dbf::DbfBuilder;

fs::path write_fixture_globals() {
    static std::atomic<unsigned> counter{0};
    auto                         tmp =
        fs::temp_directory_path() / ("d2_stack_inspect_resolved_" + std::to_string(counter++));
    fs::remove_all(tmp);
    fs::create_directories(tmp);

    // Tglobal.dbf: minimal text table (needed for NAME_TXT resolution)
    {
        DbfBuilder builder{{"TXT_ID", 'C', 20}, {"TEXT", 'C', 50}};
        builder.add_record({"L_SOLDIER", "Soldier"});
        builder.add_record({"L_FIGHTER", "Fighter"});
        builder.add_record({"UNIT_TEST_NAME", "Test Unit"});
        builder.add_record({"UNIT_TEST_DESC", "A test unit for resolved inspection"});
        builder.write(tmp / "Tglobal.dbf");
    }

    // LunitC.dbf: unit category
    {
        DbfBuilder builder{{"ID", 'N', 3}, {"TEXT", 'C', 20}};
        builder.add_record({"1", "L_SOLDIER"});
        builder.add_record({"3", "L_LEADER"});
        builder.write(tmp / "LunitC.dbf");
    }

    // LunitB.dbf: unit branch
    {
        DbfBuilder builder{{"ID", 'N', 3}, {"TEXT", 'C', 20}};
        builder.add_record({"1", "L_FIGHTER"});
        builder.write(tmp / "LunitB.dbf");
    }

    // LDthAnim.dbf: death animation
    {
        DbfBuilder builder{{"ID", 'N', 3}, {"TEXT", 'C', 20}};
        builder.add_record({"1", "L_HUMAN"});
        builder.write(tmp / "LDthAnim.dbf");
    }

    // LattC.dbf, LattS.dbf, LAttR.dbf, Gattacks.dbf, GDynUpgr.dbf,
    // Grace.dbf, GMabi.dbf, Gimmu.dbf, GimmuC.dbf: empty DBFs
    for (const auto& name : {"LattC.dbf", "LattS.dbf", "LAttR.dbf", "Gattacks.dbf", "GDynUpgr.dbf",
                             "Grace.dbf", "GMabi.dbf", "Gimmu.dbf", "GimmuC.dbf"}) {
        auto           path = tmp / name;
        std::ofstream  ofs(path, std::ios::binary);
        constexpr char eof_marker = 0x1a;
        ofs.write(&eof_marker, 1);
        ofs.close();
    }

    // Gunits.dbf: the main unit definition
    {
        DbfBuilder builder{{"UNIT_ID", 'C', 16},   {"RACE_ID", 'C', 16},    {"SUBRACE", 'N', 3},
                           {"LEVEL", 'N', 3},      {"HIT_POINT", 'N', 5},   {"ARMOR", 'N', 3},
                           {"REGEN", 'N', 3},      {"MOVE", 'N', 3},        {"SCOUT", 'N', 3},
                           {"LEADERSHIP", 'N', 3}, {"NEGOTIATE", 'N', 3},   {"XP_KILLED", 'N', 5},
                           {"XP_NEXT", 'N', 5},    {"SIZE_SMALL", 'C', 1},  {"SEX_M", 'C', 1},
                           {"ATCK_TWICE", 'C', 1}, {"WATER_ONLY", 'C', 1},  {"UNIT_CAT", 'N', 3},
                           {"BRANCH", 'N', 3},     {"NAME_TXT", 'C', 20},   {"DESC_TXT", 'C', 20},
                           {"ABIL_TXT", 'C', 20},  {"ENROLL_C", 'C', 20},   {"ENROLL_B", 'C', 20},
                           {"REVIVE_C", 'C', 20},  {"HEAL_C", 'C', 20},     {"TRAINING_C", 'C', 20},
                           {"ATTACK_ID", 'C', 16}, {"ATTACK2_ID", 'C', 16}, {"PREV_ID", 'C', 16},
                           {"BASE_UNIT", 'C', 16}, {"UPGRADE_B", 'C', 16},  {"DYN_UPG1", 'C', 16},
                           {"DYN_UPG2", 'C', 16},  {"DYN_UPG_LV", 'N', 3},  {"DEATH_ANIM", 'N', 3}};

        // Unit with explicit name, level=1, hit_points=50
        builder.add_record({"g000uu0042",
                            "g000su0001",
                            "1",
                            "1",
                            "50",
                            "10",
                            "2",
                            "4",
                            "0",
                            "0",
                            "0",
                            "0",
                            "0",
                            "T",
                            "T",
                            "F",
                            "F",
                            "3",
                            "1",
                            "UNIT_TEST_NAME",
                            "UNIT_TEST_DESC",
                            "",
                            "",
                            "",
                            "",
                            "",
                            "",
                            "",
                            "",
                            "",
                            "",
                            "",
                            "",
                            "",
                            "1"});

        // Large unit: size_small = false
        builder.add_record({"g000uu0050", "g000su0001", "1", "3", "200", "15", "0", "3", "0",
                            "0",          "0",          "0", "0", "F",   "T",  "F", "F", "1",
                            "1",          "",           "",  "",  "",    "",   "",  "",  "",
                            "",           "",           "",  "",  "",    "",   "",  "",  "1"});
        builder.write(tmp / "Gunits.dbf");
    }

    return tmp;
}

} // namespace

TEST(StackInspectionBuilder, ResolvedDefSeparatesInstanceHpFromDefinitionBaseHp) {
    auto             globals_path = write_fixture_globals();
    GameDataRegistry game_data(globals_path);
    fs::remove_all(globals_path);

    // Build world with a unit that has current_hp != definition hit_points
    d2runtime::AdventureWorldState world;
    d2runtime::AdventureStack      stack;
    stack.id = "S143STHP";
    stack.leader_id = "S143UNHP";
    stack.leader_alive = 1;
    stack.group.members[0] = "S143UNHP";
    stack.group.positions[0] = 0;
    world.stacks.push_back(std::move(stack));

    d2runtime::AdventureUnitInstance unit;
    unit.id = "S143UNHP";
    unit.type_id = "G000UU0042";
    unit.serialized_level = 3; // instance level != definition level (1)
    unit.current_hp = 100;     // instance HP != definition base_hp (50)
    unit.xp = 55;
    unit.creation = 0;
    unit.transformed = 0;
    unit.dynamic_level = std::nullopt;
    unit.modifier_ids = {};
    world.units.push_back(std::move(unit));

    StackInspectionBuilder builder(world, game_data);
    auto                   model = builder.build("S143STHP");
    ASSERT_TRUE(model.has_value());
    ASSERT_GE(model->members.size(), 1);

    const auto& member = model->members[0];

    // Instance values
    EXPECT_EQ(member.instance_id, "S143UNHP");
    EXPECT_EQ(member.type_id, "G000UU0042");
    EXPECT_EQ(member.current_hp, 100);
    EXPECT_EQ(member.serialized_level, 3);

    // Definition resolved
    EXPECT_TRUE(member.definition_resolved);
    ASSERT_TRUE(member.definition.has_value());

    // Definition values (different from instance)
    EXPECT_EQ(member.definition->base_hp, 50);
    EXPECT_EQ(member.definition->definition_level, 1);

    // Definition identity fields
    EXPECT_EQ(member.definition->unit_id, "g000uu0042");
    EXPECT_EQ(member.definition->name, "Test Unit");
    EXPECT_EQ(member.definition->description, "A test unit for resolved inspection");
}

TEST(StackInspectionBuilder, ResolvedLargeUnitAtAnchorFourProducesRowTwo) {
    auto             globals_path = write_fixture_globals();
    GameDataRegistry game_data(globals_path);
    fs::remove_all(globals_path);

    // Build world with a large unit at anchor 4
    d2runtime::AdventureWorldState world;
    d2runtime::AdventureStack      stack;
    stack.id = "S143STLG";
    stack.leader_id = "S143UNLG";
    stack.leader_alive = 1;
    stack.group.members[0] = "S143UNLG";
    stack.group.positions[0] = 4;
    world.stacks.push_back(std::move(stack));

    d2runtime::AdventureUnitInstance unit;
    unit.id = "S143UNLG";
    unit.type_id = "G000UU0050"; // large unit (size_small = false)
    unit.serialized_level = 1;
    unit.current_hp = 200;
    unit.xp = 0;
    unit.creation = 0;
    unit.transformed = 0;
    unit.dynamic_level = std::nullopt;
    unit.modifier_ids = {};
    world.units.push_back(std::move(unit));

    StackInspectionBuilder builder(world, game_data);
    auto                   model = builder.build("S143STLG");
    ASSERT_TRUE(model.has_value());
    ASSERT_EQ(model->members.size(), 1u);

    const auto& member = model->members[0];
    EXPECT_EQ(member.member_index, 0);
    EXPECT_EQ(member.instance_id, "S143UNLG");

    // Formation cells — large unit at anchor 4 → {4,5}
    ASSERT_EQ(member.formation_cells.size(), 2u);
    EXPECT_EQ(member.formation_cells[0], 4);
    EXPECT_EQ(member.formation_cells[1], 5);

    // Definition resolved and recognized as large
    EXPECT_TRUE(member.definition_resolved);
    ASSERT_TRUE(member.definition.has_value());
    EXPECT_FALSE(member.definition->size_small);

    // Plan assets and verify layout path
    PortraitManifestIndex portraits{PortraitManifest{}};
    auto                  tmp = fs::temp_directory_path() / "d2_sires_empty";
    fs::create_directories(tmp);
    for (const auto& name :
         {"Tglobal.dbf", "Gattacks.dbf", "Gunits.dbf", "Gupgrade.dbf", "Graces.dbf"}) {
        std::ofstream(tmp / name).put(0x1a);
    }
    GameDataRegistry reg(tmp);
    std::error_code  ec;
    fs::remove_all(tmp, ec);
    auto plan = plan_stack_info_assets(*model, portraits, reg);
    ASSERT_EQ(plan.planned_portraits.size(), 1u);
    const auto& pp = plan.planned_portraits[0];
    EXPECT_TRUE(pp.is_large);
    EXPECT_EQ(pp.formation_cell, 4);
    EXPECT_EQ(pp.layout_path, "/stack_info/formation/large_row_2");
}

} // namespace d2engine
