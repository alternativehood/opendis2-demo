#include <gtest/gtest.h>

#include <d2adventure_render/map_geometry.hpp>
#include <d2adventure_render/map_preparer.hpp>
#include <d2adventure_render/treasure_contributor.hpp>
#include <d2adventure_render/terrain/treasure_asset_catalog.hpp>
#include <d2engine/assets/ff_asset_store.hpp>
#include <d2engine/assets/treasure_asset_catalog_builder.hpp>
#include <d2runtime/AdventureWorldBuilder.hpp>
#include <d2scenario/SgParser.hpp>

#include <cstdlib>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <exception>
#include <fstream>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;
namespace d2ar = d2engine::adventure_render;
namespace d2rt = d2runtime;

namespace {

struct TreasureMapping {
    d2rt::AdventureTreasurePlacement placement;
    int                              image;
    const char*                      logical_sprite;
};

constexpr TreasureMapping kLandMappings[] = {
    {d2rt::AdventureTreasurePlacement::Land, 0, "G000BG0000100"},
    {d2rt::AdventureTreasurePlacement::Land, 1, "G000BG0000101"},
    {d2rt::AdventureTreasurePlacement::Land, 2, "G000BG0000102"},
    {d2rt::AdventureTreasurePlacement::Land, 3, "G000BG0000103"},
    {d2rt::AdventureTreasurePlacement::Land, 4, "G000BG0000104"},
    {d2rt::AdventureTreasurePlacement::Land, 5, "G000BG0000105"},
    {d2rt::AdventureTreasurePlacement::Land, 6, "G000BG0000106"},
    {d2rt::AdventureTreasurePlacement::Land, 7, "G000BG0000107"},
};

constexpr TreasureMapping kWaterMappings[] = {
    {d2rt::AdventureTreasurePlacement::Water, 0, "G000BG0000000"},
    {d2rt::AdventureTreasurePlacement::Water, 1, "G000BG0000001"},
    {d2rt::AdventureTreasurePlacement::Water, 2, "G000BG0000002"},
    {d2rt::AdventureTreasurePlacement::Water, 3, "G000BG0000003"},
};

fs::path find_sg_file() {
    const char* env = std::getenv("OPENDIS2_ADVENTURE_TEST_SG");
    if (env != nullptr && env[0] != '\0') {
        fs::path p(env);
        if (fs::is_regular_file(p)) {
            return p;
        }
    }
    const char* game_root_env = std::getenv("DISCIPLES2_GAME_ROOT");
    if (game_root_env != nullptr && game_root_env[0] != '\0') {
        const fs::path p = fs::path(game_root_env) / "all_objects_demo_config_20260718_101736.sg";
        if (fs::is_regular_file(p)) {
            return p;
        }
    }
    const auto repo = fs::path(OPENDIS2_SOURCE_DIR) / "testdata" / "test_map.sg";
    if (fs::is_regular_file(repo)) {
        return repo;
    }
    return {};
}

} // namespace

TEST(TreasureAssetCatalogIntegration, BuildsCatalogFromRealGameData) {
    const char* root_env = std::getenv("DISCIPLES2_GAME_ROOT");
    if (root_env == nullptr || root_env[0] == '\0') {
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set";
    }

    d2engine::FfAssetStore store(root_env);
    const auto             catalog = d2engine::build_treasure_asset_catalog(store);
    for (const auto& mapping : kLandMappings) {
        const auto* found = catalog.find(mapping.placement, mapping.image);
        ASSERT_NE(found, nullptr) << mapping.image;
        EXPECT_EQ(found->container_path, "Imgs/IsoCmon.ff") << mapping.image;
        EXPECT_EQ(found->logical_sprite, mapping.logical_sprite) << mapping.image;

        const auto meta = store.sprite_metadata("Imgs/IsoCmon.ff", mapping.logical_sprite);
        EXPECT_EQ(found->canvas_foot_x, meta.canvas_foot_x) << mapping.image;
        EXPECT_EQ(found->canvas_foot_y, meta.canvas_foot_y) << mapping.image;
        EXPECT_EQ(found->canvas_width, meta.canvas_width) << mapping.image;
        EXPECT_EQ(found->canvas_height, meta.canvas_height) << mapping.image;
    }
    for (const auto& mapping : kWaterMappings) {
        const auto* found = catalog.find(mapping.placement, mapping.image);
        ASSERT_NE(found, nullptr) << mapping.image;
        EXPECT_EQ(found->container_path, "Imgs/IsoCmon.ff") << mapping.image;
        EXPECT_EQ(found->logical_sprite, mapping.logical_sprite) << mapping.image;

        const auto meta = store.sprite_metadata("Imgs/IsoCmon.ff", mapping.logical_sprite);
        EXPECT_EQ(found->canvas_foot_x, meta.canvas_foot_x) << mapping.image;
        EXPECT_EQ(found->canvas_foot_y, meta.canvas_foot_y) << mapping.image;
        EXPECT_EQ(found->canvas_width, meta.canvas_width) << mapping.image;
        EXPECT_EQ(found->canvas_height, meta.canvas_height) << mapping.image;
    }
}

