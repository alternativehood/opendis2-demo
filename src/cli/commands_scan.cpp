#include "commands_scan.hpp"
#include "d2res/game_scanner.hpp"
#include <d2log/log.hpp>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace {
auto kLog = d2log::get("d2.app"); // NOLINT(cert-err58-cpp)
} // namespace

int cmd_scan(const std::string& game_dir, const std::string& out_dir) {
    if (!fs::is_directory(game_dir)) {
        kLog->error("game_directory_not_found path={}", game_dir);
        return 1;
    }

    const d2res::ScanResult result = d2res::GameScanner::scan(game_dir);

    std::error_code ec;
    fs::create_directories(out_dir, ec);
    if (ec) {
        kLog->error("cannot_create_output_dir error={}", ec.message());
        return 1;
    }

    // Write game_manifest.json
    {
        std::ofstream f(fs::path(out_dir) / "game_manifest.json");
        if (!f) {
            kLog->error("cannot_write_game_manifest");
            return 1;
        }
        f << result.to_json().dump(2) << '\n';
    }

    // Write warnings.txt if any
    if (!result.warnings.empty()) {
        std::ofstream f(fs::path(out_dir) / "warnings.txt");
        for (const auto& w : result.warnings)
            f << w << '\n';
    }

    kLog->info("scan_complete total_files={} containers={} warnings={}", result.total_files,
               result.containers.size(), result.warnings.size());
    kLog->info("manifest out={}", (fs::path(out_dir) / "game_manifest.json").string());

    return 0;
}
