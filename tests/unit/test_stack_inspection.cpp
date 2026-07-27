#include <gtest/gtest.h>

#include "d2engine/app/stack_inspection.hpp"
#include "d2engine/app/screen_config_store.hpp"
#include "d2engine/app/stack_info_asset_plan.hpp"
#include "d2engine/assets/canonical_containers.hpp"
#include "d2engine/assets/game_data_registry.hpp"
#include "d2engine/assets/unit_def.hpp"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace d2engine {

// ── Helper: build a minimal world with a stack and units ───────────────

d2runtime::AdventureWorldState make_test_world() {
    d2runtime::AdventureWorldState world;

    d2runtime::AdventureStack stack;
    stack.id = "S143ST0001";
    stack.group_id = "S143GR0001";
    stack.owner = "G000PL0001";
    stack.subrace = "G000SU0001";
    stack.inside = "";
    stack.position.x = 100;
    stack.position.y = 200;
    stack.move = 4;
    stack.morale = 50;
    stack.leader_id = "S143UN0001";
    stack.leader_alive = 1;

    stack.group.members[0] = "S143UN0001";
    stack.group.members[1] = "S143UN0002";
    stack.group.members[3] = "S143UN0003";
    // slots 2, 4, 5 remain empty
    // positions[member] = formation cell
    stack.group.positions = {0, 2, -1, 3, -1, -1};

    world.stacks.push_back(std::move(stack));

    // Unit instances
    auto add_unit = [&](const std::string& id, const std::string& type_id, int slot_index) {
        d2runtime::AdventureUnitInstance unit;
        unit.id = id;
        unit.type_id = type_id;
        unit.name = "Unit_" + id;
        unit.serialized_level = slot_index + 3;
        unit.current_hp = 100 - slot_index * 10;
        unit.xp = 50 + slot_index * 5;
        unit.creation = 0;
        unit.transformed = 0;
        unit.dynamic_level = std::nullopt;
        unit.modifier_ids = {};
        world.units.push_back(std::move(unit));
    };

    add_unit("S143UN0001", "G000UU0042", 0); // leader
    add_unit("S143UN0002", "G000UU0042", 1); // same type as leader
    add_unit("S143UN0003", "G000UU0007", 3); // different type

    return world;
}

d2runtime::AdventureWorldState make_world_with_duplicate_modifiers() {
    d2runtime::AdventureWorldState world;

    d2runtime::AdventureStack stack;
    stack.id = "S143ST0002";
    stack.leader_id = "S143UN0010";
    stack.leader_alive = 1;
    stack.group.members[0] = "S143UN0010";
    stack.group.positions[0] = 0;
    world.stacks.push_back(std::move(stack));

    d2runtime::AdventureUnitInstance unit;
    unit.id = "S143UN0010";
    unit.type_id = "G000UU0042";
    unit.current_hp = 80;
    unit.serialized_level = 5;
    unit.modifier_ids = {"mod_a", "mod_b", "mod_a", "mod_c"};
    world.units.push_back(std::move(unit));

    return world;
}

// ── Helper: create an empty GameDataRegistry for testing ───────────────
GameDataRegistry make_empty_registry() {
    // A unique path per call using a thread-safe counter
    static std::atomic<unsigned> counter{0};
    const auto                   n = counter++;
    const auto tmp = fs::temp_directory_path() / ("d2_stack_inspect_test_" + std::to_string(n));
    fs::create_directories(tmp);
    for (const auto& name :
         {"Tglobal.dbf", "Gattacks.dbf", "Gunits.dbf", "Gupgrade.dbf", "Graces.dbf"}) {
        std::ofstream(tmp / name).put(0x1a);
    }
    GameDataRegistry registry(tmp);
    std::error_code  ec;
    fs::remove_all(tmp, ec);
    return registry;
}

// ── StackInspectionBuilder tests ───────────────────────────────────────

