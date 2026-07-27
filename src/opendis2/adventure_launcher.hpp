#pragma once

#include <filesystem>
#include <string>

namespace opendis2 {

int run_headless_adventure(const std::string& scenario_path);
int run_graphical_adventure(const std::string& scenario_path, const std::string& game_root,
                            const std::string&    debug_battle_script_path = {},
                            std::filesystem::path config_root_override = {});

} // namespace opendis2
