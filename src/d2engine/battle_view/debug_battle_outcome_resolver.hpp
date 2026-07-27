#pragma once

#include "battle_selection_controller.hpp"

#include <vector>

namespace d2engine {

class DebugBattleOutcomeResolver {
public:
    [[nodiscard]] static std::vector<BattleVisualEvent>
    resolve(const BattleVisualEvent& emitted, const BattleSelectionModel& selection);
};

} // namespace d2engine
