#include <d2runtime/AdventureWorldBuilder.hpp>
#include <d2runtime/AdventureWorldState.hpp>
#include <d2runtime/AdventureTerrainDecoder.hpp>

#include <d2scenario/ScenarioTemplate.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <algorithm>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <set>
#include <variant>
#include <vector>

using namespace d2runtime;
using namespace d2scenario;

// ── Build minimal world from empty ScenarioTemplate ─────────────────────────

TEST(AdventureWorldBuilder, BuildsMinimalWorldFromEmptyTemplate) {
    ScenarioTemplate tmpl;
    tmpl.info.id = SgObjectId("scn_test");
    tmpl.info.name = "Test Scenario";

    AdventureWorldBuilder builder;
    auto                  result = builder.build(tmpl);

    EXPECT_EQ(result.world.scenario_id, "scn_test");
    EXPECT_EQ(result.world.scenario_name, "Test Scenario");
    EXPECT_EQ(result.world.semantic_object_count, 0u);
    EXPECT_EQ(result.world.runtime_object_count, 0u);
    EXPECT_EQ(result.world.map_width, 0);
    EXPECT_EQ(result.world.map_height, 0);
}

// ── Map dimensions preserved from terrain grid ──────────────────────────────

TEST(AdventureWorldBuilder, MapDimensionsPreservedFromTerrain) {
    ScenarioTemplate tmpl;
    tmpl.info.id = SgObjectId("scn_map");
    tmpl.info.map_size = 0; // should be ignored when terrain is valid

    tmpl.map.terrain.width = 72;
    tmpl.map.terrain.height = 72;
    // Rectangular grid: 72 rows × 72 columns
    tmpl.map.terrain.tiles.assign(72, std::vector<uint32_t>(72, 0));

    AdventureWorldBuilder builder;
    auto                  result = builder.build(tmpl);

    EXPECT_EQ(result.world.map_width, 72);
    EXPECT_EQ(result.world.map_height, 72);
    EXPECT_EQ(result.world.terrain_tiles, 5184);

    // No map-size errors when terrain is valid
    for (const auto& d : result.diagnostics) {
        EXPECT_NE(d.kind, BuildDiagnosticKind::MissingMapDimensions);
        EXPECT_NE(d.kind, BuildDiagnosticKind::InvalidMapDimensions);
    }
}

// ── Fallback to info.map_size when terrain is absent ────────────────────────

TEST(AdventureWorldBuilder, FallbackToInfoMapSize) {
    ScenarioTemplate tmpl;
    tmpl.info.id = SgObjectId("scn_fb");
    tmpl.info.map_size = 36;

    AdventureWorldBuilder builder;
    auto                  result = builder.build(tmpl);

    EXPECT_EQ(result.world.map_width, 36);
    EXPECT_EQ(result.world.map_height, 36);
    EXPECT_EQ(result.world.terrain_tiles, 0);

    // No diagnostics about missing dimensions
    for (const auto& d : result.diagnostics) {
        EXPECT_NE(d.kind, BuildDiagnosticKind::MissingMapDimensions);
        EXPECT_NE(d.kind, BuildDiagnosticKind::InvalidMapDimensions);
    }
}

// ── Terrain tile count is width * height ────────────────────────────────────

TEST(AdventureWorldBuilder, TerrainTileCountIsWidthTimesHeight) {
    ScenarioTemplate tmpl;
    tmpl.info.id = SgObjectId("scn_ttc");

    tmpl.map.terrain.width = 48;
    tmpl.map.terrain.height = 36;
    tmpl.map.terrain.tiles.assign(36, std::vector<uint32_t>(48, 0));

    AdventureWorldBuilder builder;
    auto                  result = builder.build(tmpl);

    EXPECT_EQ(result.world.terrain_tiles, 48 * 36); // 1728
}

// ── Diagnostics for missing dimensions ──────────────────────────────────────

TEST(AdventureWorldBuilder, DiagnosticsForMissingMapDimensions) {
    ScenarioTemplate tmpl;
    tmpl.info.id = SgObjectId("scn_nomap");

    AdventureWorldBuilder builder;
    auto                  result = builder.build(tmpl);

    EXPECT_GE(result.diagnostics.size(), 1u);

    bool found_missing = false;
    for (const auto& d : result.diagnostics) {
        if (d.kind == BuildDiagnosticKind::MissingMapDimensions) {
            found_missing = true;
            break;
        }
    }
    EXPECT_TRUE(found_missing);
}

// ── Structured diagnostic when both terrain and info.map_size are invalid ──

TEST(AdventureWorldBuilder, StructuredDiagWhenBothDimsInvalid) {
    ScenarioTemplate tmpl;
    tmpl.info.id = SgObjectId("scn_invd");
    tmpl.info.map_size = 0;

    AdventureWorldBuilder builder;
    auto                  result = builder.build(tmpl);

    bool found_missing = false;
    for (const auto& d : result.diagnostics) {
        if (d.kind == BuildDiagnosticKind::MissingMapDimensions) {
            found_missing = true;
            EXPECT_FALSE(d.message.empty());
            EXPECT_EQ(d.object_id, tmpl.info.id);
        }
    }
    EXPECT_TRUE(found_missing);
    EXPECT_EQ(result.world.map_width, 0);
    EXPECT_EQ(result.world.map_height, 0);
}

// ── Counts semantic objects ─────────────────────────────────────────────────

TEST(AdventureWorldBuilder, CountsSemanticObjects) {
    ScenarioTemplate tmpl;
    tmpl.info.id = SgObjectId("scn_count");
    tmpl.map.terrain.width = 10;
    tmpl.map.terrain.height = 10;
    tmpl.map.terrain.tiles.assign(10, std::vector<uint32_t>(10, 0));

    SgUnit u1;
    u1.id = SgObjectId("u1");
    u1.type_id = SgObjectId("type_a");
    tmpl.units.push_back(u1);

    SgUnit u2;
    u2.id = SgObjectId("u2");
    u2.type_id = SgObjectId("type_b");
    tmpl.units.push_back(u2);

    SgStack s1;
    s1.id = SgObjectId("s1");
    s1.owner = SgObjectId("owner1");
    tmpl.stacks.push_back(s1);

    SgCityOrVillage c1;
    c1.id = SgObjectId("c1");
    tmpl.cities.push_back(c1);

    SgPlayer p1;
    p1.id = SgObjectId("p1");
    tmpl.players.push_back(p1);

    AdventureWorldBuilder builder;
    auto                  result = builder.build(tmpl);

    EXPECT_EQ(result.world.semantic_object_count, 5u);
    EXPECT_EQ(result.world.runtime_object_count, 7u);
}

// ── Runtime objects are independent of ScenarioTemplate ─────────────────────

TEST(AdventureWorldBuilder, WorldDoesNotRetainPointersIntoTemplate) {
    auto tmpl = std::make_unique<ScenarioTemplate>();
    tmpl->info.id = SgObjectId("scn_ptr");
    tmpl->info.name = "Ptr Test";
    tmpl->map.terrain.width = 10;
    tmpl->map.terrain.height = 10;
    tmpl->map.terrain.tiles.assign(10, std::vector<uint32_t>(10, 0));

    SgUnit u1;
    u1.id = SgObjectId("unit_1");
    u1.type_id = SgObjectId("type_x");
    tmpl->units.push_back(u1);

    SgStack s1;
    s1.id = SgObjectId("stack_a");
    s1.owner = SgObjectId("owner_x");
    tmpl->stacks.push_back(s1);

    AdventureWorldBuilder builder;
    auto                  result = builder.build(*tmpl);

    tmpl.reset();

    EXPECT_EQ(result.world.scenario_id, "scn_ptr");
    EXPECT_EQ(result.world.runtime_object_count, 3u);
    EXPECT_EQ(result.world.objects[0].id, "unit_1");
    EXPECT_EQ(result.world.objects[1].id, "stack_a");
}

// ── Empty/unknown object id diagnostics ─────────────────────────────────────

TEST(AdventureWorldBuilder, DiagnosticsForEmptyObjectId) {
    ScenarioTemplate tmpl;
    tmpl.info.id = SgObjectId("scn_emptoid");
    tmpl.map.terrain.width = 10;
    tmpl.map.terrain.height = 10;
    tmpl.map.terrain.tiles.assign(10, std::vector<uint32_t>(10, 0));

    SgUnit u1;
    u1.type_id = SgObjectId("type_empty");
    tmpl.units.push_back(u1);

    AdventureWorldBuilder builder;
    auto                  result = builder.build(tmpl);

    EXPECT_GE(result.diagnostics.size(), 1u);
    bool found_empty = false;
    for (const auto& d : result.diagnostics) {
        if (d.kind == BuildDiagnosticKind::EmptyObjectId) {
            found_empty = true;
            break;
        }
    }
    EXPECT_TRUE(found_empty);
    EXPECT_EQ(result.world.runtime_object_count, 0u);
}

// ── Structured diagnostics ──────────────────────────────────────────────────

TEST(AdventureWorldBuilder, DiagnosticsAreStructured) {
    ScenarioTemplate tmpl;
    tmpl.info.id = SgObjectId("scn_diag");

    AdventureWorldBuilder builder;
    auto                  result = builder.build(tmpl);

    for (const auto& d : result.diagnostics) {
        EXPECT_FALSE(d.message.empty());
    }
}

// ── warning_count / error_count helpers ────────────────────────────────────

TEST(AdventureWorldBuilder, WarningAndErrorCounters) {
    ScenarioTemplate tmpl;
    tmpl.info.id = SgObjectId("scn_werr");

    SgUnit u1;
    tmpl.units.push_back(u1);

    AdventureWorldBuilder builder;
    auto                  result = builder.build(tmpl);

    // 2 errors: MissingMapDimensions (no terrain, no map_size) + EmptyObjectId (unit)
    EXPECT_EQ(result.error_count(), 2u);
    EXPECT_EQ(result.warning_count(), 0u);
}

// ── Valid terrain does not produce info.map_size diagnostics ────────────────

TEST(AdventureWorldBuilder, ValidTerrainDoesNotEmitInfoMapSizeErrors) {
    ScenarioTemplate tmpl;
    tmpl.info.id = SgObjectId("scn_vt");
    tmpl.info.map_size = 0; // ← would trigger errors if terrain weren't valid

    tmpl.map.terrain.width = 72;
    tmpl.map.terrain.height = 72;
    tmpl.map.terrain.tiles.assign(72, std::vector<uint32_t>(72, 0));

    AdventureWorldBuilder builder;
    auto                  result = builder.build(tmpl);

    EXPECT_EQ(result.world.map_width, 72);
    EXPECT_EQ(result.world.map_height, 72);

    for (const auto& d : result.diagnostics) {
        EXPECT_NE(d.kind, BuildDiagnosticKind::MissingMapDimensions)
            << "terrain is valid, should not emit MissingMapDimensions";
        EXPECT_NE(d.kind, BuildDiagnosticKind::InvalidMapDimensions)
            << "terrain is valid, should not emit InvalidMapDimensions";
    }
}

TEST(AdventureTerrainGrid, EmptyBoundsAndCheckedAccess) {
    AdventureTerrainGrid grid;
    EXPECT_TRUE(grid.empty());
    EXPECT_EQ(grid.size(), 0u);
    EXPECT_FALSE(grid.contains(0, 0));
    EXPECT_EQ(grid.tile_at(0, 0), nullptr);
}

