#include <gtest/gtest.h>

#include "tests/test_dbf_builder.hpp"
#include "tests/test_helpers.hpp"

#include "d2engine/assets/adventure_stack_actor_request_resolver.hpp"
#include "d2engine/assets/game_data_registry.hpp"

#include <d2runtime/AdventureGroundType.hpp>
#include <d2runtime/AdventureIsoDirection.hpp>
#include <d2runtime/AdventureWorldState.hpp>
#include <d2runtime/MovementCapabilities.hpp>

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

using namespace d2engine;
using namespace d2runtime;

// ── Ground leader on land → Unit presentation ───────────────────────────────

TEST(AdventureStackActorRequest, GroundLeaderOnLandIsUnit) {
    TempDir tmp("GroundLeaderOnLandIsUnit");

    {
        test_dbf::DbfBuilder gunits({
            {"UNIT_ID", 'C', 20},
            {"RACE_ID", 'C', 20},
            {"WATER_ONLY", 'C', 1},
        });
        gunits.add_record({"G000UU0001", "G000RR0000", ""});
        gunits.write(tmp.path() / "Gunits.dbf");
    }
    {
        test_dbf::DbfBuilder gmabi({
            {"UNIT_ID", 'C', 20},
            {"M_ABILITY", 'N', 3},
        });
        gmabi.write(tmp.path() / "GMabi.dbf");
    }

    GameDataRegistry registry(tmp.path());

    AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.terrain.width = 10;
    world.terrain.height = 10;
    world.terrain.tiles.assign(100, AdventureTerrainTile{1});

    AdventureUnitInstance unit;
    unit.id = "u1";
    unit.type_id = "G000UU0001";
    world.units.push_back(unit);

    AdventureStack stack;
    stack.id = "S1";
    stack.position = {2, 3};
    stack.leader_id = "u1";
    stack.facing = AdventureIsoDirection::D3;
    world.stacks.push_back(stack);

    AdventureStackActorRequestResolver resolver(registry);
    const auto                         request = resolver.resolve(world, world.stacks[0]);

    EXPECT_EQ(request.presentation.kind, AdventureActorPresentationKind::Unit);
    EXPECT_TRUE(request.race_id.empty());
    EXPECT_EQ(request.leader_unit_type_id, "G000UU0001");
    EXPECT_EQ(request.direction, AdventureIsoDirection::D3);
}

// ── Ground leader on water → Boat presentation ─────────────────────────────

TEST(AdventureStackActorRequest, GroundLeaderOnWaterIsBoat) {
    TempDir tmp("GroundLeaderOnWaterIsBoat");

    {
        test_dbf::DbfBuilder gunits({
            {"UNIT_ID", 'C', 20},
            {"RACE_ID", 'C', 20},
            {"WATER_ONLY", 'C', 1},
        });
        gunits.add_record({"G000UU0001", "G000RR0000", ""});
        gunits.write(tmp.path() / "Gunits.dbf");
    }
    {
        test_dbf::DbfBuilder gmabi({
            {"UNIT_ID", 'C', 20},
            {"M_ABILITY", 'N', 3},
        });
        gmabi.write(tmp.path() / "GMabi.dbf");
    }

    GameDataRegistry registry(tmp.path());

    AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.terrain.width = 10;
    world.terrain.height = 10;
    world.terrain.tiles.assign(100, AdventureTerrainTile{1});
    world.terrain.tiles[3 * 10 + 2] = AdventureTerrainTile{7};

    AdventureUnitInstance unit;
    unit.id = "u1";
    unit.type_id = "G000UU0001";
    world.units.push_back(unit);

    AdventureSubraceRef subrace;
    subrace.id = "G000G00001";
    subrace.player_id = "P1";
    subrace.race_id = "G000RR0003";
    subrace.subrace = 0;
    subrace.number = 0;
    world.subraces.push_back(subrace);

    AdventureStack stack;
    stack.id = "S1";
    stack.position = {2, 3};
    stack.leader_id = "u1";
    stack.facing = AdventureIsoDirection::D3;
    stack.owner = "P1";
    stack.subrace = "G000G00001";
    world.stacks.push_back(stack);

    AdventureStackActorRequestResolver resolver(registry);
    const auto                         request = resolver.resolve(world, world.stacks[0]);

    EXPECT_EQ(request.presentation.kind, AdventureActorPresentationKind::Boat);
    EXPECT_EQ(request.race_id, "G000RR0003");
    EXPECT_EQ(request.leader_unit_type_id, "G000UU0001");
}

