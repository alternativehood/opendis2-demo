#include "commands_extract_wdt.hpp"
#include "d2res/wdt_reader.hpp"

#include <d2log/log.hpp>

namespace {
auto kLog = d2log::get("d2.app"); // NOLINT(cert-err58-cpp)
} // namespace

int cmd_extract_wdt(const std::string& file_path, const std::string& out_dir) {
    const auto result = d2res::WdtReader::open(file_path, out_dir);

    if (!result.ok) {
        if (result.error.find("not found") != std::string::npos) {
            kLog->warn("wdt_file_not_found path={}", file_path);
            return 0;
        }
        if (result.format == "unknown") {
            kLog->warn("not_mqdb_file raw_dump_written out={}", out_dir);
            return 0;
        }
        kLog->error("extract_wdt_failed error={}", result.error);
        return 1;
    }

    kLog->info("extract_wdt records={} out={}", result.record_count, out_dir);
    return 0;
}
