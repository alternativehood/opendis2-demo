#pragma once

#include <d2runtime/MapCellCoord.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace d2adventure {

enum class AdventureRouteMarkerKind : std::uint8_t {
    Normal,
    ActionLimit,
};

struct AdventureRoutePreviewStep {
    std::size_t              route_step_index = 0;
    d2runtime::MapCellCoord  cell{};
    AdventureRouteMarkerKind marker = AdventureRouteMarkerKind::Normal;
    std::optional<int>       remaining_movement_points;

    bool operator==(const AdventureRoutePreviewStep&) const = default;
};

struct AdventureRoutePreview {
    d2runtime::MapCellCoord                start{};
    d2runtime::MapCellCoord                destination{};
    std::vector<AdventureRoutePreviewStep> steps;

    bool operator==(const AdventureRoutePreview&) const = default;
};

} // namespace d2adventure
