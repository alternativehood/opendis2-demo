#include "battle_screen.hpp"
#include "battle_scenario.hpp"
#include "battle_scenario_bootstrapper.hpp"
#include "tree_layout_editor.hpp"
#include "battle_screen_action.hpp"
#include "battle_screen_input.hpp"
#include "battle_viewer_controller.hpp"
#include "battle_viewer_renderer.hpp"
#include "debug_overlay_renderer.hpp"

#include "../assets/asset_runtime.hpp"
#include "../assets/game_data_registry.hpp"
#include "../assets/portrait_manifest.hpp"
#include "../battle_adapters/raw_ff_animation_catalog.hpp"
#include "../battle_adapters/sdl_battle_texture_provider.hpp"
#include "../battle_view/battle_presenter.hpp"
#include "../battle_view/battle_startup_texture_warmup.hpp"
#include "../render/color.hpp"
#include "../render/game_texture_cache.hpp"
#include "../render/renderer2d.hpp"
#include "../render/render_asset_runtime.hpp"
#include "../render/text/text_box_renderer.hpp"

#include "d2res/dbf_reader.hpp"
#include "d2res/opt_maps.hpp"

#include <d2log/log.hpp>

#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace d2engine {

namespace {

auto kLog = d2log::get("d2.app"); // NOLINT(cert-err58-cpp)

std::string
format_lazy_miss_summary(const std::vector<GameTextureCache::LazyRenderMissDiagnostic>& misses,
                         std::size_t start_index) {
    std::ostringstream out;
    for (std::size_t i = start_index; i < misses.size(); ++i) {
        const auto& miss = misses[i];
        if (i != start_index) {
            out << "; ";
        }
        out << miss.container_path << "/" << miss.image_name << " frame=" << miss.frame_number
            << " render_ms=" << miss.render_time_ms;
        if (!miss.script_step.empty()) {
            out << " step=" << miss.script_step;
        }
        if (!miss.script_envelope.empty()) {
            out << " envelope=" << miss.script_envelope;
        }
    }
    return out.str();
}

} // namespace

struct BattleStartupArtifacts {
    std::unique_ptr<BattleScene>           scene = std::make_unique<BattleScene>();
    BattleScenePresentationState           presentation;
    std::unique_ptr<BattlePresenter>       presenter;
    std::unique_ptr<RawFfAnimationCatalog> animation_catalog;
    std::unique_ptr<BattleScenarioRuntime> scenario_runtime;
    std::unique_ptr<BattleUnitFactory>     unit_factory;
    BattleScenario                         scenario;
    std::string                            play_sequence_id;
    std::size_t                            start_step = 0;
    std::vector<UnitDebugMetadata>         unit_debug;
    std::vector<std::string>               background_names;
    std::vector<std::string>               background_overlay_names;
    std::size_t                            background_index = 0;
};

class BattleStartupSession {
public:
    explicit BattleStartupSession(BattleScreen& screen, std::string preferred_sequence = {})
        : screen_(screen), preferred_sequence_(std::move(preferred_sequence)) {
        artifacts_.presentation = screen.scene_presentation_;
    }

    [[nodiscard]] BattleStartupProgress progress() const { return progress_; }
    [[nodiscard]] bool                  complete() const noexcept { return complete_; }
    [[nodiscard]] bool                  failed() const noexcept { return failed_; }

    void advance(BattleStartupBudget budget) {
        if (complete_ || failed_) {
            return;
        }

        switch (progress_.stage) {
        case BattleStartupStage::Bootstrap:
            bootstrap();
            break;
        case BattleStartupStage::DiscoverBackgrounds:
            discover_backgrounds();
            break;
        case BattleStartupStage::BuildWarmupPlan:
            build_warmup_plan();
            break;
        case BattleStartupStage::SubmitAssetPreparation:
            submit_asset_preparation();
            break;
        case BattleStartupStage::UploadAssets:
            upload_assets(budget);
            break;
        case BattleStartupStage::FinalizeScenario:
            progress_.stage = BattleStartupStage::Complete;
            progress_.current_operation = "complete";
            complete_ = true;
            break;
        case BattleStartupStage::Complete:
        case BattleStartupStage::Failed:
            break;
        }
    }

