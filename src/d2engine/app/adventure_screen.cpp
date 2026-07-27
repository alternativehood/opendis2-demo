#include "adventure_screen.hpp"
#include "adventure_animation_helpers.hpp"
#include "adventure_interaction_mask.hpp"

#include "../assets/asset_runtime_catalog_adapter.hpp"

#include "../assets/image_asset_key.hpp"
#include "../assets/portrait_manifest_index.hpp"
#include "../render/render_asset_runtime.hpp"
#include "../render/renderer2d.hpp"

#include <d2adventure_render/adventure_render_types.hpp>
#include <d2adventure_render/iso_depth_resolver.hpp>
#include <d2adventure_render/map_stack_presentation_ids.hpp>
#include <d2log/log.hpp>

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <variant>

namespace d2engine {

static auto kLog = d2log::get("d2.scrn"); // NOLINT

AdventureScreen::AdventureScreen(AppRuntimeContext&                               runtime,
                                 std::unique_ptr<d2game::GameSession>             session,
                                 d2engine::adventure_render::PreparedAdventureMap prepared_map,
                                 AdventureRenderState render_state, int logical_viewport_w,
                                 int logical_viewport_h, TreeLayout tree_layout,
                                 std::string config_source, std::function<void()> request_quit,
                                 std::function<void()>                     request_debug_battle,
                                 std::function<void(StackInspectionModel)> request_stack_info,
                                 AdventureWorldVisualResources             world_visuals,
                                 adventure_render::StackBannerAssetCatalog banner_catalog)
    : Screen(std::move(tree_layout), std::move(config_source)), runtime_(runtime),
      session_(std::move(session)), request_quit_(std::move(request_quit)),
      request_debug_battle_(std::move(request_debug_battle)),
      request_stack_info_(std::move(request_stack_info)), prepared_map_(std::move(prepared_map)),
      render_state_(std::move(render_state)), logical_viewport_w_(logical_viewport_w),
      logical_viewport_h_(logical_viewport_h), world_visuals_(std::move(world_visuals)),
      banner_catalog_(std::move(banner_catalog)) {
    if (!session_) {
        throw std::invalid_argument("AdventureScreen: missing GameSession");
    }
    inspection_builder_.emplace(session_->world(), runtime_.game_data);
}

AdventureScreen::~AdventureScreen() = default;

d2game::AdventureMovementDebugSnapshot AdventureScreen::movement_debug_snapshot() const {
    return session_->adventure_movement_debug_snapshot();
}

d2game::GameCommandResult AdventureScreen::debug_reset_selected_stack_movement_points() {
    auto result =
        session_->handle_command(d2game::GameDebugResetSelectedAdventureMovementPointsCommand{});
    log_movement_result(result);
    sync_route_preview_presentation();
    return result;
}

d2game::GameCommandResult AdventureScreen::debug_grant_selected_stack_free_movement_points() {
    auto result = session_->handle_command(
        d2game::GameDebugGrantSelectedAdventureFreeMovementPointsCommand{});
    log_movement_result(result);
    sync_route_preview_presentation();
    return result;
}

void AdventureScreen::debug_toggle_follow_unit() {
    follow_unit_ = !follow_unit_;
    if (follow_unit_)
        follow_moving_unit_camera();
}

void AdventureScreen::debug_toggle_banners() {
    show_banners_ = !show_banners_;
}

bool AdventureScreen::primitive_visible(
    const adventure_render::PreparedAdventureRenderPrimitive& prim) const {
    return show_banners_ ||
           prim.visibility_group != adventure_render::AdventureRenderVisibilityGroup::Banners;
}

std::vector<std::string> AdventureScreen::required_layout_nodes() {
    return {"/adventure"};
}

void AdventureScreen::on_enter() {
    session_->handle_command(d2game::GameInspectWorldCommand{});
    summary_ = session_->inspect();
    summary_loaded_ = true;

    pick_index_.build(prepared_map_, world_visuals_.selection_circle);
    pick_index_built_ = true;

    if (render_state_.has_terrain_texture()) {
        center_camera(logical_viewport_w_, logical_viewport_h_);
        const auto& cam = render_state_.camera();

        if (cam.viewport_width <= 0 || cam.viewport_height <= 0) {
            kLog->error("adventure_screen camera viewport is zero (viewport={}x{}) "
                        "logical_viewport={}x{} texture={}x{}",
                        cam.viewport_width, cam.viewport_height, logical_viewport_w_,
                        logical_viewport_h_, render_state_.texture_width(),
                        render_state_.texture_height());
        }

        kLog->info("adventure_screen entered scenario={} grid={}x{} canvas={}x{} texture={}x{} "
                   "camera={},{}+{}x{} objects={} primitives={} portraits={} pick_targets={}",
                   summary_.scenario_id, prepared_map_.geometry.map_width,
                   prepared_map_.geometry.map_height, prepared_map_.canvas_width(),
                   prepared_map_.canvas_height(), render_state_.texture_width(),
                   render_state_.texture_height(), cam.canvas_x, cam.canvas_y, cam.viewport_width,
                   cam.viewport_height, summary_.runtime_objects,
                   prepared_map_.world_graph.world.size(), runtime_.portraits.size(),
                   pick_index_.size());
    } else {
        kLog->info("adventure_screen entered scenario={} grid={}x{} objects={} (no terrain)",
                   summary_.scenario_id, prepared_map_.geometry.map_width,
                   prepared_map_.geometry.map_height, summary_.runtime_objects);
    }

    ensure_animation_players();
    sync_route_preview_presentation();

    std::size_t anim_prim_count = 0;
    std::size_t anim_player_count = animation_players_.size();
    std::size_t anim_looping = 0;
    std::size_t anim_total_frames = 0;
    for (const auto& prim : prepared_map_.world_graph.world) {
        if (prim.animation.has_value())
            ++anim_prim_count;
    }
    for (const auto& prim : prepared_map_.world_graph.ground_overlay) {
        if (prim.animation.has_value())
            ++anim_prim_count;
    }
    for (const auto& prim : prepared_map_.world_graph.world_overlay) {
        if (prim.animation.has_value())
            ++anim_prim_count;
    }
    for (const auto& [id, player] : animation_players_) {
        anim_total_frames += player.sequence().frames.size();
        if (player.sequence().is_looping)
            ++anim_looping;
        kLog->debug("animation stable_id={} name={} container={} frames={} looping={}", id,
                    player.sequence().name, player.sequence().container_path,
                    player.sequence().frames.size(), player.sequence().is_looping);
    }
    if (anim_prim_count > 0 || anim_player_count > 0) {
        kLog->info("adventure_animations primitives={} players={} looping={} total_frames={}",
                   anim_prim_count, anim_player_count, anim_looping, anim_total_frames);
    }
}

void AdventureScreen::on_revealed() {
    if (last_pointer_valid_)
        refresh_pointer_interaction();
}

CursorKind AdventureScreen::cursor_kind() const {
    if (session_->adventure_movement_state().is_moving())
        return CursorKind::Default;
    return interaction_stack_.has_value() ? CursorKind::SelectUnit : CursorKind::Default;
}

bool AdventureScreen::handle_input(const InputEvent& event) {
    auto action = AdventureScreenInputHandler::handle(event);
    if (!action.has_value())
        return false;

    if (std::get_if<AdventureCancel>(&*action)) {
        if (request_quit_) {
            request_quit_();
        }
        return true;
    }

    if (session_->adventure_movement_state().is_moving())
        return true;

    if (std::get_if<AdventureOpenDebugBattle>(&*action)) {
        if (request_debug_battle_) {
            request_debug_battle_();
        }
        return true;
    }

    if (auto* pan = std::get_if<AdventurePanCamera>(&*action)) {
        if (render_state_.has_terrain_texture()) {
            auto        cam = render_state_.camera();
            const float z = cam.zoom();
            const int   dx = static_cast<int>(std::lround(static_cast<float>(pan->dx) / z));
            const int   dy = static_cast<int>(std::lround(static_cast<float>(pan->dy) / z));
            cam.canvas_x += dx;
            cam.canvas_y += dy;
            commit_camera(cam);
        }
        return true;
    }

    if (auto* pointer_at = std::get_if<AdventurePointerAt>(&*action)) {
        handle_pointer_at(pointer_at->x, pointer_at->y);
        return true;
    }

    if (auto* select = std::get_if<AdventureSelectAt>(&*action)) {
        handle_select_at(select->x, select->y);
        return true;
    }

    if (auto* inspect = std::get_if<AdventureInspectAt>(&*action)) {
        handle_inspect_at(inspect->x, inspect->y);
        return true;
    }

    if (std::get_if<AdventureZoomIn>(&*action)) {
        handle_zoom_in();
        return true;
    }

    if (std::get_if<AdventureZoomOut>(&*action)) {
        handle_zoom_out();
        return true;
    }

    return false;
}

static std::optional<AdventureStackRef> pick_to_ref(const AdventurePickTarget* t) {
    if (t == nullptr)
        return std::nullopt;
    return AdventureStackRef{.stable_id = t->stable_id, .object_id = t->object_id, .cell = t->cell};
}

void AdventureScreen::store_pointer_position(int logical_x, int logical_y) {
    last_pointer_x_ = logical_x;
    last_pointer_y_ = logical_y;
    last_pointer_valid_ = true;
}

void AdventureScreen::refresh_pointer_interaction() {
    if (!last_pointer_valid_ || !pick_index_built_ || !render_state_.has_terrain_texture()) {
        hovered_stack_.reset();
        interaction_stack_.reset();
        return;
    }

    AdventureHitTester           hit_tester(pick_index_, render_state_.camera());
    const AdventurePointerResult result = hit_tester.hit_test(last_pointer_x_, last_pointer_y_);

    const bool had_hover = hovered_stack_.has_value();
    const bool had_interaction = interaction_stack_.has_value();

    hovered_stack_ = pick_to_ref(result.occupied_cell_hover);
    interaction_stack_ = pick_to_ref(result.interaction_target);

    const bool has_hover = hovered_stack_.has_value();
    const bool has_interaction = interaction_stack_.has_value();

    const bool hover_changed =
        had_hover != has_hover ||
        (has_hover && hovered_stack_->object_id != (had_hover ? prev_hover_id_ : ""));
    const bool interaction_changed =
        had_interaction != has_interaction ||
        (has_interaction &&
         interaction_stack_->object_id != (had_interaction ? prev_interaction_id_ : ""));

    if (hover_changed || interaction_changed) {
        prev_hover_id_ = has_hover ? hovered_stack_->object_id : "";
        prev_interaction_id_ = has_interaction ? interaction_stack_->object_id : "";
        kLog->debug("adventure_pointer_refresh pointer=({},{}) camera=({},{}) hover={} "
                    "interaction={}",
                    last_pointer_x_, last_pointer_y_, render_state_.camera().canvas_x,
                    render_state_.camera().canvas_y, has_hover ? hovered_stack_->object_id : "none",
                    has_interaction ? interaction_stack_->object_id : "none");
    }
}

void AdventureScreen::handle_pointer_at(int logical_x, int logical_y) {
    store_pointer_position(logical_x, logical_y);
    refresh_pointer_interaction();
}

void AdventureScreen::handle_select_at(int logical_x, int logical_y) {
    store_pointer_position(logical_x, logical_y);
    refresh_pointer_interaction();

    AdventureMovementClickTarget target;
    if (interaction_stack_.has_value())
        target.stack_id = interaction_stack_->object_id;
    target.cell = map_cell_at_screen(logical_x, logical_y);
    if (auto result = AdventureMovementClickController::handle_left_click(*session_, target)) {
        log_movement_result(*result);
        for (const auto& event : result->events) {
            if (const auto* started = std::get_if<d2game::AdventureMovementStarted>(&event)) {
                const auto& state = session_->adventure_movement_state();
                if (!state.execution)
                    throw std::logic_error("movement started without execution state");
                AssetRuntimeCatalogAdapter         catalog(runtime_.assets);
                AdventureMovementVisualPlanBuilder builder(runtime_.game_data, catalog,
                                                           banner_catalog_);
                movement_visual_plan_ = builder.build(session_->world(), *state.execution);
                preload_adventure_movement_visual_plan(*movement_visual_plan_, runtime_.assets,
                                                       runtime_.render_assets);
                validate_adventure_movement_animation_domains(
                    *movement_visual_plan_, prepared_map_.geometry, animation_players_,
                    route_preview_animation_players_);
                movement_presentation_.begin(*movement_visual_plan_, prepared_map_.geometry);
                hovered_stack_.reset();
                interaction_stack_.reset();
                (void)started;
            }
        }
    }
    sync_route_preview_presentation();
}

void AdventureScreen::handle_inspect_at(int logical_x, int logical_y) {
    store_pointer_position(logical_x, logical_y);
    refresh_pointer_interaction();

    if (interaction_stack_.has_value()) {
        auto model = inspection_builder_->build(interaction_stack_->object_id);
        if (model.has_value()) {
            log_stack_inspection(*model);
            if (request_stack_info_) {
                request_stack_info_(std::move(*model));
            }
        }
    }
}

void AdventureScreen::update(const d2::app::ScreenUpdateContext& context) {
    sync_route_preview_presentation();
    for (auto& [id, player] : animation_players_)
        player.update(context.animation_delta_ms);
    for (auto& [id, player] : route_preview_animation_players_)
        player.update(context.animation_delta_ms);
    if (movement_presentation_.active()) {
        const auto update = movement_presentation_.update(
            static_cast<int>(std::lround(context.animation_delta_ms)), *session_);
        for (const auto& result : update.command_results)
            log_movement_result(result);
        if (update.settlement) {
            const auto& settlement = *update.settlement;
            const auto* stack = session_->world().find_stack(settlement.stack_id);
            if (!stack || !d2runtime::is_stack_on_adventure_map(*stack))
                throw std::logic_error("movement settlement stack is not map-visible id=" +
                                       settlement.stack_id);
            if (!movement_visual_plan_ ||
                settlement.last_committed_step_index >= movement_visual_plan_->segments.size())
                throw std::logic_error("movement settlement segment is missing id=" +
                                       settlement.stack_id);
            const auto& segment =
                movement_visual_plan_->segments[settlement.last_committed_step_index];
            if (stack->position != segment.to || stack->facing != segment.direction)
                throw std::logic_error("movement settlement authoritative state mismatch id=" +
                                       settlement.stack_id);
            AdventureActorGraphUpdater::settle_stack_actor(
                *stack, settlement.idle_visual, settlement.idle_interaction_mask,
                settlement.banner_asset, settlement.banner_index, prepared_map_,
                animation_players_);
            pick_index_.build(prepared_map_, world_visuals_.selection_circle);
            pick_index_built_ = true;
            hovered_stack_.reset();
            interaction_stack_.reset();
            prev_hover_id_.clear();
            prev_interaction_id_.clear();
            movement_presentation_.clear();
            movement_visual_plan_.reset();
        } else if (!movement_presentation_.active()) {
            movement_visual_plan_.reset();
        }
        if (follow_unit_)
            follow_moving_unit_camera();
        sync_route_preview_presentation();
    }
}

std::optional<AdventureStackRef> AdventureScreen::selected_stack_visual() const {
    const auto& selected = session_->adventure_movement_state().selected_stack_id;
    if (!selected)
        return std::nullopt;
    const auto* stack = session_->world().find_stack(*selected);
    if (stack == nullptr)
        return std::nullopt;
    const auto cell = resolve_stack_selection_cell(session_->world(), *stack);
    if (!cell.has_value())
        return std::nullopt;
    return AdventureStackRef{adventure_render::stable_render_id("SelectSel:" + stack->id),
                             stack->id, *cell};
}

std::optional<d2runtime::MapCellCoord> AdventureScreen::map_cell_at_screen(int logical_x,
                                                                           int logical_y) const {
    if (!render_state_.has_terrain_texture())
        return std::nullopt;
    const auto& camera = render_state_.camera();
    return prepared_map_.geometry.canvas_to_cell(camera.screen_to_canvas_x(logical_x),
                                                 camera.screen_to_canvas_y(logical_y));
}

void AdventureScreen::log_movement_result(const d2game::GameCommandResult& result) const {
    for (const auto& event : result.events) {
        std::visit(
            [](const auto& typed_event) {
                using Event = std::decay_t<decltype(typed_event)>;
                if constexpr (std::is_same_v<Event, d2game::AdventureStackSelected>) {
                    kLog->debug("adventure_stack_selected stack={}", typed_event.stack_id);
                } else if constexpr (std::is_same_v<Event, d2game::AdventureRoutePlanned>) {
                    kLog->debug("adventure_route_planned stack={} destination=({}, {}) cost={}",
                                typed_event.stack_id, typed_event.destination.x,
                                typed_event.destination.y, typed_event.total_cost);
                } else if constexpr (std::is_same_v<Event, d2game::AdventureSelectionCleared>) {
                    kLog->debug("adventure_selection_cleared previous={}",
                                typed_event.previous_stack_id);
                } else if constexpr (std::is_same_v<Event, d2game::AdventureMovementRejected>) {
                    kLog->debug("adventure_movement_rejected action={} reason={} path_status={} "
                                "block_reason={} navigation_errors={}",
                                static_cast<int>(typed_event.action),
                                static_cast<int>(typed_event.reason),
                                typed_event.path_status
                                    ? std::to_string(static_cast<int>(*typed_event.path_status))
                                    : "none",
                                static_cast<int>(typed_event.block_reason),
                                typed_event.navigation_error_count);
                } else if constexpr (std::is_same_v<Event, d2game::AdventureMovementStarted>) {
                    kLog->debug("adventure_movement_started stack={} requested=({}, {}) "
                                "execution=({}, {}) steps={} limited={}",
                                typed_event.stack_id, typed_event.requested_destination.x,
                                typed_event.requested_destination.y,
                                typed_event.execution_destination.x,
                                typed_event.execution_destination.y, typed_event.step_count,
                                typed_event.limited_by_movement_points);
                } else if constexpr (std::is_same_v<Event, d2game::AdventureMovementCompleted>) {
                    kLog->debug("adventure_movement_completed stack={} final=({}, {}) "
                                "requested=({}, {}) limited={}",
                                typed_event.stack_id, typed_event.final_position.x,
                                typed_event.final_position.y, typed_event.requested_destination.x,
                                typed_event.requested_destination.y,
                                typed_event.limited_by_movement_points);
                } else if constexpr (std::is_same_v<Event,
                                                    d2game::AdventureMovementPointsDebugChanged>) {
                    kLog->debug("adventure_movement_points_debug_changed stack={} change={} "
                                "previous={} current={} reset={}",
                                typed_event.stack_id, static_cast<int>(typed_event.change),
                                typed_event.previous_movement_points,
                                typed_event.current_movement_points,
                                typed_event.reset_movement_points);
                }
            },
            event);
    }
}

void AdventureScreen::sync_route_preview_presentation() {
    const auto preview = session_->adventure_route_preview();
    if (preview == cached_route_preview_)
        return;
    cached_route_preview_ = preview;
    route_preview_presentation_ = {};
    route_preview_animation_players_.clear();
    if (!preview || !session_->adventure_movement_state().selected_stack_id)
        return;
    AdventureRoutePreviewPresentationBuilder builder;
    route_preview_presentation_ =
        builder.build(*preview, *session_->adventure_movement_state().selected_stack_id,
                      world_visuals_.route_preview, prepared_map_.geometry);
    validate_route_preview_animation_player_ids(route_preview_presentation_, animation_players_);
    adventure_render::PreparedAdventureRenderGraph graph;
    graph.ground_overlay = route_preview_presentation_.ground_overlays;
    graph.world = route_preview_presentation_.world_primitives;
    route_preview_animation_players_ = build_adventure_animation_players(graph);
}

void AdventureScreen::center_camera(int viewport_w, int viewport_h) {
    if (!render_state_.has_terrain_texture())
        return;
    render_state_.set_camera(AdventureCamera::centered(
        render_state_.texture_width(), render_state_.texture_height(), viewport_w, viewport_h));
}

void AdventureScreen::handle_zoom_in() {
    auto        cam = render_state_.camera();
    const float old_zoom = cam.zoom();
    if (!cam.zoom_in())
        return;
    kLog->info("adventure_camera_zoom dir=in old={:.2f} new={:.2f} camera=({},{}) "
               "visible_extent=({},{})",
               old_zoom, cam.zoom(), cam.canvas_x, cam.canvas_y, cam.visible_canvas_w(),
               cam.visible_canvas_h());
    commit_camera(cam);
}

void AdventureScreen::handle_zoom_out() {
    auto        cam = render_state_.camera();
    const float old_zoom = cam.zoom();
    if (!cam.zoom_out())
        return;
    kLog->info("adventure_camera_zoom dir=out old={:.2f} new={:.2f} camera=({},{}) "
               "visible_extent=({},{})",
               old_zoom, cam.zoom(), cam.canvas_x, cam.canvas_y, cam.visible_canvas_w(),
               cam.visible_canvas_h());
    commit_camera(cam);
}

void AdventureScreen::commit_camera(AdventureCamera cam) {
    cam.clamp_center_to_canvas_diamond(render_state_.texture_width(),
                                       render_state_.texture_height());
    render_state_.set_camera(cam);
    if (last_pointer_valid_)
        refresh_pointer_interaction();
}

void AdventureScreen::follow_moving_unit_camera() {
    if (!follow_unit_ || !render_state_.has_terrain_texture())
        return;
    std::optional<adventure_render::ScreenPoint> foot;
    if (movement_presentation_.active())
        foot = movement_presentation_.current_actor_foot();
    if (!foot && session_->adventure_movement_state().selected_stack_id) {
        const auto* stack =
            session_->world().find_stack(*session_->adventure_movement_state().selected_stack_id);
        if (stack != nullptr && d2runtime::is_stack_on_adventure_map(*stack))
            foot = prepared_map_.geometry.cell_foot_anchor(stack->position);
    }
    if (!foot)
        return;
    auto cam = render_state_.camera();
    cam.canvas_x = foot->x - cam.visible_canvas_w() / 2;
    cam.canvas_y = foot->y - cam.visible_canvas_h() / 2;
    commit_camera(cam);
}

void AdventureScreen::render(Renderer2D& renderer) {
    using namespace adventure_render;

    renderer.clear({25, 25, 38, 255});

    const auto& cam = render_state_.camera();
    const float z = cam.zoom();

    if (render_state_.has_terrain_texture() && cam.viewport_width > 0 && cam.viewport_height > 0) {
        const auto blit = compute_clipped_canvas_blit(cam, render_state_.texture_width(),
                                                      render_state_.texture_height());
        if (blit.has_value()) {
            renderer.draw_texture(render_state_.terrain_texture(), blit->source, blit->destination);
        }
    }

    std::vector<PreparedAdventureRenderPrimitive> dynamic_prims;
    std::vector<PreparedAdventureRenderPrimitive> static_world;
    static_world.reserve(prepared_map_.world_graph.world.size());
    const auto moving_stack_id = movement_presentation_.active()
                                     ? std::string(movement_presentation_.stack_id())
                                     : std::string{};
    const auto static_ids = adventure_render::map_stack_presentation_render_ids(moving_stack_id);
    for (const auto& primitive : prepared_map_.world_graph.world) {
        if (!primitive_visible(primitive))
            continue;
        if (movement_presentation_.active() &&
            (primitive.stable_id == static_ids.body || primitive.stable_id == static_ids.shadow ||
             primitive.stable_id == static_ids.banner))
            continue;
        static_world.push_back(primitive);
    }
    const auto selected_stack = selected_stack_visual();
    for (const auto& prim : route_preview_presentation_.world_primitives) {
        if (primitive_visible(prim))
            dynamic_prims.push_back(prim);
    }
    if (movement_presentation_.active()) {
        auto moving = movement_presentation_.current_world_primitives(prepared_map_.geometry);
        for (const auto& prim : moving) {
            if (primitive_visible(prim))
                dynamic_prims.push_back(prim);
        }
    }

    if (selected_stack.has_value() && !movement_presentation_.active()) {
        dynamic_prims.push_back(build_selection_primitive(
            *selected_stack, selected_stack->cell, world_visuals_.select_selected,
            stable_render_id("SelectSel:" + selected_stack->object_id), prepared_map_.geometry));
    }

    if (hovered_stack_.has_value() && !movement_presentation_.active()) {
        bool same_as_selected =
            selected_stack.has_value() && hovered_stack_->object_id == selected_stack->object_id;
        if (!same_as_selected) {
            dynamic_prims.push_back(build_selection_primitive(
                *hovered_stack_, hovered_stack_->cell, world_visuals_.select_no,
                stable_render_id("SelectHover:" + hovered_stack_->object_id),
                prepared_map_.geometry));
        }
    }

    IsoDepthResolver resolver(prepared_map_.geometry);
    auto             frame_world = compose_frame_world(static_world, dynamic_prims, resolver);

    const auto draw_primitive = [&](const auto& prim, Color color) {
        using adventure_render::resolve_adventure_frame_visual;

        if (!primitive_visible(prim)) {
            return;
        }

        std::optional<std::size_t> anim_frame_idx;
        if (prim.animation.has_value()) {
            const auto  player_id = adventure_animation_player_id(prim);
            const auto& movement_players = movement_presentation_.animation_players();
            auto        movement_it = movement_players.find(player_id);
            const auto  preview_it = route_preview_animation_players_.find(player_id);
            auto        it = animation_players_.find(player_id);
            if (movement_it == movement_players.end() &&
                preview_it == route_preview_animation_players_.end() &&
                it == animation_players_.end()) {
                std::string msg = "animated primitive missing animation player stable_id=";
                msg += std::to_string(prim.stable_id);
                msg += " label=" + prim.debug_label;
                msg += " player_id=" + std::to_string(player_id);
                msg += " animation=" + prim.animation->animation_name;
                throw std::logic_error(msg);
            }
            if (movement_it != movement_players.end())
                anim_frame_idx = movement_it->second.current_frame_index();
            else if (preview_it != route_preview_animation_players_.end())
                anim_frame_idx = preview_it->second.current_frame_index();
            else
                anim_frame_idx = it->second.current_frame_index();
        }

        const auto resolved = resolve_adventure_frame_visual(prim, anim_frame_idx);

        const float sx = cam.canvas_to_screen_x_float(static_cast<float>(resolved.draw_origin.x));
        const float sy = cam.canvas_to_screen_y_float(static_cast<float>(resolved.draw_origin.y));
        const float src_w = static_cast<float>(resolved.src_width);
        const float src_h = static_cast<float>(resolved.src_height);
        const std::string record(resolved.record_name);

        if (!prim.container_path.empty() && !record.empty()) {
            const ImageAssetKey key{.container_path = prim.container_path,
                                    .image_name = record,
                                    .kind = ImageAssetKind::ComposedSprite};
            if (auto* texture = runtime_.render_assets.textures().find(key); texture != nullptr) {
                renderer.draw_texture(texture, {sx, sy, src_w * z, src_h * z}, resolved.alpha);
                return;
            }
        }
        const auto fallback_a = static_cast<uint8_t>(
            static_cast<int>(static_cast<float>(color.a) * std::clamp(resolved.alpha, 0.0f, 1.0f)));
        renderer.draw_rect({sx, sy, 12.0f * z, 12.0f * z}, {color.r, color.g, color.b, fallback_a},
                           true);
    };

    for (const auto& p : prepared_map_.world_graph.ground_overlay)
        draw_primitive(p, Color{100, 200, 100, 180});
    for (const auto& p : route_preview_presentation_.ground_overlays)
        draw_primitive(p, Color{100, 200, 100, 180});
    for (const auto& p : frame_world.primitives)
        draw_primitive(p, Color{200, 200, 100, 180});
    for (const auto& p : prepared_map_.world_graph.world_overlay)
        draw_primitive(p, Color{200, 100, 200, 180});

    for (const auto& label : route_preview_presentation_.movement_point_labels) {
        const float anchor_x =
            cam.canvas_to_screen_x_float(static_cast<float>(label.canvas_anchor.x));
        const float anchor_y =
            cam.canvas_to_screen_y_float(static_cast<float>(label.canvas_anchor.y));
        const float label_half_width = 18.0f;
        const float label_height = 18.0f;
        const float label_width = label_half_width * 2.0f;
        const auto  rect =
            Rect{anchor_x - label_half_width, anchor_y - label_height, label_width, label_height};
        const auto text = std::to_string(label.remaining_movement_points);
        renderer.draw_text_box({{rect.x + 1.0f, rect.y + 1.0f, rect.w, rect.h},
                                text,
                                {.color = {0, 0, 0, 220},
                                 .align = TextAlign::Center,
                                 .valign = TextVAlign::Middle,
                                 .wrap = TextWrapMode::None,
                                 .overflow = TextOverflowMode::ShrinkToFit}});
        renderer.draw_text_box({rect,
                                text,
                                {.color = {255, 255, 255, 255},
                                 .align = TextAlign::Center,
                                 .valign = TextVAlign::Middle,
                                 .wrap = TextWrapMode::None,
                                 .overflow = TextOverflowMode::ShrinkToFit}});
    }

    std::ostringstream info;
    const auto         route_preview = session_->adventure_route_preview();
    bool               action_limit = false;
    if (route_preview) {
        for (const auto& step : route_preview->steps) {
            if (step.marker == d2adventure::AdventureRouteMarkerKind::ActionLimit) {
                action_limit = true;
                break;
            }
        }
    }
    info << "AdventureMap  " << summary_.scenario_id << "\n"
         << "Canvas: " << prepared_map_.canvas_width() << "x" << prepared_map_.canvas_height()
         << "\n"
         << "Camera: " << cam.canvas_x << "," << cam.canvas_y << " " << cam.viewport_width << "x"
         << cam.viewport_height << " zoom=" << z << "\n"
         << "World: " << prepared_map_.world_graph.world.size() << "  "
         << "Sel: " << (selected_stack ? selected_stack->object_id : "none") << "  "
         << "Hov: " << (hovered_stack_.has_value() ? hovered_stack_->object_id : "none")
         << "  Preview: " << (route_preview ? std::to_string(route_preview->steps.size()) : "none")
         << "  ActionLimit: " << (action_limit ? "yes" : "no");
    if (route_preview) {
        info << "  Destination: (" << route_preview->destination.x << ","
             << route_preview->destination.y << ")";
    }

    renderer.draw_debug_text(10.0f, 10.0f, info.str().c_str());
}

void AdventureScreen::ensure_animation_players() {
    if (animation_players_built_)
        return;

    animation_players_ = build_adventure_animation_players(prepared_map_.world_graph);
    animation_players_built_ = true;
}

} // namespace d2engine