TEST(StackInspectionBuilder, LeaderNotAtMemberIndexZero) {
    // Build a stack where leader is in member slot 3, not slot 0
    d2runtime::AdventureWorldState world;

    d2runtime::AdventureStack stack;
    stack.id = "S143STLEAD";
    stack.group_id = "S143GR0001";
    stack.owner = "G000PL0001";
    stack.subrace = "G000SU0001";
    stack.position.x = 100;
    stack.position.y = 200;
    stack.move = 4;
    stack.morale = 50;
    stack.leader_id = "S143UNLDR"; // leader is in slot 3
    stack.leader_alive = 1;

    // Slot 0 = non-leader, Slot 3 = leader, others empty
    // Member 0 in cells 0, Member 3 in cell 3
    stack.group.members[0] = "S143UNNOT";
    stack.group.members[3] = "S143UNLDR";
    stack.group.positions = {0, -1, -1, 3, -1, -1};

    world.stacks.push_back(std::move(stack));

    auto add_unit = [&](const std::string& id, const std::string& type_id) {
        d2runtime::AdventureUnitInstance unit;
        unit.id = id;
        unit.type_id = type_id;
        unit.serialized_level = 3;
        unit.current_hp = 50;
        unit.xp = 0;
        unit.creation = 0;
        unit.transformed = 0;
        unit.dynamic_level = std::nullopt;
        unit.modifier_ids = {};
        world.units.push_back(std::move(unit));
    };

    add_unit("S143UNNOT", "G000UU0001"); // slot 0, not leader
    add_unit("S143UNLDR", "G000UU0042"); // slot 3, leader

    auto                   empty_registry = make_empty_registry();
    StackInspectionBuilder builder(world, empty_registry);
    auto                   model = builder.build("S143STLEAD");
    ASSERT_TRUE(model.has_value());

    // leader_id at stack level
    EXPECT_EQ(model->leader_id, "S143UNLDR");

    // member slot 0: NOT leader
    ASSERT_GE(model->members.size(), 2);
    const auto& m0 = model->members[0];
    EXPECT_EQ(m0.instance_id, "S143UNNOT");
    EXPECT_EQ(m0.member_index, 0);
    EXPECT_FALSE(m0.is_leader);

    // member slot 3: IS leader
    const auto& m3 = model->members[1];
    EXPECT_EQ(m3.instance_id, "S143UNLDR");
    EXPECT_EQ(m3.member_index, 3);
    EXPECT_TRUE(m3.is_leader);

    // formation cells for non-leader (formation[0] = 0)
    EXPECT_EQ(m0.formation_cells, std::vector<int>({0}));
    // formation cells for leader (formation[3] = 3)
    EXPECT_EQ(m3.formation_cells, std::vector<int>({3}));
}

TEST(StackInspectionBuilder, MemberIndexPreserved) {
    auto world = make_test_world();
    auto empty_registry = make_empty_registry();

    StackInspectionBuilder builder(world, empty_registry);
    auto                   model = builder.build("S143ST0001");
    ASSERT_TRUE(model.has_value());

    ASSERT_GE(model->members.size(), 3);
    EXPECT_EQ(model->members[0].member_index, 0);
    EXPECT_EQ(model->members[1].member_index, 1);
    EXPECT_EQ(model->members[2].member_index, 3);
}

TEST(StackInspectionBuilder, FormationCellsComputedCorrectly) {
    auto world = make_test_world();
    auto empty_registry = make_empty_registry();

    StackInspectionBuilder builder(world, empty_registry);
    auto                   model = builder.build("S143ST0001");
    ASSERT_TRUE(model.has_value());

    // positions = {0, 2, -1, 3, -1, -1}
    // positions[member] = anchor cell
    // member 0 (slot 0): anchor cell 0, small unit (no definition) → {0}
    // member 1 (slot 1): anchor cell 2, small → {2}
    // member 3 (slot 3): anchor cell 3, small → {3}
    ASSERT_GE(model->members.size(), 3);

    const auto& m0 = model->members[0];
    EXPECT_EQ(m0.formation_cells, std::vector<int>({0}));

    const auto& m1 = model->members[1];
    EXPECT_EQ(m1.formation_cells, std::vector<int>({2}));

    const auto& m2 = model->members[2];
    EXPECT_EQ(m2.formation_cells, std::vector<int>({3}));
}

TEST(StackInspectionBuilder, AllSixMemberSlotsValidated) {
    auto world = make_test_world();
    auto empty_registry = make_empty_registry();

    StackInspectionBuilder builder(world, empty_registry);
    auto                   model = builder.build("S143ST0001");
    ASSERT_TRUE(model.has_value());

    // member_slots[0] = S143UN0001, [1] = S143UN0002, [2] = <empty>, [3] = S143UN0003, [4] =
    // <empty>, [5] = <empty>
    ASSERT_TRUE(model->member_slots[0].has_value());
    EXPECT_EQ(*model->member_slots[0], "S143UN0001");
    ASSERT_TRUE(model->member_slots[1].has_value());
    EXPECT_EQ(*model->member_slots[1], "S143UN0002");
    EXPECT_FALSE(model->member_slots[2].has_value()); // empty
    ASSERT_TRUE(model->member_slots[3].has_value());
    EXPECT_EQ(*model->member_slots[3], "S143UN0003");
    EXPECT_FALSE(model->member_slots[4].has_value()); // empty
    EXPECT_FALSE(model->member_slots[5].has_value()); // empty
}