    [[nodiscard]] BattleStartupArtifacts take_result() {
        if (!complete_) {
            throw std::logic_error("BattleStartupSession result requested before completion");
        }
        return std::move(artifacts_);
    }

private:
    void bootstrap() {
        try {
            preflight_.emplace(
                preflight_battle_session(screen_.context_.assets, screen_.context_.game_data,
                                         screen_.tuning_.state().attack_visual_intents,
                                         screen_.config_.battle_script_path, preferred_sequence_));
        } catch (const std::runtime_error& error) {
            failed_ = true;
            progress_.stage = BattleStartupStage::Failed;
            progress_.current_operation = "bootstrap failed";
            throw BattleStartupError(std::string{"battle bootstrap failed: "} + error.what());
        }
        progress_.stage = BattleStartupStage::DiscoverBackgrounds;
        progress_.current_operation = "discover backgrounds";
    }

    void discover_backgrounds() {
        auto& store = screen_.context_.store();
        try {
            for (const auto& name : store.sprites_in("Imgs/Battle.ff")) {
                const auto size = name.size();
                if (size > 3 && name[size - 3] == '_' && name[size - 2] == 'B' &&
                    name[size - 1] == 'G') {
                    artifacts_.background_names.push_back(name);
                }
            }
            std::ranges::sort(artifacts_.background_names);
        } catch (const std::runtime_error& error) {
            kLog->warn("battle_background_enumeration_failed error={}", error.what());
        }

        static const std::unordered_map<std::string, std::string> terrain_map = {
            {"HU", "HUMAN"},  {"EL", "ELF"},     {"DW", "DWARF"}, {"HE", "HERETIC"},
            {"UN", "UNDEAD"}, {"NE", "NEUTRAL"}, {"WA", "WATER"},
        };
        const auto terrain_it = terrain_map.find(preflight_->scenario.terrain);
        const auto base =
            terrain_it == terrain_map.end() ? preflight_->scenario.terrain : terrain_it->second;
        const auto bg_it = std::ranges::find(artifacts_.background_names, base + "_0_BG");
        artifacts_.background_index = bg_it == artifacts_.background_names.end()
                                          ? 0
                                          : static_cast<std::size_t>(std::distance(
                                                artifacts_.background_names.begin(), bg_it));
        if (!artifacts_.background_names.empty()) {
            const std::string& bg_name = artifacts_.background_names[artifacts_.background_index];
            if (const d2res::OptMaps* maps = store.container_maps("Imgs/Battle.ff");
                maps != nullptr) {
                artifacts_.background_overlay_names = maps->index_map.overlay_variants(bg_name);
            }
        }
        progress_.stage = BattleStartupStage::BuildWarmupPlan;
        progress_.current_operation = "build warmup plan";
    }

    void build_warmup_plan() {
        const auto boot =
            commit_battle_session(std::move(*preflight_),
                                  {.assets = screen_.context_.assets,
                                   .game_data = screen_.context_.game_data,
                                   .scene = *artifacts_.scene,
                                   .attack_intents = screen_.tuning_.state().attack_visual_intents,
                                   .scenario_path = screen_.config_.battle_script_path,
                                   .preferred_seq_id = preferred_sequence_,
                                   .out_catalog = artifacts_.animation_catalog,
                                   .out_factory = artifacts_.unit_factory,
                                   .out_presenter = artifacts_.presenter,
                                   .out_runtime = artifacts_.scenario_runtime});
        preflight_.reset();
        artifacts_.scenario = std::move(boot.scenario);
        artifacts_.play_sequence_id = boot.play_seq_id;
        artifacts_.start_step = boot.start_step;

        for (const auto& [unit_id, unit_type] :
             artifacts_.scenario_runtime->active_unit_metadata()) {
            artifacts_.unit_debug.push_back({.unit_id = unit_id, .unit_type = unit_type});
        }

        const std::string terrain = artifacts_.background_names.empty()
                                        ? std::string{}
                                        : artifacts_.background_names[artifacts_.background_index];
        const BattleRenderOptions options = BattleViewerRenderer::make_options(
            artifacts_.presentation, screen_.tuning_.state(), screen_.tree_layout(), terrain,
            artifacts_.background_overlay_names, screen_.transparent_bg_,
            screen_.tuning_.enabled());
        const BattleScriptContext context{
            .scene = *artifacts_.scene,
            .assets = {.unit_profiles = &artifacts_.presenter->unit_profiles(),
                       .lifecycle_profiles = &artifacts_.presenter->lifecycle_profiles(),
                       .effects = &artifacts_.presenter->effects(),
                       .marker = artifacts_.presenter->marker_animation(),
                       .marker_small = artifacts_.presenter->marker_small_animation(),
                       .marker_large = artifacts_.presenter->marker_large_animation()}};
        const BattleScenarioWarmupInput scenario_input{.scenario = &artifacts_.scenario,
                                                       .sequence_id = artifacts_.play_sequence_id,
                                                       .start_step_index = artifacts_.start_step,
                                                       .runtime = &*artifacts_.scenario_runtime};
        plan_ = build_battle_startup_texture_warmup_plan(artifacts_.scene->snapshot(), options,
                                                         &context, &scenario_input);
        progress_.total = plan_.entries.size();
        progress_.stage = BattleStartupStage::SubmitAssetPreparation;
        progress_.current_operation = "submit asset preparation";
    }

