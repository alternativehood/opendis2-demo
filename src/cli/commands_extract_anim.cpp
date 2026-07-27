#include "commands_extract_anim.hpp"
#include "d2res/anim_decoder.hpp"
#include "d2res/atlas_packer.hpp"
#include "d2res/gif_encoder.hpp"
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

static bool glob_match(const std::string& pat, const std::string& name) {
    if (pat.empty())
        return true;
    return d2res::platform::glob_match(pat, name);
}

static int write_animation(const d2res::DecodedAnimation& anim, const fs::path& out_dir,
                           bool write_gif, int frame_delay_ms,
                           std::vector<std::string>& batch_warnings) {
    fs::create_directories(out_dir);

    // Write frame PNGs
    for (std::size_t i = 0; i < anim.frames.size(); ++i) {
        const auto&        frame = anim.frames[i];
        std::ostringstream name_ss;
        name_ss << "frame_" << std::setfill('0') << std::setw(3) << i << ".png";
        const fs::path png_path = out_dir / name_ss.str();

        std::vector<uint8_t> png_bytes;
        const unsigned err = lodepng::encode(png_bytes, frame.rgba, frame.width, frame.height);
        if (err != 0) {
            batch_warnings.push_back("PNG encode failed for frame " + std::to_string(i) + " of " +
                                     anim.name + ": " + lodepng_error_text(err));
            continue;
        }
        std::ofstream f(png_path, std::ios::binary);
        f.write(reinterpret_cast<const char*>(png_bytes.data()),
                static_cast<std::streamsize>(png_bytes.size()));
    }

    // Write anim.json
    {
        std::ofstream f(out_dir / "anim.json");
        f << anim.metadata.dump(2);
    }

    // Write preview.gif
    if (write_gif && !anim.frames.empty()) {
        try {
            d2res::encode_gif(anim.frames, out_dir / "preview.gif", frame_delay_ms);
        } catch (const std::exception& e) {
            batch_warnings.push_back("GIF encode failed for " + anim.name + ": " + e.what());
        }
    }

    // Propagate per-animation warnings
    for (const auto& w : anim.warnings)
        batch_warnings.push_back(w);

    return 0;
}

int cmd_extract_anim(const std::string& container_path, const std::string& out_dir,
                     const std::string& name, const std::string& pattern, bool all, bool gif,
                     int frame_delay_ms, bool atlas, int atlas_max_size) {
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

    return cmd_extract_anim_from_container(container, maps, out_dir, name, pattern, all, gif,
                                           frame_delay_ms, atlas, atlas_max_size);
}

