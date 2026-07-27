#include "adventure_actor_visual_conversion.hpp"

namespace d2engine {

namespace {
adventure_render::AdventureActorVisualLayer convert_layer(const IsoActorVisualLayer& source) {
    adventure_render::AdventureActorVisualLayer result;
    result.container_path = source.container_path;
    result.logical_animation_name = source.animation_name;
    result.native_canvas_w = source.native_canvas_w;
    result.native_canvas_h = source.native_canvas_h;
    result.canvas_foot_x = source.canvas_foot_x;
    result.canvas_foot_y = source.canvas_foot_y;
    result.content_bounds = source.content_bounds;
    if (!result.content_bounds.valid()) {
        throw std::runtime_error("adventure_actor_visual_invalid_layer_content_bounds animation=" +
                                 source.animation_name);
    }
    result.frames.reserve(source.frames.size());
    for (const auto& frame : source.frames) {
        if (!frame.content_bounds.valid()) {
            throw std::runtime_error(
                "adventure_actor_visual_invalid_frame_content_bounds animation=" +
                source.animation_name + " frame=" + frame.record_name);
        }
        result.frames.push_back({.record_name = frame.record_name,
                                 .canvas_width = frame.canvas_width,
                                 .canvas_height = frame.canvas_height,
                                 .content_bounds = frame.content_bounds});
    }
    return result;
}
} // namespace

adventure_render::AdventureActorVisual to_adventure_actor_visual(const IsoActorVisual& source) {
    adventure_render::AdventureActorVisual result;
    result.presentation_kind = source.presentation_kind;
    result.resolved_owner_id = source.resolved_owner_id;
    result.body = convert_layer(source.body);
    if (source.shadow.has_value())
        result.shadow = convert_layer(*source.shadow);
    return result;
}

} // namespace d2engine
