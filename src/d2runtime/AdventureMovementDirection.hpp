#pragma once

#include "AdventureIsoDirection.hpp"

namespace d2runtime {

[[nodiscard]] AdventureIsoDirection adventure_direction_for_delta(int delta_x, int delta_y);

} // namespace d2runtime
