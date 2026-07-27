#include "commands_compare_images.hpp"
#include "d2res/image_comparer.hpp"

#include <d2log/log.hpp>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace {
auto kLog = d2log::get("d2.app"); // NOLINT(cert-err58-cpp)
} // namespace

int cmd_compare_images(const std::string& actual_dir, const std::string& expected_dir,
                       const std::string& report_path, int sample_n, bool all_mode,
                       const std::string& diff_dir, const std::string& game_dir) {
    if (!fs::is_directory(actual_dir)) {
        kLog->error("actual_directory_not_found path={}", actual_dir);
        return 1;
    }
    if (!fs::is_directory(expected_dir)) {
        kLog->error("expected_directory_not_found path={}", expected_dir);
        return 1;
    }

    kLog->info("comparing_images sample={}", all_mode ? "all" : std::to_string(sample_n));

    const auto report =
        d2res::ImageComparer::run(actual_dir, expected_dir, sample_n, all_mode, diff_dir, game_dir);

    // Write report
    if (!report_path.empty()) {
        std::error_code ec;
        fs::create_directories(fs::path(report_path).parent_path(), ec);
        std::ofstream f(report_path);
        if (!f) {
            kLog->error("cannot_write_report path={}", report_path);
            return 1;
        }
        f << report.to_json().dump(2) << '\n';
    }

    kLog->info("compare_images_complete compared={} passed={} failed={} skipped={}",
               report.total_compared, report.passed, report.failed, report.skipped);
    if (!report_path.empty())
        kLog->info("report_written path={}", report_path);

    return (report.failed > 0) ? 1 : 0;
}
