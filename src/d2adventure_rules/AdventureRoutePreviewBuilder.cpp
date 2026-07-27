#include "AdventureRoutePreviewBuilder.hpp"

#include <cstdint>
#include <stdexcept>

namespace d2adventure {

AdventureRoutePreview
AdventureRoutePreviewBuilder::build(const AdventureRoute& route,
                                    const int             initial_available_movement_points,
                                    const std::optional<std::size_t> first_unaffordable_step_index,
                                    const std::size_t first_visible_step_index) const {
    if (initial_available_movement_points < 0)
        throw std::invalid_argument("initial movement points cannot be negative");
    if (first_visible_step_index > route.steps.size())
        throw std::invalid_argument("first visible route step index is out of range");
    if (route.steps.empty() && first_unaffordable_step_index)
        throw std::invalid_argument("empty route cannot have an unaffordable step");
    if (route.steps.empty() && route.start != route.destination)
        throw std::invalid_argument("empty route must have equal endpoints");
    if (first_unaffordable_step_index && *first_unaffordable_step_index >= route.steps.size())
        throw std::invalid_argument("unaffordable route step index is out of range");
    if (!route.steps.empty() && route.steps.back().cell != route.destination)
        throw std::invalid_argument("route does not end at its destination");

    auto                       previous = route.start;
    std::optional<std::size_t> calculated_first_unaffordable;
    std::int64_t               previous_cumulative = 0;
    for (std::size_t index = 0; index < route.steps.size(); ++index) {
        const auto& step = route.steps[index];
        if (index == 0 && step.cell == route.start)
            throw std::invalid_argument("route includes its starting cell");
        const auto delta_x = static_cast<std::int64_t>(step.cell.x) - previous.x;
        const auto delta_y = static_cast<std::int64_t>(step.cell.y) - previous.y;
        if (delta_x < -1 || delta_x > 1 || delta_y < -1 || delta_y > 1 ||
            (delta_x == 0 && delta_y == 0))
            throw std::invalid_argument("route contains a non-adjacent step");
        if (step.delta_x != delta_x || step.delta_y != delta_y)
            throw std::invalid_argument("route step delta does not match its coordinates");
        if (step.step_cost <= 0 || step.cumulative_cost <= previous_cumulative ||
            step.cumulative_cost - previous_cumulative != step.step_cost)
            throw std::invalid_argument("route contains invalid cumulative costs");
        if (step.cumulative_cost > initial_available_movement_points &&
            !calculated_first_unaffordable)
            calculated_first_unaffordable = index;
        previous = step.cell;
        previous_cumulative = step.cumulative_cost;
    }
    if (!route.steps.empty() && route.total_cost != previous_cumulative)
        throw std::invalid_argument("route total cost does not match final cumulative cost");
    if (route.steps.empty() && route.total_cost != 0)
        throw std::invalid_argument("empty route must have zero total cost");
    if (calculated_first_unaffordable != first_unaffordable_step_index)
        throw std::invalid_argument("inconsistent first unaffordable route step index");

    AdventureRoutePreview preview;
    preview.start = route.start;
    preview.destination = route.destination;
    preview.steps.reserve(route.steps.size() - first_visible_step_index);
    for (std::size_t index = first_visible_step_index; index < route.steps.size(); ++index) {
        const bool action_limit =
            first_unaffordable_step_index && index >= *first_unaffordable_step_index;
        std::optional<int> remaining;
        if (!action_limit) {
            const auto balance = static_cast<std::int64_t>(initial_available_movement_points) -
                                 route.steps[index].cumulative_cost;
            if (balance < 0)
                throw std::invalid_argument("negative remaining movement points");
            remaining = static_cast<int>(balance);
        }
        preview.steps.push_back(AdventureRoutePreviewStep{
            index, route.steps[index].cell,
            action_limit ? AdventureRouteMarkerKind::ActionLimit : AdventureRouteMarkerKind::Normal,
            remaining});
    }
    return preview;
}

} // namespace d2adventure
