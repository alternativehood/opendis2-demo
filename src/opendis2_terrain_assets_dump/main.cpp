#include <d2buildinfo/build_info.hpp>
#include <d2engine/assets/ff_asset_store.hpp>
#include <d2adventure_render/terrain/terrain_asset_catalog.hpp>
#include <d2log/log.hpp>

#include <CLI/CLI.hpp>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <map>
#include <string>

namespace {

struct FamilyStats {
    int count = 0;
    int animated = 0;
    int still = 0;
    int min_w = std::numeric_limits<int>::max();
    int min_h = std::numeric_limits<int>::max();
    int max_w = 0;
    int max_h = 0;
};

void add_size(FamilyStats& stats, int width, int height, bool animated) {
    ++stats.count;
    if (animated) {
        ++stats.animated;
    } else {
        ++stats.still;
    }
    stats.min_w = std::min(stats.min_w, width);
    stats.min_h = std::min(stats.min_h, height);
    stats.max_w = std::max(stats.max_w, width);
    stats.max_h = std::max(stats.max_h, height);
}

void print_size_range(const FamilyStats& stats) {
    const int min_w = stats.count == 0 ? 0 : stats.min_w;
    const int min_h = stats.count == 0 ? 0 : stats.min_h;
    std::cout << " min_size=" << min_w << "x" << min_h << " max_size=" << stats.max_w << "x"
              << stats.max_h;
}

int validate_catalog(const d2engine::TerrainAssetCatalog& catalog) {
    if (catalog.ground_textures.empty()) {
        std::cout << "Ground.ff contains zero ground textures\n";
        return EXIT_FAILURE;
    }
    if (catalog.border_assets.empty()) {
        std::cout << "GrBorder.ff contains zero border assets\n";
        return EXIT_FAILURE;
    }
    if (catalog.terrain_overlays.empty()) {
        std::cout << "IsoTerrn.ff contains zero terrain overlay assets\n";
        return EXIT_FAILURE;
    }
    const auto has_static = [&](std::string_view container) {
        return std::ranges::any_of(catalog.static_assets, [&](const auto& asset) {
            return asset.container_path == container;
        });
    };
    if (!has_static("Imgs/IsoStill.ff")) {
        std::cout << "IsoStill.ff contains zero static assets\n";
        return EXIT_FAILURE;
    }
    if (!has_static("Imgs/IsoCmon.ff")) {
        std::cout << "IsoCmon.ff contains zero static assets\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

void print_ground(const d2engine::TerrainAssetCatalog& catalog) {
    std::map<std::string, int> counts;
    for (const auto& asset : catalog.ground_textures) {
        ++counts[asset.terrain_code];
    }

    std::cout << "Ground textures:\n";
    std::cout << "total count=" << catalog.ground_textures.size() << "\n";
    for (const auto& [code, count] : counts) {
        std::cout << code << " count=" << count << "\n";
    }
    for (const auto& asset : catalog.ground_textures) {
        std::cout << asset.terrain_code << " variant=" << asset.variant
                  << " record=" << asset.container_path << "/" << asset.record_name
                  << " size=" << asset.width << "x" << asset.height << "\n";
    }
}

void print_borders(const d2engine::TerrainAssetCatalog& catalog) {
    std::map<std::string, int> counts;
    for (const auto& asset : catalog.border_assets) {
        ++counts[asset.family];
    }

    std::cout << "\nBorder assets:\n";
    std::cout << "total count=" << catalog.border_assets.size() << "\n";
    for (const auto& [family, count] : counts) {
        std::cout << family << " count=" << count << "\n";
    }
    for (const auto& asset : catalog.border_assets) {
        std::cout << asset.family << " shape=" << asset.shape << " variant=" << asset.variant
                  << " record=" << asset.container_path << "/" << asset.record_name
                  << " size=" << asset.width << "x" << asset.height << "\n";
    }
}

void print_overlays(const d2engine::TerrainAssetCatalog& catalog, bool verbose) {
    std::map<std::string, FamilyStats> counts;
    for (const auto& asset : catalog.terrain_overlays) {
        add_size(counts[asset.family], asset.width, asset.height, asset.animated);
    }

    std::cout << "\nIsoTerrn overlays:\n";
    std::cout << "total count=" << catalog.terrain_overlays.size() << "\n";
    for (const auto& [family, stats] : counts) {
        std::cout << family << " count=" << stats.count << " animated=" << stats.animated
                  << " still=" << stats.still;
        print_size_range(stats);
        std::cout << "\n";
    }
    if (!verbose) {
        return;
    }
    for (const auto& asset : catalog.terrain_overlays) {
        std::cout << asset.family << " logical=" << asset.logical_name
                  << " record=" << asset.container_path
                  << " animated=" << (asset.animated ? "true" : "false")
                  << " frames=" << asset.frame_count << " size=" << asset.width << "x"
                  << asset.height << "\n";
    }
}

void print_static(const d2engine::TerrainAssetCatalog& catalog, bool verbose) {
    std::map<std::string, int>                                 totals;
    std::map<std::pair<std::string, std::string>, FamilyStats> counts;
    for (const auto& asset : catalog.static_assets) {
        ++totals[asset.container_path];
        add_size(counts[{asset.container_path, asset.family}], asset.width, asset.height,
                 asset.animated);
    }

    std::cout << "\nIsoStill/IsoCmon static assets:\n";
    for (const auto& [container, count] : totals) {
        std::cout << "container=" << container << " total count=" << count << "\n";
    }
    for (const auto& [key, stats] : counts) {
        std::cout << "container=" << key.first << " " << key.second << " count=" << stats.count
                  << " animated=" << stats.animated << " still=" << stats.still;
        print_size_range(stats);
        std::cout << "\n";
    }
    if (!verbose) {
        return;
    }
    for (const auto& asset : catalog.static_assets) {
        std::cout << "container=" << asset.container_path << " " << asset.family
                  << " logical=" << asset.logical_name
                  << " animated=" << (asset.animated ? "true" : "false")
                  << " frames=" << asset.frame_count << " size=" << asset.width << "x"
                  << asset.height << "\n";
    }
}

} // namespace

// NOLINTNEXTLINE(bugprone-exception-escape)
int main(int argc, char* argv[]) {
    CLI::App app{"opendis2-dev-terrain-assets-dump"};
    app.set_version_flag("--version", d2buildinfo::format_build_version());
    std::string game_root;
    bool        verbose = false;
    app.add_option("--game-root", game_root, "Path to Disciples II game root")->required();
    app.add_flag("--verbose", verbose, "Print one line per OPT-indexed asset");

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        return app.exit(e);
    }

    d2log::init({.level = "off"});
    try {
        d2engine::FfAssetStore const               store{std::filesystem::path(game_root)};
        d2engine::TerrainAssetCatalogBuilder const builder;
        const auto                                 catalog = builder.build_full(store);
        if (const int status = validate_catalog(catalog); status != EXIT_SUCCESS) {
            return status;
        }
        print_ground(catalog);
        print_borders(catalog);
        print_overlays(catalog, verbose);
        print_static(catalog, verbose);
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cout << e.what() << "\n";
        return EXIT_FAILURE;
    }
}
