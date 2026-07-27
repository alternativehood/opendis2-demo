#include "commands_extract_dat.hpp"
#include "d2res/dat_parser.hpp"

#include <d2log/log.hpp>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace {
auto kLog = d2log::get("d2.app"); // NOLINT(cert-err58-cpp)
} // namespace

int cmd_extract_dat(const std::string& file_path, const std::string& out_dir) {
    if (!fs::is_regular_file(file_path)) {
        kLog->error("file_not_found path={}", file_path);
        return 1;
    }

    std::ifstream f(file_path);
    if (!f) {
        kLog->error("cannot_open_file path={}", file_path);
        return 1;
    }
    std::string text;
    text.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());

    d2res::DatParser parser;
    parser.parse(text);

    std::error_code ec;
    fs::create_directories(out_dir, ec);
    if (ec) {
        kLog->error("cannot_create_output_dir error={}", ec.message());
        return 1;
    }

    const std::string stem = fs::path(file_path).stem().string();
    std::ofstream     out(fs::path(out_dir) / (stem + ".json"));
    if (!out) {
        kLog->error("cannot_write_json");
        return 1;
    }
    out << parser.to_json().dump(2) << '\n';

    kLog->info("extract_dat keys={} out={}", parser.entries().size(),
               (fs::path(out_dir) / (stem + ".json")).string());
    return 0;
}
