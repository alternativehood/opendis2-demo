#pragma once

#include <d2adventure_rules/AdventureMovementProfile.hpp>
#include <d2adventure_rules/AdventureRoute.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace d2game {

enum class AdventureInteractionMode : std::uint8_t {
    Idle,
    StackSelected,
    RoutePlanned,
    Moving,
};

struct AdventurePlannedMovement {
    std::string                           stack_id;
    d2adventure::AdventureMovementProfile profile = d2adventure::AdventureMovementProfile::Walking;
    d2adventure::AdventureRoute           route;
    int                                   available_movement_points = 0;
    std::size_t                           affordable_step_count = 0;
    std::int64_t                          affordable_cost = 0;
    std::optional<std::size_t>            first_unaffordable_step_index;

    [[nodiscard]] bool fully_affordable() const {
        return affordable_step_count == route.steps.size();
    }
    bool operator==(const AdventurePlannedMovement&) const = default;
};

struct AdventureMovementExecution {
    std::string                           stack_id;
    d2adventure::AdventureMovementProfile profile = d2adventure::AdventureMovementProfile::Walking;
    d2adventure::AdventureRoute           route;
    d2runtime::MapCellCoord               requested_destination{};
    bool                                  limited_by_movement_points = false;
    std::size_t                           next_step_index = 0;
    std::int64_t                          spent_cost = 0;

    [[nodiscard]] bool complete() const { return next_step_index >= route.steps.size(); }
    bool               operator==(const AdventureMovementExecution&) const = default;
};

struct AdventureMovementState {
    AdventureInteractionMode                  mode = AdventureInteractionMode::Idle;
    std::optional<std::string>                selected_stack_id;
    std::optional<AdventurePlannedMovement>   planned;
    std::optional<AdventureMovementExecution> execution;

    [[nodiscard]] bool is_moving() const { return mode == AdventureInteractionMode::Moving; }
    bool               operator==(const AdventureMovementState&) const = default;
};

} // namespace d2game
