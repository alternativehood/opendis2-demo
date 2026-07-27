#include <d2adventure_render/terrain/terrain_asset_catalog.hpp>

#include "ff_asset_store.hpp"
#include "d2res/opt_maps.hpp"

#include <d2log/log.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cctype>
#include <string>
#include <tuple>

namespace d2engine {
namespace {

auto kLog = d2log::get("d2.terrain_catalog"); // NOLINT(cert-err58-cpp)

constexpr std::string_view                kGroundContainer = "Imgs/Ground.ff";
constexpr std::string_view                kBorderContainer = "Imgs/GrBorder.ff";
constexpr std::string_view                kIsoTerrnContainer = "Imgs/IsoTerrn.ff";
constexpr std::array<std::string_view, 2> kStaticContainers = {"Imgs/IsoStill.ff",
                                                               "Imgs/IsoCmon.ff"};

struct ScopedTimer {
    using Clock = std::chrono::steady_clock;
    Clock::time_point start;
    explicit ScopedTimer() : start(Clock::now()) {}
    [[nodiscard]] double elapsed_ms() const {
        return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
    }
};

[[nodiscard]] std::string uppercase(std::string_view value) {
    std::string result(value);
    for (char& ch : result) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return result;
}

[[nodiscard]] bool equals_ignore_case(std::string_view lhs, std::string_view rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        const auto l = static_cast<unsigned char>(lhs[i]);
        const auto r = static_cast<unsigned char>(rhs[i]);
        if (std::tolower(l) != std::tolower(r)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::string resolve_opt_container_path(const FfAssetStore& store,
                                                     std::string_view    canonical_path) {
    for (const auto& container : store.containers()) {
        if (equals_ignore_case(container, canonical_path)) {
            return container;
        }
    }
    return std::string(canonical_path);
}

[[nodiscard]] std::pair<int, int> image_size(const d2res::OptMaps& maps,
                                             std::string_view      logical_name) {
    const std::string key = uppercase(logical_name);
    const auto        block_it = maps.image_map.frame_name_to_block.find(key);
    if (block_it == maps.image_map.frame_name_to_block.end()) {
        return {};
    }
    const auto& block = maps.image_map.blocks[block_it->second];
    for (const auto& frame : block.frames) {
        if (uppercase(frame.name) == key) {
            return {frame.output_width, frame.output_height};
        }
    }
    if (!block.frames.empty()) {
        return {block.frames.front().output_width, block.frames.front().output_height};
    }
    return {};
}

void add_iso_terrain_assets(TerrainAssetCatalog& catalog, const FfAssetStore& store) {
    const auto  lookup_container = resolve_opt_container_path(store, kIsoTerrnContainer);
    const auto  output_container = std::string(kIsoTerrnContainer);
    const auto* maps = store.container_maps(lookup_container);
    if (maps == nullptr) {
        throw std::runtime_error("Required OPT container cannot be opened: " + output_container);
    }

    for (const auto& logical_name : store.sprites_in(lookup_container)) {
        const auto family = parse_iso_logical_family(logical_name);
        if (!family.has_value())
            continue;
        const auto [width, height] = image_size(*maps, logical_name);
        catalog.terrain_overlays.push_back({.family = *family,
                                            .logical_name = logical_name,
                                            .container_path = output_container,
                                            .width = width,
                                            .height = height,
                                            .animated = false,
                                            .frame_count = 1});
    }

    for (const auto& logical_name : store.animations_in(lookup_container)) {
        const auto family = parse_iso_logical_family(logical_name);
        if (!family.has_value())
            continue;
        const auto sequence = store.animation_metadata(lookup_container, logical_name);
        catalog.terrain_overlays.push_back(
            {.family = *family,
             .logical_name = logical_name,
             .container_path = output_container,
             .width = sequence.native_canvas_w,
             .height = sequence.native_canvas_h,
             .animated = true,
             .frame_count = static_cast<int>(std::max<std::size_t>(sequence.frames.size(), 1))});
    }
}

void add_static_assets(TerrainAssetCatalog& catalog, const FfAssetStore& store,
                       std::string_view container_view) {
    const std::string output_container(container_view);
    const auto        lookup_container = resolve_opt_container_path(store, container_view);
    const auto*       maps = store.container_maps(lookup_container);
    if (maps == nullptr) {
        throw std::runtime_error("Required OPT container cannot be opened: " + output_container);
    }

    for (const auto& logical_name : store.sprites_in(lookup_container)) {
        const auto family = parse_iso_logical_family(logical_name);
        if (!family.has_value())
            continue;
        const auto [width, height] = image_size(*maps, logical_name);
        catalog.static_assets.push_back({.family = *family,
                                         .logical_name = logical_name,
                                         .container_path = output_container,
                                         .width = width,
                                         .height = height,
                                         .animated = false,
                                         .frame_count = 1});
    }

    for (const auto& logical_name : store.animations_in(lookup_container)) {
        const auto family = parse_iso_logical_family(logical_name);
        if (!family.has_value())
            continue;
        const auto sequence = store.animation_metadata(lookup_container, logical_name);
        catalog.static_assets.push_back(
            {.family = *family,
             .logical_name = logical_name,
             .container_path = output_container,
             .width = sequence.native_canvas_w,
             .height = sequence.native_canvas_h,
             .animated = true,
             .frame_count = static_cast<int>(std::max<std::size_t>(sequence.frames.size(), 1))});
    }
}

} // namespace

TerrainAssetCatalog
TerrainAssetCatalogBuilder::build_ground_border(const FfAssetStore& store) const {
    ScopedTimer timer;
    kLog->debug("terrain_catalog_ground_border_begin");

    TerrainAssetCatalog catalog;

    for (const auto& record_name : store.record_names(std::string(kGroundContainer))) {
        const auto parsed = parse_ground_texture_record_name(record_name);
        if (!parsed.has_value())
            continue;
        catalog.ground_textures.push_back({.terrain_code = parsed->terrain_code,
                                           .variant = parsed->variant,
                                           .container_path = std::string(kGroundContainer),
                                           .record_name = record_name});
    }

    for (const auto& record_name : store.record_names(std::string(kBorderContainer))) {
        const auto parsed = parse_border_asset_record_name(record_name);
        if (!parsed.has_value())
            continue;
        catalog.border_assets.push_back({.family = parsed->family,
                                         .shape = parsed->shape,
                                         .variant = parsed->variant,
                                         .container_path = std::string(kBorderContainer),
                                         .record_name = record_name});
    }

    sort_terrain_asset_catalog(catalog);

    for (const auto& asset : catalog.ground_textures) {
        catalog.ground_variant_index[asset.terrain_code].push_back(asset.variant);
    }
    for (auto& [code, variants] : catalog.ground_variant_index) {
        std::ranges::sort(variants);
        variants.erase(std::unique(variants.begin(), variants.end()), variants.end());
    }

    for (const auto& asset : catalog.border_assets) {
        catalog.border_variant_index[{asset.family, asset.shape}].push_back(asset.variant);
    }
    for (auto& [key, variants] : catalog.border_variant_index) {
        std::ranges::sort(variants);
        variants.erase(std::unique(variants.begin(), variants.end()), variants.end());
    }

    kLog->debug("terrain_catalog_ground_border_end duration_ms={:.2f}", timer.elapsed_ms());
    return catalog;
}

TerrainAssetCatalog TerrainAssetCatalogBuilder::build_full(const FfAssetStore& store) const {
    ScopedTimer timer;
    kLog->debug("terrain_catalog_full_begin");
    auto catalog = build_ground_border(store);
    add_iso_terrain_assets(catalog, store);
    for (std::string_view container : kStaticContainers) {
        add_static_assets(catalog, store, container);
    }
    sort_terrain_asset_catalog(catalog);
    kLog->debug("terrain_catalog_full_end duration_ms={:.2f}", timer.elapsed_ms());
    return catalog;
}

TerrainAssetCatalog TerrainAssetCatalogBuilder::build(const FfAssetStore& store) const {
    return build_ground_border(store);
}

} // namespace d2engine
