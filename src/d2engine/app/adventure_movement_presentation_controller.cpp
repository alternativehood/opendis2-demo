#include "adventure_movement_presentation_controller.hpp"

#include "../../d2adventure_render/adventure_actor_primitive_builder.hpp"
#include "../../d2adventure_render/adventure_banner_primitive_builder.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace d2engine {
namespace {
using Primitive = adventure_render::PreparedAdventureRenderPrimitive;

AdventureAnimationPlayerMap make_players(const std::vector<Primitive>& primitives) {
    adventure_render::PreparedAdventureRenderGraph graph;
    graph.world = primitives;
    return build_adventure_animation_players(graph);
}

std::vector<Primitive>
build_segment_primitives(const AdventureMovementVisualPlan&            plan,
                         const adventure_render::AdventureMapGeometry& geometry, std::size_t index,
                         double t) {
    const auto&                         segment = plan.segments[index];
    const auto                          a = geometry.cell_foot_anchor(segment.from);
    const auto                          b = geometry.cell_foot_anchor(segment.to);
    const adventure_render::ScreenPoint foot{static_cast<int>(std::lround(a.x + (b.x - a.x) * t)),
                                             static_cast<int>(std::lround(a.y + (b.y - a.y) * t))};
    d2runtime::AdventureStack           presentation_stack;
    presentation_stack.id = plan.stack_id;
    presentation_stack.position = t < 0.5 ? segment.from : segment.to;
    const auto set = adventure_render::build_adventure_actor_primitives(
        presentation_stack, segment.move_visual, geometry, foot, presentation_stack.position,
        adventure_render::stable_render_id("MovingStack:Body:" + plan.stack_id),
        adventure_render::stable_render_id("MovingStack:Shadow:" + plan.stack_id),
        "MovingStack:Body:" + plan.stack_id, "MovingStack:Shadow:" + plan.stack_id, {});
    std::vector<Primitive> result{set.body};
    if (set.shadow)
        result.push_back(*set.shadow);
    const auto banner = adventure_render::build_adventure_banner_primitive(
        set.body, segment.move_visual.body.content_bounds, plan.banner_asset,
        adventure_render::AdventureBannerDockSide::RightOfReference,
        adventure_render::stable_render_id("MovingStack:Banner:" + plan.stack_id),
        "MovingStack:Banner:" + plan.stack_id + ":" + std::to_string(plan.banner_index),
        adventure_render::AdventurePrimitiveRole::MapStackBanner, plan.stack_id,
        adventure_render::WorldRenderLevel::Actor, 1, {presentation_stack.position},
        presentation_stack.position);
    result.push_back(banner);
    return result;
}

bool same_animation(const std::optional<adventure_render::AdventureAnimationData>& a,
                    const std::optional<adventure_render::AdventureAnimationData>& b) {
    if (a.has_value() != b.has_value())
        return false;
    if (!a)
        return true;
    if (a->animation_name != b->animation_name || a->native_canvas_w != b->native_canvas_w ||
        a->native_canvas_h != b->native_canvas_h || a->is_looping != b->is_looping ||
        a->timing_source != b->timing_source || a->frames.size() != b->frames.size())
        return false;
    for (std::size_t i = 0; i < a->frames.size(); ++i) {
        if (a->frames[i].record_name != b->frames[i].record_name ||
            a->frames[i].duration_ms != b->frames[i].duration_ms)
            return false;
    }
    return true;
}

bool same_playback_identity(const std::vector<Primitive>& a, const std::vector<Primitive>& b) {
    if (a.size() != b.size())
        return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].stable_id != b[i].stable_id ||
            adventure_animation_player_id(a[i]) != adventure_animation_player_id(b[i]) ||
            a[i].animation_sync_source_id != b[i].animation_sync_source_id ||
            a[i].container_path != b[i].container_path ||
            !same_animation(a[i].animation, b[i].animation))
            return false;
    }
    return true;
}

[[noreturn]] void external_collision(std::string_view stack_id, std::size_t segment_index,
                                     const Primitive& primitive, std::string_view domain,
                                     adventure_render::StableRenderId player_id) {
    throw std::logic_error("movement animation player collision stack=" + std::string(stack_id) +
                           " segment=" + std::to_string(segment_index) +
                           " id=" + std::to_string(player_id) +
                           " primitive=" + std::to_string(primitive.stable_id) +
                           " label=" + primitive.debug_label + " domain=" + std::string(domain));
}
} // namespace

