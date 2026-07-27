#include <d2adventure_render/terrain/treasure_asset_catalog.hpp>
#include <d2engine/assets/ff_asset_store.hpp>
#include <d2engine/assets/treasure_asset_catalog_builder.hpp>

#include <gtest/gtest.h>

#include <d2runtime/AdventureTreasure.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <stdexcept>

namespace fs = std::filesystem;
namespace d2ar = d2engine::adventure_render;

namespace {

struct TreasureMapping {
    d2runtime::AdventureTreasurePlacement placement;
    int                                   image;
    const char*                           logical_sprite;
};

constexpr TreasureMapping kLandMappings[] = {
    {d2runtime::AdventureTreasurePlacement::Land, 0, "G000BG0000100"},
    {d2runtime::AdventureTreasurePlacement::Land, 1, "G000BG0000101"},
    {d2runtime::AdventureTreasurePlacement::Land, 2, "G000BG0000102"},
    {d2runtime::AdventureTreasurePlacement::Land, 3, "G000BG0000103"},
    {d2runtime::AdventureTreasurePlacement::Land, 4, "G000BG0000104"},
    {d2runtime::AdventureTreasurePlacement::Land, 5, "G000BG0000105"},
    {d2runtime::AdventureTreasurePlacement::Land, 6, "G000BG0000106"},
    {d2runtime::AdventureTreasurePlacement::Land, 7, "G000BG0000107"},
};

constexpr TreasureMapping kWaterMappings[] = {
    {d2runtime::AdventureTreasurePlacement::Water, 0, "G000BG0000000"},
    {d2runtime::AdventureTreasurePlacement::Water, 1, "G000BG0000001"},
    {d2runtime::AdventureTreasurePlacement::Water, 2, "G000BG0000002"},
    {d2runtime::AdventureTreasurePlacement::Water, 3, "G000BG0000003"},
};

d2ar::TreasureAssetCatalog make_manual_catalog() {
    d2ar::TreasureAssetCatalog catalog;
    for (const auto& mapping : kLandMappings) {
        d2ar::StaticTreasureVisual visual;
        visual.container_path = "Imgs/IsoCmon.ff";
        visual.logical_sprite = mapping.logical_sprite;
        visual.canvas_foot_x = 100;
        visual.canvas_foot_y = 100;
        visual.canvas_width = 100;
        visual.canvas_height = 100;
        catalog.land_visuals[static_cast<std::size_t>(mapping.image)] = std::move(visual);
    }
    for (const auto& mapping : kWaterMappings) {
        d2ar::StaticTreasureVisual visual;
        visual.container_path = "Imgs/IsoCmon.ff";
        visual.logical_sprite = mapping.logical_sprite;
        visual.canvas_foot_x = 320;
        visual.canvas_foot_y = 320;
        visual.canvas_width = 320;
        visual.canvas_height = 320;
        catalog.water_visuals[static_cast<std::size_t>(mapping.image)] = std::move(visual);
    }
    return catalog;
}

} // namespace

TEST(TreasureAssetCatalog, ManualCatalogResolvesAllSupportedImages) {
    const auto catalog = make_manual_catalog();
    for (const auto& mapping : kLandMappings) {
        const auto* found = catalog.find(mapping.placement, mapping.image);
        ASSERT_NE(found, nullptr);
        EXPECT_EQ(found->container_path, "Imgs/IsoCmon.ff");
        EXPECT_EQ(found->logical_sprite, mapping.logical_sprite);
        EXPECT_EQ(found->canvas_foot_x, 100);
        EXPECT_EQ(found->canvas_foot_y, 100);
        EXPECT_EQ(found->canvas_width, 100);
        EXPECT_EQ(found->canvas_height, 100);
        const auto& resolved = catalog.resolve(mapping.placement, mapping.image);
        EXPECT_EQ(&resolved, found);
    }
    for (const auto& mapping : kWaterMappings) {
        const auto* found = catalog.find(mapping.placement, mapping.image);
        ASSERT_NE(found, nullptr);
        EXPECT_EQ(found->container_path, "Imgs/IsoCmon.ff");
        EXPECT_EQ(found->logical_sprite, mapping.logical_sprite);
        EXPECT_EQ(found->canvas_foot_x, 320);
        EXPECT_EQ(found->canvas_foot_y, 320);
        EXPECT_EQ(found->canvas_width, 320);
        EXPECT_EQ(found->canvas_height, 320);
        const auto& resolved = catalog.resolve(mapping.placement, mapping.image);
        EXPECT_EQ(&resolved, found);
    }
}

TEST(TreasureAssetCatalog, UnsupportedImagesReturnNullAndThrow) {
    const auto catalog = make_manual_catalog();
    EXPECT_EQ(catalog.find(d2runtime::AdventureTreasurePlacement::Land, -1), nullptr);
    EXPECT_EQ(catalog.find(d2runtime::AdventureTreasurePlacement::Land, 8), nullptr);
    EXPECT_EQ(catalog.find(d2runtime::AdventureTreasurePlacement::Water, -1), nullptr);
    EXPECT_EQ(catalog.find(d2runtime::AdventureTreasurePlacement::Water, 4), nullptr);
    EXPECT_THROW(
        { static_cast<void>(catalog.resolve(d2runtime::AdventureTreasurePlacement::Land, -1)); },
        std::runtime_error);
    EXPECT_THROW(
        { static_cast<void>(catalog.resolve(d2runtime::AdventureTreasurePlacement::Land, 8)); },
        std::runtime_error);
    EXPECT_THROW(
        { static_cast<void>(catalog.resolve(d2runtime::AdventureTreasurePlacement::Water, -1)); },
        std::runtime_error);
    EXPECT_THROW(
        { static_cast<void>(catalog.resolve(d2runtime::AdventureTreasurePlacement::Water, 4)); },
        std::runtime_error);
}

TEST(TreasureAssetCatalogBuilderSource, DoesNotReferenceBagsMetadata) {
    const auto    source = fs::path(OPENDIS2_SOURCE_DIR) / "src" / "d2engine" / "assets" /
                           "treasure_asset_catalog_builder.cpp";
    std::ifstream in(source);
    ASSERT_TRUE(in.good()) << source;
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    EXPECT_EQ(text.find("sprite_metadata(\"Imgs/IsoCmon.ff\", \"BAGS\")"), std::string::npos);
    EXPECT_EQ(text.find("std::array<std::optional<StaticTreasureVisual>, 12>"), std::string::npos);
}

TEST(TreasureContributorSource, UsesPlacementAwareLookup) {
    const auto source =
        fs::path(OPENDIS2_SOURCE_DIR) / "src" / "d2adventure_render" / "treasure_contributor.cpp";
    std::ifstream in(source);
    ASSERT_TRUE(in.good()) << source;
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    EXPECT_EQ(text.find("catalog.find(treasure.image)"), std::string::npos);
    EXPECT_NE(text.find("catalog.find(treasure.placement, treasure.image)"), std::string::npos);
}

TEST(TreasureAssetCatalogBuilder, BuildsAllLogicalSpritesFromRealGameData) {
    const char* root_env = std::getenv("DISCIPLES2_GAME_ROOT");
    if (root_env == nullptr || root_env[0] == '\0') {
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set";
    }

    const fs::path root_path(root_env);
    if (!fs::is_directory(root_path)) {
        GTEST_SKIP() << "Game root is not a directory: " << root_path;
    }

    d2engine::FfAssetStore store(root_path);
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
