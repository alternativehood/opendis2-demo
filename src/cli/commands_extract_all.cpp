#include "commands_extract_all.hpp"
#include "commands_extract_dbf.hpp"
#include "commands_extract_dat.hpp"
#include "commands_extract_dlg.hpp"
#include "extraction_orchestrator.hpp"
#include "manifest_builder.hpp"
#include "d2res/game_scanner.hpp"

#include <d2log/log.hpp>
#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

namespace {
auto kLog = d2log::get("d2.app"); // NOLINT(cert-err58-cpp)
} // namespace

int cmd_extract_all(const std::string& game_dir, const std::string& out_dir) {
    if (!fs::is_directory(game_dir)) {
        kLog->error("game_directory_not_found path={}", game_dir);
        return 1;
    }

    kLog->info("scanning_game_directory path={}", game_dir);
    const d2res::ScanResult scan = d2res::GameScanner::scan(game_dir);
    kLog->info("found containers={}", scan.containers.size());

    std::error_code ec;
    fs::create_directories(out_dir, ec);
    if (ec) {
        kLog->error("cannot_create_output_dir error={}", ec.message());
        return 1;
    }

    ManifestBuilder builder;
    builder.set_game_root(game_dir);

    int            extracted = 0;
    nlohmann::json skipped_arr = nlohmann::json::array();
    nlohmann::json failed_arr = nlohmann::json::array();
    nlohmann::json warnings_arr = nlohmann::json::array();

    for (const auto& c : scan.containers) {
        if (!has_supported_runtime_content(c)) {
            nlohmann::json sj;
            sj["path"] = c.path;
            sj["reason"] = "unknown content type";
            skipped_arr.push_back(std::move(sj));
            continue;
        }

        const fs::path            out_sub = fs::path(out_dir) / c.path;
        const ContainerExtraction outcome = extract_container_assets(game_dir, c, out_sub);
        builder.add_container(c.path, outcome.content_kinds);
        if (!outcome.succeeded) {
            failed_arr.push_back({{"path", c.path}, {"error", outcome.error}});
            builder.add_warning("failed to extract " + c.path + ": " + outcome.error);
            continue;
        }
        for (const auto& asset : outcome.assets) {
            const fs::path relative_path = fs::relative(asset.sidecar_path, out_dir);
            builder.add_asset(to_string(asset.kind), asset.logical_name, c.path,
                              relative_path.generic_string());
        }
        for (const auto& warning : outcome.warnings)
            builder.add_warning(c.path + ": " + warning);

        ++extracted;
        kLog->info("extracted container={} index={}", c.path, extracted);
    }

    for (const auto& f : scan.other_files) {
        const bool is_data_table =
            std::ranges::find(f.likely_content, d2res::ContentKind::DataTables) !=
            f.likely_content.end();
        if (!is_data_table)
            continue;

        const fs::path out_sub = fs::path(out_dir) / f.path;
        const fs::path source = fs::path(game_dir) / f.path;
        int            rc = 1;
        if (f.type == "dbf") {
            rc = cmd_extract_dbf(source.string(), out_sub.string());
        } else if (f.type == "dat") {
            rc = cmd_extract_dat(source.string(), out_sub.string());
        } else if (f.type == "dlg") {
            rc = cmd_extract_dlg(source.string(), out_sub.string());
        }
        if (rc != 0) {
            failed_arr.push_back({{"path", f.path}, {"error", "data table extraction failed"}});
            builder.add_warning("failed to extract data table " + f.path);
            continue;
        }
        builder.add_container(f.path, {"data_tables"});
        ++extracted;
        kLog->info("extracted data_table={} index={}", f.path, extracted);

        // Add data table assets to manifest
        const std::string stem = fs::path(f.path).stem().string();
        std::error_code   ec2;
        for (const auto& entry : fs::directory_iterator(out_sub, ec2)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".json")
                continue;
            const std::string filename = entry.path().filename().string();
            // For DBF, skip schema.json and only add records.json as the primary asset
            if (f.type == "dbf" && filename == stem + ".schema.json")
                continue;
            const fs::path relative_path = fs::relative(entry.path(), out_dir);
            builder.add_asset("data_table", stem, f.path, relative_path.generic_string());
        }
    }

    for (const auto& w : scan.warnings)
        warnings_arr.push_back(w);

    // Write extraction_manifest.json (diagnostics)
    nlohmann::json manifest;
    manifest["game_root"] = game_dir;
    manifest["out_dir"] = out_dir;
    manifest["total_containers"] = static_cast<int>(scan.containers.size());
    manifest["extracted"] = extracted;
    manifest["skipped"] = skipped_arr;
    manifest["failed"] = failed_arr;
    manifest["warnings"] = warnings_arr;

    std::ofstream mf(fs::path(out_dir) / "extraction_manifest.json");
    mf << manifest.dump(2) << '\n';

    // Write game_manifest.json (runtime asset discovery)
    builder.write(fs::path(out_dir));

    kLog->info("extract_all_complete extracted={} skipped={} failed={}", extracted,
               skipped_arr.size(), failed_arr.size());
    kLog->info("extraction_manifest out={}",
               (fs::path(out_dir) / "extraction_manifest.json").string());
    kLog->info("game_manifest out={}", (fs::path(out_dir) / "game_manifest.json").string());
    return 0;
}
