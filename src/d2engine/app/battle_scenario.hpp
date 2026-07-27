#pragma once

#include "../battle_view/battle_scenario_data.hpp"

#include <filesystem>
#include <string>

namespace d2engine {

// Load a version 3 event-driven scenario from events.json.
// Strips leading // comment lines before JSON parsing.
BattleScenario load_battle_scenario(const std::filesystem::path& path);

// Strip leading // comments from scenario file content before the JSON object starts.
// Returns the content with leading comment lines removed.
std::string strip_scenario_comment_header(const std::string& raw);

} // namespace d2engine