TEST(AdventureTerrainGrid, RowMajorIndexingAndBounds) {
    AdventureTerrainGrid grid;
    grid.width = 2;
    grid.height = 2;
    grid.tiles = {{.raw_value = 0x00000001},
                  {.raw_value = 0x00000002},
                  {.raw_value = 0xAABBCCDD},
                  {.raw_value = 0xFFFFFFFF}};

    EXPECT_FALSE(grid.empty());
    EXPECT_EQ(grid.size(), 4u);
    ASSERT_NE(grid.tile_at(0, 0), nullptr);
    ASSERT_NE(grid.tile_at(1, 0), nullptr);
    ASSERT_NE(grid.tile_at(0, 1), nullptr);
    ASSERT_NE(grid.tile_at(1, 1), nullptr);
    EXPECT_EQ(grid.tile_at(0, 0)->raw_value, 0x00000001u);
    EXPECT_EQ(grid.tile_at(1, 0)->raw_value, 0x00000002u);
    EXPECT_EQ(grid.tile_at(0, 1)->raw_value, 0xAABBCCDDu);
    EXPECT_EQ(grid.tile_at(1, 1)->raw_value, 0xFFFFFFFFu);
    EXPECT_FALSE(grid.contains(2, 0));
    EXPECT_FALSE(grid.contains(0, 2));
}

TEST(AdventureWorldBuilder, CanonicalTerrainNormalization) {
    // Terrain is transposed during build so canonical[x][y] = raw_terrain[y][x].
    // For a 2×2 grid: raw row 0 = {0x01, 0x02}, raw row 1 = {0xDD, 0xFF}.
    // After transpose:
    //   canonical(0,0) = raw(0,0) = 0x01
    //   canonical(1,0) = raw(0,1) = 0xDD  (swapped)
    //   canonical(0,1) = raw(1,0) = 0x02  (swapped)
    //   canonical(1,1) = raw(1,1) = 0xFF
    ScenarioTemplate tmpl;
    tmpl.info.id = SgObjectId("scn_copy");
    tmpl.map.terrain.width = 2;
    tmpl.map.terrain.height = 2;
    tmpl.map.terrain.tiles = {{0x00000001, 0x00000002}, {0xAABBCCDD, 0xFFFFFFFF}};

    AdventureWorldBuilder builder;
    auto                  result = builder.build(tmpl);

    EXPECT_EQ(result.world.map_width, 2);
    EXPECT_EQ(result.world.map_height, 2);
    EXPECT_EQ(result.world.terrain.width, 2);
    EXPECT_EQ(result.world.terrain.height, 2);
    ASSERT_EQ(result.world.terrain.size(), 4u);
    ASSERT_NE(result.world.terrain.tile_at(0, 0), nullptr);
    EXPECT_EQ(result.world.terrain.tile_at(0, 0)->raw_value, 0x00000001u);
    ASSERT_NE(result.world.terrain.tile_at(1, 0), nullptr);
    EXPECT_EQ(result.world.terrain.tile_at(1, 0)->raw_value, 0xAABBCCDDu);
    ASSERT_NE(result.world.terrain.tile_at(0, 1), nullptr);
    EXPECT_EQ(result.world.terrain.tile_at(0, 1)->raw_value, 0x00000002u);
    ASSERT_NE(result.world.terrain.tile_at(1, 1), nullptr);
    EXPECT_EQ(result.world.terrain.tile_at(1, 1)->raw_value, 0xFFFFFFFFu);
}

// ── AdventureMapObject tests ───────────────────────────────────────────────

TEST(AdventureWorldBuilder, BuildsFootprintsFromMidgardPlan) {
    ScenarioTemplate tmpl;
    tmpl.info.id = "scn_fp";
    tmpl.map.terrain.width = 20;
    tmpl.map.terrain.height = 20;
    tmpl.map.terrain.tiles.assign(20, std::vector<uint32_t>(20, 0));

    // Add a 3x3 footprint plan for a Capital
    SgMidgardPlan plan;
    plan.id = "plan1";
    for (int dy = 0; dy < 3; ++dy) {
        for (int dx = 0; dx < 3; ++dx) {
            SgPlanEntry e;
            e.element = "S143CV0000";
            e.pos_x = 5 + dx;
            e.pos_y = 5 + dy;
            plan.entries.push_back(e);
        }
    }
    tmpl.plans.push_back(plan);

    // Add a Capital that uses this plan
    SgCityOrVillage cap;
    cap.id = "S143CV0000";
    cap.kind = "Capital";
    cap.pos_x = 5;
    cap.pos_y = 5;
    tmpl.cities.push_back(cap);

    AdventureWorldBuilder builder;
    auto                  result = builder.build(tmpl);

    ASSERT_GE(result.world.map_objects.size(), 1);
    const auto& obj = result.world.map_objects[0];
    EXPECT_EQ(obj.id, "S143CV0000");
    EXPECT_EQ(obj.kind, AdventureMapObjectKind::Capital);
    ASSERT_EQ(obj.footprint.size(), 9);

    // Check all 9 cells are present
    std::set<MapCellCoord> cells(obj.footprint.begin(), obj.footprint.end());
    for (int dy = 0; dy < 3; ++dy) {
        for (int dx = 0; dx < 3; ++dx) {
            EXPECT_TRUE(cells.contains(MapCellCoord{5 + dx, 5 + dy}))
                << "Missing footprint cell (" << (5 + dx) << "," << (5 + dy) << ")";
        }
    }
}

TEST(AdventureWorldBuilder, TypedCapitalsPreserveCapitalFootprintAndCompatibilityObject) {
    ScenarioTemplate tmpl;
    tmpl.info.id = "scn_cap";
    tmpl.map.terrain.width = 20;
    tmpl.map.terrain.height = 20;
    tmpl.map.terrain.tiles.assign(20, std::vector<uint32_t>(20, 0));

    SgMidgardPlan plan;
    plan.id = "plan_cap";
    for (int dy = 0; dy < 3; ++dy) {
        for (int dx = 0; dx < 3; ++dx) {
            SgPlanEntry e;
            e.element = "S143CV0000";
            e.pos_x = 5 + dx;
            e.pos_y = 5 + dy;
            plan.entries.push_back(e);
        }
    }
    tmpl.plans.push_back(plan);

    SgCityOrVillage cap;
    cap.id = "S143CV0000";
    cap.kind = "Capital";
    cap.owner = "S143PL0000";
    cap.subrace = "S143SR0000";
    cap.stack = "S143ST0000";
    cap.group_id = "S143GP0000";
    cap.pos_x = 5;
    cap.pos_y = 5;
    cap.unit_ids = {"S143UN0000", "S143UN0001", "G000000000",
                    "S143UN0003", "G000000000", "G000000000"};
    cap.positions = {0, 1, -1, 3, -1, -1};
    tmpl.cities.push_back(cap);

    SgCityOrVillage village;
    village.id = "S143CV0001";
    village.kind = "Village";
    village.owner = "S143PL0000";
    village.subrace = "S143SR0000";
    village.pos_x = 8;
    village.pos_y = 8;
    tmpl.cities.push_back(village);

    AdventureWorldBuilder builder;
    const auto            result = builder.build(tmpl);

    ASSERT_EQ(result.world.capitals.size(), 1u);
    const auto& typed = result.world.capitals.front();
    EXPECT_EQ(typed.id, "S143CV0000");
    EXPECT_EQ(typed.owner, "S143PL0000");
    EXPECT_EQ(typed.subrace, "S143SR0000");
    EXPECT_EQ(typed.visiting_stack_id, "S143ST0000");
    EXPECT_EQ(typed.group_id, "S143GP0000");
    EXPECT_EQ(typed.position.x, 5);
    EXPECT_EQ(typed.position.y, 5);
    EXPECT_EQ(typed.footprint.size(), 9u);
    EXPECT_EQ(typed.garrison.members[0], std::optional<std::string>("S143UN0000"));
    EXPECT_EQ(typed.garrison.members[1], std::optional<std::string>("S143UN0001"));
    EXPECT_FALSE(typed.garrison.members[2].has_value());
    EXPECT_EQ(typed.garrison.members[3], std::optional<std::string>("S143UN0003"));
    EXPECT_EQ(typed.garrison.positions[0], 0);
    EXPECT_EQ(typed.garrison.positions[1], 1);
    EXPECT_EQ(typed.garrison.positions[3], 3);
    EXPECT_EQ(typed.garrison.cell_members[0], 0);
    EXPECT_EQ(typed.garrison.cell_members[1], 1);
    EXPECT_EQ(typed.garrison.cell_members[2], -1);
    EXPECT_EQ(typed.garrison.cell_members[3], 3);

    std::set<MapCellCoord> footprint_cells(typed.footprint.begin(), typed.footprint.end());
    for (int dy = 0; dy < 3; ++dy) {
        for (int dx = 0; dx < 3; ++dx) {
            EXPECT_TRUE(footprint_cells.contains(MapCellCoord{5 + dx, 5 + dy}))
                << "Missing capital footprint cell (" << (5 + dx) << "," << (5 + dy) << ")";
        }
    }

    ASSERT_FALSE(result.world.map_objects.empty());
    const auto capital_object =
        std::find_if(result.world.map_objects.begin(), result.world.map_objects.end(),
                     [](const AdventureMapObject& mo) {
                         return mo.id == "S143CV0000" && mo.kind == AdventureMapObjectKind::Capital;
                     });
    ASSERT_NE(capital_object, result.world.map_objects.end());

    EXPECT_TRUE(std::none_of(result.world.capitals.begin(), result.world.capitals.end(),
                             [](const AdventureCapital& c) { return c.id == "S143CV0001"; }));
}

TEST(AdventureWorldBuilder, MidVillageCreatesTypedCityAndKeepsGenericCityObject) {
    ScenarioTemplate tmpl;
    tmpl.info.id = "scn_city";
    tmpl.map.terrain.width = 20;
    tmpl.map.terrain.height = 20;
    tmpl.map.terrain.tiles.assign(20, std::vector<uint32_t>(20, 0));

    SgMidgardPlan plan;
    plan.id = "city_plan";
    for (int dy = 0; dy < 2; ++dy) {
        for (int dx = 0; dx < 2; ++dx) {
            SgPlanEntry entry;
            entry.element = "S143FT0001";
            entry.pos_x = 7 + dx;
            entry.pos_y = 8 + dy;
            plan.entries.push_back(entry);
        }
    }
    tmpl.plans.push_back(plan);

    SgCityOrVillage city;
    city.id = "S143FT0001";
    city.kind = "MidVillage";
    city.name = "CITY_NAME";
    city.description = "CITY_DESC";
    city.owner = "OWNER_1";
    city.subrace = "SUBRACE_1";
    city.stack = "STACK_1";
    city.group_id = "GROUP_1";
    city.pos_x = 7;
    city.pos_y = 8;
    city.ai_priority = 42;
    city.size = 2;
    tmpl.cities.push_back(city);

    AdventureWorldBuilder builder;
    const auto            result = builder.build(tmpl);

    ASSERT_EQ(result.world.cities.size(), 1u);
    const auto* typed = result.world.find_city("S143FT0001");
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->id, "S143FT0001");
    EXPECT_EQ(typed->name_txt_id, "CITY_NAME");
    EXPECT_EQ(typed->description_txt_id, "CITY_DESC");
    EXPECT_EQ(typed->owner_id, "OWNER_1");
    EXPECT_EQ(typed->subrace_id, "SUBRACE_1");
    EXPECT_EQ(typed->stack_id, "STACK_1");
    EXPECT_EQ(typed->group_id, "GROUP_1");
    EXPECT_EQ(typed->position.x, 7);
    EXPECT_EQ(typed->position.y, 8);
    EXPECT_EQ(typed->ai_priority, 42);
    EXPECT_EQ(typed->size, 2);
    ASSERT_EQ(typed->footprint.size(), 4u);
    EXPECT_TRUE(std::find(typed->footprint.begin(), typed->footprint.end(), MapCellCoord{7, 8}) !=
                typed->footprint.end());
    EXPECT_TRUE(std::find(typed->footprint.begin(), typed->footprint.end(), MapCellCoord{8, 9}) !=
                typed->footprint.end());

    ASSERT_FALSE(result.world.map_objects.empty());
    auto generic = std::find_if(result.world.map_objects.begin(), result.world.map_objects.end(),
                                [](const auto& mo) { return mo.id == "S143FT0001"; });
    ASSERT_NE(generic, result.world.map_objects.end());
    EXPECT_EQ(generic->kind, AdventureMapObjectKind::City);
    ASSERT_EQ(generic->footprint.size(), 4u);
}