TEST(TreasurePipelineIntegration, RealIsoCmonContributorUsesLogicalSprites) {
    const char* root_env = std::getenv("DISCIPLES2_GAME_ROOT");
    if (root_env == nullptr || root_env[0] == '\0') {
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set";
    }

    d2engine::FfAssetStore store(root_env);
    const auto             catalog = d2engine::build_treasure_asset_catalog(store);

    d2runtime::AdventureWorldState world;
    world.treasures.push_back({.id = "S143BG0000",
                               .position = {1, 1},
                               .image = 0,
                               .placement = d2rt::AdventureTreasurePlacement::Land,
                               .footprint = {{1, 1}}});
    world.treasures.push_back({.id = "S143BG0001",
                               .position = {2, 1},
                               .image = 0,
                               .placement = d2rt::AdventureTreasurePlacement::Water,
                               .footprint = {{2, 1}}});
    world.treasures.push_back({.id = "S143BG0003",
                               .position = {3, 1},
                               .image = 3,
                               .placement = d2rt::AdventureTreasurePlacement::Land,
                               .footprint = {{3, 1}}});
    world.treasures.push_back({.id = "S143BG0004",
                               .position = {4, 1},
                               .image = 3,
                               .placement = d2rt::AdventureTreasurePlacement::Water,
                               .footprint = {{4, 1}}});
    world.treasures.push_back({.id = "S143BG0007",
                               .position = {5, 1},
                               .image = 7,
                               .placement = d2rt::AdventureTreasurePlacement::Land,
                               .footprint = {{5, 1}}});

    const auto                 geometry = d2ar::AdventureMapGeometry::from_source(20, 20);
    d2ar::AdventureMapPreparer preparer(geometry);
    preparer.add_contributor(d2ar::make_treasure_contributor(catalog));

    const auto result = preparer.prepare(world);
    ASSERT_EQ(result.graph.world.size(), 5u);
    ASSERT_EQ(result.diagnostics.size(), 5u);

    std::unordered_map<std::string, std::string> record_names;
    for (const auto& prim : result.graph.world) {
        record_names[prim.debug_label] = prim.record_name;
    }

    EXPECT_EQ(record_names.at("Treasure:S143BG0000"), "G000BG0000100");
    EXPECT_EQ(record_names.at("Treasure:S143BG0001"), "G000BG0000000");
    EXPECT_EQ(record_names.at("Treasure:S143BG0003"), "G000BG0000103");
    EXPECT_EQ(record_names.at("Treasure:S143BG0004"), "G000BG0000003");
    EXPECT_EQ(record_names.at("Treasure:S143BG0007"), "G000BG0000107");
}

