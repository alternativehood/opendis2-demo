#include "commands_special.hpp"
#include "d2res/mqdb.hpp"
#include "d2res/special_files.hpp"
#include "d2res/hexdump.hpp"
#include <d2log/log.hpp>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {
auto kLog = d2log::get("d2.app"); // NOLINT(cert-err58-cpp)
} // namespace

static std::string special_type_name(d2res::SpecialFileType t) {
    switch (t) {
    case d2res::SpecialFileType::NOT_SPECIAL:
        return "NOT_SPECIAL";
    case d2res::SpecialFileType::INDEX_OPT:
        return "INDEX_OPT";
    case d2res::SpecialFileType::IMAGES_OPT:
        return "IMAGES_OPT";
    case d2res::SpecialFileType::ANIMS_OPT:
        return "ANIMS_OPT";
    case d2res::SpecialFileType::UNKNOWN_OPT:
        return "UNKNOWN_OPT";
    case d2res::SpecialFileType::UNKNOWN_DAT:
        return "UNKNOWN_DAT";
    case d2res::SpecialFileType::UNKNOWN_SPECIAL:
        return "UNKNOWN_SPECIAL";
    }
    return "NOT_SPECIAL";
}

int cmd_extract_special(const std::string& container_path, const std::string& out_dir,
                        std::size_t hexdump_limit) {
    d2res::MqdbContainer container;
    try {
        container = d2res::MqdbContainer::open(container_path);
    } catch (const std::exception& e) {
        kLog->error("open_container_failed error={}", e.what());
        return 1;
    }

    std::error_code ec;
    fs::create_directories(out_dir, ec);
    if (ec) {
        kLog->error("cannot_create_output_dir path={} error={}", out_dir, ec.message());
        return 1;
    }

    const auto&              records = container.records();
    std::vector<std::string> run_warnings(container.warnings().begin(), container.warnings().end());

    json                  manifest = json::array();
    d2res::HexdumpOptions dump_opts;
    dump_opts.limit_bytes = hexdump_limit;

    for (const auto& rec : records) {
        auto stype = d2res::classify_record(rec);
        if (stype == d2res::SpecialFileType::NOT_SPECIAL)
            continue;

        const std::string& name = rec.name;
        fs::path const     bin_path = fs::path(out_dir) / name;
        fs::path const     dump_path = fs::path(out_dir) / (name + ".hexdump");

        d2res::Entry entry;
        try {
            entry = container.read_record(rec.index);
        } catch (const std::exception& e) {
            run_warnings.push_back("error reading record " + std::to_string(rec.index) + " (" +
                                   name + "): " + e.what());
            continue;
        }

        // Write binary payload
        {
            std::ofstream of(bin_path, std::ios::binary);
            if (!of) {
                run_warnings.push_back("failed to open output file: " + bin_path.string());
                continue;
            }
            if (!entry.payload.empty()) {
                of.write(reinterpret_cast<const char*>(entry.payload.data()),
                         static_cast<std::streamsize>(entry.payload.size()));
            }
        }

        // Write hexdump sidecar
        bool const truncated = entry.payload.size() > hexdump_limit;
        {
            std::ofstream hf(dump_path);
            if (hf) {
                d2res::write_hexdump(hf, std::span<const uint8_t>(entry.payload), dump_opts);
            } else {
                run_warnings.push_back("failed to open hexdump file: " + dump_path.string());
            }
        }

        json m;
        m["index"] = rec.index;
        m["id"] = rec.id;
        m["name"] = name;
        m["type"] = special_type_name(stype);
        m["size"] = rec.realFileSize;
        m["output_file"] = name;
        m["hexdump_file"] = name + ".hexdump";
        m["hexdump_truncated"] = truncated;
        manifest.push_back(std::move(m));
    }

    // Write special_manifest.json
    {
        fs::path const mp = fs::path(out_dir) / "special_manifest.json";
        std::ofstream  mf(mp);
        if (mf) {
            mf << manifest.dump(2) << '\n';
        } else {
            run_warnings.emplace_back("failed to write special_manifest.json");
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

    return 0;
}
