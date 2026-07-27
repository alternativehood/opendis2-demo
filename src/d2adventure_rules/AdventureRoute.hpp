#pragma once

#include <d2runtime/MapCellCoord.hpp>

#include <cstdint>
#include <vector>

namespace d2adventure {

struct AdventureRouteStep {
    d2runtime::MapCellCoord cell{};

    int delta_x = 0;
    int delta_y = 0;

    int          step_cost = 0;
    std::int64_t cumulative_cost = 0;

    bool operator==(const AdventureRouteStep&) const = default;
};

struct AdventureRoute {
    d2runtime::MapCellCoord start{};
    d2runtime::MapCellCoord destination{};

    std::vector<AdventureRouteStep> steps;

    std::int64_t total_cost = 0;

    [[nodiscard]] bool empty() const { return steps.empty(); }

    [[nodiscard]] std::size_t size() const { return steps.size(); }

    bool operator==(const AdventureRoute&) const = default;
};

} // namespace d2adventure
