#include "AdventureRoutePrefix.hpp"

#include <cstddef>
#include <stdexcept>

namespace d2adventure {

AdventureRoute adventure_route_prefix(const AdventureRoute& route, const std::size_t step_count) {
    if (step_count > route.steps.size())
        throw std::invalid_argument("route prefix exceeds route length");

    auto         previous = route.start;
    std::int64_t previous_cumulative = 0;
    for (const auto& step : route.steps) {
        const auto delta_x = step.cell.x - previous.x;
        const auto delta_y = step.cell.y - previous.y;
        if (delta_x < -1 || delta_x > 1 || delta_y < -1 || delta_y > 1 ||
            (delta_x == 0 && delta_y == 0) || step.delta_x != delta_x || step.delta_y != delta_y ||
            step.step_cost <= 0 || step.cumulative_cost <= previous_cumulative ||
            step.cumulative_cost - previous_cumulative != step.step_cost)
            throw std::invalid_argument("invalid adventure route");
        previous = step.cell;
        previous_cumulative = step.cumulative_cost;
    }
    if (route.steps.empty()) {
        if (route.destination != route.start || route.total_cost != 0)
            throw std::invalid_argument("invalid empty adventure route");
    } else if (route.destination != route.steps.back().cell ||
               route.total_cost != route.steps.back().cumulative_cost) {
        throw std::invalid_argument("adventure route destination or total cost mismatch");
    }

    AdventureRoute result;
    result.start = route.start;
    result.destination = route.start;
    result.steps.assign(route.steps.begin(),
                        route.steps.begin() + static_cast<std::ptrdiff_t>(step_count));
    if (step_count > 0) {
        result.destination = result.steps.back().cell;
        result.total_cost = result.steps.back().cumulative_cost;
    }
    return result;
}

} // namespace d2adventure