    void submit_asset_preparation() {
        std::vector<ImageAssetKey> keys;
        keys.reserve(plan_.entries.size());
        std::unordered_set<std::string> containers;
        for (const auto& entry : plan_.entries) {
            keys.push_back(entry.key);
            containers.insert(entry.key.container_path);
        }
        for (const auto& container : containers) {
            screen_.context_.store().prewarm(container);
        }
        preload_.emplace(screen_.context_.render_assets.begin_incremental_preload(
            std::move(keys), AssetPriority::Critical, "BattleStartup"));
        progress_.stage = BattleStartupStage::UploadAssets;
        progress_.current_operation = "upload assets";
    }

    void upload_assets(BattleStartupBudget budget) {
        preload_->advance({.max_textures = budget.max_uploads, .max_ms = budget.max_upload_ms});
        const auto upload_progress = preload_->progress();
        progress_.completed = upload_progress.uploaded + upload_progress.failed;
        if (!preload_->complete()) {
            return;
        }
        if (preload_->failed()) {
            failed_ = true;
            progress_.stage = BattleStartupStage::Failed;
            progress_.current_operation = "asset upload failed";
            throw BattleStartupError("battle startup asset upload failed");
        }
        screen_.context_.render_assets.arm_reclaim_after_next_present(preload_->finish());
        if (screen_.config_.strict_planned_assets) {
            screen_.context_.textures().set_strict_planned_assets(true);
        }
        progress_.stage = BattleStartupStage::FinalizeScenario;
        progress_.current_operation = "finalize scenario";
    }

    BattleScreen&                                screen_;
    std::string                                  preferred_sequence_;
    std::optional<BattleBootstrapPreflight>      preflight_;
    BattleStartupArtifacts                       artifacts_;
    BattleWarmupPlan                             plan_;
    std::optional<IncrementalRenderAssetPreload> preload_;
    BattleStartupProgress                        progress_;
    bool                                         complete_ = false;
    bool                                         failed_ = false;
};

BattleScreen::BattleScreen(const AppConfig& config, const AppRuntimeContext& context,
                           BattleTuningController& tuning, SDL_Renderer* renderer,
                           TreeLayout tree_layout, std::string config_source,
                           TreeLayoutEditor&     tree_layout_editor,
                           std::function<void()> request_close)
    : Screen(std::move(tree_layout), std::move(config_source)),
      request_close_(std::move(request_close)), tree_layout_editor_(tree_layout_editor),
      config_(config), context_(context), tuning_(tuning), renderer_(renderer),
      scene_(std::make_unique<BattleScene>()) {}

BattleScreen::~BattleScreen() = default;

std::vector<std::string> BattleScreen::required_layout_nodes() {
    return {"/background", "/ui/combat_frame", "/ui/left_unit_group", "/ui/right_unit_group"};
}

