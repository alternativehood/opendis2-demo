#pragma once
#include "../assets/game_data_registry.hpp"
#include "../assets/sprite_animation_catalog.hpp"
#include "../assets/adventure_stack_actor_request_resolver.hpp"
#include <d2adventure_render/stack_banner_asset_catalog.hpp>
#include <d2adventure_render/adventure_render_types.hpp>
#include <d2game/AdventureMovementState.hpp>
#include <d2runtime/AdventureWorldState.hpp>

#include <memory>

namespace d2engine {
class AssetRuntime;
class RenderAssetRuntime;
struct AdventureMovementSegmentVisual {
    std::size_t                            route_step_index = 0;
    d2runtime::MapCellCoord                from{};
    d2runtime::MapCellCoord                to{};
    d2runtime::AdventureIsoDirection       direction = d2runtime::AdventureIsoDirection::D0;
    adventure_render::AdventureActorVisual move_visual;
    adventure_render::AdventureActorVisual idle_visual_at_destination;
    std::shared_ptr<const adventure_render::InteractionMask> idle_interaction_mask_at_destination;
};
struct AdventureMovementVisualPlan {
    std::string                                 stack_id;
    d2adventure::AdventureRoute                 route;
    adventure_render::StackBannerAsset          banner_asset;
    int                                         banner_index = 0;
    std::vector<AdventureMovementSegmentVisual> segments;
};
void preload_adventure_movement_visual_plan(AdventureMovementVisualPlan& plan, AssetRuntime& assets,
                                            RenderAssetRuntime& render_assets);
class AdventureMovementVisualPlanBuilder {
public:
    AdventureMovementVisualPlanBuilder(
        const GameDataRegistry& game_data, const ISpriteAnimationCatalog& catalog,
        const adventure_render::StackBannerAssetCatalog& banner_catalog)
        : game_data_(&game_data), catalog_(&catalog), banner_catalog_(&banner_catalog) {}
    [[nodiscard]] AdventureMovementVisualPlan
    build(const d2runtime::AdventureWorldState&     world,
          const d2game::AdventureMovementExecution& execution) const;

private:
    const GameDataRegistry*                          game_data_;
    const ISpriteAnimationCatalog*                   catalog_;
    const adventure_render::StackBannerAssetCatalog* banner_catalog_;
};
} // namespace d2engine
