#include "commands_extract_images.hpp"
#include "d2res/image_decoder.hpp"
#include "d2res/mqdb.hpp"
#include "d2res/opt_maps.hpp"
#include "d2res/rgba_buffer.hpp"
#include <nlohmann/json.hpp>
#include <lodepng.h>
#include <d2log/log.hpp>
#include <filesystem>
#include <fstream>
#include "d2res/platform/glob.hpp"
#include <string>
#include <vector>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {
auto kLog = d2log::get("d2.app"); // NOLINT(cert-err58-cpp)
} // namespace

static bool glob_match(const std::string& pattern, const std::string& name) {
    if (pattern.empty())
        return true;
    return d2res::platform::glob_match(pattern, name);
}

int cmd_extract_images(const std::string& container_path, const std::string& out_dir,
                       const std::string& pattern) {
    // Open container
    d2res::MqdbContainer container;
    try {
        container = d2res::MqdbContainer::open(fs::path(container_path));
    } catch (const std::exception& e) {
        kLog->error("cannot_open_container error={}", e.what());
        return 1;
    }

    // Parse OPT maps
    d2res::OptMaps maps;
    try {
        maps = d2res::parse_image_opt_maps(container);
    } catch (const std::exception& e) {
        kLog->error("opt_parse_failed error={}", e.what());
        return 1;
    }

    // Build decoder
    d2res::ImageResourceDecoder const decoder(container, maps);

    // Create output directory
    std::error_code ec;
    fs::create_directories(out_dir, ec);
    if (ec) {
        kLog->error("cannot_create_output_dir error={}", ec.message());
        return 1;
    }

    const auto               image_names = decoder.list_images();
    std::size_t              written = 0;
    std::size_t              failed = 0;
    std::vector<std::string> warnings;
    const std::size_t        total_images = image_names.size();
    std::size_t              processed = 0;

    for (const auto& name : image_names) {
        if (!glob_match(pattern, name))
            continue;
        ++processed;
        if (processed % 100 == 0 || processed == total_images) {
            kLog->debug("decode_progress decoded={} total={}", processed, total_images);
        }

        d2res::DecodedImage img;
        try {
            img = decoder.decode_image(name);
        } catch (const std::exception& e) {
            warnings.push_back("decode failed for " + name + ": " + e.what());
            ++failed;
            continue;
        }

        const std::string safe = d2res::sanitize_filename(name);
        const fs::path    png_path = fs::path(out_dir) / (safe + ".png");
        const fs::path    json_path = fs::path(out_dir) / (safe + ".json");

        // Write PNG via lodepng
        std::vector<uint8_t> png_bytes;
        const unsigned       err = lodepng::encode(png_bytes, img.rgba, img.width, img.height);
        if (err != 0) {
            warnings.push_back("PNG encode failed for " + name + ": " +
                               std::string(lodepng_error_text(err)));
            ++failed;
            continue;
        }

        {
            std::ofstream f(png_path, std::ios::binary);
            if (!f) {
                warnings.push_back("cannot write PNG: " + png_path.string());
                ++failed;
                continue;
            }
            f.write(reinterpret_cast<const char*>(png_bytes.data()),
                    static_cast<std::streamsize>(png_bytes.size()));
        }

        // Write metadata JSON
        {
            std::ofstream f(json_path);
            if (!f) {
                warnings.push_back("cannot write JSON: " + json_path.string());
            } else {
                f << img.metadata.dump(2);
            }
        }

        ++written;
    }

    // Write images_manifest.json
    {
        const json    manifest = {{"source_container", container_path},
                                  {"total_images", image_names.size()},
                                  {"written_images", written},
                                  {"failed_images", failed},
                                  {"warnings_count", warnings.size()}};
        std::ofstream f(fs::path(out_dir) / "images_manifest.json");
        f << manifest.dump(2);
    }

    // Write warnings.txt
    {
        std::ofstream f(fs::path(out_dir) / "warnings.txt");
        for (const auto& w : warnings)
            f << w << '\n';
        for (const auto& w : maps.warnings)
            f << w << '\n';
    }

    if (written == 0 && failed > 0) {
        kLog->error("all_images_failed count={}", failed);
        return 1;
    }

    kLog->info("extract_images written={} failed={} out={}", written, failed, out_dir);
    return 0;
}
