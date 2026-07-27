#include "commands_extract.hpp"
#include "d2res/mqdb.hpp"
#include "d2res/rgba_buffer.hpp"
#include <d2log/log.hpp>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string>

namespace {
auto kLog = d2log::get("d2.app"); // NOLINT(cert-err58-cpp)
} // namespace

namespace fs = std::filesystem;
using json = nlohmann::json;

// Split off file extension (including dot), e.g. "FOO.PNG" → {"FOO", ".PNG"}
static std::pair<std::string, std::string> split_ext(const std::string& s) {
    auto pos = s.rfind('.');
    if (pos == std::string::npos)
        return {s, ""};
    return {s.substr(0, pos), s.substr(pos)};
}

int cmd_extract_raw(const std::string& container_path, const std::string& out_dir, bool overwrite) {
    d2res::MqdbContainer container;
    try {
        container = d2res::MqdbContainer::open(container_path);
    } catch (const std::exception& e) {
        kLog->error("open_container_failed error={}", e.what());
        return 1;
    }

    // Create output directory
    std::error_code ec;
    fs::create_directories(out_dir, ec);
    if (ec) {
        kLog->error("cannot_create_output_dir path={} error={}", out_dir, ec.message());
        return 1;
    }

    const auto&              records = container.records();
    std::vector<std::string> run_warnings(container.warnings().begin(), container.warnings().end());

    json                  manifest = json::array();
    std::set<std::string> used_filenames;
    bool                  any_skipped = false;

    for (const auto& rec : records) {
        // Determine output filename
        std::string out_filename;
        if (rec.name.empty()) {
            out_filename = "record_" + std::to_string(rec.index) + ".bin";
        } else {
            out_filename = d2res::sanitize_filename(rec.name);
        }

        // Detect duplicate output filenames
        if (static_cast<unsigned int>(used_filenames.contains(out_filename)) != 0u) {
            auto [stem, ext] = split_ext(out_filename);
            out_filename = stem;
            out_filename += "__record_";
            out_filename += std::to_string(rec.index);
            out_filename += "_duplicate";
            out_filename += ext;
            std::string warn = "duplicate output filename for record ";
            warn += std::to_string(rec.index);
            warn += " (name='";
            warn += rec.name;
            warn += "'), writing as: ";
            warn += out_filename;
            run_warnings.push_back(std::move(warn));
        }
        used_filenames.insert(out_filename);

        fs::path const out_path = fs::path(out_dir) / out_filename;

        // No-overwrite guard
        if (!overwrite && fs::exists(out_path)) {
            run_warnings.push_back("skipped existing file (use --overwrite): " + out_path.string());
            any_skipped = true;

            json m;
            m["index"] = rec.index;
            m["id"] = rec.id;
            m["name"] = rec.name;
            m["sanitized_name"] = out_filename;
            m["size"] = rec.realFileSize;
            m["output_file"] = out_filename;
            m["skipped"] = true;
            manifest.push_back(std::move(m));
            continue;
        }

        // Read and write payload
        try {
            auto          entry = container.read_record(rec.index);
            std::ofstream of(out_path, std::ios::binary);
            if (!of) {
                run_warnings.push_back("failed to open output file: " + out_path.string());
                continue;
            }
            if (!entry.payload.empty()) {
                of.write(reinterpret_cast<const char*>(entry.payload.data()),
                         static_cast<std::streamsize>(entry.payload.size()));
            }
        } catch (const std::exception& e) {
            run_warnings.push_back("error writing record " + std::to_string(rec.index) + ": " +
                                   e.what());
            continue;
        }

        json m;
        m["index"] = rec.index;
        m["id"] = rec.id;
        m["name"] = rec.name;
        m["sanitized_name"] = out_filename;
        m["size"] = rec.realFileSize;
        m["output_file"] = out_filename;
        m["skipped"] = false;
        manifest.push_back(std::move(m));
    }

    // Write manifest.json
    {
        fs::path const mp = fs::path(out_dir) / "manifest.json";
        std::ofstream  mf(mp);
        if (mf) {
            mf << manifest.dump(2) << '\n';
        } else {
            run_warnings.emplace_back("failed to write manifest.json");
        }
    }

    // Write warnings.txt
    if (!run_warnings.empty()) {
        fs::path const wp = fs::path(out_dir) / "warnings.txt";
        std::ofstream  wf(wp);
        if (wf) {
            for (const auto& w : run_warnings)
                wf << w << '\n';
        }
        kLog->warn("extract_warnings count={} see={}", run_warnings.size(), wp.string());
    }

    if (any_skipped) {
        kLog->warn("some_files_skipped hint=use_overwrite_flag");
        return 2;
    }

    return 0;
}
