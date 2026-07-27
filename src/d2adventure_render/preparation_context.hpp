#pragma once

// ── AdventureMapPreparationContext ──────────────────────────────────────
//
// Production dependency boundary for render contributors.
// Provides:
//   - shared AdventureMapGeometry reference
//   - AdventureRenderGraphBuilder (for adding primitives)
//   - diagnostic sink
//
// Contributors receive this context and MUST NOT access geometry or asset
// resolvers from global state.

#include "adventure_render_types.hpp"
#include "map_geometry.hpp"
#include "render_graph.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace d2engine::adventure_render {

// ── AdventureMapPreparationContext ──────────────────────────────────────
//
class PreparationContext {
public:
    PreparationContext(const AdventureMapGeometry& geometry, AdventureRenderGraphBuilder& builder)
        : geometry_(&geometry), builder_(&builder) {}

    // Accessors
    [[nodiscard]] const AdventureMapGeometry& geometry() const { return *geometry_; }

    // Primitive registration
    void add_primitive(const PreparedAdventureRenderPrimitive& prim) {
        builder_->add_primitive(prim);
    }
    void add_primitive(PreparedAdventureRenderPrimitive&& prim) {
        builder_->add_primitive(std::move(prim));
    }

    // Pick entry registration
    void add_pick_entry(PickEntry entry) { pick_entries_.push_back(std::move(entry)); }

    // Diagnostics
    void add_diagnostic(PrepareDiagnostic diag) { diagnostics_.push_back(std::move(diag)); }

    [[nodiscard]] const std::vector<PrepareDiagnostic>& diagnostics() const { return diagnostics_; }
    [[nodiscard]] std::vector<PrepareDiagnostic>        take_diagnostics() {
        return std::move(diagnostics_);
    }

    [[nodiscard]] std::vector<PickEntry> take_pick_entries() { return std::move(pick_entries_); }

private:
    const AdventureMapGeometry*    geometry_;
    AdventureRenderGraphBuilder*   builder_;
    std::vector<PickEntry>         pick_entries_;
    std::vector<PrepareDiagnostic> diagnostics_;
};

} // namespace d2engine::adventure_render
