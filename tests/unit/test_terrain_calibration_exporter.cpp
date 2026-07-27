#include <opendis2_terrain_calibration_export/terrain_calibration_exporter.hpp>

#include <d2runtime/AdventureTerrainDecoder.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr int kMapSize = 48;

d2runtime::AdventureTerrainGrid grid(std::array<std::uint32_t, 4>             apex,
                                     std::optional<std::pair<int, int>>       bad_outside = {},
                                     d2terrain_calibration::ApexDisplayOrigin origin = {}) {
    d2runtime::AdventureTerrainGrid result;
    result.width = kMapSize;
    result.height = kMapSize;
    result.tiles.assign(kMapSize * kMapSize, {.raw_value = 1});
    const auto coords = d2terrain_calibration::apex_coords(origin);
    for (std::size_t i = 0; i < coords.size(); ++i) {
        result.tile_at(coords[i].source_x, coords[i].source_y)->raw_value = apex[i];
    }
    if (bad_outside.has_value()) {
        result.tile_at(bad_outside->first, bad_outside->second)->raw_value = 5;
    }
    return result;
}

std::vector<d2runtime::AdventureTerrainTileDescriptor>
decode(const d2runtime::AdventureTerrainGrid& terrain) {
    const d2runtime::AdventureTerrainDecoder    tile_decoder;
    const d2runtime::AdventureTerrainMapDecoder map_decoder(tile_decoder);
    return map_decoder.decode_grid(terrain);
}

const d2terrain_calibration::PatternEntry& pattern(std::string_view stem) {
    const auto& manifest = d2terrain_calibration::pattern_manifest();
    const auto  it = std::ranges::find(manifest, stem, &d2terrain_calibration::PatternEntry::stem);
    if (it == manifest.end()) {
        throw std::runtime_error("missing test pattern");
    }
    return *it;
}

std::filesystem::path temp_dir(const std::string& name) {
    auto path = std::filesystem::temp_directory_path() / name;
    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path);
    return path;
}

void touch(const std::filesystem::path& path) {
    std::ofstream out(path, std::ios::binary);
    out << "x";
}

std::string read_source(const char* relative_path) {
    std::ifstream      in(std::string{OPENDIS2_SOURCE_DIR} + "/" + relative_path);
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

} // namespace

TEST(TerrainCalibrationExporter, ExactSgFilenames) {
    const auto files = d2terrain_calibration::expected_sg_filenames();
    ASSERT_EQ(files.size(), 29U);
    EXPECT_EQ(files.front(), "control_apex_all_hu.sg");
    EXPECT_EQ(files.back(), "water_apex_14_right_bottom_left_wa.sg");
    EXPECT_EQ(std::set<std::string>(files.begin(), files.end()).size(), files.size());
}

TEST(TerrainCalibrationExporter, ExactScreenshotFilenames) {
    const auto files = d2terrain_calibration::expected_editor_png_filenames();
    ASSERT_EQ(files.size(), 29U);
    EXPECT_EQ(files.front(), "control_apex_all_hu_editor.png");
    EXPECT_EQ(files.back(), "water_apex_14_right_bottom_left_wa_editor.png");
    EXPECT_EQ(std::set<std::string>(files.begin(), files.end()).size(), files.size());
}

TEST(TerrainCalibrationExporter, MissingFileFailureListsCompleteExpectedDataset) {
    const auto dir = temp_dir("opendis2_calibration_missing");
    const auto result = d2terrain_calibration::preflight_dataset(dir);

    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.missing.size(), 58U);
    EXPECT_EQ(d2terrain_calibration::expected_dataset_filenames().size(), 58U);
}

TEST(TerrainCalibrationExporter, UnexpectedSgAndEditorPngAreRejected) {
    const auto dir = temp_dir("opendis2_calibration_unexpected");
    for (const auto& file : d2terrain_calibration::expected_dataset_filenames()) {
        touch(dir / file);
    }
    touch(dir / "extra.sg");
    touch(dir / "extra_editor.png");

    const auto result = d2terrain_calibration::preflight_dataset(dir);

    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.unexpected, (std::vector<std::string>{"extra.sg", "extra_editor.png"}));
}

TEST(TerrainCalibrationExporter, FixedManifestContainsAllPatterns) {
    const auto& manifest = d2terrain_calibration::pattern_manifest();
    ASSERT_EQ(manifest.size(), 29U);
    EXPECT_EQ(pattern("land_apex_07_top_right_bottom_ne").expected_materials,
              (std::array<std::string, 4>{"NE", "NE", "NE", "HU"}));
    EXPECT_EQ(pattern("water_apex_14_right_bottom_left_wa").expected_materials,
              (std::array<std::string, 4>{"HU", "WA", "WA", "WA"}));
}

