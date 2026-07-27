#include <d2adventure_render/terrain/road_asset_catalog.hpp>
#include <d2engine/assets/ff_asset_store.hpp>
#include <d2engine/assets/road_asset_catalog_builder.hpp>

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace {

std::filesystem::path get_game_root() {
    const char* env = std::getenv("DISCIPLES2_GAME_ROOT");
    if (env == nullptr || env[0] == '\0')
        return {};
    return env;
}

} // namespace

TEST(RoadAssetCatalogIntegration, BuildsCatalogFromRealGameData) {
    const auto game_root = get_game_root();
    if (game_root.empty())
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set";

    d2engine::FfAssetStore store(game_root);
    const auto             catalog = d2engine::build_road_asset_catalog(store);

    EXPECT_EQ(catalog.container, "Imgs/IsoTerrn.ff");

    // Every index 0..15 must resolve
    for (int i = 0; i <= 15; ++i) {
        const auto* visual = catalog.find(i);
        ASSERT_NE(visual, nullptr) << "road index " << i << " must exist in game data";

        char expected_name[16];
        std::snprintf(expected_name, sizeof(expected_name), "ROAD%02d00", i);
        EXPECT_EQ(visual->logical_sprite, expected_name)
            << "road index " << i << " logical sprite mismatch";

        const auto meta = store.sprite_metadata(catalog.container, visual->logical_sprite);
        EXPECT_EQ(visual->width, meta.canvas_width) << visual->logical_sprite << " width mismatch";
        EXPECT_EQ(visual->height, meta.canvas_height)
            << visual->logical_sprite << " height mismatch";
    }

    const auto* v16 = catalog.find(16);
    EXPECT_EQ(v16, nullptr) << "ROAD1600 must not be a valid road index";
}

TEST(RoadAssetCatalogIntegration, ExactRoadFamilyCoverage) {
    const auto game_root = get_game_root();
    if (game_root.empty())
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set";

    d2engine::FfAssetStore store(game_root);

    const auto all_sprites = store.sprites_in("Imgs/IsoTerrn.ff");

    std::set<std::string> road_sprites;
    for (const auto& name : all_sprites) {
        if (name.starts_with("ROAD"))
            road_sprites.insert(name);
    }

    std::set<std::string> expected;
    for (int i = 0; i <= 15; ++i) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "ROAD%02d00", i);
        expected.insert(buf);
    }

    for (const auto& exp : expected) {
        EXPECT_TRUE(road_sprites.contains(exp))
            << "expected road sprite " << exp << " not found in IsoTerrn.ff";
    }

    for (const auto& name : road_sprites) {
        EXPECT_TRUE(expected.contains(name))
            << "unexpected road sprite " << name << " found in IsoTerrn.ff";
    }
}

TEST(RoadAssetCatalogIntegration, RoadCanvasFamilyIsUniform) {
    const auto game_root = get_game_root();
    if (game_root.empty())
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set";

    d2engine::FfAssetStore store(game_root);
    const auto             catalog = d2engine::build_road_asset_catalog(store);

    const auto* v0 = catalog.find(0);
    ASSERT_NE(v0, nullptr);

    const int ref_width = v0->width;
    const int ref_height = v0->height;

    for (int i = 1; i <= 15; ++i) {
        const auto* visual = catalog.find(i);
        ASSERT_NE(visual, nullptr) << "road index " << i;
        EXPECT_EQ(visual->width, ref_width)
            << visual->logical_sprite << " width differs from ROAD0000";
        EXPECT_EQ(visual->height, ref_height)
            << visual->logical_sprite << " height differs from ROAD0000";
    }
}

TEST(RoadAssetCatalogIntegration, DerivedVisibleFeetDifferAcrossTopologies) {
    const auto game_root = get_game_root();
    if (game_root.empty())
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set";

    d2engine::FfAssetStore store(game_root);
    const auto             catalog = d2engine::build_road_asset_catalog(store);

    struct FootInfo {
        int index;
        int foot_x;
        int foot_y;
    };
    std::vector<FootInfo> feet;
    for (int i = 0; i <= 15; ++i) {
        const auto* visual = catalog.find(i);
        ASSERT_NE(visual, nullptr);
        const auto meta = store.sprite_metadata(catalog.container, visual->logical_sprite);
        feet.push_back({i, meta.canvas_foot_x, meta.canvas_foot_y});
    }

    int unique_foot_x_count = 0;
    int unique_foot_y_count = 0;
    for (std::size_t i = 0; i < feet.size(); ++i) {
        bool x_seen = false;
        bool y_seen = false;
        for (std::size_t j = 0; j < i; ++j) {
            if (feet[j].foot_x == feet[i].foot_x)
                x_seen = true;
            if (feet[j].foot_y == feet[i].foot_y)
                y_seen = true;
        }
        if (!x_seen)
            ++unique_foot_x_count;
        if (!y_seen)
            ++unique_foot_y_count;
    }

    EXPECT_GE(unique_foot_x_count, 2) << "Visible-foot x varies too little across ROAD topologies "
                                      << "to prove topology-dependent placement is invalid";
    EXPECT_GE(unique_foot_y_count, 2) << "Visible-foot y varies too little across ROAD topologies "
                                      << "to prove topology-dependent placement is invalid";
}
