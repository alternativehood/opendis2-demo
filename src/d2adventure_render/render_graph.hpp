#pragma once

// ── AdventureRenderGraphBuilder / PreparedAdventureRenderGraph ──────────
//
// Builder pattern:
//   1. Contributors add primitives via add_primitive()
//   2. finalize(depth_resolver) groups, orders, and produces an immutable
//      PreparedAdventureRenderGraph
//
// The graph separates phases into ordered vectors.
// World primitives pass through IsoDepthResolver during finalization.

#include "adventure_render_types.hpp"
#include "iso_depth_resolver.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace d2engine::adventure_render {

// ── Prepared render graph (immutable after finalize) ───────────────────
//
struct PreparedAdventureRenderGraph {
    std::vector<PreparedAdventureRenderPrimitive> ground_overlay;
    std::vector<PreparedAdventureRenderPrimitive> world;
    std::vector<PreparedAdventureRenderPrimitive> world_overlay;
    std::vector<PreparedAdventureRenderPrimitive> fog;
    std::vector<PreparedAdventureRenderPrimitive> ui_overlay;

    // Semantic pick metadata (associated by stable_id).
    // Populated by contributors, not parsed from debug_label.
    std::vector<PickEntry> pick_entries;

    [[nodiscard]] bool empty() const {
        return ground_overlay.empty() && world.empty() && world_overlay.empty() && fog.empty() &&
               ui_overlay.empty();
    }
};

// ── Render graph builder ───────────────────────────────────────────────
//
class AdventureRenderGraphBuilder {
public:
    void add_primitive(const PreparedAdventureRenderPrimitive& prim);
    void add_primitive(PreparedAdventureRenderPrimitive&& prim);

    // Finalize: group by phase, order World via resolver, produce immutable graph.
    [[nodiscard]] PreparedAdventureRenderGraph
    finalize(const IsoDepthResolver& depth_resolver) const;

    void reset();

private:
    std::vector<PreparedAdventureRenderPrimitive> primitives_;
};

} // namespace d2engine::adventure_render