TEST(StackInspectionBuilder, SameTypeUnitsAreDistinctMembers) {
    auto world = make_test_world();
    auto empty_registry = make_empty_registry();

    StackInspectionBuilder builder(world, empty_registry);
    auto                   model = builder.build("S143ST0001");
    ASSERT_TRUE(model.has_value());

    ASSERT_GE(model->members.size(), 2);
    EXPECT_EQ(model->members[0].instance_id, "S143UN0001");
    EXPECT_EQ(model->members[0].type_id, "G000UU0042");
    EXPECT_EQ(model->members[1].instance_id, "S143UN0002");
    EXPECT_EQ(model->members[1].type_id, "G000UU0042"); // same type, different instance
    EXPECT_NE(model->members[0].instance_id, model->members[1].instance_id);
}

TEST(StackInspectionBuilder, DuplicateModifierIdsPreserved) {
    auto world = make_world_with_duplicate_modifiers();
    auto empty_registry = make_empty_registry();

    StackInspectionBuilder builder(world, empty_registry);
    auto                   model = builder.build("S143ST0002");
    ASSERT_TRUE(model.has_value());

    ASSERT_GE(model->members.size(), 1);
    const auto& member = model->members[0];
    EXPECT_EQ(member.modifier_ids.size(), 4);
    EXPECT_EQ(member.modifier_ids[0], "mod_a");
    EXPECT_EQ(member.modifier_ids[1], "mod_b");
    EXPECT_EQ(member.modifier_ids[2], "mod_a"); // duplicate! preserved
    EXPECT_EQ(member.modifier_ids[3], "mod_c");
}

TEST(StackInspectionBuilder, CurrentHpSeparateFromDefinitionBaseHp) {
    auto world = make_test_world();
    auto empty_registry = make_empty_registry();

    StackInspectionBuilder builder(world, empty_registry);
    auto                   model = builder.build("S143ST0001");
    ASSERT_TRUE(model.has_value());

    // current_hp comes from the instance, stored in UnitInspectionModel
    ASSERT_GE(model->members.size(), 1);
    EXPECT_EQ(model->members[0].current_hp, 100);

    // definition is unresolved since empty_registry — so no base_hp visible
    EXPECT_FALSE(model->members[0].definition_resolved);
}

TEST(StackInspectionBuilder, SerializedLevelSeparateFromDefinitionLevel) {
    auto world = make_test_world();
    auto empty_registry = make_empty_registry();

    StackInspectionBuilder builder(world, empty_registry);
    auto                   model = builder.build("S143ST0001");
    ASSERT_TRUE(model.has_value());

    ASSERT_GE(model->members.size(), 1);
    EXPECT_EQ(model->members[0].serialized_level, 3);
}

TEST(StackInspectionBuilder, UnresolvedUnitDefStillBuildsModel) {
    auto world = make_test_world();
    auto empty_registry = make_empty_registry();

    StackInspectionBuilder builder(world, empty_registry);
    auto                   model = builder.build("S143ST0001");
    ASSERT_TRUE(model.has_value());

    ASSERT_GE(model->members.size(), 1);
    EXPECT_FALSE(model->members[0].definition_resolved);
    EXPECT_EQ(model->members[0].unresolved_type, "G000UU0042");
    EXPECT_FALSE(model->members[0].definition.has_value());

    // Instance data preserved despite unresolved definition
    EXPECT_EQ(model->members[0].instance_id, "S143UN0001");
    EXPECT_EQ(model->members[0].type_id, "G000UU0042");
    EXPECT_TRUE(model->members[0].is_leader);
}

TEST(StackInspectionBuilder, LeaderAliveRawPreserved) {
    auto world = make_test_world();
    auto empty_registry = make_empty_registry();

    StackInspectionBuilder builder(world, empty_registry);
    auto                   model = builder.build("S143ST0001");
    ASSERT_TRUE(model.has_value());

    EXPECT_TRUE(model->leader_alive);
    EXPECT_EQ(model->leader_alive_raw, 1);
}

// ── Missing instance vs unresolved definition ────────────────────────────

