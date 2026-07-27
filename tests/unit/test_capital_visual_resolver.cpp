#include <d2adventure_render/capital_contributor.hpp>
#include <d2adventure_render/terrain/capital_asset_catalog.hpp>
#include <d2engine/assets/capital_visual_resolver.hpp>
#include <d2engine/assets/game_data_registry.hpp>

#include <d2runtime/AdventureWorldState.hpp>

#include <gtest/gtest.h>

#include "tests/test_dbf_builder.hpp"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>

namespace {

namespace fs = std::filesystem;
using test_dbf::DbfBuilder;

void write_empty_dbf(const fs::path& path) {
    std::ofstream ofs(path, std::ios::binary);
    ofs.put(0x1a);
}

fs::path write_fixture_globals() {
    static std::atomic<unsigned> counter{0};
    auto                         dir =
        fs::temp_directory_path() / ("d2_capital_visual_resolver_" + std::to_string(counter++));
    fs::create_directories(dir);

    for (const auto* name :
         {"Tglobal.dbf", "LunitB.dbf", "LunitC.dbf", "LDthAnim.dbf", "LattC.dbf", "LAttR.dbf",
          "LattS.dbf", "GDynUpgr.dbf", "Gattacks.dbf", "GMabi.dbf", "Gimmu.dbf", "GimmuC.dbf"}) {
        write_empty_dbf(dir / name);
    }

    {
        DbfBuilder b{{"RACE_ID", 'C', 16},   {"NAME_TXT", 'C', 20},  {"RACE_TYPE", 'N', 3},
                     {"PLAYABLE", 'C', 1},   {"REGEN_H", 'N', 3},    {"GUARDIAN", 'C', 16},
                     {"LEADER_1", 'C', 16},  {"LEADER_2", 'C', 16},  {"LEADER_3", 'C', 16},
                     {"LEADER_4", 'C', 16},  {"SOLDIER_1", 'C', 16}, {"SOLDIER_2", 'C', 16},
                     {"SOLDIER_3", 'C', 16}, {"SOLDIER_4", 'C', 16}, {"SOLDIER_5", 'C', 16}};
        b.add_record(
            {"g000rr0000", "", "0", "T", "0", "g000uu3001", "", "", "", "", "", "", "", "", ""});
        b.add_record(
            {"g000rr0001", "", "1", "T", "0", "g000uu4001", "", "", "", "", "", "", "", "", ""});
        b.add_record({"g000rr0002", "", "2", "T", "0", "", "", "", "", "", "", "", "", "", ""});
        b.add_record(
            {"g000rr0003", "", "3", "T", "0", "G000000000", "", "", "", "", "", "", "", "", ""});
        b.write(dir / "Grace.dbf");
    }

    {
        DbfBuilder b{{"UNIT_ID", 'C', 16},    {"RACE_ID", 'C', 16},   {"SUBRACE", 'N', 3},
                     {"LEVEL", 'N', 3},       {"HIT_POINT", 'N', 5},  {"SIZE_SMALL", 'C', 1},
                     {"UNIT_CAT", 'N', 3},    {"BRANCH", 'N', 3},     {"ATTACK_ID", 'C', 16},
                     {"ATTACK2_ID", 'C', 16}, {"DEATH_ANIM", 'N', 3}, {"NAME_TXT", 'C', 20},
                     {"DESC_TXT", 'C', 20},   {"ABIL_TXT", 'C', 20},  {"ENROLL_C", 'C', 20},
                     {"ENROLL_B", 'C', 20},   {"REVIVE_C", 'C', 20},  {"HEAL_C", 'C', 20},
                     {"TRAINING_C", 'C', 20}, {"BASE_UNIT", 'C', 16}, {"PREV_ID", 'C', 16},
                     {"UPGRADE_B", 'C', 16},  {"DYN_UPG1", 'C', 16},  {"DYN_UPG2", 'C', 16},
                     {"DYN_UPG_LV", 'N', 3}};
        b.add_record({"g000uu3001", "g000rr0000", "0", "1", "900", "T", "0", "0", "", "", "1", "",
                      "",           "",           "",  "",  "",    "",  "",  "",  "", "", "",  ""});
        b.add_record({"g000uu3002", "g000rr0000", "0", "1", "100", "T", "0", "0", "", "", "1", "",
                      "",           "",           "",  "",  "",    "",  "",  "",  "", "", "",  ""});
        b.add_record({"g000uu4001", "g000rr0001", "0", "1", "900", "T", "0", "0", "", "", "1", "",
                      "",           "",           "",  "",  "",    "",  "",  "",  "", "", "",  ""});
        b.add_record({"g000uu5001", "g000rr0000", "0", "1", "100", "T", "0", "0", "", "", "1", "",
                      "",           "",           "",  "",  "",    "",  "",  "",  "", "", "",  ""});
        b.write(dir / "Gunits.dbf");
    }

    return dir;
}

d2runtime::AdventureCapital make_capital() {
    d2runtime::AdventureCapital capital;
    capital.id = "capital_human";
    capital.subrace = "sub1";
    capital.position = {5, 5};
    capital.footprint = {{5, 5}, {6, 5}, {7, 5}, {5, 6}, {6, 6}, {7, 6}, {5, 7}, {6, 7}, {7, 7}};
    return capital;
}

d2runtime::AdventureWorldState make_world() {
    d2runtime::AdventureWorldState world;
    d2runtime::AdventureSubraceRef sr;
    sr.id = "sub1";
    sr.race_id = "g000rr0000";
    world.subraces.push_back(sr);
    return world;
}

d2engine::adventure_render::CapitalAssetCatalog make_catalog() {
    using namespace d2engine::adventure_render;
    CapitalAssetCatalog catalog;

    AnimatedCapitalVisual human_active;
    human_active.container_path = "Imgs/IsoAnim.ff";
    human_active.logical_animation_name = "G000FT0000HU0";
    human_active.canvas_foot_x = 151;
    human_active.canvas_foot_y = 252;
    human_active.animation_data.animation_name = human_active.logical_animation_name;
    human_active.animation_data.native_canvas_w = 320;
    human_active.animation_data.native_canvas_h = 320;
    human_active.animation_data.is_looping = true;
    human_active.animation_data.timing_source = AdventureAnimationTimingSource::ProvisionalFallback;
    human_active.animation_data.frames = {
        {.record_name = "H0", .duration_ms = 100, .canvas_width = 320, .canvas_height = 320},
    };

    AnimatedCapitalVisual human_ruined = human_active;
    human_ruined.logical_animation_name = "G000FT0000HUC0";
    human_ruined.animation_data.animation_name = human_ruined.logical_animation_name;

    AnimatedCapitalVisual dwarf_active = human_active;
    dwarf_active.logical_animation_name = "G000FT0000DWC0";
    dwarf_active.animation_data.animation_name = dwarf_active.logical_animation_name;

    catalog.visuals.emplace("g000rr0000", CapitalVisualSet{human_active, human_ruined});
    catalog.visuals.emplace("g000rr0001", CapitalVisualSet{dwarf_active, std::nullopt});
    return catalog;
}

TEST(CapitalVisualResolver, ResolvesActiveAndRuinedHumanByGuardianHp) {
    const auto                      globals = write_fixture_globals();
    d2engine::GameDataRegistry      game_data(globals);
    const auto                      catalog = make_catalog();
    d2engine::CapitalVisualResolver resolver(catalog, game_data);

    auto world = make_world();
    auto capital = make_capital();
    capital.garrison.members[0] = "g000uu3001";
    world.units.push_back({"g000uu3001", "G000UU3001", 0, {}, 0, "", 1, {}, 900, 0});

    auto resolved = resolver.resolve(world, capital, "G000RR0000");
    EXPECT_EQ(resolved.state, d2engine::adventure_render::CapitalVisualState::Active);
    EXPECT_EQ(resolved.guardian_type_id, "g000uu3001");
    EXPECT_EQ(resolved.guardian_instance_id, "g000uu3001");
    ASSERT_NE(resolved.visual, nullptr);
    EXPECT_EQ(resolved.visual->logical_animation_name, "G000FT0000HU0");
}

TEST(CapitalVisualResolver, ResolvesRuinedHumanWhenGuardianHpIsZeroOrNegative) {
    const auto                      globals = write_fixture_globals();
    d2engine::GameDataRegistry      game_data(globals);
    const auto                      catalog = make_catalog();
    d2engine::CapitalVisualResolver resolver(catalog, game_data);

    for (int hp : {0, -1}) {
        auto world = make_world();
        auto capital = make_capital();
        capital.garrison.members[0] = "g000uu3001";
        world.units.push_back({"g000uu3001", "g000uu3001", 0, {}, 0, "", 1, {}, hp, 0});

        auto resolved = resolver.resolve(world, capital, "g000rr0000");
        EXPECT_EQ(resolved.state, d2engine::adventure_render::CapitalVisualState::Ruined);
        ASSERT_NE(resolved.visual, nullptr);
        EXPECT_EQ(resolved.visual->logical_animation_name, "G000FT0000HUC0");
    }
}

TEST(CapitalVisualResolver, OrdinaryDefenderDoesNotCountAsGuardian) {
    const auto                      globals = write_fixture_globals();
    d2engine::GameDataRegistry      game_data(globals);
    const auto                      catalog = make_catalog();
    d2engine::CapitalVisualResolver resolver(catalog, game_data);

    auto world = make_world();
    auto capital = make_capital();
    capital.garrison.members[0] = "g000uu5001";
    world.units.push_back({"g000uu5001", "g000uu5001", 0, {}, 0, "", 1, {}, 100, 0});

    auto resolved = resolver.resolve(world, capital, "g000rr0000");
    EXPECT_EQ(resolved.state, d2engine::adventure_render::CapitalVisualState::Ruined);
    EXPECT_EQ(resolved.guardian_instance_id, "");
}

TEST(CapitalVisualResolver, VisitingStackGuardianDoesNotOverrideMissingCapitalGuardian) {
    const auto                      globals = write_fixture_globals();
    d2engine::GameDataRegistry      game_data(globals);
    const auto                      catalog = make_catalog();
    d2engine::CapitalVisualResolver resolver(catalog, game_data);

    auto world = make_world();
    auto capital = make_capital();
    capital.visiting_stack_id = "stack1";
    world.units.push_back({"g000uu3001", "g000uu3001", 0, {}, 0, "", 1, {}, 900, 0});
    d2runtime::AdventureStack stack;
    stack.id = "stack1";
    stack.leader_id = "g000uu3001";
    stack.leader_alive = 1;
    world.stacks.push_back(stack);

    auto resolved = resolver.resolve(world, capital, "g000rr0000");
    EXPECT_EQ(resolved.state, d2engine::adventure_render::CapitalVisualState::Ruined);
}

TEST(CapitalVisualResolver, EmptyOrMissingVisitingStackDoesNotMatterForLiveGuardian) {
    const auto                      globals = write_fixture_globals();
    d2engine::GameDataRegistry      game_data(globals);
    const auto                      catalog = make_catalog();
    d2engine::CapitalVisualResolver resolver(catalog, game_data);

    for (const auto& visiting_stack_id : {std::string{}, std::string{"missing_stack"}}) {
        auto world = make_world();
        auto capital = make_capital();
        capital.visiting_stack_id = visiting_stack_id;
        capital.garrison.members[0] = "g000uu3001";
        world.units.push_back({"g000uu3001", "g000uu3001", 0, {}, 0, "", 1, {}, 900, 0});

        auto resolved = resolver.resolve(world, capital, "g000rr0000");
        EXPECT_EQ(resolved.state, d2engine::adventure_render::CapitalVisualState::Active);
        EXPECT_EQ(resolved.guardian_instance_id, "g000uu3001");
    }
}

TEST(CapitalVisualResolver, ActiveDwarfUsesDWC0AndRuinedIsUnavailable) {
    const auto                      globals = write_fixture_globals();
    d2engine::GameDataRegistry      game_data(globals);
    const auto                      catalog = make_catalog();
    d2engine::CapitalVisualResolver resolver(catalog, game_data);

    auto world = make_world();
    auto capital = make_capital();
    capital.subrace = "sub2";
    d2runtime::AdventureSubraceRef sr;
    sr.id = "sub2";
    sr.race_id = "g000rr0001";
    world.subraces.push_back(sr);
    capital.garrison.members[0] = "g000uu4001";
    world.units.push_back({"g000uu4001", "g000uu4001", 0, {}, 0, "", 1, {}, 100, 0});

    auto active = resolver.resolve(world, capital, "g000rr0001");
    EXPECT_EQ(active.state, d2engine::adventure_render::CapitalVisualState::Active);
    ASSERT_NE(active.visual, nullptr);
    EXPECT_EQ(active.visual->logical_animation_name, "G000FT0000DWC0");

    world.units.front().current_hp = 0;
    EXPECT_THROW(static_cast<void>(resolver.resolve(world, capital, "g000rr0001")),
                 std::runtime_error);
}

TEST(CapitalVisualResolver, MissingRaceAndGuardianDefinitionsAreStrict) {
    const auto                      globals = write_fixture_globals();
    d2engine::GameDataRegistry      game_data(globals);
    const auto                      catalog = make_catalog();
    d2engine::CapitalVisualResolver resolver(catalog, game_data);

    auto world = make_world();
    auto capital = make_capital();

    EXPECT_THROW(static_cast<void>(resolver.resolve(world, capital, "g000rr9999")),
                 std::runtime_error);

    d2runtime::AdventureSubraceRef empty_sr;
    empty_sr.id = "sub2";
    empty_sr.race_id = "g000rr0002";
    world.subraces.push_back(empty_sr);
    capital.subrace = "sub2";
    EXPECT_THROW(static_cast<void>(resolver.resolve(world, capital, "g000rr0002")),
                 std::runtime_error);

    d2runtime::AdventureSubraceRef sentinel_sr;
    sentinel_sr.id = "sub3";
    sentinel_sr.race_id = "g000rr0003";
    world.subraces.push_back(sentinel_sr);
    capital.subrace = "sub3";
    EXPECT_THROW(static_cast<void>(resolver.resolve(world, capital, "g000rr0003")),
                 std::runtime_error);
}

TEST(CapitalVisualResolver, DanglingGarrisonMemberThrows) {
    const auto                      globals = write_fixture_globals();
    d2engine::GameDataRegistry      game_data(globals);
    const auto                      catalog = make_catalog();
    d2engine::CapitalVisualResolver resolver(catalog, game_data);

    auto world = make_world();
    auto capital = make_capital();
    capital.garrison.members[0] = "missing_unit";

    EXPECT_THROW(static_cast<void>(resolver.resolve(world, capital, "g000rr0000")),
                 std::runtime_error);
}

} // namespace
