#include <d2adventure_render/terrain/mountain_asset_catalog.hpp>
#include <d2engine/assets/ff_asset_store.hpp>
#include <d2engine/assets/mountain_asset_catalog_builder.hpp>

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <set>
#include <string>

namespace {

std::filesystem::path get_game_root() {
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    const char* env = std::getenv("DISCIPLES2_GAME_ROOT");
    if (env == nullptr || env[0] == '\0')
        return {};
    return env;
}

} // namespace

TEST(MountainAssetCatalogIntegration, BuildsCatalogFromRealGameData) {
    const auto game_root = get_game_root();
    if (game_root.empty())
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set";

    d2engine::FfAssetStore store(game_root);
    const auto             catalog = d2engine::build_mountain_asset_catalog(store);

    EXPECT_EQ(catalog.container, "Imgs/IsoTerrn.ff");

    // Every key must have race=4 and square dimensions
    int total = 0;
    for (const auto& [key, visual] : catalog.visuals) {
        ++total;
        EXPECT_EQ(key.race, 4) << "mountain sprite " << visual.logical_sprite;
        EXPECT_EQ(key.size_x, key.size_y)
            << "mountain sprite " << visual.logical_sprite << " must be square";
        EXPECT_FALSE(visual.logical_sprite.empty());
        EXPECT_GT(visual.width, 0);
        EXPECT_GT(visual.height, 0);
    }

    EXPECT_GT(total, 0) << "catalog must contain at least one neutral mountain sprite";

    // Verify the proven supported size groups have entries
    std::set<int> sizes_found;
    for (const auto& [key, visual] : catalog.visuals)
        sizes_found.insert(key.size_x);

    for (int expected_size : {1, 2, 3, 5}) {
        EXPECT_TRUE(sizes_found.contains(expected_size))
            << "catalog missing " << expected_size << "x" << expected_size
            << " neutral mountain sprites";
    }

    // Verify specific known combinations exist
    {
        const auto* v = catalog.find(4, 1, 1, 3);
        EXPECT_NE(v, nullptr) << "MOMNE0103 must exist in game data";
        if (v) {
            EXPECT_FALSE(v->logical_sprite.empty());
            EXPECT_GT(v->width, 0);
            EXPECT_GT(v->height, 0);
        }
    }
    {
        const auto* v = catalog.find(4, 2, 2, 7);
        EXPECT_NE(v, nullptr) << "MOMNE0207 must exist in game data";
    }
    {
        const auto* v = catalog.find(4, 3, 3, 0);
        EXPECT_NE(v, nullptr) << "MOMNE0300 must exist in game data";
    }
    {
        const auto* v = catalog.find(4, 5, 5, 6);
        EXPECT_NE(v, nullptr) << "MOMNE0506 must exist in game data";
    }
}

TEST(MountainAssetCatalogIntegration, MOMDW_FamilyNotCataloged) {
    const auto game_root = get_game_root();
    if (game_root.empty())
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set";

    d2engine::FfAssetStore store(game_root);
    const auto             catalog = d2engine::build_mountain_asset_catalog(store);

    // No MOMDW entries should be cataloged
    for (const auto& [key, visual] : catalog.visuals) {
        EXPECT_FALSE(visual.logical_sprite.starts_with("MOMDW"))
            << "MOMDW sprite not yet supported: " << visual.logical_sprite;
        EXPECT_FALSE(visual.logical_sprite.starts_with("MOM_DW"))
            << "MOM_DW sprite not yet supported: " << visual.logical_sprite;
        EXPECT_TRUE(visual.logical_sprite.starts_with("MOMNE"))
            << "only MOMNE sprites should be cataloged, got: " << visual.logical_sprite;
    }
}

TEST(MountainAssetCatalogIntegration, AllCatalogedSpritesHaveMetadata) {
    const auto game_root = get_game_root();
    if (game_root.empty())
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set";

    d2engine::FfAssetStore store(game_root);
    const auto             catalog = d2engine::build_mountain_asset_catalog(store);

    for (const auto& [key, visual] : catalog.visuals) {
        EXPECT_GT(visual.width, 0) << visual.logical_sprite << " has zero width";
        EXPECT_GT(visual.height, 0) << visual.logical_sprite << " has zero height";
        // canvas_foot is typically non-zero for FF sprites
        EXPECT_GE(visual.canvas_foot_x, 0)
            << visual.logical_sprite << " has negative canvas_foot_x";
        EXPECT_GE(visual.canvas_foot_y, 0)
            << visual.logical_sprite << " has negative canvas_foot_y";
    }
}
