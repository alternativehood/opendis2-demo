#pragma once

#include <d2engine/assets/adventure_terrain_asset_resolver.hpp>
#include <d2adventure_render/terrain/adventure_terrain_surface.hpp>
#include <d2runtime/AdventureTerrain.hpp>
#include <d2runtime/AdventureWorldState.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

namespace d2terrain_calibration {

struct ApexCoord {
    std::string name;
    int         display_x = 0;
    int         display_y = 0;
    int         source_x = 0;
    int         source_y = 0;
};

struct ApexDisplayOrigin {
    int x = 0;
    int y = 0;
};

struct PatternEntry {
    std::string                stem;
    std::string                dataset_group;
    int                        pattern_index = 0;
    std::array<std::string, 4> expected_materials;
};

struct DatasetPreflightResult {
    bool                     ok = true;
    std::vector<std::string> missing;
    std::vector<std::string> unexpected;
};

struct ValidationFailure {
    std::string   scenario_filename;
    std::string   location;
    std::string   expected_material;
    std::string   actual_material;
    std::uint32_t raw_value = 0;
    std::uint8_t  low_byte = 0;
};

struct ValidationResult {
    bool                           ok = true;
    std::vector<ValidationFailure> failures;
};

struct ExportConfig {
    std::filesystem::path maps_dir;
    std::filesystem::path game_root;
    std::filesystem::path out_dir;
    ApexDisplayOrigin     apex_display_origin;
};

[[nodiscard]] std::array<ApexCoord, 4>         apex_coords(ApexDisplayOrigin origin = {});
[[nodiscard]] const std::vector<PatternEntry>& pattern_manifest();
[[nodiscard]] std::vector<std::string>         expected_sg_filenames();
[[nodiscard]] std::vector<std::string>         expected_editor_png_filenames();
[[nodiscard]] std::vector<std::string>         expected_dataset_filenames();
[[nodiscard]] std::vector<int>                 candidate_shapes();
[[nodiscard]] DatasetPreflightResult preflight_dataset(const std::filesystem::path& maps_dir);
[[nodiscard]] d2engine::AdventureTerrainSurfaceInput make_display_surface_input(
    int source_width, int source_height,
    const std::vector<d2runtime::AdventureTerrainTileDescriptor>& descriptors,
    const std::vector<d2engine::ResolvedAdventureTerrainTile>&    resolved_tiles);
[[nodiscard]] ValidationResult
validate_apex_terrain(std::string_view scenario_filename, const PatternEntry& pattern,
                      const d2runtime::AdventureTerrainGrid&                        terrain,
                      const std::vector<d2runtime::AdventureTerrainTileDescriptor>& descriptors,
                      ApexDisplayOrigin                                             origin = {});
int run_export(const ExportConfig& config, std::ostream& diagnostics);

} // namespace d2terrain_calibration