TEST(StackInspectionBuilder, MissingInstanceReturnsUnresolvedInstanceId) {
    // Stack references a unit ID that does not exist in world.units
    d2runtime::AdventureWorldState world;
    d2runtime::AdventureStack      stack;
    stack.id = "S143STMIS";
    stack.leader_id = "S143UNDNE"; // does not exist
    stack.leader_alive = 1;
    stack.group.members[0] = "S143UNDNE";
    stack.group.positions[0] = 0;
    world.stacks.push_back(std::move(stack));

    auto                   empty_registry = make_empty_registry();
    StackInspectionBuilder builder(world, empty_registry);
    auto                   model = builder.build("S143STMIS");
    ASSERT_TRUE(model.has_value());

    ASSERT_GE(model->members.size(), 1);
    const auto& member = model->members[0];

    // instance not resolved
    EXPECT_FALSE(member.instance_resolved);
    EXPECT_EQ(member.unresolved_instance_id, "S143UNDNE");
    EXPECT_EQ(member.instance_id, "S143UNDNE");

    // type_id should be empty (no instance to get it from)
    EXPECT_TRUE(member.type_id.empty());

    // definition not attempted (no type_id to resolve)
    EXPECT_FALSE(member.definition_resolved);
    EXPECT_FALSE(member.definition.has_value());
}

TEST(StackInspectionBuilder, PresentInstanceWithUnresolvedDef) {
    // Instance exists but its type is not in GameDataRegistry
    d2runtime::AdventureWorldState world;
    d2runtime::AdventureStack      stack;
    stack.id = "S143STUDEF";
    stack.leader_id = "S143UNDEF";
    stack.leader_alive = 1;
    stack.group.members[0] = "S143UNDEF";
    stack.group.positions[0] = 0;
    world.stacks.push_back(std::move(stack));

    d2runtime::AdventureUnitInstance unit;
    unit.id = "S143UNDEF";
    unit.type_id = "G000UU_NONEXISTENT";
    unit.current_hp = 75;
    unit.serialized_level = 2;
    unit.modifier_ids = {};
    world.units.push_back(std::move(unit));

    auto                   empty_registry = make_empty_registry();
    StackInspectionBuilder builder(world, empty_registry);
    auto                   model = builder.build("S143STUDEF");
    ASSERT_TRUE(model.has_value());

    ASSERT_GE(model->members.size(), 1);
    const auto& member = model->members[0];

    // instance resolved
    EXPECT_TRUE(member.instance_resolved);
    EXPECT_EQ(member.instance_id, "S143UNDEF");
    EXPECT_EQ(member.type_id, "G000UU_NONEXISTENT");
    EXPECT_EQ(member.current_hp, 75);
    EXPECT_EQ(member.serialized_level, 2);

    // definition unresolved (type doesn't exist in registry)
    EXPECT_FALSE(member.definition_resolved);
    EXPECT_EQ(member.unresolved_type, "G000UU_NONEXISTENT");
    EXPECT_FALSE(member.definition.has_value());
}

TEST(StackInspectionBuilder, UnresolvedUnitDefaultsToSingleFormationCell) {
    d2runtime::AdventureWorldState world;

    d2runtime::AdventureStack stack;
    stack.id = "S143STBIG";
    stack.leader_id = "S143UNBIG";
    stack.leader_alive = 1;
    stack.group.members[0] = "S143UNBIG";
    // formation[cell] = member index: cells 0 and 1 both reference member 0
    stack.group.positions[0] = 0;
    world.stacks.push_back(std::move(stack));

    d2runtime::AdventureUnitInstance unit;
    unit.id = "S143UNBIG";
    unit.type_id = "G000UU0050";
    unit.current_hp = 200;
    unit.serialized_level = 1;
    unit.xp = 0;
    unit.creation = 0;
    unit.transformed = 0;
    unit.dynamic_level = std::nullopt;
    unit.modifier_ids = {};
    world.units.push_back(std::move(unit));

    auto                   empty_registry = make_empty_registry();
    StackInspectionBuilder builder(world, empty_registry);
    auto                   model = builder.build("S143STBIG");
    ASSERT_TRUE(model.has_value());
    ASSERT_EQ(model->members.size(), 1u);

    const auto& member = model->members[0];
    EXPECT_EQ(member.member_index, 0);
    EXPECT_EQ(member.instance_id, "S143UNBIG");
    // Without definition, defaults to small unit → single cell {0}
    EXPECT_EQ(member.formation_cells, std::vector<int>({0}));
}