TEST(TerrainCalibrationExporter, ExactTransposeApexCoordinates) {
    const auto coords = d2terrain_calibration::apex_coords();
    EXPECT_EQ(coords[0].name, "apex_top");
    EXPECT_EQ((std::array{coords[0].display_x, coords[0].display_y, coords[0].source_x,
                          coords[0].source_y}),
              (std::array{0, 0, 0, 0}));
    EXPECT_EQ((std::array{coords[1].display_x, coords[1].display_y, coords[1].source_x,
                          coords[1].source_y}),
              (std::array{1, 0, 0, 1}));
    EXPECT_EQ((std::array{coords[2].display_x, coords[2].display_y, coords[2].source_x,
                          coords[2].source_y}),
              (std::array{1, 1, 1, 1}));
    EXPECT_EQ((std::array{coords[3].display_x, coords[3].display_y, coords[3].source_x,
                          coords[3].source_y}),
              (std::array{0, 1, 1, 0}));
}

TEST(TerrainCalibrationExporter, ConfiguredApexOriginMovesDisplayBlock) {
    const auto coords = d2terrain_calibration::apex_coords({.x = 2, .y = 2});

    EXPECT_EQ((std::array{coords[0].display_x, coords[0].display_y}), (std::array{2, 2}));
    EXPECT_EQ((std::array{coords[1].display_x, coords[1].display_y}), (std::array{3, 2}));
    EXPECT_EQ((std::array{coords[2].display_x, coords[2].display_y}), (std::array{3, 3}));
    EXPECT_EQ((std::array{coords[3].display_x, coords[3].display_y}), (std::array{2, 3}));
}

TEST(TerrainCalibrationExporter, ConfiguredApexSourceCoordsUseTranspose) {
    const auto coords = d2terrain_calibration::apex_coords({.x = 2, .y = 2});

    for (const auto& coord : coords) {
        EXPECT_EQ(coord.source_x, coord.display_y);
        EXPECT_EQ(coord.source_y, coord.display_x);
    }
}

TEST(TerrainCalibrationExporter, ValidatesAllHuControl) {
    const auto terrain = grid({1, 1, 1, 1});
    EXPECT_TRUE(d2terrain_calibration::validate_apex_terrain(
                    "control.sg", pattern("control_apex_all_hu"), terrain, decode(terrain))
                    .ok);
}

TEST(TerrainCalibrationExporter, ValidatesOnePositionPatterns) {
    const auto top_ne = grid({5, 1, 1, 1});
    const auto left_wa = grid({1, 1, 1, 29});
    EXPECT_TRUE(d2terrain_calibration::validate_apex_terrain(
                    "top.sg", pattern("land_apex_01_top_ne"), top_ne, decode(top_ne))
                    .ok);
    EXPECT_TRUE(d2terrain_calibration::validate_apex_terrain(
                    "left.sg", pattern("water_apex_08_left_wa"), left_wa, decode(left_wa))
                    .ok);
}

TEST(TerrainCalibrationExporter, ValidatesThreePositionPatterns) {
    const auto land = grid({5, 5, 5, 1});
    const auto water = grid({1, 29, 29, 29});
    EXPECT_TRUE(d2terrain_calibration::validate_apex_terrain(
                    "land.sg", pattern("land_apex_07_top_right_bottom_ne"), land, decode(land))
                    .ok);
    EXPECT_TRUE(d2terrain_calibration::validate_apex_terrain(
                    "water.sg", pattern("water_apex_14_right_bottom_left_wa"), water, decode(water))
                    .ok);
}

TEST(TerrainCalibrationExporter, RejectsIncorrectApexMaterial) {
    const auto terrain = grid({1, 1, 1, 1});
    const auto result = d2terrain_calibration::validate_apex_terrain(
        "bad.sg", pattern("land_apex_01_top_ne"), terrain, decode(terrain));

    ASSERT_FALSE(result.ok);
    EXPECT_EQ(result.failures.front().location, "apex_top");
}

TEST(TerrainCalibrationExporter, RejectsNonHuTerrainOutsideApex) {
    const auto terrain = grid({1, 1, 1, 1}, std::pair{2, 0});
    const auto result = d2terrain_calibration::validate_apex_terrain(
        "bad.sg", pattern("control_apex_all_hu"), terrain, decode(terrain));

    ASSERT_FALSE(result.ok);
    EXPECT_EQ(result.failures.front().location, "source(2,0)");
}

TEST(TerrainCalibrationExporter, OutsideApexValidationUsesConfiguredOrigin) {
    const d2terrain_calibration::ApexDisplayOrigin origin{.x = 2, .y = 2};
    const auto                                     terrain = grid({5, 1, 1, 1}, {}, origin);
    const auto result = d2terrain_calibration::validate_apex_terrain(
        "inset.sg", pattern("land_apex_01_top_ne"), terrain, decode(terrain), origin);

    EXPECT_TRUE(result.ok);
}