TEST(AdventureWorldBuilder, CapitalStaysGenericAndIsNotTypedCity) {
    ScenarioTemplate tmpl;
    tmpl.info.id = "scn_cap";
    tmpl.map.terrain.width = 10;
    tmpl.map.terrain.height = 10;
    tmpl.map.terrain.tiles.assign(10, std::vector<uint32_t>(10, 0));

    SgMidgardPlan plan;
    plan.id = "cap_plan";
    SgPlanEntry entry;
    entry.element = "S143FT0000";
    entry.pos_x = 1;
    entry.pos_y = 2;
    plan.entries.push_back(entry);
    tmpl.plans.push_back(plan);

    SgCityOrVillage capital;
    capital.id = "S143FT0000";
    capital.kind = "Capital";
    capital.pos_x = 1;
    capital.pos_y = 2;
    capital.size = 5;
    tmpl.cities.push_back(capital);

    AdventureWorldBuilder builder;
    const auto            result = builder.build(tmpl);

    ASSERT_EQ(result.world.cities.size(), 0u);
    EXPECT_EQ(result.world.find_city("S143FT0000"), nullptr);

    auto generic = std::find_if(result.world.map_objects.begin(), result.world.map_objects.end(),
                                [](const auto& mo) { return mo.id == "S143FT0000"; });
    ASSERT_NE(generic, result.world.map_objects.end());
    EXPECT_EQ(generic->kind, AdventureMapObjectKind::Capital);
}

TEST(AdventureWorldBuilder, StackPositionAssignedToEmptyMemberPreservesExactCellMembers) {
    ScenarioTemplate tmpl;
    tmpl.info.id = "scn_stack_empty_member";
    tmpl.map.terrain.width = 10;
    tmpl.map.terrain.height = 10;
    tmpl.map.terrain.tiles.assign(10, std::vector<uint32_t>(10, 0));

    SgUnit unit;
    unit.id = "A";
    unit.type_id = "G000UU0001";
    tmpl.units.push_back(unit);

    SgStack stack;
    stack.id = "S";
    stack.units = {"A", "G000000000", "G000000000", "G000000000", "G000000000", "G000000000"};
    stack.positions = {1, -1, -1, -1, -1, -1};
    stack.leader_id = "A";
    tmpl.stacks.push_back(stack);

    AdventureWorldBuilder builder;
    const auto            result = builder.build(tmpl);

    ASSERT_EQ(result.world.stacks.size(), 1u);
    const auto& runtime_stack = result.world.stacks.front();
    EXPECT_EQ(runtime_stack.group.cell_members[0], 1);
    EXPECT_EQ(runtime_stack.group.positions[1], -1);
    EXPECT_TRUE(
        std::any_of(result.diagnostics.begin(), result.diagnostics.end(), [](const auto& d) {
            return d.kind == BuildDiagnosticKind::PositionAssignedToEmptyMember;
        }));
}

TEST(AdventureWorldBuilder, CapitalPositionAssignedToEmptyMemberPreservesExactCellMembers) {
    ScenarioTemplate tmpl;
    tmpl.info.id = "scn_cap_empty_member";
    tmpl.map.terrain.width = 10;
    tmpl.map.terrain.height = 10;
    tmpl.map.terrain.tiles.assign(10, std::vector<uint32_t>(10, 0));

    SgSubRace subrace;
    subrace.id = "SUB1";
    subrace.player_id = "PLAYER1";
    tmpl.subraces.push_back(subrace);

    SgPlayer player;
    player.id = "PLAYER1";
    player.race_id = "g000rr0000";
    tmpl.players.push_back(player);

    SgUnit unit;
    unit.id = "A";
    unit.type_id = "G000UU3001";
    tmpl.units.push_back(unit);

    SgCityOrVillage capital;
    capital.id = "S143FT0000";
    capital.kind = "Capital";
    capital.subrace = "SUB1";
    capital.stack = "S143KC0000";
    capital.unit_ids = {"A", "G000000000", "G000000000", "G000000000", "G000000000", "G000000000"};
    capital.positions = {1, -1, -1, -1, -1, -1};
    capital.pos_x = 5;
    capital.pos_y = 5;
    tmpl.cities.push_back(capital);

    AdventureWorldBuilder builder;
    const auto            result = builder.build(tmpl);

    ASSERT_EQ(result.world.capitals.size(), 1u);
    const auto& runtime_capital = result.world.capitals.front();
    EXPECT_EQ(runtime_capital.garrison.cell_members[0], 1);
    EXPECT_EQ(runtime_capital.garrison.positions[1], -1);
    EXPECT_TRUE(
        std::any_of(result.diagnostics.begin(), result.diagnostics.end(), [](const auto& d) {
            return d.kind == BuildDiagnosticKind::PositionAssignedToEmptyMember;
        }));
}

TEST(AdventureWorldBuilder, StackInvalidFormationCellLeavesCellMembersUnset) {
    ScenarioTemplate tmpl;
    tmpl.info.id = "scn_stack_bad_pos";
    tmpl.map.terrain.width = 10;
    tmpl.map.terrain.height = 10;
    tmpl.map.terrain.tiles.assign(10, std::vector<uint32_t>(10, 0));

    SgUnit unit;
    unit.id = "A";
    unit.type_id = "G000UU0001";
    tmpl.units.push_back(unit);

    SgStack stack;
    stack.id = "S";
    stack.units = {"A", "G000000000", "G000000000", "G000000000", "G000000000", "G000000000"};
    stack.positions = {-2, -1, -1, -1, -1, -1};
    stack.leader_id = "A";
    tmpl.stacks.push_back(stack);

    AdventureWorldBuilder builder;
    const auto            result = builder.build(tmpl);

    ASSERT_EQ(result.world.stacks.size(), 1u);
    EXPECT_EQ(result.world.stacks.front().group.cell_members[0], -1);
    EXPECT_TRUE(
        std::any_of(result.diagnostics.begin(), result.diagnostics.end(), [](const auto& d) {
            return d.kind == BuildDiagnosticKind::InvalidFormationCell;
        }));
}

TEST(AdventureWorldBuilder, CapitalInvalidFormationCellLeavesCellMembersUnset) {
    ScenarioTemplate tmpl;
    tmpl.info.id = "scn_cap_bad_pos";
    tmpl.map.terrain.width = 10;
    tmpl.map.terrain.height = 10;
    tmpl.map.terrain.tiles.assign(10, std::vector<uint32_t>(10, 0));

    SgSubRace subrace;
    subrace.id = "SUB1";
    subrace.player_id = "PLAYER1";
    tmpl.subraces.push_back(subrace);

    SgPlayer player;
    player.id = "PLAYER1";
    player.race_id = "g000rr0000";
    tmpl.players.push_back(player);

    SgUnit unit;
    unit.id = "A";
    unit.type_id = "G000UU3001";
    tmpl.units.push_back(unit);

    SgCityOrVillage capital;
    capital.id = "S143FT0000";
    capital.kind = "Capital";
    capital.subrace = "SUB1";
    capital.stack = "S143KC0000";
    capital.unit_ids = {"A", "G000000000", "G000000000", "G000000000", "G000000000", "G000000000"};
    capital.positions = {6, -1, -1, -1, -1, -1};
    capital.pos_x = 5;
    capital.pos_y = 5;
    tmpl.cities.push_back(capital);

    AdventureWorldBuilder builder;
    const auto            result = builder.build(tmpl);

    ASSERT_EQ(result.world.capitals.size(), 1u);
    EXPECT_EQ(result.world.capitals.front().garrison.cell_members[0], -1);
    EXPECT_TRUE(
        std::any_of(result.diagnostics.begin(), result.diagnostics.end(), [](const auto& d) {
            return d.kind == BuildDiagnosticKind::InvalidFormationCell;
        }));
}

TEST(AdventureWorldBuilder, InvalidCitySizeProducesBuildDiagnostic) {
    for (int size : {0, -1, 6}) {
        ScenarioTemplate tmpl;
        tmpl.info.id = "scn_bad_city";
        tmpl.map.terrain.width = 10;
        tmpl.map.terrain.height = 10;
        tmpl.map.terrain.tiles.assign(10, std::vector<uint32_t>(10, 0));

        SgCityOrVillage city;
        city.id = "S143FT0001";
        city.kind = "MidVillage";
        city.pos_x = 3;
        city.pos_y = 4;
        city.size = size;
        tmpl.cities.push_back(city);

        AdventureWorldBuilder builder;
        const auto            result = builder.build(tmpl);

        EXPECT_TRUE(std::any_of(result.diagnostics.begin(), result.diagnostics.end(),
                                [](const BuildDiagnostic& d) {
                                    return d.kind == BuildDiagnosticKind::InvalidCitySize;
                                }));
        EXPECT_GT(result.error_count(), 0u);
        EXPECT_EQ(result.world.cities.size(), 0u);
    }
}

TEST(AdventureWorldBuilder, InvalidCitySizeIsBuildError) {
    EXPECT_TRUE(is_build_error(BuildDiagnosticKind::InvalidCitySize));
}

TEST(AdventureWorldBuilder, MidVillageWithoutFootprintKeepsEmptyFootprint) {
    ScenarioTemplate tmpl;
    tmpl.info.id = "scn_city_fp";
    tmpl.map.terrain.width = 10;
    tmpl.map.terrain.height = 10;
    tmpl.map.terrain.tiles.assign(10, std::vector<uint32_t>(10, 0));

    SgCityOrVillage city;
    city.id = "S143FT0002";
    city.kind = "MidVillage";
    city.pos_x = 5;
    city.pos_y = 5;
    city.size = 1;
    tmpl.cities.push_back(city);

    AdventureWorldBuilder builder;
    const auto            result = builder.build(tmpl);

    const auto* typed = result.world.find_city("S143FT0002");
    ASSERT_NE(typed, nullptr);
    EXPECT_TRUE(typed->footprint.empty());

    auto generic = std::find_if(result.world.map_objects.begin(), result.world.map_objects.end(),
                                [](const auto& mo) { return mo.id == "S143FT0002"; });
    ASSERT_NE(generic, result.world.map_objects.end());
    EXPECT_TRUE(generic->footprint.empty());
}
TEST(AdventureWorldBuilder, BuildsRoadWithIndexVariant) {
    ScenarioTemplate tmpl;
    tmpl.info.id = "scn_rd";
    tmpl.map.terrain.width = 10;
    tmpl.map.terrain.height = 10;
    tmpl.map.terrain.tiles.assign(10, std::vector<uint32_t>(10, 0));

    SgRoad rd;
    rd.id = "S143RD0001";
    rd.index = 3;
    rd.variant = 1;
    rd.pos_x = 7;
    rd.pos_y = 8;
    tmpl.roads.push_back(rd);

    AdventureWorldBuilder builder;
    auto                  result = builder.build(tmpl);

    ASSERT_GE(result.world.map_objects.size(), 1);
    const auto* found = &result.world.map_objects[0];
    // Find the road by id
    for (const auto& mo : result.world.map_objects) {
        if (mo.id == "S143RD0001") {
            found = &mo;
            break;
        }
    }
    EXPECT_EQ(found->id, "S143RD0001");
    EXPECT_EQ(found->kind, AdventureMapObjectKind::Road);
    EXPECT_EQ(found->position.x, 7);
    EXPECT_EQ(found->position.y, 8);
    EXPECT_EQ(found->index, 3);
    EXPECT_EQ(found->variant, 1);
    // Without a plan entry, road gets 1x1 footprint at pos
    ASSERT_GE(found->footprint.size(), 1);
    EXPECT_EQ(found->footprint[0].x, 7);
    EXPECT_EQ(found->footprint[0].y, 8);
    EXPECT_FALSE(found->blocking);
}

