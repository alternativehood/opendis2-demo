#include <d2engine/assets/adventure_terrain_asset_resolver.hpp>

#include <d2runtime/AdventureTerrainDecoder.hpp>

#include <gtest/gtest.h>

namespace {

d2engine::TerrainAssetCatalog make_catalog() {
    d2engine::TerrainAssetCatalog catalog;
    catalog.ground_textures.push_back({.terrain_code = "NE",
                                       .variant = 0,
                                       .container_path = "Imgs/Ground.ff",
                                       .record_name = "NE_00.PNG",
                                       .width = 256,
                                       .height = 256});
    catalog.ground_textures.push_back({.terrain_code = "WA",
                                       .variant = 0,
                                       .container_path = "Imgs/Ground.ff",
                                       .record_name = "WA_00.PNG",
                                       .width = 256,
                                       .height = 256});
    catalog.border_assets.push_back({.family = "NE",
                                     .shape = 1,
                                     .variant = 0,
                                     .container_path = "Imgs/GrBorder.ff",
                                     .record_name = "NE_01_00.PNG",
                                     .width = 64,
                                     .height = 32});
    catalog.border_assets.push_back({.family = "WA",
                                     .shape = 1,
                                     .variant = 0,
                                     .container_path = "Imgs/GrBorder.ff",
                                     .record_name = "WA_01_00.PNG",
                                     .width = 64,
                                     .height = 32});
    return catalog;
}

d2engine::ResolvedAdventureTerrainTile resolve(uint32_t raw) {
    const auto                                    catalog = make_catalog();
    const d2runtime::AdventureTerrainDecoder      decoder;
    const d2engine::AdventureTerrainAssetResolver resolver(catalog);
    return resolver.resolve(decoder.decode_tile(raw));
}

} // namespace

TEST(AdventureTerrainAssetResolver, ExistingGroundResolvesFound) {
    const auto tile = resolve(0x00000005);
    EXPECT_TRUE(tile.ground_asset_found);
}

TEST(AdventureTerrainAssetResolver, MissingGroundResolvesMissing) {
    const auto tile = resolve(0x00000001);
    EXPECT_FALSE(tile.ground_asset_found);
    EXPECT_EQ(tile.descriptor.expected_ground_asset.record_name, "HU_00.PNG");
}

TEST(AdventureTerrainAssetResolver, DrawableExistingBorderResolvesFound) {
    const auto tile = resolve(0x0400000D);
    ASSERT_TRUE(tile.border);
    EXPECT_EQ(tile.border->descriptor.kind, d2runtime::AdventureTerrainBorderKind::Drawable);
    EXPECT_TRUE(tile.border->asset_found);
}

TEST(AdventureTerrainAssetResolver, DrawableMissingBorderResolvesMissing) {
    const auto tile = resolve(0x0800000D);
    ASSERT_TRUE(tile.border);
    EXPECT_EQ(tile.border->descriptor.kind, d2runtime::AdventureTerrainBorderKind::Drawable);
    EXPECT_FALSE(tile.border->asset_found);
    ASSERT_TRUE(tile.border->descriptor.expected_asset);
    EXPECT_EQ(tile.border->descriptor.expected_asset->record_name, "NE_02_00.PNG");
}

TEST(AdventureTerrainAssetResolver, ShapeSixteenIsNotMissing) {
    const auto tile = resolve(0x4000000D);
    ASSERT_TRUE(tile.border);
    EXPECT_EQ(tile.border->descriptor.kind,
              d2runtime::AdventureTerrainBorderKind::NonDrawableShape16);
    EXPECT_TRUE(tile.border->asset_found);
    EXPECT_FALSE(tile.border->descriptor.expected_asset.has_value());
}

TEST(AdventureTerrainAssetResolver, WaterBorderRequestsWaterFamily) {
    const auto tile = resolve(0x04000007);
    ASSERT_TRUE(tile.border);
    EXPECT_TRUE(tile.border->asset_found);
    EXPECT_EQ(tile.descriptor.terrain_code, "WA");
    EXPECT_EQ(tile.descriptor.expected_ground_asset.record_name, "WA_00.PNG");
    EXPECT_EQ(tile.border->descriptor.family, "WA");
    ASSERT_TRUE(tile.border->descriptor.expected_asset);
    EXPECT_EQ(tile.border->descriptor.expected_asset->record_name, "WA_01_00.PNG");
}
