#include <d2runtime/AdventureTerrainDecoder.hpp>

#include <gtest/gtest.h>

namespace {

d2runtime::AdventureTerrainTileDescriptor decode(uint32_t raw) {
    const d2runtime::AdventureTerrainDecoder decoder;
    return decoder.decode_tile(raw);
}

void expect_family(uint32_t raw, d2runtime::AdventureTerrainFamily family, const char* code,
                   const char* ground_record) {
    const auto tile = decode(raw);
    EXPECT_EQ(tile.family, family);
    EXPECT_EQ(tile.terrain_code, code);
    EXPECT_EQ(tile.expected_ground_asset.container_path, "Imgs/Ground.ff");
    EXPECT_EQ(tile.expected_ground_asset.record_name, ground_record);
}

void expect_material(uint32_t raw, d2runtime::AdventureTerrainMaterial material, const char* code,
                     const char* ground_record) {
    const auto tile = decode(raw);
    EXPECT_EQ(tile.material, material);
    EXPECT_EQ(tile.terrain_code, code);
    EXPECT_EQ(tile.expected_ground_asset.record_name, ground_record);
}

} // namespace

TEST(AdventureTerrainDecoder, FamilyMappingsExpectedGroundRecords) {
    expect_family(0, d2runtime::AdventureTerrainFamily::Black, "BL", "BL_00.PNG");
    expect_family(1, d2runtime::AdventureTerrainFamily::Human, "HU", "HU_00.PNG");
    expect_family(2, d2runtime::AdventureTerrainFamily::Dwarf, "DW", "DW_00.PNG");
    expect_family(3, d2runtime::AdventureTerrainFamily::Heretic, "HE", "HE_00.PNG");
    expect_family(4, d2runtime::AdventureTerrainFamily::Undead, "UN", "UN_00.PNG");
    expect_family(5, d2runtime::AdventureTerrainFamily::Neutral, "NE", "NE_00.PNG");
    expect_family(6, d2runtime::AdventureTerrainFamily::Elf, "EL", "EL_00.PNG");
    expect_family(7, d2runtime::AdventureTerrainFamily::Water, "WA", "WA_00.PNG");
}

TEST(AdventureTerrainDecoder, LowByteMaterialTableDrivesProductionMaterial) {
    expect_material(0, d2runtime::AdventureTerrainMaterial::Black, "BL", "BL_00.PNG");
    expect_material(1, d2runtime::AdventureTerrainMaterial::Human, "HU", "HU_00.PNG");
    expect_material(9, d2runtime::AdventureTerrainMaterial::Human, "HU", "HU_01.PNG");
    expect_material(2, d2runtime::AdventureTerrainMaterial::Dwarf, "DW", "DW_00.PNG");
    expect_material(10, d2runtime::AdventureTerrainMaterial::Dwarf, "DW", "DW_01.PNG");
    expect_material(3, d2runtime::AdventureTerrainMaterial::Heretic, "HE", "HE_00.PNG");
    expect_material(11, d2runtime::AdventureTerrainMaterial::Heretic, "HE", "HE_01.PNG");
    expect_material(4, d2runtime::AdventureTerrainMaterial::Undead, "UN", "UN_00.PNG");
    expect_material(12, d2runtime::AdventureTerrainMaterial::Undead, "UN", "UN_01.PNG");
    expect_material(5, d2runtime::AdventureTerrainMaterial::Neutral, "NE", "NE_00.PNG");
    expect_material(13, d2runtime::AdventureTerrainMaterial::Neutral, "NE", "NE_01.PNG");
    expect_material(37, d2runtime::AdventureTerrainMaterial::Neutral, "NE", "NE_00.PNG");
    expect_material(6, d2runtime::AdventureTerrainMaterial::Elf, "EL", "EL_00.PNG");
    expect_material(14, d2runtime::AdventureTerrainMaterial::Elf, "EL", "EL_01.PNG");
    expect_material(7, d2runtime::AdventureTerrainMaterial::Water, "WA", "WA_00.PNG");
    expect_material(29, d2runtime::AdventureTerrainMaterial::Water, "WA", "WA_00.PNG");
}

TEST(AdventureTerrainDecoder, LowByteTwentyNineIsWaterButKeepsDiagnosticFamilyId) {
    const auto tile = decode(29);
    EXPECT_EQ(tile.raw.family_id, 5);
    EXPECT_EQ(tile.material, d2runtime::AdventureTerrainMaterial::Water);
    EXPECT_EQ(tile.family, d2runtime::AdventureTerrainFamily::Water);
    EXPECT_EQ(tile.terrain_code, "WA");
    EXPECT_FALSE(tile.unknown_material);
}

TEST(AdventureTerrainDecoder, UnknownLowByteFallsBackToBlackDiagnosticMaterial) {
    const auto tile = decode(0x000000FF);
    EXPECT_EQ(tile.material, d2runtime::AdventureTerrainMaterial::Unknown);
    EXPECT_EQ(tile.family, d2runtime::AdventureTerrainFamily::Unknown);
    EXPECT_EQ(tile.terrain_code, "BL");
    EXPECT_EQ(tile.expected_ground_asset.record_name, "BL_00.PNG");
    EXPECT_TRUE(tile.unknown_material);
}

