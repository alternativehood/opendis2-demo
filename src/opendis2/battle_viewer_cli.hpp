#pragma once

#include <d2engine/app/app_config.hpp>

#include <CLI/CLI.hpp>

#include <string>

namespace d2battle_viewer {

inline void configure_cli(CLI::App& app, d2engine::AppConfig& config) {
    CLI::Validator const non_empty_game_root{
        [](std::string& value) {
            return value.empty() ? std::string{"game root must not be empty"} : std::string{};
        },
        "non-empty"};
    app.add_option("--game-root", config.game_root,
                   "Path to game root directory (for direct .ff loading)")
        ->required()
        ->check(non_empty_game_root);
    app.add_flag("--fullscreen", config.fullscreen, "Start in fullscreen mode");
    app.add_option("--scale", config.scale, "Window scale factor (default: 1.0)")
        ->default_val(1.0f)
        ->check(CLI::Range(0.1f, 10.0f));
    app.add_option("--battle-script", config.battle_script_path, "Path to v3 scenario events.json")
        ->required();
    app.add_flag("--strict-planned-assets", config.strict_planned_assets,
                 "Fail on any render-path texture miss after full preload (no lazy decode)");
    app.add_option("--log-level", config.log.level,
                   "Log level: trace|debug|info|warn|error|off (default: info, env: D2_LOG_LEVEL)");
    app.add_option("--log-file", config.log.file,
                   "Write logs to file in addition to stderr (env: D2_LOG_FILE)");
    // ponytail: add_option_function avoids dangling-ref bug (local mode_str dies before parse)
    app.add_option_function<std::string>(
           "--mode", [&config](const std::string& v) { config.debug_mode = (v == "debug"); },
           "Viewer mode: normal|debug (default: normal)")
        ->check(CLI::IsMember({"normal", "debug"}))
        ->default_val("normal");
}

} // namespace d2battle_viewer