// ── Native water traversal leader on water → Unit presentation ─────────────

TEST(AdventureStackActorRequest, NativeWaterLeaderOnWaterIsUnit) {
    TempDir tmp("NativeWaterLeaderOnWaterIsUnit");

    {
        test_dbf::DbfBuilder gunits({
            {"UNIT_ID", 'C', 20},
            {"RACE_ID", 'C', 20},
            {"WATER_ONLY", 'C', 1},
        });
        gunits.add_record({"G000UU0001", "G000RR0000", ""});
        gunits.write(tmp.path() / "Gunits.dbf");
    }
    {
        test_dbf::DbfBuilder gmabi({
            {"UNIT_ID", 'C', 20},
            {"M_ABILITY", 'N', 3},
        });
        gmabi.add_record({"G000UU0001", "3"});
        gmabi.write(tmp.path() / "GMabi.dbf");
    }

    GameDataRegistry registry(tmp.path());

    AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.terrain.width = 10;
    world.terrain.height = 10;
    world.terrain.tiles.assign(100, AdventureTerrainTile{1});
    world.terrain.tiles[3 * 10 + 2] = AdventureTerrainTile{7};

    AdventureUnitInstance unit;
    unit.id = "u1";
    unit.type_id = "G000UU0001";
    world.units.push_back(unit);

    AdventureStack stack;
    stack.id = "S1";
    stack.position = {2, 3};
    stack.leader_id = "u1";
    stack.facing = AdventureIsoDirection::D0;
    stack.owner = "P1";
    stack.subrace = "G000G00001";
    world.stacks.push_back(stack);

    AdventureStackActorRequestResolver resolver(registry);
    const auto                         request = resolver.resolve(world, world.stacks[0]);

    EXPECT_EQ(request.presentation.kind, AdventureActorPresentationKind::Unit);
    EXPECT_TRUE(request.race_id.empty());
    EXPECT_EQ(request.leader_unit_type_id, "G000UU0001");
}

// ── Water-only leader on water → Unit presentation ─────────────────────────

TEST(AdventureStackActorRequest, WaterOnlyLeaderOnWaterIsUnit) {
    TempDir tmp("WaterOnlyLeaderOnWaterIsUnit");

    {
        test_dbf::DbfBuilder gunits({
            {"UNIT_ID", 'C', 20},
            {"RACE_ID", 'C', 20},
            {"WATER_ONLY", 'C', 1},
        });
        gunits.add_record({"G000UU0001", "G000RR0000", "T"});
        gunits.write(tmp.path() / "Gunits.dbf");
    }
    {
        test_dbf::DbfBuilder gmabi({
            {"UNIT_ID", 'C', 20},
            {"M_ABILITY", 'N', 3},
        });
        gmabi.write(tmp.path() / "GMabi.dbf");
    }

    GameDataRegistry registry(tmp.path());

    AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.terrain.width = 10;
    world.terrain.height = 10;
    world.terrain.tiles.assign(100, AdventureTerrainTile{1});
    world.terrain.tiles[3 * 10 + 2] = AdventureTerrainTile{7};

    AdventureUnitInstance unit;
    unit.id = "u1";
    unit.type_id = "G000UU0001";
    world.units.push_back(unit);

    AdventureStack stack;
    stack.id = "S1";
    stack.position = {2, 3};
    stack.leader_id = "u1";
    stack.facing = AdventureIsoDirection::D0;
    world.stacks.push_back(stack);

    AdventureStackActorRequestResolver resolver(registry);
    const auto                         request = resolver.resolve(world, world.stacks[0]);

    EXPECT_EQ(request.presentation.kind, AdventureActorPresentationKind::Unit);
    EXPECT_TRUE(request.race_id.empty());
    EXPECT_EQ(request.leader_unit_type_id, "G000UU0001");
}