void BattleScreen::on_enter() {
    if (initialized_) {
        return;
    }
    startup_ = std::make_unique<BattleStartupSession>(*this);
    startup_progress_ = startup_->progress();
    phase_ = BattleScreenPhase::Loading;
    initialized_ = true;
}

void BattleScreen::commit_startup() {
    BattleStartupArtifacts artifacts = startup_->take_result();
    scenario_player_.reset();
    scenario_runtime_.reset();
    presenter_.reset();
    unit_factory_.reset();
    scenario_animation_catalog_.reset();
    scene_ = std::move(artifacts.scene);
    scene_presentation_ = std::move(artifacts.presentation);
    presenter_ = std::move(artifacts.presenter);
    scenario_animation_catalog_ = std::move(artifacts.animation_catalog);
    scenario_runtime_ = std::move(artifacts.scenario_runtime);
    unit_factory_ = std::move(artifacts.unit_factory);
    scenario_play_seq_id_ = artifacts.play_sequence_id;
    unit_debug_ = std::move(artifacts.unit_debug);
    bg_names_ = std::move(artifacts.background_names);
    bg_overlay_names_ = std::move(artifacts.background_overlay_names);
    bg_index_ = artifacts.background_index;

    scenario_player_.emplace();
    scenario_player_->load(std::move(artifacts.scenario), config_.battle_script_path);
    scenario_player_->play(scenario_play_seq_id_, artifacts.start_step);

    auto& textures = context_.textures();

    autoplay_metrics_active_ = true;
    autoplay_start_ticks_ = SDL_GetTicks();
    autoplay_lazy_misses_start_ = textures.stats().lazy_render_misses;
    autoplay_lazy_miss_diagnostics_start_ = textures.lazy_render_misses().size();
    autoplay_frame_count_ = 0;
    autoplay_render_sum_ms_ = 0.0;
    autoplay_render_max_ms_ = 0.0;

    log_first_render_after_preload_ = true;
    kLog->info("battle_scenario_loaded seq={} units={}", scenario_play_seq_id_, unit_debug_.size());
    phase_ = BattleScreenPhase::Ready;
}

void BattleScreen::refresh_unit_debug_metadata() {
    if (!scenario_runtime_) {
        return;
    }
    const auto meta = scenario_runtime_->active_unit_metadata();
    unit_debug_.clear();
    unit_debug_.reserve(meta.size());
    for (const auto& [uid, utype] : meta)
        unit_debug_.push_back({.unit_id = uid, .unit_type = utype});
}

void BattleScreen::restart_current_scenario() {
    if (!scenario_player_) {
        kLog->warn("restart_current_scenario: no active v3 scenario to restart");
        return;
    }
    const std::string sequence = scenario_play_seq_id_.empty()
                                     ? scenario_player_->current_sequence_id()
                                     : scenario_play_seq_id_;
    startup_ = std::make_unique<BattleStartupSession>(*this, sequence);
    startup_progress_ = startup_->progress();
    phase_ = BattleScreenPhase::Loading;
}

bool BattleScreen::handle_input(const InputEvent& event) {
    BattleScreenInputContext context{
        .tuning_enabled = tuning_.enabled(),
        .selectable_items = std::span{tuning_.state().current_items},
    };

    auto action = BattleScreenInputHandler::handle(event, context);
    if (!action.has_value())
        return false;

    return apply_battle_screen_action(*action);
}

