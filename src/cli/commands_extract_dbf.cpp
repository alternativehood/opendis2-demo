#include "commands_extract_dbf.hpp"
#include "d2res/dbf_reader.hpp"

#include <d2log/log.hpp>
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstdint>

namespace fs = std::filesystem;

namespace {
auto kLog = d2log::get("d2.app"); // NOLINT(cert-err58-cpp)
} // namespace

int cmd_extract_dbf(const std::string& file_path, const std::string& out_dir) {
    if (!fs::is_regular_file(file_path)) {
        kLog->error("file_not_found path={}", file_path);
        return 1;
    }

    std::ifstream f(file_path, std::ios::binary);
    if (!f) {
        kLog->error("cannot_open_file path={}", file_path);
        return 1;
    }
    std::vector<uint8_t> data;
    data.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());

    d2res::DbfReader const reader(std::span<const uint8_t>(data.data(), data.size()));

    std::error_code ec;
    fs::create_directories(out_dir, ec);
    if (ec) {
        kLog->error("cannot_create_output_dir error={}", ec.message());
        return 1;
    }

    const std::string stem = fs::path(file_path).stem().string();

    {
        std::ofstream out(fs::path(out_dir) / (stem + ".schema.json"));
        if (!out) {
            kLog->error("cannot_write_schema_json");
            return 1;
        }
        out << reader.to_schema_json(fs::path(file_path).filename().string()).dump(2) << '\n';
    }
    {
        std::ofstream out(fs::path(out_dir) / (stem + ".records.json"));
        if (!out) {
            kLog->error("cannot_write_records_json");
            return 1;
        }
        out << reader.to_records_json().dump(2) << '\n';
    }

    kLog->info("extract_dbf records={} out={}", reader.record_count(),
               (fs::path(out_dir) / stem).string());
    return 0;
}