// ── Water-only leader on land → Unit presentation ──────────────────────────

TEST(AdventureStackActorRequest, WaterOnlyLeaderOnLandIsUnit) {
    TempDir tmp("WaterOnlyLeaderOnLandIsUnit");

    {
        test_dbf::DbfBuilder gunits({
            {"UNIT_ID", 'C', 20},
            {"RACE_ID", 'C', 20},
            {"WATER_ONLY", 'C', 1},
        });
        gunits.add_record({"G000UU0001", "G000RR0000", "T"});
        gunits.write(tmp.path() / "Gunits.dbf");
    }
    {
        test_dbf::DbfBuilder gmabi({
            {"UNIT_ID", 'C', 20},
            {"M_ABILITY", 'N', 3},
        });
        gmabi.write(tmp.path() / "GMabi.dbf");
    }

    GameDataRegistry registry(tmp.path());

    AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.terrain.width = 10;
    world.terrain.height = 10;
    world.terrain.tiles.assign(100, AdventureTerrainTile{1});

    AdventureUnitInstance unit;
    unit.id = "u1";
    unit.type_id = "G000UU0001";
    world.units.push_back(unit);

    AdventureStack stack;
    stack.id = "S1";
    stack.position = {2, 3};
    stack.leader_id = "u1";
    stack.facing = AdventureIsoDirection::D0;
    world.stacks.push_back(stack);

    AdventureStackActorRequestResolver resolver(registry);
    const auto                         request = resolver.resolve(world, world.stacks[0]);

    EXPECT_EQ(request.presentation.kind, AdventureActorPresentationKind::Unit);
    EXPECT_TRUE(request.race_id.empty());
    EXPECT_EQ(request.leader_unit_type_id, "G000UU0001");
}

// ── Boat race_id comes from subrace, not unit's RACE_ID ─────────────────────

TEST(AdventureStackActorRequest, OwnedRaceRegression) {
    TempDir tmp("OwnedRaceRegression");

    {
        test_dbf::DbfBuilder gunits({
            {"UNIT_ID", 'C', 20},
            {"RACE_ID", 'C', 20},
            {"WATER_ONLY", 'C', 1},
        });
        gunits.add_record({"G000UU0001", "G000RR0000", ""});
        gunits.write(tmp.path() / "Gunits.dbf");
    }
    {
        test_dbf::DbfBuilder gmabi({
            {"UNIT_ID", 'C', 20},
            {"M_ABILITY", 'N', 3},
        });
        gmabi.write(tmp.path() / "GMabi.dbf");
    }

    GameDataRegistry registry(tmp.path());

    AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.terrain.width = 10;
    world.terrain.height = 10;
    world.terrain.tiles.assign(100, AdventureTerrainTile{1});
    world.terrain.tiles[3 * 10 + 2] = AdventureTerrainTile{7};

    AdventureUnitInstance unit;
    unit.id = "u1";
    unit.type_id = "G000UU0001";
    world.units.push_back(unit);

    AdventureSubraceRef subrace;
    subrace.id = "G000G00001";
    subrace.player_id = "P1";
    subrace.race_id = "G000RR0005";
    subrace.subrace = 0;
    subrace.number = 0;
    world.subraces.push_back(subrace);

    AdventureStack stack;
    stack.id = "S1";
    stack.position = {2, 3};
    stack.leader_id = "u1";
    stack.facing = AdventureIsoDirection::D0;
    stack.owner = "P1";
    stack.subrace = "G000G00001";
    world.stacks.push_back(stack);

    AdventureStackActorRequestResolver resolver(registry);
    const auto                         request = resolver.resolve(world, world.stacks[0]);

    EXPECT_EQ(request.presentation.kind, AdventureActorPresentationKind::Boat);
    EXPECT_EQ(request.race_id, "G000RR0005");
}

