#pragma once

#include "app_config.hpp"
#include "app_runtime_context.hpp"
#include "battle_screen_action.hpp"
#include "screen.hpp"

#include "../assets/portrait_manifest_index.hpp"
#include "../battle_view/battle_debug_scene_controller.hpp"
#include "../battle_view/battle_presenter.hpp"
#include "../battle_view/battle_scenario_executor.hpp"
#include "../battle_view/battle_scenario_runtime.hpp"
#include "../battle_view/battle_scene.hpp"
#include "../battle_view/battle_scene_presentation_state.hpp"
#include "../battle_view/battle_startup_texture_warmup.hpp"
#include "../battle_view/layout_types.hpp"
#include "debug_command_handler.hpp"
#include "battle_tuning_controller.hpp"
#include "unit_debug_metadata.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

struct SDL_Renderer;

namespace d2engine {

class BattleUnitFactory;
class DebugUiRenderer;
class RawFfAnimationCatalog;
class BattleStartupSession;

enum class BattleScreenPhase {
    Loading,
    Ready,
    Failed,
};

enum class BattleStartupStage {
    Bootstrap,
    DiscoverBackgrounds,
    BuildWarmupPlan,
    SubmitAssetPreparation,
    UploadAssets,
    FinalizeScenario,
    Complete,
    Failed,
};

struct BattleStartupProgress {
    BattleStartupStage stage = BattleStartupStage::Bootstrap;
    std::size_t        completed = 0;
    std::size_t        total = 0;
    std::string        current_operation = "bootstrap";
};

struct BattleStartupBudget {
    std::size_t max_uploads = 4;
    double      max_upload_ms = 2.0;
};

class BattleStartupError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class BattleScreen final : public Screen {
public:
    BattleScreen(const AppConfig& config, const AppRuntimeContext& context,
                 BattleTuningController& tuning, SDL_Renderer* renderer, TreeLayout tree_layout,
                 std::string config_source, TreeLayoutEditor& tree_layout_editor,
                 std::function<void()> request_close = nullptr);
    ~BattleScreen() override;

    BattleScreen(const BattleScreen&) = delete;
    BattleScreen& operator=(const BattleScreen&) = delete;
    BattleScreen(BattleScreen&&) = delete;
    BattleScreen& operator=(BattleScreen&&) = delete;

    std::string_view                name() const override { return "BattleScreen"; }
    [[nodiscard]] ScreenStackPolicy stack_policy() const noexcept override { return {}; }

    [[nodiscard]] static std::vector<std::string> required_layout_nodes();

    void on_enter() override;
    bool handle_input(const InputEvent& event) override;
    void update(const d2::app::ScreenUpdateContext& context) override;
    void render(Renderer2D& renderer) override;

private:
    friend class BattleStartupSession;

    void commit_startup();
    void restart_current_scenario();
    void refresh_unit_debug_metadata();

    bool apply_battle_screen_action(const BattleScreenAction& action);

    std::function<void()>                                   request_close_;
    /*[[maybe_unused]]*/ [[maybe_unused]] TreeLayoutEditor& tree_layout_editor_;

    const AppConfig&         config_;
    const AppRuntimeContext& context_;
    BattleTuningController&  tuning_;
    SDL_Renderer*            renderer_ = nullptr;

    std::unique_ptr<BattleScene>           scene_;
    BattleScenePresentationState           scene_presentation_;
    std::unique_ptr<BattlePresenter>       presenter_;
    std::unique_ptr<RawFfAnimationCatalog> scenario_animation_catalog_;
    std::unique_ptr<BattleScenarioRuntime> scenario_runtime_;
    std::unique_ptr<BattleUnitFactory>     unit_factory_;
    std::optional<BattleScenarioPlayer>    scenario_player_;

    std::string                    scenario_play_seq_id_;
    std::vector<UnitDebugMetadata> unit_debug_;
    std::vector<std::string>       bg_names_;
    std::vector<std::string>       bg_overlay_names_;
    std::size_t                    bg_index_ = 0;

    bool transparent_bg_ = false;
    bool visual_paused_ = false;
    bool solo_selected_ = false;
    bool draw_debug_slot_anchors_ = false;
    bool debug_hud_visible_ = true;
    bool log_first_render_after_preload_ = false;

    bool          autoplay_metrics_active_ = false;
    std::uint64_t autoplay_start_ticks_ = 0;
    std::uint64_t autoplay_lazy_misses_start_ = 0;
    std::size_t   autoplay_lazy_miss_diagnostics_start_ = 0;
    std::size_t   autoplay_frame_count_ = 0;
    double        autoplay_render_sum_ms_ = 0.0;
    double        autoplay_render_max_ms_ = 0.0;

    float mouse_ref_x_ = 0.0f;
    float mouse_ref_y_ = 0.0f;

    bool                                  initialized_ = false;
    BattleScreenPhase                     phase_ = BattleScreenPhase::Loading;
    std::unique_ptr<BattleStartupSession> startup_;
    BattleStartupProgress                 startup_progress_;
    std::string                           startup_failure_;
};

} // namespace d2engine
