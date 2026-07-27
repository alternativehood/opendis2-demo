#include "AdventureNavigationMap.hpp"

#include <utility>

namespace d2adventure {

AdventureNavigationMap::AdventureNavigationMap(int width, int height,
                                               std::vector<AdventureNavigationCell> cells)
    : width_(width), height_(height), cells_(std::move(cells)) {}

bool AdventureNavigationMap::contains(d2runtime::MapCellCoord cell) const {
    return cell.x >= 0 && cell.y >= 0 && cell.x < width_ && cell.y < height_;
}

const AdventureNavigationCell* AdventureNavigationMap::cell_at(d2runtime::MapCellCoord cell) const {
    if (!contains(cell))
        return nullptr;
    return &cells_[static_cast<std::size_t>(cell.y) * static_cast<std::size_t>(width_) +
                   static_cast<std::size_t>(cell.x)];
}

std::optional<AdventureTraversalCell>
AdventureNavigationMap::traversal_cell_at(d2runtime::MapCellCoord cell,
                                          std::string_view        ignored_stack_id) const {
    const auto* navigation_cell = cell_at(cell);
    if (navigation_cell == nullptr)
        return std::nullopt;

    AdventureTraversalCell result;
    result.ground = navigation_cell->ground;
    result.has_road = navigation_cell->has_road;
    if (navigation_cell->blocking_object_id.has_value()) {
        result.occupancy = AdventureCellOccupancy::BlockingObject;
    } else if (navigation_cell->occupying_stack_id.has_value() &&
               (ignored_stack_id.empty() ||
                navigation_cell->occupying_stack_id.value() != ignored_stack_id)) {
        result.occupancy = AdventureCellOccupancy::OccupiedByStack;
    }
    return result;
}

} // namespace d2adventure