std::optional<adventure_render::ScreenPoint>
AdventureMovementPresentationController::current_actor_foot() const {
    if (!plan_ || geometry_ == nullptr || segment_index_ >= plan_->segments.size())
        return std::nullopt;
    const auto& segment = plan_->segments[segment_index_];
    const auto  from = geometry_->cell_foot_anchor(segment.from);
    const auto  to = geometry_->cell_foot_anchor(segment.to);
    const auto  t = static_cast<double>(elapsed_ms_) /
                    static_cast<double>(AdventureMovementPresentationPolicy::segment_duration_ms);
    return adventure_render::ScreenPoint{
        static_cast<int>(
            std::lround(static_cast<double>(from.x) + static_cast<double>(to.x - from.x) * t)),
        static_cast<int>(
            std::lround(static_cast<double>(from.y) + static_cast<double>(to.y - from.y) * t))};
}

void validate_adventure_movement_animation_domains(
    const AdventureMovementVisualPlan& plan, const adventure_render::AdventureMapGeometry& geometry,
    const AdventureAnimationPlayerMap& static_players,
    const AdventureAnimationPlayerMap& route_preview_players) {
    if (plan.segments.empty())
        throw std::invalid_argument("movement animation validation requires a non-empty plan");
    for (std::size_t i = 0; i < plan.segments.size(); ++i) {
        const auto primitives = build_segment_primitives(plan, geometry, i, 0.0);
        const auto body_id =
            adventure_render::stable_render_id("MovingStack:Body:" + plan.stack_id);
        const auto shadow_id =
            adventure_render::stable_render_id("MovingStack:Shadow:" + plan.stack_id);
        std::size_t body_count = 0;
        std::size_t shadow_count = 0;
        for (const auto& primitive : primitives) {
            body_count += primitive.stable_id == body_id;
            shadow_count += primitive.stable_id == shadow_id;
        }
        if (body_count != 1 || shadow_count > 1 || (shadow_count == 1 && body_id == shadow_id)) {
            throw std::logic_error("invalid movement primitive graph stack=" + plan.stack_id +
                                   " segment=" + std::to_string(i));
        }

        adventure_render::PreparedAdventureRenderGraph graph;
        graph.world = primitives;
        const auto players = build_adventure_animation_players(graph);
        for (const auto& primitive : primitives) {
            if (!primitive.animation)
                continue;
            const auto player_id = adventure_animation_player_id(primitive);
            if (static_players.contains(player_id))
                external_collision(plan.stack_id, i, primitive, "static", player_id);
            if (route_preview_players.contains(player_id))
                external_collision(plan.stack_id, i, primitive, "route-preview", player_id);
        }
        (void)players;
    }
}

std::vector<Primitive> AdventureMovementPresentationController::primitives_for(
    const adventure_render::AdventureMapGeometry& geometry, std::size_t index, double t) const {
    return build_segment_primitives(*plan_, geometry, index, t);
}

std::vector<Primitive> AdventureMovementPresentationController::current_world_primitives(
    const adventure_render::AdventureMapGeometry& geometry) const {
    if (!plan_ || segment_index_ >= plan_->segments.size())
        return {};
    const double t = std::clamp(static_cast<double>(elapsed_ms_) /
                                    AdventureMovementPresentationPolicy::segment_duration_ms,
                                0.0, 1.0);
    return primitives_for(geometry, segment_index_, t);
}