// ── Error: out-of-bounds position ───────────────────────────────────────────

TEST(AdventureStackActorRequest, OutOfBoundsPositionThrows) {
    TempDir tmp("OutOfBoundsPositionThrows");

    {
        test_dbf::DbfBuilder gunits({
            {"UNIT_ID", 'C', 20},
            {"RACE_ID", 'C', 20},
            {"WATER_ONLY", 'C', 1},
        });
        gunits.add_record({"G000UU0001", "G000RR0000", ""});
        gunits.write(tmp.path() / "Gunits.dbf");
    }
    {
        test_dbf::DbfBuilder gmabi({
            {"UNIT_ID", 'C', 20},
            {"M_ABILITY", 'N', 3},
        });
        gmabi.write(tmp.path() / "GMabi.dbf");
    }

    GameDataRegistry registry(tmp.path());

    AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.terrain.width = 10;
    world.terrain.height = 10;
    world.terrain.tiles.assign(100, AdventureTerrainTile{1});

    AdventureUnitInstance unit;
    unit.id = "u1";
    unit.type_id = "G000UU0001";
    world.units.push_back(unit);

    AdventureStack stack;
    stack.id = "S1";
    stack.position = {10, 3};
    stack.leader_id = "u1";
    stack.facing = AdventureIsoDirection::D0;
    world.stacks.push_back(stack);

    AdventureStackActorRequestResolver resolver(registry);
    EXPECT_THROW((void)resolver.resolve(world, world.stacks[0]), std::runtime_error);
}

// ── Error: unknown terrain material ─────────────────────────────────────────

TEST(AdventureStackActorRequest, UnknownTerrainMaterialThrows) {
    TempDir tmp("UnknownTerrainMaterialThrows");

    {
        test_dbf::DbfBuilder gunits({
            {"UNIT_ID", 'C', 20},
            {"RACE_ID", 'C', 20},
            {"WATER_ONLY", 'C', 1},
        });
        gunits.add_record({"G000UU0001", "G000RR0000", ""});
        gunits.write(tmp.path() / "Gunits.dbf");
    }
    {
        test_dbf::DbfBuilder gmabi({
            {"UNIT_ID", 'C', 20},
            {"M_ABILITY", 'N', 3},
        });
        gmabi.write(tmp.path() / "GMabi.dbf");
    }

    GameDataRegistry registry(tmp.path());

    AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.terrain.width = 10;
    world.terrain.height = 10;
    world.terrain.tiles.assign(100, AdventureTerrainTile{1});
    world.terrain.tiles[3 * 10 + 2] = AdventureTerrainTile{99};

    AdventureUnitInstance unit;
    unit.id = "u1";
    unit.type_id = "G000UU0001";
    world.units.push_back(unit);

    AdventureStack stack;
    stack.id = "S1";
    stack.position = {2, 3};
    stack.leader_id = "u1";
    stack.facing = AdventureIsoDirection::D0;
    world.stacks.push_back(stack);

    AdventureStackActorRequestResolver resolver(registry);
    EXPECT_THROW((void)resolver.resolve(world, world.stacks[0]), std::runtime_error);
}

// ── Error: missing leader instance ──────────────────────────────────────────