int cmd_extract_anim_from_container(const d2res::MqdbContainer& container,
                                    const d2res::OptMaps& maps, const std::string& out_dir,
                                    const std::string& name, const std::string& pattern, bool all,
                                    bool gif, int frame_delay_ms, bool atlas, int atlas_max_size) {
    // Validate selection mode
    const int modes = (!name.empty() ? 1 : 0) + (!pattern.empty() ? 1 : 0) + (all ? 1 : 0);
    if (modes == 0) {
        kLog->error("extract_anim no_mode_specified hint=use_name_pattern_or_all");
        return 1;
    }

    d2res::AnimationDecoder const decoder(container, maps);
    std::vector<std::string>      warnings;

    // Single animation mode: write directly into out_dir
    if (!name.empty()) {
        d2res::DecodedAnimation anim;
        try {
            anim = decoder.decode_animation(name);
        } catch (const std::exception& e) {
            kLog->error("decode_animation_failed error={}", e.what());
            return 1;
        }
        // Atlas mode: pack frames into atlas sheets instead of individual PNGs
        if (atlas) {
            fs::create_directories(fs::path(out_dir));
            std::vector<std::pair<std::string, d2res::RgbaBuffer>> frame_bufs;
            frame_bufs.reserve(anim.frames.size());
            for (auto& fr : anim.frames) {
                d2res::RgbaBuffer buf;
                buf.width = fr.width;
                buf.height = fr.height;
                buf.rgba = std::move(fr.rgba);
                frame_bufs.emplace_back(fr.logical_name, std::move(buf));
            }
            const auto atlas_result = d2res::AtlasPacker::pack(frame_bufs, atlas_max_size);

            for (std::size_t i = 0; i < atlas_result.sheets.size(); ++i) {
                std::ostringstream sname;
                sname << "atlas_" << std::setfill('0') << std::setw(3) << i << ".png";
                const auto&          canvas = atlas_result.sheets[i].canvas;
                std::vector<uint8_t> png_bytes;
                lodepng::encode(png_bytes, canvas.rgba, canvas.width, canvas.height);
                std::ofstream f(fs::path(out_dir) / sname.str(), std::ios::binary);
                f.write(reinterpret_cast<const char*>(png_bytes.data()),
                        static_cast<std::streamsize>(png_bytes.size()));
            }
            {
                std::ofstream f(fs::path(out_dir) / "atlas.json");
                f << atlas_result.to_json(container.path().string(), atlas_max_size).dump(2);
            }
        }

        if (!atlas) {
            write_animation(anim, fs::path(out_dir), gif, frame_delay_ms, warnings);
        } else {
            // Atlas mode: write anim.json and optional GIF, but no individual frame PNGs
            fs::create_directories(fs::path(out_dir));
            {
                std::ofstream f(fs::path(out_dir) / "anim.json");
                f << anim.metadata.dump(2);
            }
            if (gif && !anim.frames.empty()) {
                try {
                    d2res::encode_gif(anim.frames, fs::path(out_dir) / "preview.gif",
                                      frame_delay_ms);
                } catch (const std::exception& e) {
                    warnings.push_back("GIF encode failed: " + std::string(e.what()));
                }
            }
            for (const auto& w : anim.warnings)
                warnings.push_back(w);
        }

        // Write warnings.txt if any
        std::ofstream wf(fs::path(out_dir) / "warnings.txt");
        for (const auto& w : warnings)
            wf << w << '\n';

        kLog->info("extract_anim frames={} name={} out={}", anim.frames.size(), anim.name, out_dir);
        return 0;
    }

    // Multi-animation mode (--pattern / --all): write each to a subdirectory
    std::error_code ec;
    fs::create_directories(out_dir, ec);

    const auto  all_names = decoder.list_animations();
    std::size_t written = 0;
    std::size_t failed = 0;

    const std::size_t total = [&]() {
        if (all)
            return all_names.size();
        std::size_t n = 0;
        for (const auto& n_str : all_names) {
            if (glob_match(pattern, n_str))
                ++n;
        }
        return n;
    }();

    std::size_t processed = 0;
    for (const auto& anim_name : all_names) {
        if (!all && !glob_match(pattern, anim_name))
            continue;

        ++processed;
        if (processed % 100 == 0 || processed == total) {
            kLog->debug("decode_progress decoded={} total={}", processed, total);
        }

        const fs::path          anim_dir = fs::path(out_dir) / d2res::sanitize_filename(anim_name);
        d2res::DecodedAnimation anim;
        try {
            anim = decoder.decode_animation(anim_name);
        } catch (const std::exception& e) {
            warnings.push_back("decode failed for " + anim_name + ": " + e.what());
            ++failed;
            continue;
        }
        write_animation(anim, anim_dir, gif, frame_delay_ms, warnings);
        ++written;
    }

    // Manifest
    {
        const json    manifest = {{"source_container", container.path().string()},
                                  {"total_animations", all_names.size()},
                                  {"written_animations", written},
                                  {"failed_animations", failed},
                                  {"warnings_count", warnings.size()}};
        std::ofstream f(fs::path(out_dir) / "anim_manifest.json");
        f << manifest.dump(2);
    }

    // Warnings
    {
        std::ofstream f(fs::path(out_dir) / "warnings.txt");
        for (const auto& w : warnings)
            f << w << '\n';
    }

    kLog->info("extract_anim written={} failed={} out={}", written, failed, out_dir);
    return (written == 0 && failed > 0) ? 1 : 0;
}
