#include "adventure_actor_primitive_builder.hpp"

#include <algorithm>
#include <stdexcept>

namespace d2engine::adventure_render {
namespace {

void build_layer(PreparedAdventureRenderPrimitive& primitive,
                 const AdventureActorVisualLayer& layer, StableRenderId id,
                 const std::string& label, AdventurePrimitiveRole semantic_role,
                 const std::string& semantic_object_id, WorldRenderLevel level, int suborder,
                 ScreenPoint foot, float alpha,
                 const AdventureActorPrimitivePlaybackPolicy& policy) {
    primitive.stable_id = id;
    primitive.debug_label = label;
    primitive.semantic_role = semantic_role;
    primitive.semantic_object_id = semantic_object_id;
    primitive.phase = AdventureRenderPhase::World;
    primitive.level = level;
    primitive.local_suborder = suborder;
    primitive.container_path = layer.container_path;
    primitive.draw_origin = {foot.x - layer.canvas_foot_x, foot.y - layer.canvas_foot_y};
    primitive.alpha = alpha;
    primitive.src_width = layer.frames.front().canvas_width;
    primitive.src_height = layer.frames.front().canvas_height;
    primitive.content_bounds = layer.content_bounds;
    if (!primitive.content_bounds.valid()) {
        throw std::invalid_argument("actor layer has invalid content bounds stack=" + label);
    }
    int max_width = 0;
    int max_height = 0;
    for (const auto& frame : layer.frames) {
        max_width = std::max(max_width, frame.canvas_width);
        max_height = std::max(max_height, frame.canvas_height);
    }
    if (max_width == 0)
        max_width = layer.native_canvas_w;
    if (max_height == 0)
        max_height = layer.native_canvas_h;
    primitive.visual_bounds = {primitive.draw_origin.x, primitive.draw_origin.y,
                               primitive.draw_origin.x + max_width,
                               primitive.draw_origin.y + max_height};
    primitive.record_name = layer.frames.front().record_name;
    if (layer.frames.size() <= 1)
        return;
    AdventureAnimationData animation;
    animation.animation_name = layer.logical_animation_name;
    animation.native_canvas_w = layer.native_canvas_w;
    animation.native_canvas_h = layer.native_canvas_h;
    animation.is_looping = policy.is_looping;
    animation.timing_source = policy.timing_source;
    animation.frames.reserve(layer.frames.size());
    for (const auto& frame : layer.frames) {
        animation.frames.push_back({.record_name = frame.record_name,
                                    .duration_ms = policy.frame_duration_ms,
                                    .canvas_width = frame.canvas_width,
                                    .canvas_height = frame.canvas_height});
    }
    primitive.animation = std::move(animation);
}

} // namespace

AdventureActorPrimitiveSet build_adventure_actor_primitives(
    const d2runtime::AdventureStack& stack, const AdventureActorVisual& visual,
    const AdventureMapGeometry&, ScreenPoint foot, IsoDepthAnchor depth_anchor,
    StableRenderId body_stable_id, StableRenderId shadow_stable_id, std::string body_debug_label,
    std::string shadow_debug_label, const AdventureActorPrimitivePlaybackPolicy& policy) {
    if (visual.body.frames.empty())
        throw std::invalid_argument("actor body has no frames stack=" + stack.id);
    AdventureActorPrimitiveSet result;
    build_layer(result.body, visual.body, body_stable_id, body_debug_label,
                AdventurePrimitiveRole::MapStackBody, stack.id, WorldRenderLevel::Actor, 0, foot,
                1.0f, policy);
    result.body.footprint = {depth_anchor};
    result.body.depth_anchor = depth_anchor;
    if (visual.shadow.has_value()) {
        if (visual.shadow->frames.empty())
            throw std::invalid_argument("actor shadow has no frames stack=" + stack.id);
        PreparedAdventureRenderPrimitive shadow;
        build_layer(shadow, *visual.shadow, shadow_stable_id, shadow_debug_label,
                    AdventurePrimitiveRole::Unspecified, stack.id, WorldRenderLevel::ActorUnderlay,
                    kActorShadowSuborder, foot, AdventureActorShadowPresentationPolicy::alpha,
                    policy);
        shadow.footprint = {depth_anchor};
        shadow.depth_anchor = depth_anchor;
        if (!shadow.content_bounds.valid()) {
            throw std::invalid_argument("actor shadow has invalid content bounds stack=" +
                                        shadow_debug_label);
        }
        if (visual.shadow->frames.size() == visual.body.frames.size())
            shadow.animation_sync_source_id = body_stable_id;
        result.shadow = std::move(shadow);
    }
    return result;
}

} // namespace d2engine::adventure_render