TEST(AdventureWorldBuilder, BuildsBagWithImage) {
    ScenarioTemplate tmpl;
    tmpl.info.id = "scn_bag";
    tmpl.map.terrain.width = 10;
    tmpl.map.terrain.height = 10;
    tmpl.map.terrain.tiles.assign(10, std::vector<uint32_t>(10, 0));

    SgBag bag;
    bag.id = "S143BG0001";
    bag.image = 5;
    bag.pos_x = 2;
    bag.pos_y = 3;
    bag.looter = "S143PL0001";
    bag.items = {"S143IT0001", "S143IT0002"};
    tmpl.bags.push_back(bag);

    AdventureWorldBuilder builder;
    auto                  result = builder.build(tmpl);

    ASSERT_GE(result.world.map_objects.size(), 1);
    const auto* found = &result.world.map_objects[0];
    for (const auto& mo : result.world.map_objects) {
        if (mo.id == "S143BG0001") {
            found = &mo;
            break;
        }
    }
    EXPECT_EQ(found->kind, AdventureMapObjectKind::Bag);
    EXPECT_EQ(found->image, 5);
    EXPECT_EQ(found->position.x, 2);
    EXPECT_EQ(found->position.y, 3);

    const auto* treasure = result.world.find_treasure("S143BG0001");
    ASSERT_NE(treasure, nullptr);
    EXPECT_EQ(treasure->id, "S143BG0001");
    EXPECT_EQ(treasure->looter_id, "S143PL0001");
    ASSERT_EQ(treasure->item_ids.size(), 2u);
    EXPECT_EQ(treasure->item_ids[0], "S143IT0001");
    EXPECT_EQ(treasure->item_ids[1], "S143IT0002");
    EXPECT_EQ(treasure->position.x, 2);
    EXPECT_EQ(treasure->position.y, 3);
    EXPECT_EQ(treasure->image, 5);
    EXPECT_EQ(treasure->placement, AdventureTreasurePlacement::Land);
    ASSERT_EQ(treasure->footprint.size(), 1u);
    EXPECT_EQ(treasure->footprint[0].x, 2);
    EXPECT_EQ(treasure->footprint[0].y, 3);
}

TEST(AdventureWorldBuilder, BagsReceivePlacementFromTerrainGround) {
    ScenarioTemplate tmpl;
    tmpl.info.id = "scn_bag_placement";
    tmpl.map.terrain.width = 1;
    tmpl.map.terrain.height = 2;
    tmpl.map.terrain.tiles.assign(2, std::vector<uint32_t>(1, 0));
    tmpl.map.terrain.tiles[0][0] = 7;
    tmpl.map.terrain.tiles[1][0] = 0;

    SgBag water_bag;
    water_bag.id = "S143BGW000";
    water_bag.pos_x = 0;
    water_bag.pos_y = 0;
    tmpl.bags.push_back(water_bag);

    SgBag land_bag;
    land_bag.id = "S143BGL000";
    land_bag.pos_x = 1;
    land_bag.pos_y = 0;
    tmpl.bags.push_back(land_bag);

    AdventureWorldBuilder builder;
    const auto            result = builder.build(tmpl);

    ASSERT_EQ(result.world.treasures.size(), 2u);
    const auto* water_treasure = result.world.find_treasure("S143BGW000");
    const auto* land_treasure = result.world.find_treasure("S143BGL000");
    ASSERT_NE(water_treasure, nullptr);
    ASSERT_NE(land_treasure, nullptr);
    EXPECT_EQ(water_treasure->placement, AdventureTreasurePlacement::Water);
    EXPECT_EQ(land_treasure->placement, AdventureTreasurePlacement::Land);
    EXPECT_EQ(result.world.runtime_object_count, 2u);
    EXPECT_TRUE(std::none_of(result.diagnostics.begin(), result.diagnostics.end(),
                             [](const BuildDiagnostic& d) {
                                 return d.kind == BuildDiagnosticKind::MissingTreasureGroundCell;
                             }));
}

TEST(AdventureWorldBuilder, MissingTreasureGroundCellWarnsAndDefaultsToLand) {
    ScenarioTemplate tmpl;
    tmpl.info.id = "scn_bag_oob";
    tmpl.map.terrain.width = 1;
    tmpl.map.terrain.height = 1;
    tmpl.map.terrain.tiles.assign(1, std::vector<uint32_t>(1, 0));

    SgBag bag;
    bag.id = "S143BGOOB";
    bag.pos_x = 3;
    bag.pos_y = 4;
    tmpl.bags.push_back(bag);

    AdventureWorldBuilder builder;
    const auto            result = builder.build(tmpl);

    const auto* treasure = result.world.find_treasure("S143BGOOB");
    ASSERT_NE(treasure, nullptr);
    EXPECT_EQ(treasure->placement, AdventureTreasurePlacement::Land);
    EXPECT_TRUE(std::any_of(result.diagnostics.begin(), result.diagnostics.end(),
                            [](const BuildDiagnostic& d) {
                                return d.kind == BuildDiagnosticKind::MissingTreasureGroundCell;
                            }));
    EXPECT_EQ(result.world.runtime_object_count, 1u);
}

TEST(AdventureWorldBuilder, BagTypedTreasureFallbackDoesNotDoubleCountRuntimeObjects) {
    ScenarioTemplate tmpl;
    tmpl.info.id = "scn_bag_fallback";
    tmpl.map.terrain.width = 10;
    tmpl.map.terrain.height = 10;
    tmpl.map.terrain.tiles.assign(10, std::vector<uint32_t>(10, 0));

    SgBag bag;
    bag.id = "S143BG0002";
    bag.pos_x = 4;
    bag.pos_y = 5;
    tmpl.bags.push_back(bag);

    AdventureWorldBuilder builder;
    auto                  result = builder.build(tmpl);

    EXPECT_EQ(result.world.runtime_object_count, 1u);
    ASSERT_EQ(result.world.map_objects.size(), 1u);
    ASSERT_EQ(result.world.treasures.size(), 1u);

    const auto& treasure = result.world.treasures.front();
    ASSERT_EQ(treasure.footprint.size(), 1u);
    EXPECT_EQ(treasure.footprint[0].x, 4);
    EXPECT_EQ(treasure.footprint[0].y, 5);
}

TEST(AdventureWorldBuilder, BuildsCrystalWithResource) {
    ScenarioTemplate tmpl;
    tmpl.info.id = "scn_cr";
    tmpl.map.terrain.width = 10;
    tmpl.map.terrain.height = 10;
    tmpl.map.terrain.tiles.assign(10, std::vector<uint32_t>(10, 0));

    SgCrystal cr;
    cr.id = "S143CR0001";
    cr.resource = 42;
    cr.pos_x = 5;
    cr.pos_y = 5;
    tmpl.crystals.push_back(cr);

    AdventureWorldBuilder builder;
    auto                  result = builder.build(tmpl);

    ASSERT_GE(result.world.map_objects.size(), 1);
    const auto* found = &result.world.map_objects[0];
    for (const auto& mo : result.world.map_objects) {
        if (mo.id == "S143CR0001") {
            found = &mo;
            break;
        }
    }
    EXPECT_EQ(found->kind, AdventureMapObjectKind::ResourceNode);
    EXPECT_EQ(found->resource, 42);
    EXPECT_EQ(found->position.x, 5);
    EXPECT_EQ(found->position.y, 5);
}

TEST(AdventureWorldBuilder, BuildsMountainsFromMidMountains) {
    ScenarioTemplate tmpl;
    tmpl.info.id = "scn_mt";
    tmpl.map.terrain.width = 50;
    tmpl.map.terrain.height = 50;
    tmpl.map.terrain.tiles.assign(50, std::vector<uint32_t>(50, 0));

    SgMidMountains mt;
    mt.id = "S143MM0001";
    SgMountainEntry e;
    e.id_mount = 42; // non-zero, different from image value
    e.pos_x = 10;
    e.pos_y = 20;
    e.size_x = 4;
    e.size_y = 3;
    e.image = 7;
    e.race = 2;
    mt.entries.push_back(e);
    tmpl.mountains.push_back(mt);

    AdventureWorldBuilder builder;
    auto                  result = builder.build(tmpl);

    ASSERT_GE(result.world.map_objects.size(), 1);
    const auto* found = &result.world.map_objects[0];
    for (const auto& mo : result.world.map_objects) {
        if (mo.image == 7) {
            found = &mo;
            break;
        }
    }
    EXPECT_EQ(found->kind, AdventureMapObjectKind::Mountain);
    EXPECT_EQ(found->id, "S143MM0001/42")
        << "generic Mountain ID must match typed AdventureMountain ID";
    EXPECT_EQ(found->position.x, 10);
    EXPECT_EQ(found->position.y, 20);
    EXPECT_EQ(found->image, 7);
    EXPECT_EQ(found->race, 2);
    // id_mount is preserved independently from image
    EXPECT_EQ(found->id_mount, 42) << "id_mount must survive parser -> runtime";
    // 4x3 footprint (generic)
    ASSERT_EQ(found->footprint.size(), 12);
    std::set<MapCellCoord> cells(found->footprint.begin(), found->footprint.end());
    for (int dy = 0; dy < 3; ++dy) {
        for (int dx = 0; dx < 4; ++dx) {
            EXPECT_TRUE(cells.contains(MapCellCoord{10 + dx, 20 + dy}))
                << "Missing mountain cell (" << (10 + dx) << "," << (20 + dy) << ")";
        }
    }

    // Typed AdventureMountain (regression: footprint must be copied before mo move)
    ASSERT_EQ(result.world.mountains.size(), 1);
    EXPECT_EQ(result.world.mountains[0].id, "S143MM0001/42");
    EXPECT_EQ(result.world.mountains[0].id, found->id)
        << "typed Mountain.id must equal generic AdventureMapObject.id";
    EXPECT_EQ(result.world.mountains[0].id_mount, 42);
    EXPECT_EQ(result.world.mountains[0].image, 7);
    EXPECT_EQ(result.world.mountains[0].race, 2);
    EXPECT_EQ(result.world.mountains[0].position.x, 10);
    EXPECT_EQ(result.world.mountains[0].position.y, 20);
    EXPECT_EQ(result.world.mountains[0].size_x, 4);
    EXPECT_EQ(result.world.mountains[0].size_y, 3);
    ASSERT_EQ(result.world.mountains[0].footprint.size(), 12);
    for (int dy = 0; dy < 3; ++dy) {
        for (int dx = 0; dx < 4; ++dx) {
            EXPECT_TRUE(std::find(result.world.mountains[0].footprint.begin(),
                                  result.world.mountains[0].footprint.end(),
                                  MapCellCoord{10 + dx, 20 + dy}) !=
                        result.world.mountains[0].footprint.end())
                << "Missing mountain cell (" << (10 + dx) << "," << (20 + dy) << ")";
        }
    }
}

