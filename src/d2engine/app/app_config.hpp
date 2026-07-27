#pragma once

#include "d2log/log.hpp"

#include <filesystem>
#include <string>

namespace d2engine {

struct AppConfig {
    std::string           game_root;
    std::string           scenario_path;
    std::string           battle_script_path;
    bool                  fullscreen = false;
    float                 scale = 1.0f;
    int                   logical_width = 1416;
    int                   logical_height = 852;
    bool                  strict_planned_assets = false;
    bool                  debug_mode = false;
    std::filesystem::path config_root_override;
    d2log::LogConfig      log;
};

} // namespace d2engine
