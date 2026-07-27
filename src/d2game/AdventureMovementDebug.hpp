#pragma once

#include "AdventureMovementState.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace d2game {

inline constexpr int kAdventureDebugFreeMovementPoints = 1024 * 1024;

enum class AdventureMovementDebugChange : std::uint8_t {
    Reset,
    Free,
};

struct AdventureMovementDebugSnapshot {
    AdventureInteractionMode   mode = AdventureInteractionMode::Idle;
    std::optional<std::string> selected_stack_id;
    std::optional<int>         current_movement_points;
    std::optional<int>         reset_movement_points;

    [[nodiscard]] bool can_edit() const {
        return selected_stack_id.has_value() && current_movement_points.has_value() &&
               reset_movement_points.has_value() && mode != AdventureInteractionMode::Moving;
    }
};

} // namespace d2game
