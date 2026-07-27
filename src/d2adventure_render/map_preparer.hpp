#pragma once

#include "adventure_render_types.hpp"
#include "map_geometry.hpp"
#include "preparation_context.hpp"
#include "prepared_adventure_map.hpp"
#include "render_graph.hpp"
#include "terrain/adventure_terrain_surface.hpp"
#include "terrain/terrain_asset_catalog.hpp"

#include <d2runtime/AdventureIsoDirection.hpp>
#include <d2runtime/AdventureWorldState.hpp>

#include "terrain/tree_asset_catalog.hpp"
#include "terrain/mountain_asset_catalog.hpp"
#include "terrain/landmark_asset_catalog.hpp"
#include "terrain/road_asset_catalog.hpp"
#include "stack_banner_asset_catalog.hpp"

#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace d2engine::adventure_render {

// ── Contributor function type ──────────────────────────────────────────
using RenderContributor =
    std::function<void(const d2runtime::AdventureWorldState&, PreparationContext&)>;

using StackActorVisualResolver = std::function<std::optional<AdventureActorVisual>(
    const d2runtime::AdventureStack& stack, const d2runtime::AdventureUnitInstance& leader)>;

// ── Prepare result ─────────────────────────────────────────────────────
struct PrepareResult {
    PreparedAdventureRenderGraph   graph;
    std::vector<PickEntry>         pick_entries;
    std::vector<PrepareDiagnostic> diagnostics;

    [[nodiscard]] bool empty() const { return graph.empty() && diagnostics.empty(); }
    [[nodiscard]] bool has_unresolved() const {
        for (const auto& d : diagnostics) {
            if (d.kind != PrepareDiagnosticKind::Resolved) {
                return true;
            }
        }
        return false;
    }
};

// ── AdventureMapPreparer ────────────────────────────────────────────────
//
// Owns the contributor chain, the shared geometry, and (optionally) terrain
// preparation.  When terrain dependencies are provided, prepare_full()
// produces a PreparedAdventureMap containing both terrain and world graph.
class AdventureMapPreparer {
public:
    explicit AdventureMapPreparer(const AdventureMapGeometry& geometry);

    // Register terrain composer for full map preparation.
    // Must be called before prepare_full() if terrain is desired.
    void set_terrain_composer(const AdventureTerrainSurfaceComposer& composer,
                              const TerrainAssetCatalog&             catalog);

    // Add a contributor at the end of the chain.
    void add_contributor(RenderContributor contributor);

    // Prepare only world primitives (no terrain).  Returns PrepareResult.
    [[nodiscard]] PrepareResult prepare(const d2runtime::AdventureWorldState& world) const;

    // Full map preparation: world primitives + terrain (if composer set).
    // Produces a PreparedAdventureMap that owns both terrain and world graph.
    [[nodiscard]] PreparedAdventureMap
    prepare_full(const d2runtime::AdventureWorldState&        world,
                 const AdventureTerrainSurfaceInput&          terrain_input,
                 const AdventureTerrainSurfaceComposeOptions& options = {}) const;

    [[nodiscard]] const AdventureMapGeometry& geometry() const { return *geometry_; }

private:
    const AdventureMapGeometry*            geometry_;
    const AdventureTerrainSurfaceComposer* composer_ = nullptr;
    const TerrainAssetCatalog*             catalog_ = nullptr;
    std::vector<RenderContributor>         contributors_;
};

// ── Actor visual validation ─────────────────────────────────────────────
// Validates all fields of a resolved AdventureActorVisual (both body and optional shadow).
// Throws std::runtime_error with full identity on any invalid field.
// Called by the stack actor contributor.
void validate_adventure_actor_visual_or_throw(const d2runtime::AdventureStack&        stack,
                                              const d2runtime::AdventureUnitInstance& leader,
                                              const AdventureActorVisual&             visual);

// ── Built-in contributor factories ─────────────────────────────────────
RenderContributor make_road_contributor(const RoadAssetCatalog& catalog);
RenderContributor make_mountain_contributor(const MountainAssetCatalog& catalog);
RenderContributor make_landmark_contributor(const LandmarkAssetCatalog& catalog);
RenderContributor
make_stack_actor_contributor(StackActorVisualResolver       resolver,
                             const StackBannerAssetCatalog* banner_catalog = nullptr);
RenderContributor
make_forest_contributor(const TreeAssetCatalog&                                       tree_catalog,
                        const std::vector<d2runtime::AdventureTerrainTileDescriptor>& descriptors);

} // namespace d2engine::adventure_render