TEST(AdventureWorldBuilder, MountainEntriesInSameMidMountainsHaveUniqueGenericIds) {
    ScenarioTemplate tmpl;
    tmpl.info.id = "scn_multi";
    tmpl.map.terrain.width = 20;
    tmpl.map.terrain.height = 20;
    tmpl.map.terrain.tiles.assign(20, std::vector<uint32_t>(20, 0));

    SgMidMountains mt;
    mt.id = "S143MM0100";

    SgMountainEntry a;
    a.id_mount = 10;
    a.pos_x = 2;
    a.pos_y = 3;
    a.size_x = 1;
    a.size_y = 1;
    a.image = 5;
    a.race = 4;
    mt.entries.push_back(a);

    SgMountainEntry b;
    b.id_mount = 20;
    b.pos_x = 6;
    b.pos_y = 7;
    b.size_x = 2;
    b.size_y = 2;
    b.image = 5; // same IMAGE as entry A
    b.race = 4;
    mt.entries.push_back(b);

    tmpl.mountains.push_back(mt);

    AdventureWorldBuilder builder;
    auto                  result = builder.build(tmpl);

    ASSERT_EQ(result.world.mountains.size(), 2);
    ASSERT_EQ(result.world.map_objects.size(), 2);

    EXPECT_EQ(result.world.mountains[0].id, "S143MM0100/10");
    EXPECT_EQ(result.world.mountains[1].id, "S143MM0100/20");

    // Generic Mountain IDs must match typed Mountain IDs
    EXPECT_EQ(result.world.map_objects[0].id, "S143MM0100/10");
    EXPECT_EQ(result.world.map_objects[1].id, "S143MM0100/20");

    // Cross-verify: typed and generic IDs match for each entry
    EXPECT_EQ(result.world.mountains[0].id, result.world.map_objects[0].id);
    EXPECT_EQ(result.world.mountains[1].id, result.world.map_objects[1].id);

    // Generic IDs must NOT collide
    EXPECT_NE(result.world.map_objects[0].id, result.world.map_objects[1].id);

    // Both generic objects are Mountains
    EXPECT_EQ(result.world.map_objects[0].kind, AdventureMapObjectKind::Mountain);
    EXPECT_EQ(result.world.map_objects[1].kind, AdventureMapObjectKind::Mountain);
}

TEST(AdventureWorldBuilder, MountainIdentityIndependentOfImage) {
    ScenarioTemplate tmpl;
    tmpl.info.id = "scn_idimg";
    tmpl.map.terrain.width = 10;
    tmpl.map.terrain.height = 10;
    tmpl.map.terrain.tiles.assign(10, std::vector<uint32_t>(10, 0));

    SgMidMountains mt;
    mt.id = "S143MM0020";

    SgMountainEntry a;
    a.id_mount = 30;
    a.pos_x = 0;
    a.pos_y = 0;
    a.size_x = 1;
    a.size_y = 1;
    a.image = 7;
    a.race = 4;
    mt.entries.push_back(a);

    SgMountainEntry b;
    b.id_mount = 40;
    b.pos_x = 5;
    b.pos_y = 5;
    b.size_x = 1;
    b.size_y = 1;
    b.image = 7; // same IMAGE, different ID_MOUNT
    b.race = 4;
    mt.entries.push_back(b);

    tmpl.mountains.push_back(mt);

    AdventureWorldBuilder builder;
    auto                  result = builder.build(tmpl);

    ASSERT_EQ(result.world.mountains.size(), 2);

    // Both have same IMAGE but different IDs
    EXPECT_EQ(result.world.mountains[0].image, 7);
    EXPECT_EQ(result.world.mountains[1].image, 7);
    EXPECT_EQ(result.world.mountains[0].id, "S143MM0020/30");
    EXPECT_EQ(result.world.mountains[1].id, "S143MM0020/40");
    EXPECT_NE(result.world.mountains[0].id, result.world.mountains[1].id);

    ASSERT_EQ(result.world.map_objects.size(), 2);
    EXPECT_EQ(result.world.map_objects[0].id, "S143MM0020/30");
    EXPECT_EQ(result.world.map_objects[1].id, "S143MM0020/40");
    EXPECT_NE(result.world.map_objects[0].id, result.world.map_objects[1].id);
}

TEST(AdventureWorldBuilder, BuildsLandmarksWithPreservedFields) {
    ScenarioTemplate tmpl;
    tmpl.info.id = "scn_lm";
    tmpl.map.terrain.width = 20;
    tmpl.map.terrain.height = 20;
    tmpl.map.terrain.tiles.assign(20, std::vector<uint32_t>(20, 0));

    SgLandmark lm;
    lm.id = "S095MM003e";
    lm.pos_x = 5;
    lm.pos_y = 10;
    lm.type = "G000MG0099";
    lm.map_gfx_id = "G000MG0099";
    lm.image = "IMG:42";
    lm.name = "Test Landmark";
    tmpl.landmarks.push_back(lm);

    AdventureWorldBuilder builder;
    auto                  result = builder.build(tmpl);

    ASSERT_EQ(result.world.landmarks.size(), 1);
    EXPECT_EQ(result.world.landmarks[0].id, "S095MM003e");
    EXPECT_EQ(result.world.landmarks[0].type_id, "G000MG0099");
    EXPECT_EQ(result.world.landmarks[0].map_gfx_id, "G000MG0099");
    EXPECT_EQ(result.world.landmarks[0].image, "IMG:42");
    EXPECT_EQ(result.world.landmarks[0].position.x, 5);
    EXPECT_EQ(result.world.landmarks[0].position.y, 10);

    // Generic and typed IDs must match
    ASSERT_GE(result.world.map_objects.size(), 1);
    const auto* found = &result.world.map_objects[0];
    for (const auto& mo : result.world.map_objects) {
        if (mo.kind == AdventureMapObjectKind::Landmark) {
            found = &mo;
            break;
        }
    }
    EXPECT_EQ(found->kind, AdventureMapObjectKind::Landmark);
    EXPECT_EQ(found->id, "S095MM003e");
    EXPECT_EQ(result.world.landmarks[0].id, found->id);
}

TEST(AdventureWorldBuilder, TwoLandmarksSameTypeHaveDistinctIds) {
    ScenarioTemplate tmpl;
    tmpl.info.id = "scn_lm2";
    tmpl.map.terrain.width = 10;
    tmpl.map.terrain.height = 10;
    tmpl.map.terrain.tiles.assign(10, std::vector<uint32_t>(10, 0));

    SgLandmark a;
    a.id = "S095MM003a";
    a.pos_x = 1;
    a.pos_y = 1;
    a.type = "G000MG0099";
    tmpl.landmarks.push_back(a);

    SgLandmark b;
    b.id = "S095MM003b";
    b.pos_x = 5;
    b.pos_y = 5;
    b.type = "G000MG0099"; // same TYPE, different instance
    tmpl.landmarks.push_back(b);

    AdventureWorldBuilder builder;
    auto                  result = builder.build(tmpl);

    ASSERT_EQ(result.world.landmarks.size(), 2);
    EXPECT_EQ(result.world.landmarks[0].id, "S095MM003a");
    EXPECT_EQ(result.world.landmarks[1].id, "S095MM003b");
    EXPECT_EQ(result.world.landmarks[0].type_id, "G000MG0099");
    EXPECT_EQ(result.world.landmarks[1].type_id, "G000MG0099");
    EXPECT_NE(result.world.landmarks[0].id, result.world.landmarks[1].id);
}

TEST(AdventureWorldBuilder, RuntimeObjectCountIncludesMapObjects) {
    ScenarioTemplate tmpl;
    tmpl.info.id = "scn_rcnt";
    tmpl.map.terrain.width = 10;
    tmpl.map.terrain.height = 10;
    tmpl.map.terrain.tiles.assign(10, std::vector<uint32_t>(10, 0));

    // One semantic object (unit)
    SgUnit u;
    u.id = "S143UN0001";
    u.type_id = "G000UU0001";
    tmpl.units.push_back(u);

    // One map object (road)
    SgRoad rd;
    rd.id = "S143RD0001";
    rd.pos_x = 1;
    rd.pos_y = 1;
    tmpl.roads.push_back(rd);

    AdventureWorldBuilder builder;
    auto                  result = builder.build(tmpl);

    // semantic_object_count includes the unit AND the road
    EXPECT_GE(result.world.semantic_object_count, 2);

    // runtime_object_count includes objects + map_objects
    EXPECT_EQ(result.world.runtime_object_count,
              result.world.objects.size() + result.world.map_objects.size());
    EXPECT_GE(result.world.runtime_object_count, 2);
}

TEST(AdventureWorldBuilder, DuplicateFootprintCellCreatesDiag) {
    ScenarioTemplate tmpl;
    tmpl.info.id = "scn_dup";
    tmpl.map.terrain.width = 10;
    tmpl.map.terrain.height = 10;
    tmpl.map.terrain.tiles.assign(10, std::vector<uint32_t>(10, 0));

    // Plan with duplicate cell for same object
    SgMidgardPlan plan;
    plan.id = "plan_dup";
    for (int i = 0; i < 3; ++i) {
        SgPlanEntry e;
        e.element = "S143OBJ0001";
        e.pos_x = 5;
        e.pos_y = 5;
        plan.entries.push_back(e);
    }
    tmpl.plans.push_back(plan);

    AdventureWorldBuilder builder;
    auto                  result = builder.build(tmpl);

    bool found_dup = false;
    for (const auto& d : result.diagnostics) {
        if (d.kind == BuildDiagnosticKind::DuplicateFootprint) {
            found_dup = true;
            EXPECT_TRUE(d.message.find("S143OBJ0001") != std::string::npos);
            break;
        }
    }
    EXPECT_TRUE(found_dup) << "Expected DuplicateFootprint diagnostic for duplicate cell";
}

TEST(AdventureWorldBuilder, SiteMageProducesTypedAndGenericObjects) {
    ScenarioTemplate tmpl;
    tmpl.info.id = "scn_sitemage";
    tmpl.map.terrain.width = 10;
    tmpl.map.terrain.height = 10;
    tmpl.map.terrain.tiles.assign(10, std::vector<uint32_t>(10, 0));

    SgSite site;
    site.id = "S143MG0001";
    site.kind = "MidSiteMage";
    site.pos_x = 3;
    site.pos_y = 4;
    site.image_iso = 1;
    site.image_interface = 9;
    site.title = "Mage Tower";
    site.description = "A tower of magic";
    site.qty_spell = 2;
    site.spells = {"G000SP0001", "G000SP0002"};
    tmpl.sites.push_back(site);

    SgMidgardPlan plan;
    plan.id = "plan_site";
    plan.entries.push_back({"S143MG0001", 3, 4});
    tmpl.plans.push_back(plan);

    AdventureWorldBuilder builder;
    auto                  result = builder.build(tmpl);

    ASSERT_EQ(result.world.sites.size(), 1u);
    const auto& typed = result.world.sites.front();
    EXPECT_EQ(typed.id, "S143MG0001");
    EXPECT_EQ(typed.kind, AdventureSiteKind::Mage);
    EXPECT_EQ(typed.image_iso, 1);
    EXPECT_EQ(typed.image_interface, 9);
    EXPECT_EQ(typed.footprint.size(), 1u);
    ASSERT_TRUE(std::holds_alternative<AdventureMageSiteData>(typed.payload));
    EXPECT_EQ(std::get<AdventureMageSiteData>(typed.payload).declared_spell_count, 2);
    EXPECT_EQ(std::get<AdventureMageSiteData>(typed.payload).spell_ids.size(), 2u);

    bool found = false;
    for (const auto& mo : result.world.map_objects) {
        if (mo.id == "S143MG0001") {
            found = true;
            EXPECT_EQ(mo.kind, AdventureMapObjectKind::SiteMage);
            EXPECT_EQ(mo.position.x, 3);
            EXPECT_EQ(mo.position.y, 4);
            EXPECT_EQ(mo.image, 1);
            break;
        }
    }
    EXPECT_TRUE(found) << "MidSiteMage should produce an AdventureMapObject";
    EXPECT_EQ(result.world.runtime_object_count, result.world.map_objects.size());
}