AdventureMovementPresentationUpdate
AdventureMovementPresentationController::update(int                  animation_delta_ms,
                                                d2game::GameSession& session) {
    AdventureMovementPresentationUpdate out;
    if (!plan_ || !geometry_)
        return out;
    if (animation_delta_ms < 0)
        throw std::invalid_argument("movement presentation received negative animation delta");

    int remaining_delta = animation_delta_ms;
    while (plan_ && remaining_delta > 0) {
        const int segment_remaining =
            AdventureMovementPresentationPolicy::segment_duration_ms - elapsed_ms_;
        const int consumed = std::min(remaining_delta, segment_remaining);
        for (auto& [id, player] : animation_players_)
            player.update(static_cast<float>(consumed));
        elapsed_ms_ += consumed;
        remaining_delta -= consumed;
        if (elapsed_ms_ < AdventureMovementPresentationPolicy::segment_duration_ms)
            break;

        elapsed_ms_ = 0;
        const auto& expected = plan_->segments[segment_index_];
        const auto  result =
            session.handle_command(d2game::GameAdvanceAdventureMovementStepCommand{});
        out.command_results.push_back(result);
        const d2game::AdventureMovementStepCommitted* committed = nullptr;
        const d2game::AdventureMovementInterrupted*   interrupted = nullptr;
        for (const auto& event : result.events) {
            if (const auto* value = std::get_if<d2game::AdventureMovementStepCommitted>(&event))
                committed = value;
            if (const auto* value = std::get_if<d2game::AdventureMovementInterrupted>(&event))
                interrupted = value;
        }
        if (interrupted) {
            if (segment_index_ != 0) {
                const auto& settlement_segment = plan_->segments[segment_index_ - 1];
                if (!settlement_segment.idle_interaction_mask_at_destination)
                    throw std::logic_error(
                        "movement settlement mask missing stack=" + plan_->stack_id +
                        " route_step=" + std::to_string(settlement_segment.route_step_index));
                out.settlement = AdventureMovementActorSettlement{
                    plan_->stack_id,
                    settlement_segment.idle_visual_at_destination,
                    settlement_segment.idle_interaction_mask_at_destination,
                    plan_->banner_asset,
                    plan_->banner_index,
                    settlement_segment.route_step_index,
                    false};
            } else {
                plan_.reset();
                animation_players_.clear();
                active_primitives_.clear();
            }
            break;
        }
        const auto mismatch = [&](std::string_view field, const auto& actual, const auto& wanted) {
            std::ostringstream msg;
            msg << "movement commit mismatch stack=" << plan_->stack_id
                << " segment=" << segment_index_ << " field=" << field << " expected=" << wanted
                << " actual=" << actual;
            return msg.str();
        };
        const auto stop_with_error = [&](std::string message) {
            plan_.reset();
            animation_players_.clear();
            active_primitives_.clear();
            throw std::logic_error(std::move(message));
        };
        if (!committed) {
            stop_with_error("movement commit missing stack=" + plan_->stack_id +
                            " segment=" + std::to_string(segment_index_));
        }
        if (committed->stack_id != plan_->stack_id) {
            stop_with_error(mismatch("stack_id", committed->stack_id, plan_->stack_id));
        }
        if (committed->step_index != expected.route_step_index) {
            stop_with_error(
                mismatch("step_index", committed->step_index, expected.route_step_index));
        }
        if (committed->from != expected.from || committed->to != expected.to) {
            stop_with_error("movement commit mismatch stack=" + plan_->stack_id +
                            " segment=" + std::to_string(segment_index_) +
                            " expected_from/to=" + std::to_string(expected.from.x) + "," +
                            std::to_string(expected.from.y) + "->" + std::to_string(expected.to.x) +
                            "," + std::to_string(expected.to.y));
        }
        if (committed->direction != expected.direction) {
            stop_with_error(mismatch("direction", static_cast<int>(committed->direction),
                                     static_cast<int>(expected.direction)));
        }

        const bool final = segment_index_ + 1 == plan_->segments.size();
        ++segment_index_;
        if (final) {
            if (!expected.idle_interaction_mask_at_destination)
                stop_with_error("movement settlement mask missing stack=" + plan_->stack_id +
                                " route_step=" + std::to_string(expected.route_step_index));
            bool completed = false;
            for (const auto& event : result.events)
                completed =
                    completed || std::holds_alternative<d2game::AdventureMovementCompleted>(event);
            out.settlement =
                AdventureMovementActorSettlement{plan_->stack_id,
                                                 expected.idle_visual_at_destination,
                                                 expected.idle_interaction_mask_at_destination,
                                                 plan_->banner_asset,
                                                 plan_->banner_index,
                                                 expected.route_step_index,
                                                 completed};
            break;
        }

        auto next_primitives = primitives_for(*geometry_, segment_index_, 0.0);
        if (!same_playback_identity(active_primitives_, next_primitives)) {
            animation_players_ = make_players(next_primitives);
            for (auto& [id, player] : animation_players_)
                player.play();
        }
        active_primitives_ = std::move(next_primitives);
    }
    return out;
}
} // namespace d2engine