bool BattleScreen::apply_battle_screen_action(const BattleScreenAction& action) {
    // Viewer/presenter actions
    if (const auto* viewer = std::get_if<BattleViewerAction>(&action)) {
        using enum BattleViewerAction;
        switch (*viewer) {
        case Quit:
            if (request_close_) {
                request_close_();
            }
            return true;
        case ToggleDebugOverlay:
            draw_debug_slot_anchors_ = !draw_debug_slot_anchors_;
            return true;
        case ToggleLayers:
            scene_presentation_.toggle_layers();
            return true;
        case ToggleTransparent:
            transparent_bg_ = !transparent_bg_;
            return true;
        case CycleBackground: {
            auto& store = context_.store();
            if (!bg_names_.empty()) {
                bg_index_ = (bg_index_ + 1) % bg_names_.size();
                const std::string& bg_name = bg_names_[bg_index_];
                bg_overlay_names_.clear();
                if (const d2res::OptMaps* maps = store.container_maps("Imgs/Battle.ff");
                    maps != nullptr) {
                    bg_overlay_names_ = maps->index_map.overlay_variants(bg_name);
                }
                kLog->info("background name={} overlays={}", bg_name, bg_overlay_names_.size());
                for (const auto& ov : bg_overlay_names_)
                    kLog->info("background overlay variant={}", ov);
            }
            return true;
        }
        case None:
            return false;
        case TriggerAttack:
        case SelectNextUnit:
        case SelectNextTarget:
        case SetRoleIdle:
        case SetRoleHit:
        case SetRoleDeath:
        case SetRoleAttack:
        case SetRoleHeff:
        case SetRoleTuch:
        case StepForward:
        case StepBackward:
            if (presenter_) {
                apply_battle_viewer_action(*viewer, *presenter_);
                return true;
            }
            return false;
        }
    }

    // Screen presentation/debug state
    if (std::get_if<ToggleDebugHud>(&action)) {
        if (tuning_.enabled()) {
            debug_hud_visible_ = !debug_hud_visible_;
            kLog->debug("debug_hud state={}", debug_hud_visible_ ? "visible" : "hidden");
        }
        return true;
    }
    if (std::get_if<ToggleSoloSelected>(&action)) {
        if (tuning_.enabled()) {
            solo_selected_ = !solo_selected_;
            kLog->debug("solo_mode state={}", solo_selected_ ? "on" : "off");
        }
        return true;
    }
    if (std::get_if<ToggleVisualPause>(&action)) {
        visual_paused_ = !visual_paused_;
        kLog->debug("visual_pause state={}", visual_paused_ ? "on" : "off");
        return true;
    }
    if (std::get_if<RestartScenario>(&action)) {
        if (scenario_player_) {
            restart_current_scenario();
        }
        return true;
    }
    if (std::get_if<LogMousePosition>(&action)) {
        kLog->debug("mouse x={:.0f} y={:.0f}", static_cast<double>(mouse_ref_x_),
                    static_cast<double>(mouse_ref_y_));
        return true;
    }

    // Debug scene manipulation
    if (const auto* move = std::get_if<MoveSelectedDebugUnit>(&action)) {
        if (presenter_ && draw_debug_slot_anchors_) {
            const auto sel_id = presenter_->selected_entity_id();
            auto*      unit = scene_->try_unit_by_id(sel_id);
            if (unit != nullptr) {
                static_cast<void>(BattleDebugSceneController::move_unit(*scene_, sel_id,
                                                                        Vec2{move->dx, move->dy}));
            }
        }
        return true;
    }

    // Tuning lifecycle
    if (std::get_if<ToggleTuning>(&action)) {
        tuning_.toggle_enabled();
        return true;
    }
    if (std::get_if<SaveTuning>(&action)) {
        if (tuning_.enabled()) {
            const DebugRenderableItem* item = tuning_.state().selected_item();
            if (item != nullptr && item->binding.has_value()) {
                const auto binding = to_config_binding(*item->binding);
                if (binding.has_value() && binding->owner_kind == BindingOwnerKind::TreeLayout) {
                    tree_layout_editor_.select_screen(instance_id());
                    tree_layout_editor_.select_node(binding->tree_path);
                    tree_layout_editor_.save();
                } else {
                    tuning_.save();
                }
            } else {
                tuning_.save();
            }
        }
        return true;
    }
    if (std::get_if<RevertAllTuning>(&action)) {
        if (tuning_.enabled()) {
            tree_layout_editor_.select_screen(instance_id());
            tree_layout_editor_.revert_all();
            tuning_.revert_all();
        }
        return true;
    }
    if (std::get_if<RevertSelectedTuning>(&action)) {
        if (tuning_.enabled()) {
            const DebugRenderableItem* item = tuning_.state().selected_item();
            if (item != nullptr && item->binding.has_value()) {
                const auto binding = to_config_binding(*item->binding);
                if (binding.has_value() && binding->owner_kind == BindingOwnerKind::TreeLayout) {
                    tree_layout_editor_.select_screen(instance_id());
                    tree_layout_editor_.select_node(binding->tree_path);
                    tree_layout_editor_.revert_selected();
                } else {
                    tuning_.revert_selected();
                }
            }
        }
        return true;
    }
    if (std::get_if<LogSelectedTuning>(&action)) {
        if (tuning_.enabled()) {
            // log_selected_visual_config equivalent
            const DebugRenderableItem* item = tuning_.state().selected_item();
            if (item != nullptr && item->binding.has_value()) {
                const auto binding = to_config_binding(*item->binding);
                if (binding.has_value()) {
                    if (binding->owner_kind == BindingOwnerKind::TreeLayout) {
                        const auto tree_node = tree_layout().node(binding->tree_path);
                        if (tree_node.has_value()) {
                            kLog->debug("debug_tuning_patch TreeLayout={} x={:.1f} y={:.1f} "
                                        "w={:.1f} h={:.1f} level={} kind={}",
                                        binding->tree_path, static_cast<double>(tree_node->x),
                                        static_cast<double>(tree_node->y),
                                        static_cast<double>(tree_node->w),
                                        static_cast<double>(tree_node->h), tree_node->level,
                                        tree_node->kind);
                        }
                    } else {
                        const VisualPlacementValue value = tuning_.state().placement(*binding);
                        kLog->debug("debug_tuning_patch key={} x={:.2f} y={:.2f} "
                                    "scale_x={:.3f} scale_y={:.3f} level={}",
                                    binding->key(), static_cast<double>(value.x),
                                    static_cast<double>(value.y),
                                    static_cast<double>(value.scale_x),
                                    static_cast<double>(value.scale_y), value.level);
                    }
                }
            }
        }
        return true;
    }

    // Tuning edit action
    if (const auto* edit = std::get_if<ApplyTuningEdit>(&action)) {
        const DebugRenderableItem* item = tuning_.state().selected_item();
        if (item != nullptr && item->binding.has_value()) {
            const auto binding = to_config_binding(*item->binding);
            if (binding.has_value() && binding->owner_kind == BindingOwnerKind::TreeLayout) {
                tree_layout_editor_.select_screen(instance_id());
                tree_layout_editor_.select_node(binding->tree_path);
                return tree_layout_editor_.apply_edit(edit->edit);
            }
        }
        return tuning_.apply_edit(edit->edit);
    }

    // Debug pointer position update
    if (const auto* ptr = std::get_if<UpdateDebugPointerPosition>(&action)) {
        const LayoutScale scale = layout_scale_for(tuning_.state().layout_metrics,
                                                   config_.logical_width, config_.logical_height);
        mouse_ref_x_ = static_cast<float>(ptr->logical_x) / scale.sx;
        mouse_ref_y_ = static_cast<float>(ptr->logical_y) / scale.sy;
        return true;
    }

    if (const auto* sel = std::get_if<SelectDebugItem>(&action)) {
        if (tuning_.enabled()) {
            tuning_.state().selected_debug_id = sel->item_id;
            const DebugRenderableItem* item = tuning_.state().selected_item();
            if (item != nullptr && item->binding.has_value()) {
                const auto binding = to_config_binding(*item->binding);
                if (binding.has_value() && binding->owner_kind == BindingOwnerKind::TreeLayout) {
                    tree_layout_editor_.select_screen(instance_id());
                    tree_layout_editor_.select_node(binding->tree_path);
                }
            }
            kLog->debug("debug_tuning selected id={} path={}", sel->item_id, sel->tree_path);
            return true;
        }
        return false;
    }

    // Consumed tuning input (no game action needed)
    if (std::get_if<ConsumeTuningInput>(&action)) {
        return true;
    }

    return false;
}

