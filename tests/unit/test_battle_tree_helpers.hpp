#pragma once

#include "d2engine/app/battle_screen.hpp"
#include "d2engine/app/screen_config_store.hpp"
#include "d2engine/render/render_tree.hpp"

#include <filesystem>

namespace d2engine {
namespace test {

[[nodiscard]] inline ScreenConfig load_battle_screen_config() {
    ScreenConfigStore store(std::filesystem::path(OPENDIS2_SOURCE_DIR) / "configs");
    return store.load_validated("battle_screen", BattleScreen::required_layout_nodes());
}

[[nodiscard]] inline TreeLayout load_battle_tree_layout() {
    auto config = load_battle_screen_config();
    return std::move(config.tree_layout);
}

} // namespace test
} // namespace d2engine