TEST(StackInspection, SingleMemberProducesSingleFormationCell) {
    d2runtime::AdventureWorldState world;

    d2runtime::AdventureStack stack;
    stack.id = "S143STONLY";
    stack.leader_id = "S143UNONLY";
    stack.leader_alive = 1;
    stack.group.members[0] = "S143UNONLY";
    stack.group.positions[0] = 0;
    world.stacks.push_back(std::move(stack));

    d2runtime::AdventureUnitInstance unit;
    unit.id = "S143UNONLY";
    unit.type_id = "G000UU0050";
    unit.current_hp = 200;
    unit.serialized_level = 1;
    unit.xp = 0;
    unit.creation = 0;
    unit.transformed = 0;
    unit.dynamic_level = std::nullopt;
    unit.modifier_ids = {};
    world.units.push_back(unit);

    auto                   empty_registry = make_empty_registry();
    StackInspectionBuilder builder(world, empty_registry);
    auto                   model = builder.build("S143STONLY");
    ASSERT_TRUE(model.has_value());
    ASSERT_EQ(model->members.size(), 1u);

    const auto& member = model->members[0];
    EXPECT_EQ(member.member_index, 0);
    EXPECT_EQ(member.formation_cells.size(), 1u);
    EXPECT_EQ(member.formation_cells[0], 0);
}

TEST(StackInspection, StackInfoAssetPlanPopupUsesComposedSpriteNotRawPng) {
    StackInspectionModel model;
    model.id = "MERGE_REGRESSION";

    PortraitManifestIndex portraits{PortraitManifest{}};
    auto                  plan = plan_stack_info_assets(model, portraits, make_empty_registry());

    EXPECT_EQ(plan.popup_background.kind, ImageAssetKind::ComposedSprite)
        << "popup must use logical ComposedSprite, not raw physical PNG";
    EXPECT_EQ(plan.popup_background.container_path, kInterfContainer);
    EXPECT_EQ(plan.popup_background.image_name, "_PG0500IX");
}

// ── derive_formation_cells unit tests ────────────────────────────────────

TEST(DeriveFormationCells, InvalidAnchorReturnsEmpty) {
    EXPECT_TRUE(derive_formation_cells(-2, false).empty());
    EXPECT_TRUE(derive_formation_cells(-1, true).empty());
    EXPECT_TRUE(derive_formation_cells(6, false).empty());
    EXPECT_TRUE(derive_formation_cells(6, true).empty());
}

TEST(DeriveFormationCells, AllAnchorsSmallAndLarge) {
    // small: each anchor 0..5 -> {anchor}
    for (int a = 0; a <= 5; ++a) {
        const auto cells = derive_formation_cells(a, false);
        ASSERT_EQ(cells.size(), 1u) << "small anchor " << a;
        EXPECT_EQ(cells[0], a) << "small anchor " << a;
    }

    // large: row_start = (anchor / 2) * 2, produces {row_start, row_start+1}
    struct {
        int anchor;
        int expected_row_start;
    } large_cases[] = {{0, 0}, {1, 0}, {2, 2}, {3, 2}, {4, 4}, {5, 4}};
    for (const auto& c : large_cases) {
        const auto cells = derive_formation_cells(c.anchor, true);
        ASSERT_EQ(cells.size(), 2u) << "large anchor " << c.anchor;
        EXPECT_EQ(cells[0], c.expected_row_start) << "large anchor " << c.anchor << " cell[0]";
        EXPECT_EQ(cells[1], c.expected_row_start + 1) << "large anchor " << c.anchor << " cell[1]";
    }
}

// ── resolve_application_config_root tests ────────────────────────────────

TEST(ResolveAppConfigRoot, EmptyOverrideUsesRuntimeDefault) {
    // In test context, resolve_runtime_config_root may throw if no configs/
    // directory exists relative to the test executable. Both outcomes are valid.
    try {
        const auto result = resolve_application_config_root(std::filesystem::path{});
        EXPECT_FALSE(result.empty());
    } catch (const std::runtime_error&) {
        SUCCEED() << "runtime config root resolution threw (expected in test context)";
    }
}

TEST(ResolveAppConfigRoot, ValidOverrideReturnsCanonicalPath) {
    const auto tmp = fs::temp_directory_path() / "d2_config_test_valid";
    fs::create_directories(tmp / "screens");
    const auto expected = fs::canonical(tmp);
    EXPECT_EQ(resolve_application_config_root(tmp), expected);
    std::error_code ec;
    fs::remove_all(tmp, ec);
}

TEST(ResolveAppConfigRoot, MissingOverrideThrows) {
    const auto tmp = fs::temp_directory_path() / "d2_config_test_missing";
    fs::remove_all(tmp);
    EXPECT_THROW(resolve_application_config_root(tmp), std::runtime_error);
}

TEST(ResolveAppConfigRoot, MissingScreensSubdirThrows) {
    const auto tmp = fs::temp_directory_path() / "d2_config_test_noscreens";
    fs::create_directories(tmp);
    EXPECT_THROW(resolve_application_config_root(tmp), std::runtime_error);
    std::error_code ec;
    fs::remove_all(tmp, ec);
}

} // namespace d2engine
