#pragma once

#include "battle_screen_action.hpp"
#include "battle_tuning_state.hpp"
#include "screen_config_store.hpp"

#include <filesystem>

namespace d2engine {

class BattleTuningController {
public:
    explicit BattleTuningController(ScreenConfigStore& config_store)
        : config_store_(config_store) {}

    [[nodiscard]] bool enabled() const noexcept { return enabled_; }
    void               set_enabled(bool enabled) noexcept { enabled_ = enabled; }
    void               toggle_enabled();

    [[nodiscard]] BattleTuningState&       state() noexcept { return state_; }
    [[nodiscard]] const BattleTuningState& state() const noexcept { return state_; }

    bool apply_edit(const DebugTuningEditAction& action);

    void save();
    void revert_selected();
    void revert_all();

    void update_placement(const ConfigBinding& binding, const VisualPlacementValue& updated);

    // cppcheck-suppress unusedFunction
    void set_config_path(std::filesystem::path path) { config_path_ = std::move(path); }

private:
    bool                  enabled_ = false;
    BattleTuningState     state_;
    std::filesystem::path config_path_;
    ScreenConfigStore&    config_store_;
};

} // namespace d2engine
