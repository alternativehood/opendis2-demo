#include "commands_extract_atlas.hpp"
#include "d2res/atlas_packer.hpp"
#include "d2res/image_decoder.hpp"
#include "d2res/mqdb.hpp"
#include "d2res/opt_maps.hpp"
#include "d2res/rgba_buffer.hpp"
#include <nlohmann/json.hpp>
#include <lodepng.h>
#include "d2res/platform/glob.hpp"
#include <d2log/log.hpp>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {
auto kLog = d2log::get("d2.app"); // NOLINT(cert-err58-cpp)
} // namespace

static bool glob_match_atlas(const std::string& pattern, const std::string& name) {
    if (pattern.empty())
        return true;
    return d2res::platform::glob_match(pattern, name);
}

int cmd_extract_atlas(const std::string& container_path, const std::string& out_dir,
                      const std::string& pattern, bool all, int max_size) {
    d2res::MqdbContainer container;
    try {
        container = d2res::MqdbContainer::open(fs::path(container_path));
    } catch (const std::exception& e) {
        kLog->error("cannot_open_container error={}", e.what());
        return 1;
    }

    d2res::OptMaps maps;
    try {
        maps = d2res::parse_opt_maps(container);
    } catch (const std::exception& e) {
        kLog->error("opt_parse_failed error={}", e.what());
        return 1;
    }

    return cmd_extract_atlas_from_container(container, maps, out_dir, pattern, all, max_size);
}

int cmd_extract_atlas_from_container(const d2res::MqdbContainer& container,
                                     const d2res::OptMaps& maps, const std::string& out_dir,
                                     const std::string& pattern, bool all, int max_size) {
    d2res::ImageResourceDecoder const decoder(container, maps);

    std::error_code ec;
    fs::create_directories(out_dir, ec);
    if (ec) {
        kLog->error("cannot_create_output_dir error={}", ec.message());
        return 1;
    }

    const auto                                             image_names = decoder.list_images();
    std::vector<std::pair<std::string, d2res::RgbaBuffer>> sprites;
    sprites.reserve(image_names.size());
    std::vector<std::string> warnings;

    std::size_t total_matching = 0;
    for (const auto& name : image_names) {
        if (!all && !glob_match_atlas(pattern, name))
            continue;
        ++total_matching;
    }

    std::size_t processed = 0;
    for (const auto& name : image_names) {
        if (!all && !glob_match_atlas(pattern, name))
            continue;

        try {
            auto              img = decoder.decode_image(name);
            d2res::RgbaBuffer buf;
            buf.width = img.width;
            buf.height = img.height;
            buf.rgba = std::move(img.rgba);
            sprites.emplace_back(name, std::move(buf));
        } catch (const std::exception& e) {
            warnings.push_back("decode failed for " + name + ": " + e.what());
        }

        ++processed;
        if (processed % 500 == 0 || processed == total_matching) {
            kLog->debug("decode_progress decoded={} total={}", processed, total_matching);
        }
    }

    if (sprites.empty()) {
        kLog->error("no_images_matched");
        return 1;
    }

    // Pack
    const d2res::AtlasResult result = d2res::AtlasPacker::pack(sprites, max_size);

    // Write atlas sheets
    for (std::size_t i = 0; i < result.sheets.size(); ++i) {
        std::ostringstream name;
        name << "atlas_" << std::setfill('0') << std::setw(3) << i << ".png";
        const fs::path png_path = fs::path(out_dir) / name.str();

        const auto&          canvas = result.sheets[i].canvas;
        std::vector<uint8_t> png_bytes;
        const unsigned err = lodepng::encode(png_bytes, canvas.rgba, canvas.width, canvas.height);
        if (err != 0) {
            kLog->error("png_encode_failed sheet={} error={}", i, lodepng_error_text(err));
            return 1;
        }
        std::ofstream f(png_path, std::ios::binary);
        if (!f) {
            kLog->error("cannot_write_png path={}", png_path.string());
            return 1;
        }
        f.write(reinterpret_cast<const char*>(png_bytes.data()),
                static_cast<std::streamsize>(png_bytes.size()));
    }

    // Write atlas.json
    {
        const json    manifest = result.to_json(container.path().string(), max_size);
        std::ofstream f(fs::path(out_dir) / "atlas.json");
        f << manifest.dump(2);
    }

    // Write warnings.txt if needed
    for (const auto& s : result.skipped)
        warnings.push_back("skipped (oversized): " + s);
    for (const auto& w : maps.warnings)
        warnings.push_back(w);
    if (!warnings.empty()) {
        std::ofstream f(fs::path(out_dir) / "warnings.txt");
        for (const auto& w : warnings)
            f << w << '\n';
    }

    kLog->info("extract_atlas sprites={} sheets={} skipped={} out={}", result.entries.size(),
               result.sheets.size(), result.skipped.size(), out_dir);
    return 0;
}