TEST(AdventureStackActorRequest, MissingLeaderInstanceThrows) {
    TempDir tmp("MissingLeaderInstanceThrows");

    {
        test_dbf::DbfBuilder gunits({
            {"UNIT_ID", 'C', 20},
            {"RACE_ID", 'C', 20},
            {"WATER_ONLY", 'C', 1},
        });
        gunits.add_record({"G000UU0001", "G000RR0000", ""});
        gunits.write(tmp.path() / "Gunits.dbf");
    }
    {
        test_dbf::DbfBuilder gmabi({
            {"UNIT_ID", 'C', 20},
            {"M_ABILITY", 'N', 3},
        });
        gmabi.write(tmp.path() / "GMabi.dbf");
    }

    GameDataRegistry registry(tmp.path());

    AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.terrain.width = 10;
    world.terrain.height = 10;
    world.terrain.tiles.assign(100, AdventureTerrainTile{1});

    AdventureStack stack;
    stack.id = "S1";
    stack.position = {2, 3};
    stack.leader_id = "nonexistent";
    stack.facing = AdventureIsoDirection::D0;
    world.stacks.push_back(stack);

    AdventureStackActorRequestResolver resolver(registry);
    EXPECT_THROW((void)resolver.resolve(world, world.stacks[0]), std::runtime_error);
}

// ── Error: leader unit type not in registry ─────────────────────────────────

TEST(AdventureStackActorRequest, MissingLeaderUnitDefThrows) {
    TempDir tmp("MissingLeaderUnitDefThrows");

    // Gunits.dbf exists but has no records matching the leader's type_id
    {
        test_dbf::DbfBuilder gunits({
            {"UNIT_ID", 'C', 20},
            {"RACE_ID", 'C', 20},
            {"WATER_ONLY", 'C', 1},
        });
        gunits.write(tmp.path() / "Gunits.dbf");
    }
    {
        test_dbf::DbfBuilder gmabi({
            {"UNIT_ID", 'C', 20},
            {"M_ABILITY", 'N', 3},
        });
        gmabi.write(tmp.path() / "GMabi.dbf");
    }

    GameDataRegistry registry(tmp.path());

    AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.terrain.width = 10;
    world.terrain.height = 10;
    world.terrain.tiles.assign(100, AdventureTerrainTile{1});

    AdventureUnitInstance unit;
    unit.id = "u1";
    unit.type_id = "G000UU0001";
    world.units.push_back(unit);

    AdventureStack stack;
    stack.id = "S1";
    stack.position = {2, 3};
    stack.leader_id = "u1";
    stack.facing = AdventureIsoDirection::D0;
    world.stacks.push_back(stack);

    AdventureStackActorRequestResolver resolver(registry);
    EXPECT_THROW((void)resolver.resolve(world, world.stacks[0]), std::runtime_error);
}

// ── Error: boat presentation with empty subrace ─────────────────────────────

TEST(AdventureStackActorRequest, EmptySubraceThrows) {
    TempDir tmp("EmptySubraceThrows");

    {
        test_dbf::DbfBuilder gunits({
            {"UNIT_ID", 'C', 20},
            {"RACE_ID", 'C', 20},
            {"WATER_ONLY", 'C', 1},
        });
        gunits.add_record({"G000UU0001", "G000RR0000", ""});
        gunits.write(tmp.path() / "Gunits.dbf");
    }
    {
        test_dbf::DbfBuilder gmabi({
            {"UNIT_ID", 'C', 20},
            {"M_ABILITY", 'N', 3},
        });
        gmabi.write(tmp.path() / "GMabi.dbf");
    }

    GameDataRegistry registry(tmp.path());

    AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.terrain.width = 10;
    world.terrain.height = 10;
    world.terrain.tiles.assign(100, AdventureTerrainTile{1});
    world.terrain.tiles[3 * 10 + 2] = AdventureTerrainTile{7};

    AdventureUnitInstance unit;
    unit.id = "u1";
    unit.type_id = "G000UU0001";
    world.units.push_back(unit);

    AdventureStack stack;
    stack.id = "S1";
    stack.position = {2, 3};
    stack.leader_id = "u1";
    stack.facing = AdventureIsoDirection::D0;
    stack.owner = "P1";
    // subrace intentionally left empty
    world.stacks.push_back(stack);

    AdventureStackActorRequestResolver resolver(registry);
    EXPECT_THROW((void)resolver.resolve(world, world.stacks[0]), std::runtime_error);
}

