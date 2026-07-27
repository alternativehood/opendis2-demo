#include "AdventureMovementDirection.hpp"

#include <stdexcept>

namespace d2runtime {

AdventureIsoDirection adventure_direction_for_delta(const int delta_x, const int delta_y) {
    if (delta_x < -1 || delta_x > 1 || delta_y < -1 || delta_y > 1 ||
        (delta_x == 0 && delta_y == 0))
        throw std::invalid_argument("invalid adventure movement delta");
    if (delta_x == 1 && delta_y == 0)
        return AdventureIsoDirection::D6;
    if (delta_x == 1 && delta_y == -1)
        return AdventureIsoDirection::D5;
    if (delta_x == 0 && delta_y == -1)
        return AdventureIsoDirection::D4;
    if (delta_x == -1 && delta_y == -1)
        return AdventureIsoDirection::D3;
    if (delta_x == -1 && delta_y == 0)
        return AdventureIsoDirection::D2;
    if (delta_x == -1 && delta_y == 1)
        return AdventureIsoDirection::D1;
    if (delta_x == 0 && delta_y == 1)
        return AdventureIsoDirection::D0;
    return AdventureIsoDirection::D7;
}

} // namespace d2runtime
