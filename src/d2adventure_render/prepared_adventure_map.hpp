#pragma once

#include "adventure_render_types.hpp"
#include "map_geometry.hpp"
#include "render_graph.hpp"

#include <d2adventure_render/terrain/adventure_terrain_surface.hpp>

#include <optional>
#include <unordered_map>
#include <vector>

namespace d2engine::adventure_render {

struct PreparedAdventureMap {
    // Move-only — contains internal pointer relationships that would dangle
    // on default copy (terrain commands reference geometry, ground fields, etc.)
    PreparedAdventureMap() = default;
    ~PreparedAdventureMap() = default;
    PreparedAdventureMap(const PreparedAdventureMap&) = delete;
    PreparedAdventureMap& operator=(const PreparedAdventureMap&) = delete;
    PreparedAdventureMap(PreparedAdventureMap&&) = default;
    PreparedAdventureMap& operator=(PreparedAdventureMap&&) = default;

    AdventureMapGeometry geometry;

    std::optional<PreparedAdventureTerrainMap> terrain;

    PreparedAdventureRenderGraph world_graph;

    // Semantic pick entries (stable_id → object_id mapping).
    // Populated by contributors, NOT parsed from debug_label.
    std::vector<PickEntry> pick_entries;

    // Diagnostics from the prepare phase (unresolved objects, etc.).
    std::vector<PrepareDiagnostic> diagnostics;

    // Canonical cell interaction metrics for coarse hover.
    CellInteractionMetrics cell_interaction_metrics;

    [[nodiscard]] bool empty() const { return !terrain.has_value() && world_graph.empty(); }

    // cppcheck-suppress unusedFunction
    [[nodiscard]] bool has_terrain() const { return terrain.has_value(); }

    [[nodiscard]] int canvas_width() const { return geometry.canvas_width; }
    [[nodiscard]] int canvas_height() const { return geometry.canvas_height; }
};

} // namespace d2engine::adventure_render