TEST(AdventureWorldBuilder, SiteMerchantProducesTypedAndGenericObjects) {
    ScenarioTemplate tmpl;
    tmpl.info.id = "scn_sitemerchant";
    tmpl.map.terrain.width = 10;
    tmpl.map.terrain.height = 10;
    tmpl.map.terrain.tiles.assign(10, std::vector<uint32_t>(10, 0));

    SgSite site;
    site.id = "S143MR0001";
    site.kind = "MidSiteMerchant";
    site.pos_x = 2;
    site.pos_y = 6;
    site.image_iso = 0;
    site.image_interface = 3;
    site.title = "Merchant";
    site.description = "Sells";
    site.buy_armor = "G000IT0001";
    site.qty_item = 1;
    site.items = {"G000IT0001"};
    site.missions = {"G000MS0001"};
    tmpl.sites.push_back(site);

    SgMidgardPlan plan;
    plan.id = "plan_site2";
    plan.entries.push_back({"S143MR0001", 2, 6});
    tmpl.plans.push_back(plan);

    AdventureWorldBuilder builder;
    auto                  result = builder.build(tmpl);

    ASSERT_EQ(result.world.sites.size(), 1u);
    const auto& typed = result.world.sites.front();
    EXPECT_EQ(typed.kind, AdventureSiteKind::Merchant);
    ASSERT_TRUE(std::holds_alternative<AdventureMerchantSiteData>(typed.payload));
    const auto& payload = std::get<AdventureMerchantSiteData>(typed.payload);
    EXPECT_EQ(payload.buy_armor, "G000IT0001");
    EXPECT_EQ(payload.declared_item_count, 1);
    EXPECT_EQ(payload.item_ids.size(), 1u);
    EXPECT_EQ(payload.mission_ids.size(), 1u);
    EXPECT_EQ(result.world.map_objects.front().image, 0);
}

TEST(AdventureWorldBuilder, SiteMercsProducesTypedAndGenericObjects) {
    ScenarioTemplate tmpl;
    tmpl.info.id = "scn_sitemercs";
    tmpl.map.terrain.width = 10;
    tmpl.map.terrain.height = 10;
    tmpl.map.terrain.tiles.assign(10, std::vector<uint32_t>(10, 0));

    SgSite site;
    site.id = "S143MC0001";
    site.kind = "MidSiteMercs";
    site.pos_x = 4;
    site.pos_y = 1;
    site.image_iso = 4;
    site.image_interface = 7;
    site.title = "Mercs";
    site.description = "Hires";
    site.qty_unit = 2;
    site.units = {"G000UN0001", "G000UN0002"};
    tmpl.sites.push_back(site);

    SgMidgardPlan plan;
    plan.id = "plan_site3";
    plan.entries.push_back({"S143MC0001", 4, 1});
    tmpl.plans.push_back(plan);

    AdventureWorldBuilder builder;
    auto                  result = builder.build(tmpl);

    ASSERT_EQ(result.world.sites.size(), 1u);
    const auto& typed = result.world.sites.front();
    EXPECT_EQ(typed.kind, AdventureSiteKind::Mercenary);
    ASSERT_TRUE(std::holds_alternative<AdventureMercenarySiteData>(typed.payload));
    const auto& payload = std::get<AdventureMercenarySiteData>(typed.payload);
    EXPECT_EQ(payload.declared_unit_count, 2);
    EXPECT_EQ(payload.unit_ids.size(), 2u);
    EXPECT_EQ(result.world.map_objects.front().image, 4);
}

TEST(AdventureWorldBuilder, SiteTrainerProducesTypedAndGenericObjects) {
    ScenarioTemplate tmpl;
    tmpl.info.id = "scn_sitetrainer";
    tmpl.map.terrain.width = 10;
    tmpl.map.terrain.height = 10;
    tmpl.map.terrain.tiles.assign(10, std::vector<uint32_t>(10, 0));

    SgSite site;
    site.id = "S143TR0001";
    site.kind = "MidSiteTrainer";
    site.pos_x = 5;
    site.pos_y = 5;
    site.image_iso = 0;
    site.image_interface = 12;
    site.title = "Trainer";
    site.description = "Training site";
    site.visitor = "G000ST0001";
    site.ai_priority = 7;
    tmpl.sites.push_back(site);

    SgMidgardPlan plan;
    plan.id = "plan_sitetrainer";
    plan.entries.push_back({site.id, 5, 5});
    plan.entries.push_back({site.id, 6, 5});
    plan.entries.push_back({site.id, 6, 6});
    tmpl.plans.push_back(plan);

    AdventureWorldBuilder builder;
    auto                  result = builder.build(tmpl);

    ASSERT_EQ(result.world.sites.size(), 1u);
    const auto& typed = result.world.sites.front();
    EXPECT_EQ(typed.id, site.id);
    EXPECT_EQ(typed.kind, AdventureSiteKind::Trainer);
    EXPECT_EQ(typed.title, site.title);
    EXPECT_EQ(typed.description, site.description);
    EXPECT_EQ(typed.image_iso, 0);
    EXPECT_EQ(typed.image_interface, 12);
    EXPECT_EQ(typed.position, (MapCellCoord{5, 5}));
    EXPECT_EQ(typed.visitor_id, site.visitor);
    EXPECT_EQ(typed.ai_priority, 7);
    EXPECT_EQ(typed.footprint, (std::vector<MapCellCoord>{{5, 5}, {6, 5}, {6, 6}}));
    EXPECT_TRUE(std::holds_alternative<AdventureTrainerSiteData>(typed.payload));
    ASSERT_EQ(result.world.map_objects.size(), 1u);
    EXPECT_EQ(result.world.map_objects.front().kind, AdventureMapObjectKind::SiteTrainer);
    EXPECT_EQ(result.world.map_objects.front().image, 0);
    EXPECT_EQ(result.world.map_objects.front().footprint, typed.footprint);
    EXPECT_EQ(result.world.runtime_object_count,
              result.world.objects.size() + result.world.map_objects.size());
}

TEST(AdventureWorldBuilder, SiteTrainerAcceptsImageRangeZeroThroughThree) {
    for (int image = 0; image <= 3; ++image) {
        ScenarioTemplate tmpl;
        tmpl.info.id = "scn_sitetrainer_valid";
        tmpl.map.terrain.width = 10;
        tmpl.map.terrain.height = 10;
        tmpl.map.terrain.tiles.assign(10, std::vector<uint32_t>(10, 0));
        SgSite site;
        site.id = "S143TR000" + std::to_string(image);
        site.kind = "MidSiteTrainer";
        site.image_iso = image;
        tmpl.sites.push_back(site);

        const auto result = AdventureWorldBuilder().build(tmpl);
        ASSERT_EQ(result.world.sites.size(), 1u) << "image=" << image;
        EXPECT_EQ(result.world.sites.front().kind, AdventureSiteKind::Trainer);
        EXPECT_FALSE(std::any_of(
            result.diagnostics.begin(), result.diagnostics.end(), [](const auto& diagnostic) {
                return diagnostic.kind == BuildDiagnosticKind::InvalidSiteImageIndex;
            }));
    }
}

TEST(AdventureWorldBuilder, SiteTrainerEmptyFootprintIsNotDefaulted) {
    ScenarioTemplate tmpl;
    tmpl.info.id = "scn_sitetrainer_empty";
    tmpl.map.terrain.width = 10;
    tmpl.map.terrain.height = 10;
    tmpl.map.terrain.tiles.assign(10, std::vector<uint32_t>(10, 0));
    tmpl.sites.push_back({.id = "S143TR_EMPTY", .kind = "MidSiteTrainer", .image_iso = 0});

    const auto result = AdventureWorldBuilder().build(tmpl);
    ASSERT_EQ(result.world.sites.size(), 1u);
    EXPECT_TRUE(result.world.sites.front().footprint.empty());
    ASSERT_EQ(result.world.map_objects.size(), 1u);
    EXPECT_TRUE(result.world.map_objects.front().footprint.empty());
    EXPECT_TRUE(std::any_of(result.diagnostics.begin(), result.diagnostics.end(),
                            [](const auto& diagnostic) {
                                return diagnostic.kind == BuildDiagnosticKind::MissingFootprint;
                            }));
}

TEST(AdventureWorldBuilder, SiteFootprintIsPreservedAndNotDefaulted) {
    ScenarioTemplate tmpl;
    tmpl.info.id = "scn_sitefp";
    tmpl.map.terrain.width = 10;
    tmpl.map.terrain.height = 10;
    tmpl.map.terrain.tiles.assign(10, std::vector<uint32_t>(10, 0));

    SgSite site;
    site.id = "S143MG0002";
    site.kind = "MidSiteMage";
    site.image_iso = 2;
    tmpl.sites.push_back(site);

    AdventureWorldBuilder builder;
    auto                  result = builder.build(tmpl);

    ASSERT_EQ(result.world.map_objects.size(), 1u);
    EXPECT_TRUE(result.world.map_objects.front().footprint.empty());
    ASSERT_EQ(result.world.sites.size(), 1u);
    EXPECT_TRUE(result.world.sites.front().footprint.empty());
}

TEST(AdventureWorldBuilder, InvalidSiteImageIndexProducesBuildError) {
    EXPECT_TRUE(is_build_error(BuildDiagnosticKind::InvalidSiteImageIndex));

    auto build_site = [](const std::string& kind, int image_iso) {
        ScenarioTemplate tmpl;
        tmpl.info.id = "scn_badsite";
        tmpl.map.terrain.width = 10;
        tmpl.map.terrain.height = 10;
        tmpl.map.terrain.tiles.assign(10, std::vector<uint32_t>(10, 0));

        SgSite site;
        site.id = "S143BAD000";
        site.kind = kind;
        site.image_iso = image_iso;
        tmpl.sites.push_back(site);

        AdventureWorldBuilder builder;
        return builder.build(tmpl);
    };

    const auto mage_low = build_site("MidSiteMage", -1);
    const auto mage_high = build_site("MidSiteMage", 4);
    const auto merchant_low = build_site("MidSiteMerchant", -1);
    const auto merchant_high = build_site("MidSiteMerchant", 8);
    const auto merc_low = build_site("MidSiteMercs", -1);
    const auto merc_high = build_site("MidSiteMercs", 5);
    const auto trainer_low = build_site("MidSiteTrainer", -1);
    const auto trainer_high = build_site("MidSiteTrainer", 4);

    for (const auto* result : {&mage_low, &mage_high, &merchant_low, &merchant_high, &merc_low,
                               &merc_high, &trainer_low, &trainer_high}) {
        EXPECT_TRUE(std::any_of(result->diagnostics.begin(), result->diagnostics.end(),
                                [](const BuildDiagnostic& d) {
                                    return d.kind == BuildDiagnosticKind::InvalidSiteImageIndex;
                                }));
        EXPECT_TRUE(result->world.sites.empty());
        EXPECT_EQ(result->world.map_objects.size(), 1u);
    }
}