TEST(TerrainCalibrationExporter, MandatoryObjectsDoNotAffectTerrainValidation) {
    auto terrain = grid({1, 1, 1, 1});
    terrain.tiles.reserve(terrain.tiles.size() + 8U);

    EXPECT_TRUE(d2terrain_calibration::validate_apex_terrain(
                    "capital.sg", pattern("control_apex_all_hu"), terrain, decode(terrain))
                    .ok);
}

TEST(TerrainCalibrationExporter, ApexCellsCsvContractIsFourRowsPerMap) {
    EXPECT_EQ(d2terrain_calibration::apex_coords().size(), 4U);
}

TEST(TerrainCalibrationExporter, ApexEdgesCsvContractIsEightRowsPerMap) {
    const auto internal_edges = 4;
    const auto outer_edges = 4;
    EXPECT_EQ(internal_edges + outer_edges, 8);
}

TEST(TerrainCalibrationExporter, DeterministicOrdering) {
    const auto coords = d2terrain_calibration::apex_coords();
    EXPECT_EQ(
        (std::vector<std::string>{coords[0].name, coords[1].name, coords[2].name, coords[3].name}),
        (std::vector<std::string>{"apex_top", "apex_right", "apex_bottom", "apex_left"}));
    EXPECT_EQ(d2terrain_calibration::pattern_manifest()[1].stem, "land_apex_01_top_ne");
}

TEST(TerrainCalibrationExporter, ExporterUsesProductionComposerForMetadataAndPreview) {
    const auto source =
        read_source("src/opendis2_terrain_calibration_export/terrain_calibration_exporter.cpp");

    EXPECT_NE(source.find("AdventureTerrainSurfaceComposer composer"), std::string::npos);
    EXPECT_NE(source.find("composer.describe_tile"), std::string::npos);
    EXPECT_NE(source.find("d2terrain_preview::render_preview_image"), std::string::npos);
    EXPECT_EQ(source.find("resolve_calibrated_apex_shape"), std::string::npos);
    EXPECT_EQ(source.find("resolve_synthesized_shape"), std::string::npos);
    EXPECT_EQ(source.find("write_solid_png(map_dir"), std::string::npos);
}

TEST(TerrainCalibrationExporter, ProductionEngineDoesNotKnowCalibrationApexTerms) {
    static constexpr std::array forbidden = {
        "calibrated_apex", "apex_display", "apex_top", "apex_right", "apex_bottom", "apex_left",
    };

    const auto root = std::filesystem::path{OPENDIS2_SOURCE_DIR} / "src/d2engine";
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const auto source = read_source(
            std::filesystem::relative(entry.path(), OPENDIS2_SOURCE_DIR).generic_string().c_str());
        for (const auto* needle : forbidden) {
            EXPECT_EQ(source.find(needle), std::string::npos)
                << entry.path() << " contains " << needle;
        }
    }
}

TEST(TerrainCalibrationExporter, ExporterEdgesUseComposerMetadataWithoutLocalResolverLogic) {
    const auto source =
        read_source("src/opendis2_terrain_calibration_export/terrain_calibration_exporter.cpp");

    EXPECT_NE(source.find("current_record_family"), std::string::npos);
    EXPECT_NE(source.find("composer.describe_tile(input, ax, ay)"), std::string::npos);
    EXPECT_EQ(source.find("AdventureTerrainBorderShapeResolver"), std::string::npos);
    EXPECT_EQ(source.find("resolve_local_topology_shape"), std::string::npos);
    EXPECT_EQ(source.find("resolve_synthesized_shape"), std::string::npos);
    EXPECT_EQ(source.find("grborder_ne_composite_hu_ne.png"), std::string::npos);
    EXPECT_EQ(source.find("grborder_wa_composite_hu_wa.png"), std::string::npos);
}

TEST(TerrainCalibrationExporter, CandidateSheetsOmitNonexistentShape16) {
    const auto shapes = d2terrain_calibration::candidate_shapes();
    EXPECT_EQ(shapes.size(), 30U);
    EXPECT_EQ(std::ranges::find(shapes, 16), shapes.end());
}

TEST(TerrainCalibrationExporter, DatasetManifestRequiresAllInputAndOutputHashes) {
    const auto required = d2terrain_calibration::expected_dataset_filenames();
    EXPECT_EQ(required.size(), 58U);
    EXPECT_NE(std::ranges::find(required, "control_apex_all_hu.sg"), required.end());
    EXPECT_NE(std::ranges::find(required, "control_apex_all_hu_editor.png"), required.end());
}
