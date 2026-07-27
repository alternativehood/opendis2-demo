#include "adventure_launcher.hpp"
#include "cli_setup.hpp"
#include "launch_options.hpp"

#include <d2engine/app/application.hpp>
#include <d2log/log.hpp>

#include <CLI/CLI.hpp>

#include <cstdlib>
#include <exception>
#include <iostream>

namespace {

// Returns <source>/configs when the developer source checkout exists,
// otherwise returns an empty path (falling back to runtime resolution).
std::filesystem::path developer_source_config_root() {
    auto candidate = std::filesystem::path(OPENDIS2_SOURCE_DIR) / "configs";
    if (std::filesystem::is_directory(candidate))
        return candidate;
    return {};
}

} // namespace

// NOLINTNEXTLINE(bugprone-exception-escape)
int main(int argc, char* argv[]) {
    CLI::App app{"opendis2 — Disciples II reimplementation engine (production binary)\n"
                 "Development tools use the opendis2-dev-* naming convention."};

    opendis2::LaunchOptions opts;
    opendis2::setup_cli(app, opts);

    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        return app.exit(e);
    }

    switch (opts.mode) {
    case opendis2::LaunchOptions::Mode::BattleViewer: {
        d2log::init(opts.battle_viewer_config.log);
        opts.battle_viewer_config.config_root_override = developer_source_config_root();

        try {
            d2engine::Application viewer(opts.battle_viewer_config);
            viewer.start_battle_screen([&viewer]() { viewer.request_quit(); });
            return viewer.run();
        } catch (const std::exception& e) {
            d2log::get("d2.app")->error("fatal: {}", e.what());
            return EXIT_FAILURE;
        }
    }
    case opendis2::LaunchOptions::Mode::Adventure: {
        if (opts.headless) {
            return opendis2::run_headless_adventure(opts.scenario_path.string());
        }
        return opendis2::run_graphical_adventure(opts.scenario_path.string(), opts.game_root,
                                                 opts.debug_battle_script_path.string(),
                                                 developer_source_config_root());
    }
    case opendis2::LaunchOptions::Mode::None:
        break;
    }

    std::cout << app.help() << std::endl;
    return EXIT_SUCCESS;
}
