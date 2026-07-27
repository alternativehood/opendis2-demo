#include "render_graph.hpp"

#include <algorithm>
#include <utility>

namespace d2engine::adventure_render {

void AdventureRenderGraphBuilder::add_primitive(const PreparedAdventureRenderPrimitive& prim) {
    primitives_.push_back(prim);
}

void AdventureRenderGraphBuilder::add_primitive(PreparedAdventureRenderPrimitive&& prim) {
    primitives_.push_back(std::move(prim));
}

void AdventureRenderGraphBuilder::reset() {
    primitives_.clear();
}

PreparedAdventureRenderGraph
AdventureRenderGraphBuilder::finalize(const IsoDepthResolver& depth_resolver) const {
    PreparedAdventureRenderGraph graph;

    for (const auto& prim : primitives_) {
        switch (prim.phase) {
        case AdventureRenderPhase::GroundOverlay:
            graph.ground_overlay.push_back(prim);
            break;
        case AdventureRenderPhase::World:
            graph.world.push_back(prim);
            break;
        case AdventureRenderPhase::WorldOverlay:
            graph.world_overlay.push_back(prim);
            break;
        case AdventureRenderPhase::Fog:
            graph.fog.push_back(prim);
            break;
        case AdventureRenderPhase::UIOverlay:
            graph.ui_overlay.push_back(prim);
            break;
        case AdventureRenderPhase::Terrain:
            // Terrain is handled externally via AdventureTerrainSurfaceComposer.
            // If a primitive has phase=Terrain in the generic pipeline, keep it
            // but the main render pass does not use it yet.
            break;
        }
    }

    // Order world primitives via depth resolver
    if (!graph.world.empty()) {
        const auto                                    order = depth_resolver.resolve(graph.world);
        std::vector<PreparedAdventureRenderPrimitive> ordered;
        ordered.reserve(order.size());
        for (const auto idx : order)
            ordered.push_back(std::move(graph.world[idx]));
        graph.world = std::move(ordered);
    }

    return graph;
}

} // namespace d2engine::adventure_render
