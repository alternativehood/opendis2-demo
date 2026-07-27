#pragma once

#include "adventure_animation_helpers.hpp"

#include <d2adventure_render/prepared_adventure_map.hpp>
#include <d2adventure_render/stack_banner_asset_catalog.hpp>
#include <d2runtime/AdventureWorldState.hpp>

#include <memory>

namespace d2engine {
class AdventureActorGraphUpdater {
public:
    static void settle_stack_actor(
        const d2runtime::AdventureStack&                         stack,
        const adventure_render::AdventureActorVisual&            idle_visual,
        std::shared_ptr<const adventure_render::InteractionMask> idle_interaction_mask,
        const adventure_render::StackBannerAsset& banner_asset, int banner_index,
        adventure_render::PreparedAdventureMap& prepared_map,
        AdventureAnimationPlayerMap&            static_animation_players);
};
} // namespace d2engine