TEST(AdventureTerrainDecoder, DecodesNeutralWithoutBorder) {
    const auto tile = decode(0x0000000D);
    EXPECT_EQ(tile.raw.low_byte, 0x0D);
    EXPECT_EQ(tile.raw.family_id, 5);
    EXPECT_EQ(tile.raw.terrain_flags, 0x08);
    EXPECT_EQ(tile.raw.variant_bits, 1);
    EXPECT_EQ(tile.raw.border_shape, 0);
    EXPECT_FALSE(tile.raw.has_border_shape);
    EXPECT_FALSE(tile.raw.has_drawable_border_shape);
    EXPECT_EQ(tile.family, d2runtime::AdventureTerrainFamily::Neutral);
    EXPECT_EQ(tile.terrain_code, "NE");
    EXPECT_FALSE(tile.border.has_value());
}

TEST(AdventureTerrainDecoder, DecodesShapeOneNeutralBorder) {
    const auto tile = decode(0x0400000D);
    EXPECT_EQ(tile.raw.border_shape, 1);
    EXPECT_TRUE(tile.raw.has_border_shape);
    EXPECT_TRUE(tile.raw.has_drawable_border_shape);
    ASSERT_TRUE(tile.border);
    EXPECT_EQ(tile.border->kind, d2runtime::AdventureTerrainBorderKind::Drawable);
    EXPECT_EQ(tile.border->family, "NE");
    ASSERT_TRUE(tile.border->expected_asset);
    EXPECT_EQ(tile.border->expected_asset->record_name, "NE_01_00.PNG");
}

TEST(AdventureTerrainDecoder, WaterBorderUsesWaterFamily) {
    const auto tile = decode(0x04000007);
    EXPECT_EQ(tile.family, d2runtime::AdventureTerrainFamily::Water);
    EXPECT_EQ(tile.terrain_code, "WA");
    ASSERT_TRUE(tile.border);
    EXPECT_EQ(tile.border->family, "WA");
    ASSERT_TRUE(tile.border->expected_asset);
    EXPECT_EQ(tile.border->expected_asset->record_name, "WA_01_00.PNG");
}

TEST(AdventureTerrainDecoder, ShapeSixteenIsNonDrawable) {
    const auto tile = decode(0x4000000D);
    EXPECT_EQ(tile.raw.border_shape, 16);
    EXPECT_TRUE(tile.raw.has_border_shape);
    EXPECT_FALSE(tile.raw.has_drawable_border_shape);
    ASSERT_TRUE(tile.border);
    EXPECT_EQ(tile.border->kind, d2runtime::AdventureTerrainBorderKind::NonDrawableShape16);
    EXPECT_EQ(tile.border->shape, 16);
    EXPECT_EQ(tile.border->family, "NE");
    EXPECT_FALSE(tile.border->expected_asset.has_value());
}

TEST(AdventureTerrainDecoder, DecodesShapeThirtyOneBorder) {
    const auto tile = decode(0x7C00000D);
    EXPECT_EQ(tile.raw.border_shape, 31);
    ASSERT_TRUE(tile.border);
    EXPECT_EQ(tile.border->kind, d2runtime::AdventureTerrainBorderKind::Drawable);
    ASSERT_TRUE(tile.border->expected_asset);
    EXPECT_EQ(tile.border->expected_asset->record_name, "NE_31_00.PNG");
}

TEST(AdventureTerrainDecoder, DecodesByteAndWordFields) {
    const auto tile = decode(0xAABBCCDD);
    EXPECT_EQ(tile.raw.low_byte, 0xDD);
    EXPECT_EQ(tile.raw.byte1, 0xCC);
    EXPECT_EQ(tile.raw.byte2, 0xBB);
    EXPECT_EQ(tile.raw.high_byte, 0xAA);
    EXPECT_EQ(tile.raw.low_word, 0xCCDD);
    EXPECT_EQ(tile.raw.high_word, 0xAABB);
}

TEST(AdventureTerrainMapDecoder, DecodesRuntimeGridRowMajor) {
    const d2runtime::AdventureTerrainDecoder    tile_decoder;
    const d2runtime::AdventureTerrainMapDecoder map_decoder(tile_decoder);
    d2runtime::AdventureTerrainGrid             grid;
    grid.width = 2;
    grid.height = 2;
    grid.tiles = {{.raw_value = 1}, {.raw_value = 2}, {.raw_value = 3}, {.raw_value = 4}};

    const auto tiles = map_decoder.decode_grid(grid);

    ASSERT_EQ(tiles.size(), 4u);
    EXPECT_EQ(tiles[0].raw_value, 1u);
    EXPECT_EQ(tiles[1].raw_value, 2u);
    EXPECT_EQ(tiles[2].raw_value, 3u);
    EXPECT_EQ(tiles[3].raw_value, 4u);
}
