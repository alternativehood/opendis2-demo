#include "commands_extract_sounds.hpp"
#include "d2res/mqdb.hpp"
#include "d2res/rgba_buffer.hpp"
#include "d2res/wdb_decoder.hpp"
#include <nlohmann/json.hpp>
#include "d2res/platform/glob.hpp"
#include <d2log/log.hpp>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {
auto kLog = d2log::get("d2.app"); // NOLINT(cert-err58-cpp)
} // namespace

static bool glob_match_sounds(const std::string& pattern, const std::string& name) {
    if (pattern.empty())
        return true;
    return d2res::platform::glob_match(pattern, name);
}

int cmd_extract_sounds(const std::string& container_path, const std::string& out_dir,
                       const std::string& pattern, bool all) {
    d2res::MqdbContainer container;
    try {
        container = d2res::MqdbContainer::open(fs::path(container_path));
    } catch (const std::exception& e) {
        kLog->error("cannot_open_container error={}", e.what());
        return 1;
    }

    std::error_code ec;
    fs::create_directories(out_dir, ec);
    if (ec) {
        kLog->error("cannot_create_output_dir error={}", ec.message());
        return 1;
    }

    d2res::WdbDecoder const decoder(container);
    const auto              sound_names = decoder.list_sounds();

    std::size_t              written = 0;
    std::size_t              failed = 0;
    std::vector<std::string> warnings;
    if (sound_names.empty())
        warnings.emplace_back("container contains no named sounds");

    for (const auto& name : sound_names) {
        if (!all && !glob_match_sounds(pattern, name))
            continue;

        d2res::DecodedSound sound;
        try {
            sound = decoder.decode_sound(name);
        } catch (const std::exception& e) {
            warnings.push_back("decode failed for " + name + ": " + e.what());
            ++failed;
            continue;
        }

        const std::string safe = d2res::sanitize_filename(name);
        const std::string ext = (sound.detected_format == "WAV") ? ".wav" : ".bin";
        const fs::path    payload_path = fs::path(out_dir) / (safe + ext);
        const fs::path    json_path = fs::path(out_dir) / (safe + ".json");

        {
            std::ofstream f(payload_path, std::ios::binary);
            if (!f) {
                warnings.push_back("cannot write: " + payload_path.string());
                ++failed;
                continue;
            }
            f.write(reinterpret_cast<const char*>(sound.payload.data()),
                    static_cast<std::streamsize>(sound.payload.size()));
        }
        {
            std::ofstream f(json_path);
            f << sound.metadata.dump(2);
        }
        ++written;
    }

    {
        const json manifest = {
            {"source_container", container_path},
            {"total_sounds", sound_names.size()},
            {"written_sounds", written},
            {"failed_sounds", failed},
        };
        std::ofstream f(fs::path(out_dir) / "manifest.json");
        f << manifest.dump(2);
    }
    if (!warnings.empty()) {
        std::ofstream f(fs::path(out_dir) / "warnings.txt");
        for (const auto& w : warnings)
            f << w << '\n';
    }

    kLog->info("extract_sounds written={} failed={} out={}", written, failed, out_dir);
    if (written == 0) {
        kLog->warn("no_sounds_extracted");
    }
    return 0;
}
