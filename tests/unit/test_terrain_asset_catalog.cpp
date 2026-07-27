#include <gtest/gtest.h>

#include "d2engine/assets/terrain_asset_catalog.hpp"

namespace d2engine {
namespace {

TEST(TerrainAssetCatalogParsing, GroundNames) {
    auto hu = parse_ground_texture_record_name("HU_00.PNG");
    ASSERT_TRUE(hu.has_value());
    EXPECT_EQ(hu->terrain_code, "HU");
    EXPECT_EQ(hu->variant, 0);

    auto ne = parse_ground_texture_record_name("NE_03.PNG");
    ASSERT_TRUE(ne.has_value());
    EXPECT_EQ(ne->terrain_code, "NE");
    EXPECT_EQ(ne->variant, 3);

    auto wa = parse_ground_texture_record_name("WA_00.PNG");
    ASSERT_TRUE(wa.has_value());
    EXPECT_EQ(wa->terrain_code, "WA");
    EXPECT_EQ(wa->variant, 0);

    EXPECT_FALSE(parse_ground_texture_record_name("BL_00.PNG").has_value());
    EXPECT_FALSE(parse_ground_texture_record_name("HU_BAD.PNG").has_value());
    EXPECT_FALSE(parse_ground_texture_record_name("UNKNOWN_00.PNG").has_value());
}

TEST(TerrainAssetCatalogParsing, BorderNames) {
    auto ne01 = parse_border_asset_record_name("NE_01_00.PNG");
    ASSERT_TRUE(ne01.has_value());
    EXPECT_EQ(ne01->family, "NE");
    EXPECT_EQ(ne01->shape, 1);
    EXPECT_EQ(ne01->variant, 0);

    auto ne31 = parse_border_asset_record_name("NE_31_00.PNG");
    ASSERT_TRUE(ne31.has_value());
    EXPECT_EQ(ne31->shape, 31);

    auto wa09 = parse_border_asset_record_name("WA_09_00.PNG");
    ASSERT_TRUE(wa09.has_value());
    EXPECT_EQ(wa09->family, "WA");
    EXPECT_EQ(wa09->shape, 9);

    EXPECT_FALSE(parse_border_asset_record_name("XX_01_00.PNG").has_value());
    EXPECT_FALSE(parse_border_asset_record_name("NE_BAD_00.PNG").has_value());
}

TEST(TerrainAssetCatalogParsing, IsoFamilies) {
    EXPECT_EQ(parse_iso_logical_family("ROAD0100"), "ROAD");
    EXPECT_EQ(parse_iso_logical_family("HUF0000"), "HUF");
    EXPECT_EQ(parse_iso_logical_family("MOMDW0000"), "MOMDW");
    EXPECT_EQ(parse_iso_logical_family("FOG0000"), "FOG");
    EXPECT_FALSE(parse_iso_logical_family("ABC").has_value());
    EXPECT_FALSE(parse_iso_logical_family("123ABC").has_value());
}

TEST(TerrainAssetCatalog, SortsCatalogDeterministically) {
    TerrainAssetCatalog catalog;
    catalog.ground_textures = {
        {.terrain_code = "HU", .variant = 1, .record_name = "HU_01.PNG"},
        {.terrain_code = "BL", .variant = 0, .record_name = "BL_00.PNG"},
        {.terrain_code = "HU", .variant = 0, .record_name = "HU_00.PNG"},
    };
    catalog.border_assets = {
        {.family = "WA", .shape = 1, .variant = 0, .record_name = "WA_01_00.PNG"},
        {.family = "NE", .shape = 31, .variant = 0, .record_name = "NE_31_00.PNG"},
        {.family = "NE", .shape = 1, .variant = 0, .record_name = "NE_01_00.PNG"},
    };
    catalog.terrain_overlays = {
        {.family = "ROAD", .logical_name = "ROAD0100"},
        {.family = "FOG", .logical_name = "FOG0000"},
    };
    catalog.static_assets = {
        {.family = "ZZ", .logical_name = "ZZ0000", .container_path = "Imgs/IsoStill.ff"},
        {.family = "AA", .logical_name = "AA0000", .container_path = "Imgs/IsoCmon.ff"},
    };

    sort_terrain_asset_catalog(catalog);

    EXPECT_EQ(catalog.ground_textures[0].record_name, "BL_00.PNG");
    EXPECT_EQ(catalog.ground_textures[1].record_name, "HU_00.PNG");
    EXPECT_EQ(catalog.border_assets[0].record_name, "NE_01_00.PNG");
    EXPECT_EQ(catalog.border_assets[1].record_name, "NE_31_00.PNG");
    EXPECT_EQ(catalog.terrain_overlays[0].logical_name, "FOG0000");
    EXPECT_EQ(catalog.static_assets[0].container_path, "Imgs/IsoCmon.ff");
}

TEST(TerrainAssetCatalog, LookupHelpers) {
    TerrainAssetCatalog catalog;
    catalog.border_assets = {
        {.family = "NE", .shape = 1, .variant = 0, .record_name = "NE_01_00.PNG"},
    };

    EXPECT_EQ(catalog.find_border_asset("NE", 1, 0)->record_name, "NE_01_00.PNG");
}

} // namespace
} // namespace d2engine