void BattleScreen::update(const d2::app::ScreenUpdateContext& context) {
    if (phase_ == BattleScreenPhase::Loading) {
        try {
            startup_->advance({});
            startup_progress_ = startup_->progress();
            if (startup_->complete()) {
                commit_startup();
                startup_.reset();
            }
        } catch (const BattleStartupError& error) {
            phase_ = BattleScreenPhase::Failed;
            startup_failure_ = error.what();
            kLog->error("battle_startup_failed error={}", startup_failure_);
        }
        return;
    }
    if (phase_ == BattleScreenPhase::Failed) {
        return;
    }
    const float effective_animation_delta_ms = visual_paused_ ? 0.0F : context.animation_delta_ms;

    if (presenter_) {
        std::map<std::string, int> delays;
        for (const auto& [key, val] : tuning_.state().placements) {
            if (val.frame_delay != 0) {
                const auto& br = tuning_.state().binding_registry;
                if (const auto bit = br.find(key); bit != br.end())
                    delays[bit->second.tree_path] = val.frame_delay;
            }
        }
        presenter_->update_sequence_delays(std::move(delays));
        presenter_->update(effective_animation_delta_ms);
        if (effective_animation_delta_ms > 0.0F && scenario_player_ && scenario_runtime_) {
            scenario_player_->update(*presenter_, *scenario_runtime_);
            refresh_unit_debug_metadata();
        }
    }
    scene_->update(effective_animation_delta_ms);
}

