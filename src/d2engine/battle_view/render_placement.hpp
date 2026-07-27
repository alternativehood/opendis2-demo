#pragma once

#include "anchor_policy.hpp"
#include "track_render_layer.hpp"

namespace d2engine {

enum class DepthMode : std::uint8_t {
    AnchorY,
};

struct RenderPlacement {
    TrackRenderLayer pass = TrackRenderLayer::Base;
    DepthMode        depth_mode = DepthMode::AnchorY;
    AnchorPolicy     position_anchor = AnchorPolicy::UnitFoot;
    AnchorPolicy     depth_anchor = AnchorPolicy::UnitFoot;
    int              depth_bias = 0;
};

[[nodiscard]] constexpr RenderPlacement render_placement_for(TrackRenderLayer layer,
                                                             AnchorPolicy     anchor) noexcept {
    const TrackRenderLayer pass =
        layer == TrackRenderLayer::Effect ? TrackRenderLayer::Base : layer;
    return {.pass = pass,
            .depth_mode = DepthMode::AnchorY,
            .position_anchor = anchor,
            .depth_anchor = anchor};
}

} // namespace d2engine
