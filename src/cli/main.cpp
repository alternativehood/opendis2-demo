#include <CLI/CLI.hpp>
#include <d2buildinfo/build_info.hpp>
#include "commands_list.hpp"
#include "commands_extract.hpp"
#include "commands_special.hpp"
#include "commands_extract_images.hpp"
#include "commands_extract_anim.hpp"
#include "commands_extract_atlas.hpp"
#include "commands_extract_sounds.hpp"
#include "commands_extract_viewer.hpp"
#include "commands_scan.hpp"
#include "commands_extract_dbf.hpp"
#include "commands_extract_dlg.hpp"
#include "commands_extract_dat.hpp"
#include "commands_extract_wdt.hpp"
#include "commands_extract_all.hpp"
#include "commands_compare_images.hpp"
#include "commands_research.hpp"
#include "commands_sg_inspect.hpp"
#include "runtime_package.hpp"
#include <d2asset/engine_contract.hpp>
#include <d2log/log.hpp>
#include <cstddef>
#include <cstdlib>

int run_main(int argc, char** argv) {
    CLI::App app{"opendis2-dev-extractor — development extractor/asset CLI for OpenDis2"};
    app.set_version_flag("--version", d2buildinfo::format_build_version());
    app.require_subcommand(1);

    d2log::LogConfig log_config;
    app.add_option("--log-level", log_config.level,
                   "Log level: trace|debug|info|warn|error|off (env: D2_LOG_LEVEL)");
    app.add_option("--log-file", log_config.file, "Log file path (env: D2_LOG_FILE)");

    // ---- list --------------------------------------------------------
    auto*       list_cmd = app.add_subcommand("list", "List records in an MQDB container");
    std::string list_path;
    bool        list_json = false;
    bool        list_special = false;
    list_cmd->add_option("container", list_path, "Path to .ff, .wdb, or other MQDB file")
        ->required();
    list_cmd->add_flag("--json", list_json, "Emit JSON instead of human-readable text");
    list_cmd->add_flag("--special", list_special,
                       "Show only special-file records (names starting with '-')");

    // ---- extract-raw -------------------------------------------------
    auto* extract_cmd =
        app.add_subcommand("extract-raw", "Extract all record payloads to an output directory");
    std::string extract_path;
    std::string extract_out;
    bool        extract_overwrite = false;
    extract_cmd->add_option("container", extract_path, "Path to .ff, .wdb, or other MQDB file")
        ->required();
    extract_cmd->add_option("--out", extract_out, "Output directory")->required();
    extract_cmd->add_flag("--overwrite", extract_overwrite,
                          "Overwrite files that already exist in the output directory");

    // ---- extract-special ---------------------------------------------
    auto* special_cmd =
        app.add_subcommand("extract-special", "Extract special metadata records (-INDEX.OPT, "
                                              "-IMAGES.OPT, -ANIMS.OPT) with hexdump sidecars");
    std::string special_path;
    std::string special_out;
    std::size_t special_hexdump_limit = 16384;
    special_cmd->add_option("container", special_path, "Path to .ff, .wdb, or other MQDB file")
        ->required();
    special_cmd->add_option("--out", special_out, "Output directory")->required();
    special_cmd->add_option("--hexdump-limit", special_hexdump_limit,
                            "Maximum bytes to include in each .hexdump sidecar (default: 16384)");

    // ---- extract-images ----------------------------------------------
    auto* extract_images_cmd = app.add_subcommand(
        "extract-images", "Decode and export logical images from a .ff container");
    std::string extract_images_path;
    std::string extract_images_out;
    std::string extract_images_pattern;
    extract_images_cmd->add_option("container", extract_images_path, "Path to .ff MQDB file")
        ->required();
    extract_images_cmd->add_option("--out", extract_images_out, "Output directory")->required();
    extract_images_cmd->add_option("--pattern", extract_images_pattern,
                                   "Glob pattern to filter image names (e.g. 'DLG_H_ARCHER*')");

    // ---- extract-anim ------------------------------------------------
    auto* extract_anim_cmd =
        app.add_subcommand("extract-anim", "Decode animation frames from a .ff container");
    std::string extract_anim_path;
    std::string extract_anim_out;
    std::string extract_anim_name;
    std::string extract_anim_pattern;
    bool        extract_anim_all = false;
    bool        extract_anim_gif = false;
    int         extract_anim_delay = 100;
    bool        extract_anim_atlas = false;
    int         extract_anim_atlas_size = 4096;
    extract_anim_cmd->add_option("container", extract_anim_path, "Path to .ff MQDB file")
        ->required();
    extract_anim_cmd->add_option("--out", extract_anim_out, "Output directory")->required();
    extract_anim_cmd->add_option("--name", extract_anim_name, "Exact animation name to extract");
    extract_anim_cmd->add_option("--pattern", extract_anim_pattern,
                                 "Glob pattern to match animation names");
    extract_anim_cmd->add_flag("--all", extract_anim_all, "Extract all animations");
    extract_anim_cmd->add_flag("--gif", extract_anim_gif, "Write preview.gif alongside frame PNGs");
    extract_anim_cmd->add_option("--delay", extract_anim_delay,
                                 "Frame delay in milliseconds (default: 100)");
    extract_anim_cmd->add_flag(
        "--atlas", extract_anim_atlas,
        "Pack frames into atlas sheets (atlas_NNN.png + atlas.json) instead of individual PNGs");
    extract_anim_cmd->add_option("--atlas-max-size", extract_anim_atlas_size,
                                 "Maximum atlas sheet dimension in pixels (default: 4096)");

    // ---- extract-sounds ----------------------------------------------
    auto* extract_sounds_cmd =
        app.add_subcommand("extract-sounds", "Extract audio files from a .wdb sound bank");
    std::string extract_sounds_path;
    std::string extract_sounds_out;
    std::string extract_sounds_pattern;
    bool        extract_sounds_all = false;
    extract_sounds_cmd->add_option("container", extract_sounds_path, "Path to .wdb MQDB file")
        ->required();
    extract_sounds_cmd->add_option("--out", extract_sounds_out, "Output directory")->required();
    extract_sounds_cmd->add_option("--pattern", extract_sounds_pattern,
                                   "Glob pattern to filter sound names");
    extract_sounds_cmd->add_flag("--all", extract_sounds_all,
                                 "Extract all sounds in the container");

    // ---- extract-atlas -----------------------------------------------
    auto* extract_atlas_cmd = app.add_subcommand(
        "extract-atlas", "Pack images from a .ff container into texture atlas sheets");
    std::string extract_atlas_path;
    std::string extract_atlas_out;
    std::string extract_atlas_pattern;
    bool        extract_atlas_all = false;
    int         extract_atlas_max_size = 4096;
    extract_atlas_cmd->add_option("container", extract_atlas_path, "Path to .ff MQDB file")
        ->required();
    extract_atlas_cmd->add_option("--out", extract_atlas_out, "Output directory")->required();
    extract_atlas_cmd->add_option("--pattern", extract_atlas_pattern,
                                  "Glob pattern to filter image names");
    extract_atlas_cmd->add_flag("--all", extract_atlas_all, "Pack all images in the container");
    extract_atlas_cmd->add_option("--max-size", extract_atlas_max_size,
                                  "Maximum atlas sheet dimension in pixels (default: 4096)");

    // ---- scan -------------------------------------------------------
    auto* scan_cmd = app.add_subcommand(
        "scan", "Recursively scan a game directory and produce game_manifest.json");
    std::string scan_game_dir;
    std::string scan_out_dir;
    scan_cmd->add_option("game_dir", scan_game_dir, "Path to the game root directory")->required();
    scan_cmd->add_option("--out", scan_out_dir, "Output directory for game_manifest.json")
        ->required();

    // ---- extract-dbf ------------------------------------------------
    auto* extract_dbf_cmd = app.add_subcommand(
        "extract-dbf", "Parse a dBASE III .dbf file and write schema.json + records.json");
    std::string extract_dbf_path;
    std::string extract_dbf_out;
    extract_dbf_cmd->add_option("file", extract_dbf_path, "Path to .dbf file")->required();
    extract_dbf_cmd->add_option("--out", extract_dbf_out, "Output directory")->required();

    // ---- extract-dlg ------------------------------------------------
    auto* extract_dlg_cmd =
        app.add_subcommand("extract-dlg", "Parse a Disciples II .dlg dialog file and write JSON");
    std::string extract_dlg_path;
    std::string extract_dlg_out;
    extract_dlg_cmd->add_option("file", extract_dlg_path, "Path to .dlg file")->required();
    extract_dlg_cmd->add_option("--out", extract_dlg_out, "Output directory")->required();

    // ---- extract-dat ------------------------------------------------
    auto* extract_dat_cmd =
        app.add_subcommand("extract-dat", "Parse a KEY=VALUE .dat file and write JSON");
    std::string extract_dat_path;
    std::string extract_dat_out;
    extract_dat_cmd->add_option("file", extract_dat_path, "Path to .dat file")->required();
    extract_dat_cmd->add_option("--out", extract_dat_out, "Output directory")->required();

    // ---- extract-wdt ------------------------------------------------
    auto* extract_wdt_cmd = app.add_subcommand(
        "extract-wdt",
        "Parse a WDT sound-mapping file (best-effort MQDB; absent file is a warning not error)");
    std::string extract_wdt_path;
    std::string extract_wdt_out;
    extract_wdt_cmd->add_option("file", extract_wdt_path, "Path to .wdt file")->required();
    extract_wdt_cmd->add_option("--out", extract_wdt_out, "Output directory")->required();

    // ---- extract-all ------------------------------------------------
    auto* extract_all_cmd =
        app.add_subcommand("extract-all", "Scan and extract all game assets in one pass");
    std::string extract_all_game_dir;
    std::string extract_all_out;
    extract_all_cmd->add_option("game_dir", extract_all_game_dir, "Path to the game root directory")
        ->required();
    extract_all_cmd->add_option("--out", extract_all_out, "Output directory")->required();

    // ---- build-runtime-assets ---------------------------------------
    auto* build_runtime_assets_cmd = app.add_subcommand(
        "build-runtime-assets", "Build a validated canonical runtime asset package");
    std::string build_runtime_game_dir;
    std::string build_runtime_out;
    build_runtime_assets_cmd
        ->add_option("game_dir", build_runtime_game_dir, "Path to the game root directory")
        ->required();
    build_runtime_assets_cmd->add_option("--out", build_runtime_out, "Package output directory")
        ->required();
    std::vector<std::string> build_runtime_containers;
    build_runtime_assets_cmd
        ->add_option("--containers", build_runtime_containers,
                     "Only process specific containers (can be specified multiple times)")
        ->expected(-1);

    // ---- validate-runtime-assets ------------------------------------
    auto* validate_runtime_assets_cmd = app.add_subcommand(
        "validate-runtime-assets", "Validate an existing canonical runtime asset package");
    std::string validate_runtime_root;
    std::string validate_runtime_report;
    validate_runtime_assets_cmd
        ->add_option("asset_root", validate_runtime_root, "Runtime asset package root")
        ->required();
    validate_runtime_assets_cmd->add_option(
        "--report", validate_runtime_report,
        "Validation report path (default: <asset_root>/reports/validation_report.json)");

    // ---- compare-images ---------------------------------------------
    auto* compare_images_cmd = app.add_subcommand(
        "compare-images", "Pixel-exact comparison of extracted images against reference dataset");
    std::string cmp_actual;
    std::string cmp_expected;
    std::string cmp_report;
    std::string cmp_diffs;
    std::string cmp_game_dir;
    int         cmp_sample = 1000;
    bool        cmp_all = false;
    compare_images_cmd->add_option("--actual", cmp_actual, "Extracted output directory")
        ->required();
    compare_images_cmd->add_option("--expected", cmp_expected, "Reference dataset directory")
        ->required();
    compare_images_cmd->add_option("--report", cmp_report, "Output JSON report path")->required();
    compare_images_cmd->add_option("--sample", cmp_sample,
                                   "Number of images to compare (default: 1000)");
    compare_images_cmd->add_flag("--all", cmp_all, "Compare all matched images");
    compare_images_cmd->add_option("--diffs", cmp_diffs, "Directory for diff PNG images");
    compare_images_cmd->add_option("--game-dir", cmp_game_dir,
                                   "Game root for idx→name mapping (optional but recommended)");

    // ---- mqrc-inventory ---------------------------------------------
    auto* mqrc_inventory_cmd =
        app.add_subcommand("mqrc-inventory", "Write raw MQRC record inventory JSON");
    std::string mqrc_inventory_path;
    mqrc_inventory_cmd
        ->add_option("container", mqrc_inventory_path, "Path to .ff or .wdb MQDB file")
        ->required();

    // ---- battle-fx-report -------------------------------------------
    auto* battle_fx_report_cmd =
        app.add_subcommand("battle-fx-report", "Write battle FX research report JSON");
    std::string              battle_fx_container;
    std::string              battle_fx_game_root;
    std::vector<std::string> battle_fx_units;
    std::string              battle_fx_out;
    std::string              battle_fx_markdown;
    std::string              battle_fx_contact_sheet;
    battle_fx_report_cmd
        ->add_option("container", battle_fx_container, "Path to BatUnits.ff or another .ff")
        ->required();
    battle_fx_report_cmd->add_option("--game-root", battle_fx_game_root,
                                     "Game root for Globals/Gunits.dbf base-unit resolution");
    battle_fx_report_cmd->add_option("--unit", battle_fx_units, "Unit id such as UU0103")
        ->expected(-1)
        ->required();
    battle_fx_report_cmd->add_option("--out", battle_fx_out, "Output JSON path");
    battle_fx_report_cmd->add_option("--markdown", battle_fx_markdown, "Output markdown path");
    battle_fx_report_cmd->add_option("--contact-sheet", battle_fx_contact_sheet,
                                     "Optional contact sheet path placeholder");

    // ---- extract-viewer ---------------------------------------------
    auto* extract_viewer_cmd = app.add_subcommand(
        "extract-viewer",
        "Generate a self-contained HTML viewer from an extracted asset directory");
    std::string extract_viewer_src;
    std::string extract_viewer_out;
    extract_viewer_cmd
        ->add_option("--src", extract_viewer_src, "Source directory containing PNG/GIF/WAV files")
        ->required();
    extract_viewer_cmd->add_option("--out", extract_viewer_out, "Output HTML file path")
        ->required();

    // ---- inspect ----------------------------------------------------
    auto* inspect_cmd = app.add_subcommand(
        "inspect",
        "Inspect a runtime asset package (animation frames, sounds, data-tables, references)");
    std::string inspect_asset_root;
    std::string inspect_animation;
    std::string inspect_sound;
    std::string inspect_data_table;
    std::string inspect_row;
    std::string inspect_reference_table;
    std::string inspect_reference_row;
    std::string inspect_reference_target;
    inspect_cmd->add_option("asset_root", inspect_asset_root, "Runtime asset package root")
        ->required();
    inspect_cmd->add_option("--animation", inspect_animation, "Animation asset ID to inspect");
    inspect_cmd->add_option("--sound", inspect_sound, "Sound asset ID to inspect");
    inspect_cmd->add_option("--data-table", inspect_data_table, "Data-table asset ID to inspect");
    inspect_cmd->add_option("--row", inspect_row, "Data-table row key to select");
    inspect_cmd->add_option("--reference-table", inspect_reference_table,
                            "Reference source table asset ID");
    inspect_cmd->add_option("--reference-row", inspect_reference_row, "Reference source row key");
    inspect_cmd->add_option("--reference-target", inspect_reference_target,
                            "Reference target asset ID");

    // ---- sg-inspect -------------------------------------------------
    auto* sg_inspect_cmd =
        app.add_subcommand("sg-inspect", "Inspect a .sg scenario file and print parsed structures");
    std::string sg_inspect_path;
    bool        sg_inspect_summary = true;
    std::string sg_inspect_json;
    std::string sg_inspect_terrain_csv;
    std::string sg_inspect_objects_csv;
    std::string sg_inspect_globals;
    bool        sg_inspect_report_globals = false;
    bool        sg_inspect_annotate_ids = false;
    std::string sg_inspect_terrain_debug_csv;
    sg_inspect_cmd->add_option("file", sg_inspect_path, "Path to .sg scenario file")->required();
    sg_inspect_cmd->add_flag("--summary", sg_inspect_summary,
                             "Print human-readable summary (default: true)");
    sg_inspect_cmd->add_option("--dump-json", sg_inspect_json,
                               "Dump all parsed data as JSON to this file");
    sg_inspect_cmd->add_option("--dump-terrain-csv", sg_inspect_terrain_csv,
                               "Dump terrain tile values as CSV to this file");
    sg_inspect_cmd->add_option("--dump-objects-csv", sg_inspect_objects_csv,
                               "Dump object index as CSV to this file");
    sg_inspect_cmd->add_option(
        "--globals", sg_inspect_globals,
        "Game root (with Globals/), Globals directory, or dir with .dbf files");
    sg_inspect_cmd->add_flag("--report-global-ids", sg_inspect_report_globals,
                             "Print unresolved global ID report");
    sg_inspect_cmd->add_flag("--annotate-ids", sg_inspect_annotate_ids,
                             "Print annotated global ID list with DBF resolution");
    sg_inspect_cmd->add_option("--dump-terrain-debug-csv", sg_inspect_terrain_debug_csv,
                               "Dump terrain tiles with hex/debug info");

    // ------------------------------------------------------------------
    try {
        try {
            app.parse(argc, argv);
        } catch (const CLI::ParseError& e) {
            return app.exit(e);
        }

        d2log::init(log_config);

        if (list_cmd->parsed())
            return cmd_list(list_path, list_json, list_special);

        if (extract_cmd->parsed())
            return cmd_extract_raw(extract_path, extract_out, extract_overwrite);

        if (special_cmd->parsed())
            return cmd_extract_special(special_path, special_out, special_hexdump_limit);

        if (extract_images_cmd->parsed()) {
            return cmd_extract_images(extract_images_path, extract_images_out,
                                      extract_images_pattern);
        }

        if (extract_anim_cmd->parsed()) {
            return cmd_extract_anim(extract_anim_path, extract_anim_out, extract_anim_name,
                                    extract_anim_pattern, extract_anim_all, extract_anim_gif,
                                    extract_anim_delay, extract_anim_atlas,
                                    extract_anim_atlas_size);
        }

        if (extract_atlas_cmd->parsed()) {
            return cmd_extract_atlas(extract_atlas_path, extract_atlas_out, extract_atlas_pattern,
                                     extract_atlas_all, extract_atlas_max_size);
        }

        if (extract_sounds_cmd->parsed()) {
            return cmd_extract_sounds(extract_sounds_path, extract_sounds_out,
                                      extract_sounds_pattern, extract_sounds_all);
        }

        if (extract_viewer_cmd->parsed())
            return cmd_extract_viewer(extract_viewer_src, extract_viewer_out);

        if (scan_cmd->parsed())
            return cmd_scan(scan_game_dir, scan_out_dir);

        if (extract_dbf_cmd->parsed())
            return cmd_extract_dbf(extract_dbf_path, extract_dbf_out);

        if (extract_dlg_cmd->parsed())
            return cmd_extract_dlg(extract_dlg_path, extract_dlg_out);

        if (extract_dat_cmd->parsed())
            return cmd_extract_dat(extract_dat_path, extract_dat_out);

        if (extract_wdt_cmd->parsed())
            return cmd_extract_wdt(extract_wdt_path, extract_wdt_out);

        if (extract_all_cmd->parsed())
            return cmd_extract_all(extract_all_game_dir, extract_all_out);

        if (build_runtime_assets_cmd->parsed()) {
            return cmd_build_runtime_assets(build_runtime_game_dir, build_runtime_out,
                                            build_runtime_containers);
        }

        if (validate_runtime_assets_cmd->parsed())
            return cmd_validate_runtime_assets(validate_runtime_root, validate_runtime_report);

        if (compare_images_cmd->parsed()) {
            return cmd_compare_images(cmp_actual, cmp_expected, cmp_report, cmp_sample, cmp_all,
                                      cmp_diffs, cmp_game_dir);
        }

        if (mqrc_inventory_cmd->parsed())
            return cmd_mqrc_inventory(mqrc_inventory_path);

        if (battle_fx_report_cmd->parsed()) {
            return cmd_battle_fx_report(battle_fx_container, battle_fx_game_root, battle_fx_units,
                                        battle_fx_out, battle_fx_markdown, battle_fx_contact_sheet);
        }

        if (sg_inspect_cmd->parsed()) {
            return cmd_sg_inspect(sg_inspect_path, sg_inspect_summary, sg_inspect_json,
                                  sg_inspect_terrain_csv, sg_inspect_objects_csv,
                                  sg_inspect_globals, sg_inspect_report_globals,
                                  sg_inspect_annotate_ids, sg_inspect_terrain_debug_csv);
        }

        if (inspect_cmd->parsed()) {
            std::vector<std::string> args = {inspect_asset_root};
            if (!inspect_animation.empty()) {
                args.emplace_back("--animation");
                args.push_back(inspect_animation);
            }
            if (!inspect_sound.empty()) {
                args.emplace_back("--sound");
                args.push_back(inspect_sound);
            }
            if (!inspect_data_table.empty()) {
                args.emplace_back("--data-table");
                args.push_back(inspect_data_table);
            }
            if (!inspect_row.empty()) {
                args.emplace_back("--row");
                args.push_back(inspect_row);
            }
            if (!inspect_reference_table.empty()) {
                args.emplace_back("--reference-table");
                args.push_back(inspect_reference_table);
            }
            if (!inspect_reference_row.empty()) {
                args.emplace_back("--reference-row");
                args.push_back(inspect_reference_row);
            }
            if (!inspect_reference_target.empty()) {
                args.emplace_back("--reference-target");
                args.push_back(inspect_reference_target);
            }
            return d2asset::run_d2asset_inspect(args, std::cout);
        }

        return 0;
    } catch (const std::exception& e) {
        d2log::get("d2.app")->error("fatal: {}", e.what());
        return 1;
    } catch (...) {
        d2log::get("d2.app")->error("fatal: unknown exception");
        return 1;
    }
}

int main(int argc, char** argv) {
    try {
        return run_main(argc, argv);
    } catch (const std::exception& e) {
        d2log::write_fatal_stderr(e.what());
        return EXIT_FAILURE;
    } catch (...) {
        d2log::write_fatal_stderr("unknown exception");
        return EXIT_FAILURE;
    }
}
