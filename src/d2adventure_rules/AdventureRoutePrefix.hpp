#pragma once

#include "AdventureRoute.hpp"

#include <cstddef>

namespace d2adventure {

[[nodiscard]] AdventureRoute adventure_route_prefix(const AdventureRoute& route,
                                                    std::size_t           step_count);

} // namespace d2adventure