TEST(AdventureWorldBuilder, UnknownPlanReferenceCreatesDiag) {
    ScenarioTemplate tmpl;
    tmpl.info.id = "scn_unref";
    tmpl.map.terrain.width = 10;
    tmpl.map.terrain.height = 10;
    tmpl.map.terrain.tiles.assign(10, std::vector<uint32_t>(10, 0));

    SgMidgardPlan plan;
    plan.id = "plan_unref";
    SgPlanEntry e;
    e.element = "S143UNKNOWN";
    e.pos_x = 1;
    e.pos_y = 1;
    plan.entries.push_back(e);
    tmpl.plans.push_back(plan);

    AdventureWorldBuilder builder;
    auto                  result = builder.build(tmpl);

    bool found = false;
    for (const auto& d : result.diagnostics) {
        if (d.kind == BuildDiagnosticKind::UnknownPlanReference) {
            found = true;
            EXPECT_TRUE(d.message.find("S143UNKNOWN") != std::string::npos);
            break;
        }
    }
    EXPECT_TRUE(found) << "Expected UnknownPlanReference diagnostic for unreferenced plan entry";
}

TEST(AdventureWorldBuilder, BuildsTypedUnitsAndStacksWithExplicitLeader) {
    ScenarioTemplate tmpl;
    tmpl.info.id = "scn_stack";
    tmpl.map.terrain.width = 10;
    tmpl.map.terrain.height = 10;
    tmpl.map.terrain.tiles.assign(10, std::vector<uint32_t>(10, 0));

    for (const auto& id : {"A", "B", "C"}) {
        SgUnit unit;
        unit.id = id;
        unit.type_id = std::string("G000UU000") + (id == std::string("A")   ? "1"
                                                   : id == std::string("B") ? "2"
                                                                            : "3");
        unit.modifier_ids = {"M1", "M1"};
        unit.transformed = 4;
        unit.dynamic_level = 5;
        unit.hp = 10;
        unit.xp = 20;
        tmpl.units.push_back(unit);
    }

    SgStack stack;
    stack.id = "S";
    stack.group_id = "G";
    stack.units = {"A", "B", "C", "G000000000", "G000000000", "G000000000"};
    // Cell-indexed: cell 0 → member 0 (A), cell 1 → member 1 (B), cell 2 → member 2 (C)
    stack.positions = {0, 1, 2, -1, -1, -1};
    stack.leader_id = "C";
    stack.leader_alive = 3;
    stack.pos_x = 6;
    stack.pos_y = 7;
    tmpl.stacks.push_back(stack);

    AdventureWorldBuilder builder;
    const auto            result = builder.build(tmpl);

    ASSERT_EQ(result.world.units.size(), 3u);
    ASSERT_EQ(result.world.stacks.size(), 1u);
    const auto& runtime_stack = result.world.stacks[0];
    EXPECT_EQ(runtime_stack.id, "S");
    EXPECT_EQ(runtime_stack.group_id, "G");
    EXPECT_EQ(runtime_stack.inside, "");
    ASSERT_TRUE(runtime_stack.group.members[0].has_value());
    ASSERT_TRUE(runtime_stack.group.members[1].has_value());
    ASSERT_TRUE(runtime_stack.group.members[2].has_value());
    EXPECT_EQ(*runtime_stack.group.members[0], "A");
    EXPECT_EQ(*runtime_stack.group.members[1], "B");
    EXPECT_EQ(*runtime_stack.group.members[2], "C");
    // Inverted: cell 0 → member 0, cell 1 → member 1, cell 2 → member 2
    EXPECT_EQ(runtime_stack.group.positions[0], 0);
    EXPECT_EQ(runtime_stack.group.positions[1], 1);
    EXPECT_EQ(runtime_stack.group.positions[2], 2);
    EXPECT_EQ(runtime_stack.group.positions[3], -1);
    EXPECT_EQ(runtime_stack.group.positions[4], -1);
    EXPECT_EQ(runtime_stack.group.positions[5], -1);
    EXPECT_EQ(runtime_stack.leader_id, "C");
    EXPECT_NE(result.world.find_unit("C"), nullptr);
    EXPECT_NE(result.world.find_stack("S"), nullptr);
}

TEST(AdventureWorldBuilder, PositionValueIsFormationCellForSameMember) {
    ScenarioTemplate tmpl;
    tmpl.info.id = "scn_pos_ismember";
    tmpl.map.terrain.width = 10;
    tmpl.map.terrain.height = 10;
    tmpl.map.terrain.tiles.assign(10, std::vector<uint32_t>(10, 0));

    for (const auto& id : {"A", "B", "C"}) {
        SgUnit unit;
        unit.id = id;
        unit.type_id = "G000UU0001";
        tmpl.units.push_back(unit);
    }

    // Cell-indexed: cell 2 → member 0 (A occupies formation cell 2)
    SgStack stack;
    stack.id = "S";
    stack.units = {"A", "B", "C", "G000000000", "G000000000", "G000000000"};
    stack.positions = {-1, -1, 0, -1, -1, -1};
    stack.leader_id = "C";
    tmpl.stacks.push_back(stack);

    AdventureWorldBuilder builder;
    const auto            result = builder.build(tmpl);

    ASSERT_EQ(result.world.stacks.size(), 1u);
    EXPECT_EQ(result.world.stacks[0].group.members[0].value_or(""), "A");
    // After inversion: cell 2 → member 0 → positions[0] = 2
    EXPECT_EQ(result.world.stacks[0].group.positions[0], 2);
    // No diagnostic: member 0 is non-empty, position 2 is a valid formation cell
    const auto has_invalid_diag =
        std::any_of(result.diagnostics.begin(), result.diagnostics.end(), [](const auto& d) {
            return d.kind == BuildDiagnosticKind::InvalidFormationCell;
        });
    EXPECT_FALSE(has_invalid_diag);
}

TEST(AdventureWorldBuilder, PreservesStackInside) {
    ScenarioTemplate tmpl;
    tmpl.info.id = "scn_stack_inside";
    tmpl.map.terrain.width = 10;
    tmpl.map.terrain.height = 10;
    tmpl.map.terrain.tiles.assign(10, std::vector<uint32_t>(10, 0));

    SgUnit unit;
    unit.id = "A";
    unit.type_id = "G000UU0001";
    tmpl.units.push_back(unit);

    SgStack stack;
    stack.id = "S";
    stack.units = {"A", "G000000000", "G000000000", "G000000000", "G000000000", "G000000000"};
    stack.positions = {0, -1, -1, -1, -1, -1};
    stack.leader_id = "A";
    stack.inside = "S143FT0000";
    tmpl.stacks.push_back(stack);

    AdventureWorldBuilder builder;
    const auto            result = builder.build(tmpl);

    ASSERT_EQ(result.world.stacks.size(), 1u);
    EXPECT_EQ(result.world.stacks[0].inside, "S143FT0000");
    EXPECT_NE(result.world.find_stack("S"), nullptr);
}

TEST(AdventureWorldBuilder, FindContainedStackLocationResolvesVillageCapitalAndRejectsInvalid) {
    d2runtime::AdventureWorldState world;

    d2runtime::AdventureCity city;
    city.id = "CITY";
    city.stack_id = "STACK_CITY";
    city.footprint = {{1, 1}, {2, 1}, {1, 2}, {2, 2}};
    world.cities.push_back(city);

    d2runtime::AdventureCapital capital;
    capital.id = "CAP";
    capital.visiting_stack_id = "STACK_CAP";
    capital.footprint = {{4, 4}, {5, 4}, {4, 5}, {5, 5}};
    world.capitals.push_back(capital);

    d2runtime::AdventureStack village_stack;
    village_stack.id = "STACK_CITY";
    village_stack.inside = "CITY";
    world.stacks.push_back(village_stack);

    d2runtime::AdventureStack capital_stack;
    capital_stack.id = "STACK_CAP";
    capital_stack.inside = "CAP";
    world.stacks.push_back(capital_stack);

    d2runtime::AdventureStack visible_stack;
    visible_stack.id = "VISIBLE";
    visible_stack.inside = "";
    world.stacks.push_back(visible_stack);

    const auto* village_found = world.find_stack("STACK_CITY");
    ASSERT_NE(village_found, nullptr);
    const auto village_location = world.find_contained_stack_location(*village_found);
    ASSERT_TRUE(village_location.has_value());
    EXPECT_EQ(village_location->kind, d2runtime::AdventureSettlementKind::Village);
    EXPECT_EQ(village_location->settlement_id, "CITY");
    ASSERT_NE(village_location->footprint, nullptr);
    EXPECT_EQ(village_location->footprint->size(), city.footprint.size());

    const auto* capital_found = world.find_stack("STACK_CAP");
    ASSERT_NE(capital_found, nullptr);
    const auto capital_location = world.find_contained_stack_location(*capital_found);
    ASSERT_TRUE(capital_location.has_value());
    EXPECT_EQ(capital_location->kind, d2runtime::AdventureSettlementKind::Capital);
    EXPECT_EQ(capital_location->settlement_id, "CAP");
    ASSERT_NE(capital_location->footprint, nullptr);
    EXPECT_EQ(capital_location->footprint->size(), capital.footprint.size());

    const auto* visible_found = world.find_stack("VISIBLE");
    ASSERT_NE(visible_found, nullptr);
    EXPECT_FALSE(world.find_contained_stack_location(*visible_found).has_value());

    d2runtime::AdventureStack dangling;
    dangling.id = "DANGLING";
    dangling.inside = "MISSING";
    EXPECT_FALSE(world.find_contained_stack_location(dangling).has_value());

    d2runtime::AdventureStack city_mismatch;
    city_mismatch.id = "STACK_CITY";
    city_mismatch.inside = "CITY";
    world.cities.front().stack_id = "OTHER";
    EXPECT_FALSE(world.find_contained_stack_location(city_mismatch).has_value());

    d2runtime::AdventureStack capital_mismatch;
    capital_mismatch.id = "STACK_CAP";
    capital_mismatch.inside = "CAP";
    world.capitals.front().visiting_stack_id = "OTHER";
    EXPECT_FALSE(world.find_contained_stack_location(capital_mismatch).has_value());
}

