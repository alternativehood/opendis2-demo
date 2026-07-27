#pragma once

#include "AdventureNavigationMap.hpp"

#include <d2runtime/AdventureWorldState.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace d2adventure {

enum class AdventureNavigationDiagnosticKind : std::uint8_t {
    InvalidMapDimensions,
    TerrainDimensionsMismatch,
    TerrainTileCountMismatch,
    RoadOutOfBounds,
    BlockingObjectMissingFootprint,
    BlockingObjectFootprintOutOfBounds,
    OverlappingBlockingObjects,
    StackOutOfBounds,
    OverlappingStacks,
    StackOnBlockingObject,
};

struct AdventureNavigationDiagnostic {
    AdventureNavigationDiagnosticKind kind{};
    d2runtime::MapCellCoord           cell{};
    std::string                       object_id;
    std::string                       conflicting_object_id;
    std::string                       message;
};

[[nodiscard]] bool is_adventure_navigation_error(AdventureNavigationDiagnosticKind kind);

struct AdventureNavigationMapBuildResult {
    std::optional<AdventureNavigationMap>      map;
    std::vector<AdventureNavigationDiagnostic> diagnostics;

    [[nodiscard]] std::size_t error_count() const;
    [[nodiscard]] std::size_t warning_count() const;
    [[nodiscard]] bool        success() const;
};

class AdventureNavigationMapBuilder {
public:
    [[nodiscard]] AdventureNavigationMapBuildResult
    build(const d2runtime::AdventureWorldState& world) const;
};

} // namespace d2adventure