void BattleScreen::render(Renderer2D& r) {
    if (phase_ != BattleScreenPhase::Ready) {
        r.clear(phase_ == BattleScreenPhase::Failed
                    ? Color{.r = 0x40, .g = 0x08, .b = 0x08, .a = 0xff}
                    : Color{.r = 0x14, .g = 0x14, .b = 0x18, .a = 0xff});
        if (phase_ == BattleScreenPhase::Loading) {
            constexpr float kBarWidth = 480.0F;
            constexpr float kBarHeight = 20.0F;
            constexpr float kBarX = 468.0F;
            constexpr float kBarY = 460.0F;
            const float     total =
                static_cast<float>(std::max<std::size_t>(startup_progress_.total, 1));
            const float fraction =
                std::clamp(static_cast<float>(startup_progress_.completed) / total, 0.0F, 1.0F);
            r.draw_rect({.x = kBarX, .y = kBarY, .w = kBarWidth, .h = kBarHeight},
                        Color{.r = 0x35, .g = 0x35, .b = 0x40, .a = 0xff});
            r.draw_rect({.x = kBarX, .y = kBarY, .w = kBarWidth * fraction, .h = kBarHeight},
                        Color{.r = 0x78, .g = 0xa8, .b = 0xe0, .a = 0xff});
            r.draw_debug_text(468.0F, 430.0F, startup_progress_.current_operation.c_str());
        } else {
            r.draw_debug_text(468.0F, 430.0F, "Battle startup failed");
            r.draw_debug_text(468.0F, 450.0F, startup_failure_.c_str());
        }
        return;
    }
    auto& textures = context_.textures();

    // Clear and set blend mode
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
    if (transparent_bg_) {
        r.clear(Color{.r = 0, .g = 0, .b = 0, .a = 0});
    } else {
        r.clear(Color{.r = 0x1a, .g = 0x1a, .b = 0x1a, .a = 0xff});
    }
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);

    const LayoutScale scale = layout_scale_for(tuning_.state().layout_metrics,
                                               config_.logical_width, config_.logical_height);
    const bool        show_debug_overlay = tuning_.enabled() && debug_hud_visible_;

    std::vector<DebugRenderableItem> tunable_items;

    {
        GameTextureCache::LazyRenderContext render_context{
            .frame_number = autoplay_metrics_active_ ? autoplay_frame_count_ + 1 : 0};
        if (scenario_player_ && presenter_) {
            render_context.script_step = scenario_player_->current_step_id();
            const auto& exec = presenter_->visual_step_execution();
            if (!exec.started_envelope_ids.empty()) {
                render_context.script_envelope = exec.started_envelope_ids.front();
            } else if (!exec.waiting_envelope_ids.empty()) {
                render_context.script_envelope = exec.waiting_envelope_ids.front();
            }
        }
        textures.set_lazy_render_context(std::move(render_context));
        SdlBattleTextureProvider texture_provider{textures};
        const std::string_view   solo_id =
            solo_selected_ ? std::string_view{tuning_.state().selected_debug_id} : "";
        scene_presentation_.info_left_unit =
            presenter_ ? presenter_->selected_unit_id() : UnitInstanceId{};
        scene_presentation_.info_right_unit =
            presenter_ ? presenter_->selected_target_id() : UnitInstanceId{};
        BattleViewerRenderer::render(
            *scene_, scale, texture_provider, r, scene_presentation_, tuning_.state(),
            tree_layout(), bg_names_.empty() ? std::string{} : bg_names_[bg_index_],
            bg_overlay_names_, transparent_bg_, draw_debug_slot_anchors_, tuning_.enabled(),
            solo_id, &context_.portraits, &textures, &tunable_items);
        textures.clear_lazy_render_context();
    }

    tuning_.state().set_current_items(std::move(tunable_items));

    // Debug overlay rendering
    int win_w = 0;
    int win_h = 0;
    SDL_GetWindowSize(SDL_GetRenderWindow(renderer_), &win_w, &win_h);
    if (presenter_ && show_debug_overlay) {
        DebugOverlayRenderer::draw(DebugOverlayFrame{.renderer = r,
                                                     .scene = *scene_,
                                                     .presenter = *presenter_,
                                                     .unit_debug = unit_debug_,
                                                     .textures = &textures,
                                                     .script_path = config_.battle_script_path,
                                                     .tuning = tuning_.state(),
                                                     .scale = scale,
                                                     .mouse_ref_x = mouse_ref_x_,
                                                     .mouse_ref_y = mouse_ref_y_,
                                                     .window_width = win_w,
                                                     .window_height = win_h});
    }

    // Autoplay metrics
    const auto render_started = std::chrono::steady_clock::now();
    if (log_first_render_after_preload_) {
        log_first_render_after_preload_ = false;
        const double elapsed_ms = std::chrono::duration<double, std::milli>(
                                      std::chrono::steady_clock::now() - render_started)
                                      .count();
        kLog->info("startup_timing stage=first_render_after_preload duration_ms={:.2f}",
                   elapsed_ms);
    }
    if (autoplay_metrics_active_) {
        const double        elapsed_ms = std::chrono::duration<double, std::milli>(
                                             std::chrono::steady_clock::now() - render_started)
                                             .count();
        const std::uint64_t now = SDL_GetTicks();
        const std::uint64_t lazy_misses =
            textures.stats().lazy_render_misses - autoplay_lazy_misses_start_;
        if (autoplay_frame_count_ < 60) {
            kLog->debug("autoplay_frame frame={} render_ms={:.2f} lazy_misses={}",
                        autoplay_frame_count_ + 1, elapsed_ms,
                        static_cast<unsigned long long>(lazy_misses));
            autoplay_render_sum_ms_ += elapsed_ms;
            autoplay_render_max_ms_ = std::max(autoplay_render_max_ms_, elapsed_ms);
        }
        ++autoplay_frame_count_;
        if (autoplay_frame_count_ == 60) {
            kLog->info("autoplay_summary avg_ms={:.2f} max_ms={:.2f}",
                       autoplay_render_sum_ms_ / 60.0, autoplay_render_max_ms_);
        }
        if (now - autoplay_start_ticks_ >= 5000) {
            kLog->info("autoplay_5s_lazy_misses count={}",
                       static_cast<unsigned long long>(lazy_misses));
            {
                const std::string summary = format_lazy_miss_summary(
                    textures.lazy_render_misses(), autoplay_lazy_miss_diagnostics_start_);
                if (!summary.empty()) {
                    kLog->info("autoplay_5s_lazy_miss_keys keys={}", summary);
                }
                kLog->info("plan_vs_render planned={} uploaded={} render_requested={}",
                           textures.planned_key_count(), textures.all_keys().size(),
                           textures.render_requested_key_count());
                if (config_.strict_planned_assets) {
                    const auto& strict_misses = textures.strict_miss_keys();
                    kLog->info("strict_planned_assets misses={}", strict_misses.size());
                    for (const auto& miss_key : strict_misses) {
                        kLog->info("strict_miss key={}", miss_key);
                    }
                    if (strict_misses.empty()) {
                        kLog->info("strict_planned_assets PASS");
                    }
                }
            }
            autoplay_metrics_active_ = false;
        }
    }
}

} // namespace d2engine