// ── Error: dangling subrace reference ───────────────────────────────────────

TEST(AdventureStackActorRequest, DanglingSubraceThrows) {
    TempDir tmp("DanglingSubraceThrows");

    {
        test_dbf::DbfBuilder gunits({
            {"UNIT_ID", 'C', 20},
            {"RACE_ID", 'C', 20},
            {"WATER_ONLY", 'C', 1},
        });
        gunits.add_record({"G000UU0001", "G000RR0000", ""});
        gunits.write(tmp.path() / "Gunits.dbf");
    }
    {
        test_dbf::DbfBuilder gmabi({
            {"UNIT_ID", 'C', 20},
            {"M_ABILITY", 'N', 3},
        });
        gmabi.write(tmp.path() / "GMabi.dbf");
    }

    GameDataRegistry registry(tmp.path());

    AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.terrain.width = 10;
    world.terrain.height = 10;
    world.terrain.tiles.assign(100, AdventureTerrainTile{1});
    world.terrain.tiles[3 * 10 + 2] = AdventureTerrainTile{7};

    AdventureUnitInstance unit;
    unit.id = "u1";
    unit.type_id = "G000UU0001";
    world.units.push_back(unit);

    // subraces list is empty — no subrace to match

    AdventureStack stack;
    stack.id = "S1";
    stack.position = {2, 3};
    stack.leader_id = "u1";
    stack.facing = AdventureIsoDirection::D0;
    stack.owner = "P1";
    stack.subrace = "G000G00001";
    world.stacks.push_back(stack);

    AdventureStackActorRequestResolver resolver(registry);
    EXPECT_THROW((void)resolver.resolve(world, world.stacks[0]), std::runtime_error);
}

// ── Error: subrace with empty race_id ───────────────────────────────────────

TEST(AdventureStackActorRequest, EmptyRaceIdThrows) {
    TempDir tmp("EmptyRaceIdThrows");

    {
        test_dbf::DbfBuilder gunits({
            {"UNIT_ID", 'C', 20},
            {"RACE_ID", 'C', 20},
            {"WATER_ONLY", 'C', 1},
        });
        gunits.add_record({"G000UU0001", "G000RR0000", ""});
        gunits.write(tmp.path() / "Gunits.dbf");
    }
    {
        test_dbf::DbfBuilder gmabi({
            {"UNIT_ID", 'C', 20},
            {"M_ABILITY", 'N', 3},
        });
        gmabi.write(tmp.path() / "GMabi.dbf");
    }

    GameDataRegistry registry(tmp.path());

    AdventureWorldState world;
    world.map_width = 10;
    world.map_height = 10;
    world.terrain.width = 10;
    world.terrain.height = 10;
    world.terrain.tiles.assign(100, AdventureTerrainTile{1});
    world.terrain.tiles[3 * 10 + 2] = AdventureTerrainTile{7};

    AdventureUnitInstance unit;
    unit.id = "u1";
    unit.type_id = "G000UU0001";
    world.units.push_back(unit);

    AdventureSubraceRef subrace;
    subrace.id = "G000G00001";
    subrace.player_id = "P1";
    subrace.race_id = "";
    subrace.subrace = 0;
    subrace.number = 0;
    world.subraces.push_back(subrace);

    AdventureStack stack;
    stack.id = "S1";
    stack.position = {2, 3};
    stack.leader_id = "u1";
    stack.facing = AdventureIsoDirection::D0;
    stack.owner = "P1";
    stack.subrace = "G000G00001";
    world.stacks.push_back(stack);

    AdventureStackActorRequestResolver resolver(registry);
    EXPECT_THROW((void)resolver.resolve(world, world.stacks[0]), std::runtime_error);
}
