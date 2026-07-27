#include "ruin_asset_catalog_builder.hpp"

#include "ff_asset_store.hpp"

#include <d2adventure_render/terrain/ruin_asset_catalog.hpp>

#include <array>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

namespace d2engine {

namespace {

constexpr std::string_view kContainerPath = "Imgs/IsoCmon.ff";

struct RuinMapping {
    int              image;
    std::string_view logical_sprite;
};

[[noreturn]] void throw_ruin_asset_error(std::string_view kind, std::string_view logical) {
    throw std::runtime_error("ruin_asset_catalog_" + std::string(kind) +
                             " logical=" + std::string(logical));
}

[[nodiscard]] ::d2engine::adventure_render::StaticRuinVisual
build_ruin_visual(FfAssetStore& store, std::string_view logical_sprite) {
    ::d2engine::adventure_render::StaticRuinVisual visual;
    visual.container_path = std::string(kContainerPath);
    visual.logical_sprite = std::string(logical_sprite);

    FfAssetStore::SpriteMetadata meta{};
    try {
        meta = store.sprite_metadata(kContainerPath, logical_sprite);
    } catch (const std::exception&) {
        throw_ruin_asset_error("missing_sprite", logical_sprite);
    }

    if (meta.canvas_width <= 0 || meta.canvas_height <= 0) {
        throw_ruin_asset_error("invalid_dimensions", logical_sprite);
    }
    if (!meta.content_bounds.valid()) {
        throw_ruin_asset_error("invalid_content_bounds", logical_sprite);
    }

    visual.canvas_foot_x = meta.canvas_foot_x;
    visual.canvas_foot_y = meta.canvas_foot_y;
    visual.canvas_width = meta.canvas_width;
    visual.canvas_height = meta.canvas_height;
    visual.content_bounds = meta.content_bounds;
    return visual;
}

template <std::size_t N>
std::size_t populate_mapping(std::array<std::array<std::optional<adventure_render::RuinVisual>,
                                                   adventure_render::RuinAssetCatalog::kImageCount>,
                                        2>&                    visuals,
                             std::size_t                       placement_index,
                             const std::array<RuinMapping, N>& mappings, FfAssetStore& store) {
    for (const auto& mapping : mappings) {
        visuals[placement_index][static_cast<std::size_t>(mapping.image)] =
            build_ruin_visual(store, mapping.logical_sprite);
    }
    return mappings.size();
}

} // namespace

::d2engine::adventure_render::RuinAssetCatalog build_ruin_asset_catalog(FfAssetStore& store) {
    constexpr std::array<RuinMapping, 9> kLandMappings{{
        {0, "G000RU0000000"},
        {1, "G000RU0000001"},
        {2, "G000RU0000002"},
        {3, "G000RU0000003"},
        {4, "G000RU0000004"},
        {5, "G000RU0000005"},
        {6, "G000RU0000006"},
        {7, "G000RU0000007"},
        {8, "G000RU0000008"},
    }};
    constexpr std::array<RuinMapping, 9> kWaterMappings{{
        {0, "G000RU0000100"},
        {1, "G000RU0000101"},
        {2, "G000RU0000102"},
        {3, "G000RU0000103"},
        {4, "G000RU0000104"},
        {5, "G000RU0000105"},
        {6, "G000RU0000106"},
        {7, "G000RU0000107"},
        {8, "G000RU0000108"},
    }};

    adventure_render::RuinAssetCatalog catalog;
    const auto land_count = populate_mapping(catalog.visuals, 0u, kLandMappings, store);
    const auto water_count = populate_mapping(catalog.visuals, 1u, kWaterMappings, store);
    if (land_count != kLandMappings.size() || water_count != kWaterMappings.size()) {
        throw std::runtime_error(
            "ruin_asset_catalog_mapping_count_mismatch land=" + std::to_string(land_count) +
            " water=" + std::to_string(water_count));
    }
    return catalog;
}

} // namespace d2engine
