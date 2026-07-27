#include "terrain_preview_image.hpp"

#include <d2buildinfo/build_info.hpp>
#include <d2engine/assets/adventure_terrain_asset_resolver.hpp>
#include <d2adventure_render/terrain/adventure_terrain_surface.hpp>
#include <d2engine/assets/ff_asset_store.hpp>
#include <d2adventure_render/terrain/terrain_asset_catalog.hpp>
#include <d2log/log.hpp>
#include <d2runtime/AdventureTerrainDecoder.hpp>
#include <d2runtime/AdventureWorldBuilder.hpp>
#include <d2scenario/SgParser.hpp>

#include <CLI/CLI.hpp>
#include <lodepng.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Config {
    std::string game_root;
    std::string scenario;
    std::string output;
    int         max_size = 4096;
    bool        benchmark_png = false;
};

std::vector<std::uint8_t> read_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("cannot read scenario: " + path.string());
    }
    const auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
    file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size));
    return data;
}

struct PhaseTimer {
    std::chrono::steady_clock::time_point start;
    PhaseTimer() : start(std::chrono::steady_clock::now()) {}
    double elapsed_ms() const {
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
            .count();
    }
};

// Fast lossless PNG config for dev preview (prioritizes speed over compression).
void setup_fast_lodepng(lodepng::State& state) {
    state.encoder.filter_strategy = LFS_ZERO;
    state.encoder.zlibsettings.windowsize = 256;
}

void write_png(const std::filesystem::path& path, const d2terrain_preview::PreviewImage& image,
               double* out_encode_ms, double* out_write_ms, std::size_t* out_bytes) {
    PhaseTimer     encode_timer;
    lodepng::State state;
    setup_fast_lodepng(state);
    std::vector<std::uint8_t> png;
    const auto err = lodepng::encode(png, image.rgba, static_cast<unsigned>(image.output_width),
                                     static_cast<unsigned>(image.output_height), state);
    if (err != 0) {
        throw std::runtime_error(std::string("PNG encode failed: ") + lodepng_error_text(err));
    }
    *out_encode_ms = encode_timer.elapsed_ms();
    *out_bytes = png.size();

    PhaseTimer write_timer;
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("cannot write PNG: " + path.string());
    }
    out.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
    *out_write_ms = write_timer.elapsed_ms();
}

// Benchmark: encode same image with multiple configs, report timings.
void benchmark_png_configs(const d2terrain_preview::PreviewImage& image) {
    const unsigned w = static_cast<unsigned>(image.output_width);
    const unsigned h = static_cast<unsigned>(image.output_height);

    // Config A: default lodepng settings
    {
        PhaseTimer                t;
        std::vector<std::uint8_t> png;
        lodepng::encode(png, image.rgba, w, h);
        std::cerr << "preview_png_benchmark config=default encode_ms=" << t.elapsed_ms()
                  << " bytes=" << png.size() << "\n";
    }

    // Config B: LFS_ZERO + windowsize=256
    {
        PhaseTimer     t;
        lodepng::State s;
        setup_fast_lodepng(s);
        std::vector<std::uint8_t> png;
        const auto                err = lodepng::encode(png, image.rgba, w, h, s);
        if (err == 0) {
            std::cerr << "preview_png_benchmark config=fast_lfs_zero_ws256 encode_ms="
                      << t.elapsed_ms() << " bytes=" << png.size() << "\n";
        }
    }

    // Config C: stored-only (btype=0 via LFS_ZERO + no zlib compression)
    {
        PhaseTimer     t;
        lodepng::State s;
        setup_fast_lodepng(s);
        s.encoder.zlibsettings.btype = 0;
        std::vector<std::uint8_t> png;
        const auto                err = lodepng::encode(png, image.rgba, w, h, s);
        if (err == 0) {
            std::cerr << "preview_png_benchmark config=stored_btype0 encode_ms=" << t.elapsed_ms()
                      << " bytes=" << png.size() << "\n";
        }
    }
}

} // namespace

