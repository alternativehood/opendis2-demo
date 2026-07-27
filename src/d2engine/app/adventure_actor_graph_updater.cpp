#include "adventure_actor_graph_updater.hpp"

#include "../../d2adventure_render/adventure_actor_primitive_builder.hpp"
#include "../../d2adventure_render/adventure_banner_primitive_builder.hpp"
#include "../../d2adventure_render/map_stack_presentation_ids.hpp"
#include <d2adventure_render/iso_depth_resolver.hpp>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace d2engine {
void AdventureActorGraphUpdater::settle_stack_actor(
    const d2runtime::AdventureStack&                         stack,
    const adventure_render::AdventureActorVisual&            idle_visual,
    std::shared_ptr<const adventure_render::InteractionMask> idle_interaction_mask,
    const adventure_render::StackBannerAsset& banner_asset, int banner_index,
    adventure_render::PreparedAdventureMap& prepared_map,
    AdventureAnimationPlayerMap&            static_animation_players) {
    if (!d2runtime::is_stack_on_adventure_map(stack))
        throw std::logic_error("cannot settle hidden stack=" + stack.id);
    if (!idle_interaction_mask)
        throw std::logic_error("cannot settle stack without interaction mask stack=" + stack.id);
    using namespace adventure_render;
    const auto                              ids = map_stack_presentation_render_ids(stack.id);
    std::size_t                             body_count = 0;
    std::size_t                             shadow_count = 0;
    std::size_t                             banner_count = 0;
    const PreparedAdventureRenderPrimitive* body_primitive = nullptr;
    const PreparedAdventureRenderPrimitive* banner_primitive = nullptr;
    for (const auto& primitive : prepared_map.world_graph.world) {
        if (primitive.stable_id == ids.body) {
            ++body_count;
            body_primitive = &primitive;
        }
        if (primitive.stable_id == ids.shadow) {
            ++shadow_count;
        }
        if (primitive.stable_id == ids.banner) {
            ++banner_count;
            banner_primitive = &primitive;
        }
    }
    if (body_count != 1)
        throw std::logic_error("map_stack_settle_body_count stack=" + stack.id +
                               " expected=1 actual=" + std::to_string(body_count));
    if (shadow_count > 1)
        throw std::logic_error("map_stack_settle_shadow_count stack=" + stack.id +
                               " expected=0..1 actual=" + std::to_string(shadow_count));
    if (banner_count != 1)
        throw std::logic_error("map_stack_settle_banner_count stack=" + stack.id +
                               " expected=1 actual=" + std::to_string(banner_count));

    if (body_primitive == nullptr ||
        body_primitive->semantic_role != AdventurePrimitiveRole::MapStackBody ||
        body_primitive->semantic_object_id != stack.id) {
        throw std::logic_error("map_stack_settle_body_semantic_mismatch stack=" + stack.id);
    }
    if (banner_primitive == nullptr ||
        banner_primitive->semantic_role != AdventurePrimitiveRole::MapStackBanner ||
        banner_primitive->semantic_object_id != stack.id) {
        throw std::logic_error("map_stack_settle_banner_semantic_mismatch stack=" + stack.id);
    }
    if (banner_primitive->visibility_group != AdventureRenderVisibilityGroup::Banners ||
        banner_primitive->animation.has_value() || banner_primitive->interaction_mask != nullptr) {
        throw std::logic_error("map_stack_settle_banner_contract_mismatch stack=" + stack.id);
    }

    const auto foot = prepared_map.geometry.cell_foot_anchor(stack.position);
    auto       replacement = build_adventure_actor_primitives(
        stack, idle_visual, prepared_map.geometry, foot, stack.position, ids.body, ids.shadow,
        "Stack:" + stack.id, "StackShadow:" + stack.id,
        {.frame_duration_ms = 100,
         .is_looping = true,
         .timing_source = AdventureAnimationTimingSource::ProvisionalFallback});
    const int expected_width =
        replacement.body.visual_bounds.max_x - replacement.body.visual_bounds.min_x;
    const int expected_height =
        replacement.body.visual_bounds.max_y - replacement.body.visual_bounds.min_y;
    if (idle_interaction_mask->width != expected_width ||
        idle_interaction_mask->height != expected_height) {
        throw std::logic_error("settled actor interaction mask size mismatch stack=" + stack.id +
                               " actual=" + std::to_string(idle_interaction_mask->width) + "x" +
                               std::to_string(idle_interaction_mask->height) +
                               " expected=" + std::to_string(expected_width) + "x" +
                               std::to_string(expected_height));
    }
    replacement.body.interaction_mask = std::move(idle_interaction_mask);
    if (replacement.shadow && replacement.shadow->interaction_mask != nullptr)
        throw std::logic_error("settled actor shadow interaction mask is not null stack=" +
                               stack.id);
    std::vector<PreparedAdventureRenderPrimitive> world;
    world.reserve(prepared_map.world_graph.world.size() + (replacement.shadow ? 1 : 0) + 1);
    for (const auto& primitive : prepared_map.world_graph.world) {
        if (primitive.stable_id == ids.body || primitive.stable_id == ids.shadow ||
            primitive.stable_id == ids.banner)
            continue;
        world.push_back(primitive);
    }
    world.push_back(replacement.body);
    if (replacement.shadow)
        world.push_back(*replacement.shadow);
    const auto banner = build_adventure_banner_primitive(
        replacement.body, idle_visual.body.content_bounds, banner_asset,
        adventure_render::AdventureBannerDockSide::RightOfReference, ids.banner,
        "StackBanner:" + stack.id + ":" + std::to_string(banner_index),
        adventure_render::AdventurePrimitiveRole::MapStackBanner, stack.id,
        adventure_render::WorldRenderLevel::Actor, 1, {stack.position}, stack.position);
    world.push_back(banner);
    const auto order = IsoDepthResolver(prepared_map.geometry).resolve(world);
    std::vector<PreparedAdventureRenderPrimitive> sorted;
    sorted.reserve(world.size());
    for (const auto index : order)
        sorted.push_back(std::move(world[index]));
    prepared_map.world_graph.world = std::move(sorted);

    static_animation_players.erase(ids.body);
    static_animation_players.erase(ids.shadow);
    PreparedAdventureRenderGraph graph;
    graph.world = {replacement.body};
    if (replacement.shadow)
        graph.world.push_back(*replacement.shadow);
    auto replacements = build_adventure_animation_players(graph);
    for (auto& [id, player] : replacements)
        static_animation_players.insert_or_assign(id, std::move(player));

    std::size_t                             settled_body_count = 0;
    std::size_t                             settled_shadow_count = 0;
    std::size_t                             settled_banner_count = 0;
    const PreparedAdventureRenderPrimitive* settled_body = nullptr;
    const PreparedAdventureRenderPrimitive* settled_shadow = nullptr;
    const PreparedAdventureRenderPrimitive* settled_banner = nullptr;
    for (const auto& primitive : prepared_map.world_graph.world) {
        if (primitive.stable_id == ids.body) {
            ++settled_body_count;
            settled_body = &primitive;
        }
        if (primitive.stable_id == ids.shadow) {
            ++settled_shadow_count;
            settled_shadow = &primitive;
        }
        if (primitive.stable_id == ids.banner) {
            ++settled_banner_count;
            settled_banner = &primitive;
        }
    }

    if (settled_body == nullptr || settled_body->footprint.size() != 1 ||
        settled_body->footprint[0] != stack.position ||
        settled_body->depth_anchor != stack.position ||
        settled_body->semantic_role != AdventurePrimitiveRole::MapStackBody ||
        settled_body->semantic_object_id != stack.id) {
        throw std::logic_error("map_stack_settle_postcondition_body_failed stack=" + stack.id);
    }
    if (settled_body_count != 1) {
        throw std::logic_error("map_stack_settle_postcondition_body_failed stack=" + stack.id);
    }
    if (settled_shadow_count != static_cast<std::size_t>(replacement.shadow.has_value())) {
        throw std::logic_error("map_stack_settle_postcondition_shadow_failed stack=" + stack.id);
    }
    if (settled_banner_count != 1) {
        throw std::logic_error("map_stack_settle_postcondition_banner_failed stack=" + stack.id);
    }
    if (settled_banner == nullptr || settled_banner->footprint.size() != 1 ||
        settled_banner->footprint[0] != stack.position ||
        settled_banner->depth_anchor != stack.position ||
        settled_banner->semantic_role != AdventurePrimitiveRole::MapStackBanner ||
        settled_banner->semantic_object_id != stack.id || settled_banner->animation.has_value() ||
        settled_banner->interaction_mask != nullptr ||
        settled_banner->stable_id == settled_body->stable_id) {
        throw std::logic_error("map_stack_settle_postcondition_banner_failed stack=" + stack.id);
    }
    if (replacement.shadow.has_value()) {
        if (settled_shadow == nullptr || settled_shadow->footprint.size() != 1 ||
            settled_shadow->footprint[0] != stack.position ||
            settled_shadow->depth_anchor != stack.position) {
            throw std::logic_error("map_stack_settle_postcondition_shadow_failed stack=" +
                                   stack.id);
        }
    } else if (settled_shadow_count != 0) {
        throw std::logic_error("map_stack_settle_postcondition_shadow_failed stack=" + stack.id);
    }

    if (static_animation_players.contains(ids.banner)) {
        throw std::logic_error("map_stack_banner_has_animation_player stack=" + stack.id);
    }
}
} // namespace d2engine
