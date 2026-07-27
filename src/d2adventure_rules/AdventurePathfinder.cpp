#include "AdventurePathfinder.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <queue>
#include <utility>
#include <vector>

namespace d2adventure {

namespace {

constexpr std::array<d2runtime::MapCellCoord, 8> kNeighborDeltas = {
    d2runtime::MapCellCoord{0, -1}, d2runtime::MapCellCoord{1, -1},  d2runtime::MapCellCoord{1, 0},
    d2runtime::MapCellCoord{1, 1},  d2runtime::MapCellCoord{0, 1},   d2runtime::MapCellCoord{-1, 1},
    d2runtime::MapCellCoord{-1, 0}, d2runtime::MapCellCoord{-1, -1},
};

struct OpenEntry {
    std::size_t   index = 0;
    std::int64_t  g = 0;
    std::int64_t  h = 0;
    std::int64_t  f = 0;
    std::uint64_t sequence = 0;
};

struct OpenEntryCompare {
    bool operator()(const OpenEntry& lhs, const OpenEntry& rhs) const {
        if (lhs.f != rhs.f)
            return lhs.f > rhs.f;
        if (lhs.h != rhs.h)
            return lhs.h > rhs.h;
        return lhs.sequence > rhs.sequence;
    }
};

std::size_t index_for(const d2runtime::MapCellCoord cell, const int width) {
    return static_cast<std::size_t>(cell.y) * static_cast<std::size_t>(width) +
           static_cast<std::size_t>(cell.x);
}

d2runtime::MapCellCoord cell_for(const std::size_t index, const int width) {
    return {static_cast<int>(index % static_cast<std::size_t>(width)),
            static_cast<int>(index / static_cast<std::size_t>(width))};
}

std::int64_t chebyshev_distance(const d2runtime::MapCellCoord from,
                                const d2runtime::MapCellCoord to) {
    const auto dx = std::abs(static_cast<std::int64_t>(from.x) - to.x);
    const auto dy = std::abs(static_cast<std::int64_t>(from.y) - to.y);
    return std::max(dx, dy);
}

std::int64_t minimum_passable_step_cost(const AdventureMovementProfile profile) {
    constexpr std::array<AdventureTraversalCell, 6> synthetic_cells = {
        AdventureTraversalCell{d2runtime::AdventureGroundType::Plain, false},
        AdventureTraversalCell{d2runtime::AdventureGroundType::Plain, true},
        AdventureTraversalCell{d2runtime::AdventureGroundType::Forest, false},
        AdventureTraversalCell{d2runtime::AdventureGroundType::Forest, true},
        AdventureTraversalCell{d2runtime::AdventureGroundType::Water, false},
        AdventureTraversalCell{d2runtime::AdventureGroundType::Water, true},
    };

    std::int64_t minimum = std::numeric_limits<std::int64_t>::max();
    for (const auto& cell : synthetic_cells) {
        const auto decision = evaluate_adventure_movement_cell(profile, cell);
        if (decision.passable && decision.movement_cost > 0)
            minimum = std::min(minimum, static_cast<std::int64_t>(decision.movement_cost));
    }
    return minimum == std::numeric_limits<std::int64_t>::max() ? 0 : minimum;
}

std::int64_t heuristic(const d2runtime::MapCellCoord cell,
                       const d2runtime::MapCellCoord destination,
                       const std::int64_t            minimum_step_cost) {
    return chebyshev_distance(cell, destination) * minimum_step_cost;
}

AdventurePathResult no_path() {
    return {AdventurePathStatus::NoPath, std::nullopt, AdventureMovementBlockReason::None};
}

std::optional<AdventureRoute>
reconstruct_route(const std::vector<std::int64_t>& predecessor,
                  const std::vector<std::int64_t>& best_g, const std::size_t start_index,
                  const std::size_t destination_index, const int width,
                  const d2runtime::MapCellCoord start, const d2runtime::MapCellCoord destination) {
    std::vector<std::size_t> reversed;
    auto                     current = destination_index;
    for (std::size_t count = 0; count <= predecessor.size(); ++count) {
        reversed.push_back(current);
        if (current == start_index)
            break;
        const auto previous = predecessor[current];
        if (previous < 0 || static_cast<std::size_t>(previous) >= predecessor.size())
            return std::nullopt;
        current = static_cast<std::size_t>(previous);
    }
    if (reversed.back() != start_index)
        return std::nullopt;
    std::reverse(reversed.begin(), reversed.end());

    AdventureRoute route;
    route.start = start;
    route.destination = destination;
    d2runtime::MapCellCoord previous_cell = start;
    std::int64_t            previous_cost = 0;
    for (std::size_t position = 1; position < reversed.size(); ++position) {
        const auto current_cell = cell_for(reversed[position], width);
        const auto delta_x = current_cell.x - previous_cell.x;
        const auto delta_y = current_cell.y - previous_cell.y;
        const auto cumulative_cost = best_g[reversed[position]];
        const auto step_cost = cumulative_cost - previous_cost;
        if (delta_x < -1 || delta_x > 1 || delta_y < -1 || delta_y > 1 ||
            (delta_x == 0 && delta_y == 0) || step_cost <= 0 ||
            step_cost > std::numeric_limits<int>::max() || cumulative_cost <= previous_cost)
            return std::nullopt;
        route.steps.push_back(
            {current_cell, delta_x, delta_y, static_cast<int>(step_cost), cumulative_cost});
        previous_cell = current_cell;
        previous_cost = cumulative_cost;
    }
    if (route.steps.empty() || route.steps.back().cell != destination ||
        route.steps.back().cumulative_cost != best_g[destination_index])
        return std::nullopt;
    route.total_cost = best_g[destination_index];
    return route;
}

} // namespace

AdventurePathResult AdventurePathfinder::find_path(const AdventureNavigationMap&  map,
                                                   const d2runtime::MapCellCoord  start,
                                                   const d2runtime::MapCellCoord  destination,
                                                   const AdventureMovementProfile profile,
                                                   const std::string_view moving_stack_id) const {
    if (map.empty())
        return {AdventurePathStatus::EmptyNavigationMap, std::nullopt,
                AdventureMovementBlockReason::None};
    if (!map.contains(start))
        return {AdventurePathStatus::StartOutOfBounds, std::nullopt,
                AdventureMovementBlockReason::None};
    if (!map.contains(destination))
        return {AdventurePathStatus::DestinationOutOfBounds, std::nullopt,
                AdventureMovementBlockReason::None};

    const auto start_cell = map.traversal_cell_at(start, moving_stack_id);
    const auto start_decision = evaluate_adventure_movement_cell(profile, *start_cell);
    if (!start_decision.passable)
        return {AdventurePathStatus::StartBlocked, std::nullopt, start_decision.block_reason};

    const auto destination_cell = map.traversal_cell_at(destination, moving_stack_id);
    const auto destination_decision = evaluate_adventure_movement_cell(profile, *destination_cell);
    if (!destination_decision.passable)
        return {AdventurePathStatus::DestinationBlocked, std::nullopt,
                destination_decision.block_reason};

    if (start == destination) {
        AdventureRoute route;
        route.start = start;
        route.destination = destination;
        return {AdventurePathStatus::AlreadyAtDestination, std::move(route),
                AdventureMovementBlockReason::None};
    }

    const auto                minimum_step_cost = minimum_passable_step_cost(profile);
    const auto                infinity = std::numeric_limits<std::int64_t>::max();
    std::vector<std::int64_t> best_g(map.size(), infinity);
    std::vector<std::int64_t> predecessor(map.size(), -1);
    std::vector<bool>         closed(map.size(), false);
    std::priority_queue<OpenEntry, std::vector<OpenEntry>, OpenEntryCompare> open;
    std::uint64_t                                                            sequence = 0;
    const auto start_index = index_for(start, map.width());
    const auto destination_index = index_for(destination, map.width());
    best_g[start_index] = 0;
    const auto start_h = heuristic(start, destination, minimum_step_cost);
    open.push({start_index, 0, start_h, start_h, sequence++});

    while (!open.empty()) {
        const auto current = open.top();
        open.pop();
        if (current.g != best_g[current.index] || closed[current.index])
            continue;
        closed[current.index] = true;
        if (current.index == destination_index) {
            auto route = reconstruct_route(predecessor, best_g, start_index, destination_index,
                                           map.width(), start, destination);
            if (!route)
                return no_path();
            return {AdventurePathStatus::Found, std::move(route),
                    AdventureMovementBlockReason::None};
        }

        const auto current_cell = cell_for(current.index, map.width());
        for (const auto delta : kNeighborDeltas) {
            const d2runtime::MapCellCoord neighbor{current_cell.x + delta.x,
                                                   current_cell.y + delta.y};
            if (!map.contains(neighbor))
                continue;
            const auto traversal = map.traversal_cell_at(neighbor, moving_stack_id);
            const auto decision = evaluate_adventure_movement_cell(profile, *traversal);
            if (!decision.passable)
                continue;
            const auto edge_cost = static_cast<std::int64_t>(decision.movement_cost);
            if (current.g > infinity - edge_cost)
                continue;
            const auto neighbor_index = index_for(neighbor, map.width());
            const auto tentative_g = current.g + edge_cost;
            if (tentative_g >= best_g[neighbor_index])
                continue;
            best_g[neighbor_index] = tentative_g;
            predecessor[neighbor_index] = static_cast<std::int64_t>(current.index);
            const auto h = heuristic(neighbor, destination, minimum_step_cost);
            open.push({neighbor_index, tentative_g, h, tentative_g + h, sequence++});
        }
    }
    return no_path();
}

} // namespace d2adventure
