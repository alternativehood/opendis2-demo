#pragma once

#include <compare>

namespace d2runtime {

// Canonical map-cell coordinate.
// After AdventureWorldState construction, every terrain cell and world object
// uses the same coordinate domain.  (x,y) means one physical scenario cell.
struct MapCellCoord {
    int x = 0;
    int y = 0;

    bool operator==(const MapCellCoord&) const = default;
    auto operator<=>(const MapCellCoord&) const = default;
};

} // namespace d2runtime
