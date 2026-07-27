#pragma once

#include "adventure_interaction_mask.hpp"
#include "adventure_pick_index.hpp"
#include "adventure_screen_input.hpp"
#include "adventure_selection_builder.hpp"
#include "adventure_visual_resources.hpp"
#include "adventure_route_preview_presentation.hpp"
#include "adventure_movement_click_controller.hpp"
#include "adventure_movement_visual_plan.hpp"
#include "adventure_movement_presentation_controller.hpp"
#include "adventure_actor_graph_updater.hpp"
#include "adventure_animation_helpers.hpp"
#include "app_runtime_context.hpp"
#include "screen.hpp"
#include "stack_inspection.hpp"

#include "../assets/image_asset_key.hpp"
#include "../render/adventure_render_state.hpp"
#include "../render/render_asset_runtime.hpp"
#include "../animation/animation_player.hpp"

#include <d2adventure_render/adventure_frame_composer.hpp>
#include <d2adventure_render/prepared_adventure_map.hpp>
#include <d2adventure_render/stack_banner_asset_catalog.hpp>
#include <d2game/GameSession.hpp>
#include <d2runtime/AdventureIsoDirection.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace d2engine {

class RenderAssetRuntime;

class AdventureScreen final : public Screen {
public:
    AdventureScreen(AppRuntimeContext& runtime, std::unique_ptr<d2game::GameSession> session,
                    d2engine::adventure_render::PreparedAdventureMap prepared_map,
                    AdventureRenderState render_state, int logical_viewport_w,
                    int logical_viewport_h, TreeLayout tree_layout, std::string config_source,
                    std::function<void()>                     request_quit = nullptr,
                    std::function<void()>                     request_debug_battle = nullptr,
                    std::function<void(StackInspectionModel)> request_stack_info = nullptr,
                    AdventureWorldVisualResources             world_visuals = {},
                    adventure_render::StackBannerAssetCatalog banner_catalog = {});
    ~AdventureScreen() override;

    AdventureScreen(const AdventureScreen&) = delete;
    AdventureScreen& operator=(const AdventureScreen&) = delete;
    AdventureScreen(AdventureScreen&&) = delete;
    AdventureScreen& operator=(AdventureScreen&&) = delete;

    std::string_view                name() const override { return "AdventureScreen"; }
    [[nodiscard]] ScreenStackPolicy stack_policy() const noexcept override { return {}; }

    void       on_enter() override;
    void       on_revealed() override;
    CursorKind cursor_kind() const override;

    bool handle_input(const InputEvent& event) override;
    void update(const d2::app::ScreenUpdateContext& context) override;
    void render(Renderer2D& renderer) override;

    [[nodiscard]] d2game::AdventureMovementDebugSnapshot movement_debug_snapshot() const;
    d2game::GameCommandResult debug_reset_selected_stack_movement_points();
    d2game::GameCommandResult debug_grant_selected_stack_free_movement_points();
    void                      debug_toggle_follow_unit();
    [[nodiscard]] bool        debug_follow_unit_enabled() const { return follow_unit_; }
    void                      debug_toggle_banners();
    [[nodiscard]] bool        debug_banners_visible() const { return show_banners_; }
    [[nodiscard]] bool        debug_banners_enabled() const { return show_banners_; }

    [[nodiscard]] static std::vector<std::string> required_layout_nodes();

private:
    void center_camera(int viewport_w, int viewport_h);
    void handle_pointer_at(int logical_x, int logical_y);
    void handle_select_at(int logical_x, int logical_y);
    void handle_inspect_at(int logical_x, int logical_y);

    void refresh_pointer_interaction();

    void handle_zoom_in();
    void handle_zoom_out();

    void store_pointer_position(int logical_x, int logical_y);

    void commit_camera(AdventureCamera cam);
    void follow_moving_unit_camera();
    [[nodiscard]] bool
    primitive_visible(const adventure_render::PreparedAdventureRenderPrimitive& prim) const;

    void ensure_animation_players();
    void sync_route_preview_presentation();
    void log_movement_result(const d2game::GameCommandResult& result) const;
    [[nodiscard]] std::optional<AdventureStackRef>       selected_stack_visual() const;
    [[nodiscard]] std::optional<d2runtime::MapCellCoord> map_cell_at_screen(int logical_x,
                                                                            int logical_y) const;

    AppRuntimeContext&                               runtime_;
    std::unique_ptr<d2game::GameSession>             session_;
    std::function<void()>                            request_quit_;
    std::function<void()>                            request_debug_battle_;
    std::function<void(StackInspectionModel)>        request_stack_info_;
    d2engine::adventure_render::PreparedAdventureMap prepared_map_;
    AdventureRenderState                             render_state_;
    int                                              logical_viewport_w_ = 1416;
    int                                              logical_viewport_h_ = 852;
    d2game::WorldInspectSummary                      summary_;
    bool                                             summary_loaded_ = false;
    AdventurePickIndex                               pick_index_;
    std::optional<StackInspectionBuilder>            inspection_builder_;
    bool                                             pick_index_built_ = false;

    AdventureWorldVisualResources             world_visuals_;
    adventure_render::StackBannerAssetCatalog banner_catalog_;

    std::optional<AdventureStackRef>                  hovered_stack_;
    std::optional<AdventureStackRef>                  interaction_stack_;
    std::optional<d2adventure::AdventureRoutePreview> cached_route_preview_;
    AdventureRoutePreviewPresentation                 route_preview_presentation_;
    AdventureAnimationPlayerMap                       route_preview_animation_players_;
    std::optional<AdventureMovementVisualPlan>        movement_visual_plan_;
    AdventureMovementPresentationController           movement_presentation_;
    bool                                              follow_unit_ = false;
    bool                                              show_banners_ = true;

    int  last_pointer_x_ = 0;
    int  last_pointer_y_ = 0;
    bool last_pointer_valid_ = false;

    std::string prev_hover_id_;
    std::string prev_interaction_id_;

    std::unordered_map<adventure_render::StableRenderId, AnimationPlayer> animation_players_;
    bool animation_players_built_ = false;
};

} // namespace d2engine
