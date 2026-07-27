#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace d2engine {

class FfAssetStore;

struct TerrainGroundTextureAsset {
    std::string terrain_code;
    int         variant = 0;
    std::string container_path;
    std::string record_name;
    int         width = 0;
    int         height = 0;
};

struct TerrainBorderAsset {
    std::string family;
    int         shape = 0;
    int         variant = 0;
    std::string container_path;
    std::string record_name;
    int         width = 0;
    int         height = 0;
};

struct IsoTerrainOverlayAsset {
    std::string family;
    std::string logical_name;
    std::string container_path;
    int         width = 0;
    int         height = 0;
    bool        animated = false;
    int         frame_count = 1;
};

struct IsoStaticAsset {
    std::string family;
    std::string logical_name;
    std::string container_path;
    int         width = 0;
    int         height = 0;
    bool        animated = false;
    int         frame_count = 1;
};

struct TerrainAssetCatalog {
    std::vector<TerrainGroundTextureAsset> ground_textures;
    std::vector<TerrainBorderAsset>        border_assets;
    std::vector<IsoTerrainOverlayAsset>    terrain_overlays;
    std::vector<IsoStaticAsset>            static_assets;

    // Index: terrain_code -> sorted existing variant numbers.
    // Built from ground_textures during catalog construction.
    std::map<std::string, std::vector<int>> ground_variant_index;

    // Index: (family, shape) -> sorted existing variant numbers.
    // Built from border_assets during catalog construction.
    std::map<std::pair<std::string, int>, std::vector<int>> border_variant_index;

    [[nodiscard]] std::optional<TerrainBorderAsset> find_border_asset(std::string_view family,
                                                                      int shape, int variant) const;
};

struct ParsedGroundTextureName {
    std::string terrain_code;
    int         variant = 0;
};

struct ParsedBorderAssetName {
    std::string family;
    int         shape = 0;
    int         variant = 0;
};

[[nodiscard]] std::optional<ParsedGroundTextureName>
parse_ground_texture_record_name(std::string_view record_name);
[[nodiscard]] std::optional<ParsedBorderAssetName>
parse_border_asset_record_name(std::string_view record_name);
[[nodiscard]] std::optional<std::string> parse_iso_logical_family(std::string_view logical_name);

void sort_terrain_asset_catalog(TerrainAssetCatalog& catalog);

class TerrainAssetCatalogBuilder {
public:
    // Minimal catalog for terrain surface rendering (Ground + GrBorder only).
    // Does NOT open IsoTerrn/IsoStill/IsoCmon and does NOT decode animations.
    [[nodiscard]] TerrainAssetCatalog build_ground_border(const FfAssetStore& store) const;

    // Full catalog including iso overlays and statics. Used by dev dump tools.
    [[nodiscard]] TerrainAssetCatalog build_full(const FfAssetStore& store) const;

    // Default build: ground + border only (fast path for preview/render).
    [[nodiscard]] TerrainAssetCatalog build(const FfAssetStore& store) const;
};

} // namespace d2engine