TEST(AdventureWorldBuilder, TypedStackValidationDiagnostics) {
    auto build_with_stack = [](SgStack stack) {
        ScenarioTemplate tmpl;
        tmpl.info.id = "scn_diag_stack";
        tmpl.map.terrain.width = 10;
        tmpl.map.terrain.height = 10;
        tmpl.map.terrain.tiles.assign(10, std::vector<uint32_t>(10, 0));
        SgUnit unit;
        unit.id = "A";
        unit.type_id = "G000UU0001";
        tmpl.units.push_back(unit);
        tmpl.stacks.push_back(std::move(stack));
        AdventureWorldBuilder builder;
        return builder.build(tmpl);
    };

    auto has_kind = [](const AdventureWorldBuildResult& result, BuildDiagnosticKind kind) {
        return std::any_of(result.diagnostics.begin(), result.diagnostics.end(),
                           [&](const BuildDiagnostic& d) { return d.kind == kind; });
    };

    SgStack dangling_member;
    dangling_member.id = "S1";
    dangling_member.units = {"MISSING",    "G000000000", "G000000000",
                             "G000000000", "G000000000", "G000000000"};
    // Cell-indexed: cell 0 → member 0 (MISSING unit)
    dangling_member.positions = {0, -1, -1, -1, -1, -1};
    dangling_member.leader_id = "MISSING";
    EXPECT_TRUE(has_kind(build_with_stack(dangling_member),
                         BuildDiagnosticKind::DanglingStackUnitReference));

    SgStack bad_index;
    bad_index.id = "S2";
    bad_index.units = {"A", "G000000000", "G000000000", "G000000000", "G000000000", "G000000000"};
    // Cell-indexed: cell 0 references member 6 (out of range)
    bad_index.positions = {6, -1, -1, -1, -1, -1};
    bad_index.leader_id = "A";
    EXPECT_TRUE(has_kind(build_with_stack(bad_index), BuildDiagnosticKind::InvalidFormationCell));

    SgStack empty_member;
    empty_member.id = "S3";
    empty_member.units = {"A",          "G000000000", "G000000000",
                          "G000000000", "G000000000", "G000000000"};
    // Cell-indexed: cell 0 → member 1 (which is empty slot) → PositionAssignedToEmptyMember
    empty_member.positions = {1, -1, -1, -1, -1, -1};
    empty_member.leader_id = "A";
    EXPECT_TRUE(has_kind(build_with_stack(empty_member),
                         BuildDiagnosticKind::PositionAssignedToEmptyMember));

    SgStack dangling_leader;
    dangling_leader.id = "S4";
    dangling_leader.units = {"A",          "G000000000", "G000000000",
                             "G000000000", "G000000000", "G000000000"};
    // Cell-indexed: cell 0 → member 0 (A, which exists)
    dangling_leader.positions = {0, -1, -1, -1, -1, -1};
    dangling_leader.leader_id = "MISSING";
    EXPECT_TRUE(
        has_kind(build_with_stack(dangling_leader), BuildDiagnosticKind::DanglingStackLeader));

    SgStack leader_not_member;
    leader_not_member.id = "S5";
    leader_not_member.units = {"G000000000", "G000000000", "G000000000",
                               "G000000000", "G000000000", "G000000000"};
    leader_not_member.positions = {-1, -1, -1, -1, -1, -1};
    leader_not_member.leader_id = "A";
    EXPECT_TRUE(
        has_kind(build_with_stack(leader_not_member), BuildDiagnosticKind::LeaderNotInStackGroup));
}

TEST(AdventureWorldBuilder, NonSymmetricFormationCellsFromCellIndexedMapping) {
    // Verifies correct inversion of cell-indexed POS[cell] = member_index
    // into the runtime member-indexed convenience view positions[member_idx] = cell.
    //
    // Cell-indexed input: {3, 2, 0, 1, 4, -1}
    //   cell 0 → member 3 (D)
    //   cell 1 → member 2 (C)
    //   cell 2 → member 0 (A)
    //   cell 3 → member 1 (B)
    //   cell 4 → member 4 (E)
    //   cell 5 → empty
    //
    // Expected inversion (runtime):
    //   member 0 (A) → cell 2
    //   member 1 (B) → cell 3
    //   member 2 (C) → cell 1
    //   member 3 (D) → cell 0
    //   member 4 (E) → cell 4
    //   member 5     → cell -1
    ScenarioTemplate tmpl;
    tmpl.info.id = "scn_non_sym";
    tmpl.map.terrain.width = 10;
    tmpl.map.terrain.height = 10;
    tmpl.map.terrain.tiles.assign(10, std::vector<uint32_t>(10, 0));

    for (const auto& id : {"A", "B", "C", "D", "E"}) {
        SgUnit unit;
        unit.id = id;
        unit.type_id = "G000UU0001";
        tmpl.units.push_back(unit);
    }

    SgStack stack;
    stack.id = "S";
    stack.units = {"A", "B", "C", "D", "E", "G000000000"};
    // Cell-indexed: POS[cell] = member_index
    stack.positions = {3, 2, 0, 1, 4, -1};
    stack.leader_id = "A";
    tmpl.stacks.push_back(stack);

    AdventureWorldBuilder builder;
    const auto            result = builder.build(tmpl);

    ASSERT_EQ(result.world.stacks.size(), 1u);
    const auto& g = result.world.stacks[0].group;
    // After inversion
    EXPECT_EQ(g.positions[0], 2);
    EXPECT_EQ(g.positions[1], 3);
    EXPECT_EQ(g.positions[2], 1);
    EXPECT_EQ(g.positions[3], 0);
    EXPECT_EQ(g.positions[4], 4);
    EXPECT_EQ(g.positions[5], -1);
    // Must NOT be identity propagation (member-indexed values read naively)
    EXPECT_NE(g.positions[0], 3);
    EXPECT_NE(g.positions[1], 2);
    EXPECT_NE(g.positions[2], 0);
    EXPECT_NE(g.positions[3], 1);
}

TEST(AdventureWorldBuilder, SparseFormationCellZeroInCellTwo) {
    // POS = [-1, -1, 0, -1, -1, -1] — cell 2 contains member 0
    // Inversion should place member 0 at formation cell 2.
    ScenarioTemplate tmpl;
    tmpl.info.id = "scn_sparse";
    tmpl.map.terrain.width = 10;
    tmpl.map.terrain.height = 10;
    tmpl.map.terrain.tiles.assign(10, std::vector<uint32_t>(10, 0));

    SgUnit unit;
    unit.id = "U";
    unit.type_id = "G000UU0001";
    tmpl.units.push_back(unit);

    SgStack stack;
    stack.id = "S";
    stack.units = {"U", "G000000000", "G000000000", "G000000000", "G000000000", "G000000000"};
    stack.positions = {-1, -1, 0, -1, -1, -1};
    stack.leader_id = "U";
    tmpl.stacks.push_back(stack);

    AdventureWorldBuilder builder;
    const auto            result = builder.build(tmpl);

    ASSERT_EQ(result.world.stacks.size(), 1u);
    EXPECT_EQ(result.world.stacks[0].group.positions[0], 2);
}

TEST(AdventureWorldBuilder, LargeUnitFootprintFromCellIndexedPositions) {
    // Large unit occupies cells 2 and 3: POS_2 = 0, POS_3 = 0
    // After inversion, member 0 should get anchor cell 2 (first/leftmost occurrence).
    // derive_formation_cells(2, large=true) → {2, 3}
    ScenarioTemplate tmpl;
    tmpl.info.id = "scn_large";
    tmpl.map.terrain.width = 10;
    tmpl.map.terrain.height = 10;
    tmpl.map.terrain.tiles.assign(10, std::vector<uint32_t>(10, 0));

    SgUnit unit;
    unit.id = "L";
    unit.type_id = "G000UU0001";
    tmpl.units.push_back(unit);

    SgStack stack;
    stack.id = "S";
    stack.units = {"L", "G000000000", "G000000000", "G000000000", "G000000000", "G000000000"};
    // Cell-indexed: cells 2 and 3 both point to member 0
    stack.positions = {-1, -1, 0, 0, -1, -1};
    stack.leader_id = "L";
    tmpl.stacks.push_back(stack);

    AdventureWorldBuilder builder;
    const auto            result = builder.build(tmpl);

    ASSERT_EQ(result.world.stacks.size(), 1u);
    // Anchor is the first occurrence (cell 2), not cell 3
    EXPECT_EQ(result.world.stacks[0].group.positions[0], 2);
}

TEST(AdventureWorldBuilder, MapSeedPropagatedToWorldState) {
    ScenarioTemplate tmpl;
    tmpl.info.id = "scn_seed";
    tmpl.info.map_seed = 12345;
    tmpl.map.terrain.width = 10;
    tmpl.map.terrain.height = 10;
    tmpl.map.terrain.tiles.assign(10, std::vector<uint32_t>(10, 0));

    AdventureWorldBuilder builder;
    const auto            result = builder.build(tmpl);
    EXPECT_EQ(result.world.map_seed, 12345);
}

TEST(AdventureWorldBuilder, UnknownPlanRefDiagnosedForUnrecognizedIds) {
    ScenarioTemplate tmpl;
    tmpl.info.id = "scn_unknown_plan";
    tmpl.map.terrain.width = 10;
    tmpl.map.terrain.height = 10;
    tmpl.map.terrain.tiles.assign(10, std::vector<uint32_t>(10, 0));

    SgMidgardPlan plan;
    plan.id = "plan1";
    plan.entries.push_back({"NONEXISTENT_ID", 0, 0});
    tmpl.plans.push_back(plan);

    AdventureWorldBuilder builder;
    const auto            result = builder.build(tmpl);

    const auto has_unknown =
        std::any_of(result.diagnostics.begin(), result.diagnostics.end(), [](const auto& d) {
            return d.kind == BuildDiagnosticKind::UnknownPlanReference &&
                   d.object_id == "NONEXISTENT_ID";
        });
    EXPECT_TRUE(has_unknown) << "unrecognized plan ref must produce UnknownPlanReference";
}

TEST(AdventureWorldBuilder, ForestTerrainCode9To14Detected) {
    AdventureTerrainDecoder decoder;
    EXPECT_FALSE(decoder.decode_tile(1).is_forest);
    EXPECT_FALSE(decoder.decode_tile(6).is_forest);
    EXPECT_TRUE(decoder.decode_tile(9).is_forest);
    EXPECT_TRUE(decoder.decode_tile(10).is_forest);
    EXPECT_TRUE(decoder.decode_tile(11).is_forest);
    EXPECT_TRUE(decoder.decode_tile(12).is_forest);
    EXPECT_TRUE(decoder.decode_tile(13).is_forest);
    EXPECT_TRUE(decoder.decode_tile(14).is_forest);

    EXPECT_FALSE(decoder.decode_tile(7).is_forest);
    EXPECT_FALSE(decoder.decode_tile(8).is_forest);
    EXPECT_FALSE(decoder.decode_tile(15).is_forest);
    EXPECT_FALSE(decoder.decode_tile(0).is_forest);

    EXPECT_EQ(decoder.decode_tile(9).material, AdventureTerrainMaterial::Human);
    EXPECT_EQ(decoder.decode_tile(10).material, AdventureTerrainMaterial::Dwarf);
    EXPECT_EQ(decoder.decode_tile(14).material, AdventureTerrainMaterial::Elf);
}

TEST(AdventureWorldBuilder, NonSquareCanonicalTerrain) {
    ScenarioTemplate tmpl;
    tmpl.info.id = "scn_nonsquare";
    tmpl.map.terrain.width = 7;
    tmpl.map.terrain.height = 4;
    tmpl.map.terrain.tiles.assign(
        static_cast<std::size_t>(tmpl.map.terrain.height),
        std::vector<uint32_t>(static_cast<std::size_t>(tmpl.map.terrain.width), 0));
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 7; ++x) {
            tmpl.map.terrain.tiles[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] =
                static_cast<uint32_t>(y * 100 + x);
        }
    }

    AdventureWorldBuilder builder;
    auto                  result = builder.build(tmpl);

    // Canonical: map_width = raw_height = 4, map_height = raw_width = 7
    EXPECT_EQ(result.world.map_width, 4);
    EXPECT_EQ(result.world.map_height, 7);
    EXPECT_EQ(result.world.terrain.width, 4);
    EXPECT_EQ(result.world.terrain.height, 7);

    // Check a few canonical cells: canonical[x][y] = raw[y][x]
    EXPECT_EQ(result.world.terrain.tile_at(0, 0)->raw_value, 0u);
    EXPECT_EQ(result.world.terrain.tile_at(1, 0)->raw_value, 100u);
    EXPECT_EQ(result.world.terrain.tile_at(2, 0)->raw_value, 200u);
    EXPECT_EQ(result.world.terrain.tile_at(0, 1)->raw_value, 1u);
}
