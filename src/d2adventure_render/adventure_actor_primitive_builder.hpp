#pragma once

#include "adventure_render_types.hpp"
#include "map_geometry.hpp"

#include <d2runtime/AdventureWorldState.hpp>

#include <optional>
#include <string>

namespace d2engine::adventure_render {

struct AdventureActorPrimitivePlaybackPolicy {
    int                            frame_duration_ms = 100;
    bool                           is_looping = true;
    AdventureAnimationTimingSource timing_source =
        AdventureAnimationTimingSource::ProvisionalFallback;
};

struct AdventureActorPrimitiveSet {
    PreparedAdventureRenderPrimitive                body;
    std::optional<PreparedAdventureRenderPrimitive> shadow;
};

[[nodiscard]] AdventureActorPrimitiveSet build_adventure_actor_primitives(
    const d2runtime::AdventureStack& stack, const AdventureActorVisual& visual,
    const AdventureMapGeometry& geometry, ScreenPoint foot, IsoDepthAnchor depth_anchor,
    StableRenderId body_stable_id, StableRenderId shadow_stable_id, std::string body_debug_label,
    std::string shadow_debug_label, const AdventureActorPrimitivePlaybackPolicy& playback_policy);

} // namespace d2engine::adventure_render
