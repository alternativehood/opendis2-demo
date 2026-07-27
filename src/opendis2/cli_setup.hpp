#pragma once

#include "launch_options.hpp"

#include <d2buildinfo/build_info.hpp>

#include "battle_viewer_cli.hpp"

#include <CLI/CLI.hpp>

#include <string>

namespace opendis2 {

inline void setup_cli(CLI::App& app, LaunchOptions& options) {
    app.set_version_flag("--version", d2buildinfo::format_build_version());
    app.require_subcommand(0, 1);

    auto* bv_cmd = app.add_subcommand("battle-viewer", "Launch the battle viewer");
    bv_cmd->callback([&options] { options.mode = LaunchOptions::Mode::BattleViewer; });
    d2battle_viewer::configure_cli(*bv_cmd, options.battle_viewer_config);

    auto* adv_cmd = app.add_subcommand("adventure", "Launch adventure mode");
    adv_cmd->callback([&options] { options.mode = LaunchOptions::Mode::Adventure; });
    adv_cmd->add_option("--scenario", options.scenario_path, "Path to .sg scenario file")
        ->required()
        ->check(CLI::ExistingFile);
    adv_cmd->add_option("--game-root", options.game_root, "Path to Disciples II game root")
        ->envname("DISCIPLES2_GAME_ROOT");
    adv_cmd
        ->add_option("--debug-battle-script", options.debug_battle_script_path,
                     "Developer-only battle script used by the Adventure -> Battle transition")
        ->check(CLI::ExistingFile);
    adv_cmd->add_flag("--headless", options.headless, "Run without SDL window (CI/debug)");
}

} // namespace opendis2
