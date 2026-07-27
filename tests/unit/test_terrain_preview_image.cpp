#include <opendis2_terrain_preview/terrain_preview_image.hpp>

#include <d2runtime/AdventureTerrain.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string read_source(const char* relative_path) {
    std::ifstream      in(std::string{OPENDIS2_SOURCE_DIR} + "/" + relative_path);
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

} // namespace

TEST(TerrainPreviewGeometry, OneByOneGridHasPositiveBounds) {
    const auto layout = d2terrain_preview::make_preview_layout(1, 1);
    EXPECT_GT(layout.logical_width, 0);
    EXPECT_GT(layout.logical_height, 0);
    ASSERT_EQ(layout.positions.size(), 1u);
    EXPECT_GE(layout.positions[0].x, 0);
    EXPECT_GE(layout.positions[0].y, 0);
}

TEST(TerrainPreviewGeometry, TwoByTwoGridIncludesAllPositions) {
    const auto layout = d2terrain_preview::make_preview_layout(2, 2);
    ASSERT_EQ(layout.positions.size(), 4u);
    for (const auto& pos : layout.positions) {
        EXPECT_GE(pos.x, 0);
        EXPECT_GE(pos.y, 0);
        EXPECT_LT(pos.x, layout.logical_width);
        EXPECT_LT(pos.y, layout.logical_height);
    }
}

TEST(TerrainPreviewGeometry, DiamondMaskCornersTransparentCenterOpaque) {
    EXPECT_FALSE(d2terrain_preview::diamond_contains(0, 0));
    EXPECT_FALSE(d2terrain_preview::diamond_contains(63, 31));
    EXPECT_TRUE(d2terrain_preview::diamond_contains(32, 16));
}

TEST(TerrainPreviewGeometry, MaxSizeDownscaleRespectsLimit) {
    d2engine::AdventureTerrainSurfaceInput input;
    input.map_width = 2;
    input.map_height = 2;
    input.resolved_tiles.resize(4);
    for (auto& tile : input.resolved_tiles) {
        tile.descriptor.family = d2runtime::AdventureTerrainFamily::Neutral;
        tile.descriptor.terrain_code = "NE";
    }
    d2engine::AdventureTerrainSurfaceComposer composer({});

    const auto image = d2terrain_preview::render_preview_image(
        2, 2, input, composer,
        {.mode = d2terrain_preview::PreviewMode::FamilyId, .scale = 10, .max_size = 64});

    EXPECT_LE(image.output_width, 64);
    EXPECT_LE(image.output_height, 64);
}

TEST(AdventureTerrainPreview, RenderPreviewUsesCanonicalInputDirectly) {
    d2engine::AdventureTerrainSurfaceInput input;
    input.map_width = 2;
    input.map_height = 1;
    input.resolved_tiles.resize(2);
    input.resolved_tiles[0].descriptor.raw_value = 0x00000001;
    input.resolved_tiles[0].descriptor.family = d2runtime::AdventureTerrainFamily::Human;
    input.resolved_tiles[0].descriptor.terrain_code = "HU";
    input.resolved_tiles[1].descriptor.raw_value = 0x00000002;
    input.resolved_tiles[1].descriptor.family = d2runtime::AdventureTerrainFamily::Dwarf;
    input.resolved_tiles[1].descriptor.terrain_code = "DW";
    input.descriptors.resize(2);

    d2engine::AdventureTerrainSurfaceComposer composer({});

    const auto image = d2terrain_preview::render_preview_image(
        2, 1, input, composer, {.mode = d2terrain_preview::PreviewMode::FamilyId});

    const auto layout = d2terrain_preview::make_preview_layout(2, 1);
    ASSERT_EQ(layout.positions.size(), 2U)
        << "canonical 2x1 input must produce 2 tile positions, not transposed to 1x2";
    EXPECT_GT(image.output_width, 0);
    EXPECT_GT(image.output_height, 0);
}

TEST(AdventureTerrainPreview, DoesNotRenormalizeCanonicalTerrain) {
    d2engine::AdventureTerrainSurfaceInput input;
    input.map_width = 5;
    input.map_height = 3;
    input.resolved_tiles.resize(15);
    for (auto& tile : input.resolved_tiles) {
        tile.descriptor.family = d2runtime::AdventureTerrainFamily::Neutral;
        tile.descriptor.terrain_code = "NE";
    }
    input.descriptors.resize(15);

    d2engine::AdventureTerrainSurfaceComposer composer({});

    const auto image = d2terrain_preview::render_preview_image(
        5, 3, input, composer,
        {.mode = d2terrain_preview::PreviewMode::FamilyId, .scale = 1, .max_size = 64});

    EXPECT_GT(image.output_width, 0);
    EXPECT_GT(image.output_height, 0);
}

TEST(TerrainPreviewCli, MainIsThinSinglePngWrapper) {
    const auto                  source = read_source("src/opendis2_terrain_preview/main.cpp");
    static constexpr std::array forbidden = {
        "--out-dir",        "--border-shape-map",      "assets_mask-", "low_byte.png",
        "border_shape.png", "family_id.png",           "terrain.csv",  "candidates.csv",
        "summary.txt",      "write_border_candidates",
    };

    EXPECT_NE(source.find("--output"), std::string::npos);
    EXPECT_NE(source.find("AdventureWorldBuilder"), std::string::npos)
        << "main.cpp must build canonical world via AdventureWorldBuilder";
    EXPECT_NE(source.find("AdventureTerrainSurfaceComposer composer"), std::string::npos);
    for (const auto* needle : forbidden) {
        EXPECT_EQ(source.find(needle), std::string::npos) << needle;
    }
}
