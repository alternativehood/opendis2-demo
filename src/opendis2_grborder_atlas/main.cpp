#include <d2buildinfo/build_info.hpp>
#include <d2engine/assets/ff_asset_store.hpp>
#include <d2log/log.hpp>
#include <d2res/rgba_buffer.hpp>

#include <CLI/CLI.hpp>
#include <lodepng.h>

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Pixel {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;
};

std::string record(std::string_view family, int shape) {
    char buffer[32] = {};
    std::snprintf(buffer, sizeof(buffer), "%.*s_%02d_00.PNG", static_cast<int>(family.size()),
                  family.data(), shape);
    return buffer;
}

Pixel sample(const d2res::RgbaBuffer& image, int x, int y) {
    if (x < 0 || y < 0 || x >= static_cast<int>(image.width) ||
        y >= static_cast<int>(image.height) || image.rgba.empty()) {
        return {};
    }
    const auto* p = image.rgba.data() +
                    (static_cast<std::size_t>(y) * image.width + static_cast<std::size_t>(x)) * 4U;
    return {p[0], p[1], p[2], p[3]};
}

void put(std::vector<std::uint8_t>& rgba, int width, int x, int y, Pixel p) {
    const auto i = (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                    static_cast<std::size_t>(x)) *
                   4U;
    rgba[i] = p.r;
    rgba[i + 1] = p.g;
    rgba[i + 2] = p.b;
    rgba[i + 3] = p.a;
}

void write_png(const std::filesystem::path& path, const std::vector<std::uint8_t>& rgba, int width,
               int height) {
    std::vector<std::uint8_t> png;
    const auto                err =
        lodepng::encode(png, rgba, static_cast<unsigned>(width), static_cast<unsigned>(height));
    if (err != 0) {
        throw std::runtime_error(lodepng_error_text(err));
    }
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
}

void write_family_atlas(const d2engine::FfAssetStore& store, const std::filesystem::path& out,
                        std::string_view family, bool composite) {
    constexpr int             cell_w = 64;
    constexpr int             cell_h = 32;
    constexpr int             width = cell_w;
    constexpr int             height = cell_h * 30;
    std::vector<std::uint8_t> rgba(static_cast<std::size_t>(width) * height * 4U, 255);
    int                       row = 0;
    for (int shape = 1; shape <= 31; ++shape) {
        if (shape == 16) {
            continue;
        }
        const auto image_buf = store.copy_raw_png("Imgs/GrBorder.ff", record(family, shape));
        const auto image = image_buf.has_value() ? *image_buf : d2res::RgbaBuffer{};
        for (int y = 0; y < cell_h; ++y) {
            for (int x = 0; x < cell_w; ++x) {
                const auto mask = sample(image, x, y);
                const bool magenta = mask.r > 240 && mask.g < 16 && mask.b > 240;
                Pixel      out_px = mask;
                if (composite) {
                    const double luma =
                        magenta || mask.a == 0
                            ? 0.0
                            : (static_cast<double>(mask.r) + mask.g + mask.b) / (3.0 * 255.0);
                    out_px = {
                        static_cast<std::uint8_t>(std::clamp(40.0 * (1.0 - luma), 0.0, 255.0)),
                        static_cast<std::uint8_t>(std::clamp(160.0 * (1.0 - luma), 0.0, 255.0)),
                        static_cast<std::uint8_t>(std::clamp(220.0 * luma, 0.0, 255.0)), 255};
                }
                put(rgba, width, x, row * cell_h + y, out_px);
            }
        }
        ++row;
    }
    write_png(out, rgba, width, height);
}

} // namespace

int main(int argc, char** argv) { // NOLINT(bugprone-exception-escape)
    try {
        std::string game_root;
        std::string out_dir;
        std::string family = "both";
        std::string material_a = "HU";
        std::string material_b = "WA";
        CLI::App    app{"opendis2-dev-grborder-atlas"};
        app.set_version_flag("--version", d2buildinfo::format_build_version());
        app.add_option("--game-root", game_root)->required();
        app.add_option("--out-dir", out_dir)->required();
        app.add_option("--family", family)->check(CLI::IsMember({"NE", "WA", "both"}));
        app.add_option("--material-a", material_a);
        app.add_option("--material-b", material_b);
        app.parse(argc, argv);
        d2log::init({});
        std::filesystem::create_directories(out_dir);
        const d2engine::FfAssetStore store(game_root);
        const auto                   emit = [&](std::string_view fam) {
            write_family_atlas(store,
                               std::filesystem::path(out_dir) /
                                   ("grborder_" + std::string(fam) + "_raw_atlas.png"),
                               fam, false);
            write_family_atlas(store,
                               std::filesystem::path(out_dir) /
                                   ("grborder_" + std::string(fam) + "_composite_A-" + material_a +
                                    "_B-" + material_b + ".png"),
                               fam, true);
        };
        if (family == "both" || family == "NE") {
            emit("NE");
        }
        if (family == "both" || family == "WA") {
            emit("WA");
        }
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        d2log::get("d2.grborder_atlas")->error("fatal: {}", e.what());
        return EXIT_FAILURE;
    }
}
