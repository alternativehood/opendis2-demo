#pragma once

#include "AdventureRoutePreview.hpp"

#include <d2adventure_rules/AdventureRoute.hpp>

#include <cstddef>
#include <optional>

namespace d2adventure {

class AdventureRoutePreviewBuilder {
public:
    [[nodiscard]] AdventureRoutePreview
    build(const AdventureRoute& route, int initial_available_movement_points,
          std::optional<std::size_t> first_unaffordable_step_index,
          std::size_t                first_visible_step_index = 0) const;
};

} // namespace d2adventure