TEST(TreasurePipelineIntegration, TestMapRegressionBuildsTypedTreasuresAndRendersBags) {
    const auto sg_path = find_sg_file();
    if (sg_path.empty()) {
        GTEST_SKIP() << "test_map.sg not found";
    }

    std::ifstream in(sg_path, std::ios::binary | std::ios::ate);
    if (!in) {
        GTEST_SKIP() << "cannot open SG file: " << sg_path;
    }

    const auto           size = in.tellg();
    std::vector<uint8_t> data(static_cast<std::size_t>(size));
    in.seekg(0);
    in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));

    d2scenario::SgParser parser(data);
    const auto           parse_result = parser.parse();
    if (parse_result.scenario.bags.empty()) {
        GTEST_SKIP() << "scenario fixture has no MidBag records";
    }

    d2runtime::AdventureWorldBuilder builder;
    const auto                       build_result = builder.build(parse_result.scenario);

    ASSERT_EQ(build_result.world.treasures.size(), parse_result.scenario.bags.size());
    ASSERT_FALSE(build_result.world.treasures.empty());

    auto generic_bag = std::find_if(
        build_result.world.map_objects.begin(), build_result.world.map_objects.end(),
        [](const auto& mo) { return mo.kind == d2runtime::AdventureMapObjectKind::Bag; });
    ASSERT_NE(generic_bag, build_result.world.map_objects.end());

    const char* root_env = std::getenv("DISCIPLES2_GAME_ROOT");
    if (root_env == nullptr || root_env[0] == '\0') {
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set";
    }

    d2engine::FfAssetStore store(root_env);
    const auto             catalog = d2engine::build_treasure_asset_catalog(store);

    const auto geometry = d2ar::AdventureMapGeometry::from_source(build_result.world.map_width,
                                                                  build_result.world.map_height);
    d2ar::AdventureMapPreparer preparer(geometry);
    preparer.add_contributor(d2ar::make_treasure_contributor(catalog));

    const auto render_result = preparer.prepare(build_result.world);

    std::size_t treasure_primitives = 0;
    for (const auto& prim : render_result.graph.world) {
        if (!prim.debug_label.starts_with("Treasure:")) {
            continue;
        }
        ++treasure_primitives;
        const auto* treasure = build_result.world.find_treasure(prim.debug_label.substr(9));
        ASSERT_NE(treasure, nullptr);
        const auto* visual = catalog.find(treasure->placement, treasure->image);
        ASSERT_NE(visual, nullptr);
        EXPECT_EQ(prim.record_name, visual->logical_sprite);
        EXPECT_EQ(prim.container_path, "Imgs/IsoCmon.ff");
        EXPECT_NE(prim.record_name, "BAGS");
    }

    EXPECT_GT(treasure_primitives, 0u);
    EXPECT_LE(treasure_primitives, build_result.world.treasures.size());
}

TEST(TreasurePipelineIntegration, RealScenarioPlacementSelectsPlacementSpecificSprites) {
    const auto sg_path = find_sg_file();
    if (sg_path.empty()) {
        GTEST_SKIP() << "test_map.sg or real SG fixture not found";
    }

    std::ifstream in(sg_path, std::ios::binary | std::ios::ate);
    if (!in) {
        GTEST_SKIP() << "cannot open SG file: " << sg_path;
    }

    const auto           size = in.tellg();
    std::vector<uint8_t> data(static_cast<std::size_t>(size));
    in.seekg(0);
    in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));

    d2scenario::SgParser parser(data);
    const auto           parse_result = parser.parse();
    if (parse_result.scenario.bags.empty()) {
        GTEST_SKIP() << "scenario fixture has no MidBag records";
    }

    d2runtime::AdventureWorldBuilder builder;
    const auto                       build_result = builder.build(parse_result.scenario);

    const char* root_env = std::getenv("DISCIPLES2_GAME_ROOT");
    if (root_env == nullptr || root_env[0] == '\0') {
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set";
    }

    d2engine::FfAssetStore store(root_env);
    const auto             catalog = d2engine::build_treasure_asset_catalog(store);
    const auto geometry = d2ar::AdventureMapGeometry::from_source(build_result.world.map_width,
                                                                  build_result.world.map_height);
    d2ar::AdventureMapPreparer preparer(geometry);
    preparer.add_contributor(d2ar::make_treasure_contributor(catalog));

    const auto render_result = preparer.prepare(build_result.world);

    std::size_t land_rendered = 0;
    std::size_t water_rendered = 0;
    for (const auto& prim : render_result.graph.world) {
        if (!prim.debug_label.starts_with("Treasure:")) {
            continue;
        }
        const auto* treasure = build_result.world.find_treasure(prim.debug_label.substr(9));
        ASSERT_NE(treasure, nullptr);
        const auto* visual = catalog.find(treasure->placement, treasure->image);
        ASSERT_NE(visual, nullptr);
        EXPECT_EQ(prim.record_name, visual->logical_sprite);
        if (treasure->placement == d2rt::AdventureTreasurePlacement::Water) {
            ++water_rendered;
            EXPECT_TRUE(prim.record_name.starts_with("G000BG000000"));
        } else {
            ++land_rendered;
            EXPECT_TRUE(prim.record_name.starts_with("G000BG000010"));
        }
    }

    EXPECT_GT(land_rendered + water_rendered, 0u);
}