int main(int argc, char* argv[]) {
    Config   config;
    CLI::App app{"opendis2-dev-terrain-preview"};
    app.set_version_flag("--version", d2buildinfo::format_build_version());
    app.add_option("--game-root", config.game_root, "Disciples II game root")->required();
    app.add_option("--scenario", config.scenario, "Scenario .sg path")->required();
    app.add_option("--output", config.output, "Output preview PNG")->required();
    app.add_option("--max-size", config.max_size, "Maximum output width or height")
        ->check(CLI::PositiveNumber);
    app.add_flag("--benchmark-png", config.benchmark_png,
                 "Benchmark multiple PNG encode configs (in addition to normal encode)");

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        return app.exit(e);
    }

    try {
        d2log::init({.level = std::getenv("D2_LOG_LEVEL") == nullptr ? "off" : ""});

        PhaseTimer total_timer;

        PhaseTimer           parse_timer;
        const auto           bytes = read_file(config.scenario);
        d2scenario::SgParser parser(bytes);
        const auto           parsed = parser.parse();
        const double         parse_ms = parse_timer.elapsed_ms();

        PhaseTimer   build_timer;
        const auto   built = d2runtime::AdventureWorldBuilder{}.build(parsed.scenario);
        const double build_ms = build_timer.elapsed_ms();

        PhaseTimer                           catalog_timer;
        d2engine::FfAssetStore               store(config.game_root);
        d2engine::TerrainAssetCatalogBuilder catalog_builder;
        const auto                           catalog = catalog_builder.build(store);
        const double                         catalog_ms = catalog_timer.elapsed_ms();

        PhaseTimer                                  decode_timer;
        const d2runtime::AdventureTerrainDecoder    tile_decoder;
        const d2runtime::AdventureTerrainMapDecoder map_decoder(tile_decoder);
        const auto descriptors = map_decoder.decode_grid(built.world.terrain);
        const auto resolved =
            d2engine::AdventureTerrainAssetResolver(catalog).resolve_all(descriptors);
        const double decode_ms = decode_timer.elapsed_ms();

        const d2engine::AdventureTerrainSurfaceInput surface_input{
            .map_width = built.world.terrain.width,
            .map_height = built.world.terrain.height,
            .descriptors = descriptors,
            .resolved_tiles = resolved,
        };
        const d2engine::AdventureTerrainSurfaceComposer composer(store, catalog);

        // Phase: prepare terrain
        PhaseTimer   prepare_timer;
        const auto   prepared = composer.prepare_full_map(surface_input);
        const double prepare_ms = prepare_timer.elapsed_ms();

        // Phase: render terrain
        PhaseTimer   render_timer;
        const auto   terrain_surface = composer.render_prepared_full_map(prepared);
        const double render_ms = render_timer.elapsed_ms();

        // Phase: scale preview
        PhaseTimer scale_timer;
        const auto image =
            d2terrain_preview::preview_from_surface(terrain_surface, config.max_size);
        const double scale_ms = scale_timer.elapsed_ms();

        // Phase: encode PNG
        double      encode_ms = 0.0;
        double      write_ms = 0.0;
        std::size_t png_bytes = 0;
        write_png(config.output, image, &encode_ms, &write_ms, &png_bytes);
        std::cout << std::filesystem::absolute(config.output).string() << '\n';

        if (config.benchmark_png)
            benchmark_png_configs(image);

        std::cerr << "preview_timings parse_ms=" << parse_ms << " build_ms=" << build_ms
                  << " catalog_ms=" << catalog_ms << " decode_ms=" << decode_ms
                  << " prepare_ms=" << prepare_ms << " render_ms=" << render_ms
                  << " scale_ms=" << scale_ms << " encode_ms=" << encode_ms
                  << " write_ms=" << write_ms << " total_ms=" << total_timer.elapsed_ms()
                  << " png_bytes=" << png_bytes << "\n";

        // Explicit full asset access dump at DEBUG level (dev diagnostic).
        store.dump_access_report(d2engine::FfAssetStore::FfAccessDumpMode::Full);

        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        d2log::init({.level = "error"});
        d2log::get("d2.terrain_preview")->error("fatal: {}", e.what());
        return EXIT_FAILURE;
    }
}
