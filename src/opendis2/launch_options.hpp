#pragma once

#include <d2buildinfo/build_info.hpp>
#include <d2engine/app/app_config.hpp>

#include <filesystem>

namespace opendis2 {

inline constexpr auto kProjectVersion = d2buildinfo::kProjectVersion;

struct LaunchOptions {
    enum class Mode { None, BattleViewer, Adventure };

    Mode mode = Mode::None;

    std::filesystem::path scenario_path;
    std::string           game_root;
    std::filesystem::path debug_battle_script_path;
    bool                  headless = false;

    d2engine::AppConfig battle_viewer_config;
};

} // namespace opendis2
