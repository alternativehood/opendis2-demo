#include "application.hpp"
#include "battle_screen.hpp"
#include "screen_config_store.hpp"

#include "../audio/audio_runtime_factory.hpp"
#include "../assets/game_data_registry.hpp"
#include "../assets/portrait_manifest.hpp"
#include "../assets/asset_runtime.hpp"
#include "../battle_view/battle_render_tree_contract.hpp"
#include "../input/sdl_input_backend.hpp"
#include "../platform/sdl_context.hpp"
#include "../render/render_asset_runtime.hpp"
#include "../render/renderer2d.hpp"
#include "../render/sdl_frame_presenter.hpp"

#include "debug_command_handler.hpp"
#include "debug_ui_renderer.hpp"
#include "tree_layout_debug_overlay.hpp"
#include "upscale_controller.hpp"

#include "d2res/dbf_reader.hpp"

#include <d2log/log.hpp>

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <deque>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace d2engine {

struct StdinState {
    std::atomic<bool>       active{false};
    std::mutex              mtx;
    std::deque<std::string> queue;
};

namespace {

auto kLog = d2log::get("d2.app"); // NOLINT(cert-err58-cpp)

class StartupTimer {
public:
    explicit StartupTimer(const char* name)
        : name_(name), started_(std::chrono::steady_clock::now()) {}

    StartupTimer(const StartupTimer&) = delete;
    StartupTimer& operator=(const StartupTimer&) = delete;
    StartupTimer(StartupTimer&&) = delete;
    StartupTimer& operator=(StartupTimer&&) = delete;

    ~StartupTimer() noexcept {
        const double elapsed_ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started_)
                .count();
        d2log::get("d2.app")->info("startup_timing stage={} duration_ms={:.2f}", name_, elapsed_ms);
    }

private:
    const char*                           name_;
    std::chrono::steady_clock::time_point started_;
};

class ApplicationFrameScope {
public:
    explicit ApplicationFrameScope(SdlFramePresenter& presenter) : presenter_(presenter) {
        presenter_.begin_scene_frame();
    }

    ~ApplicationFrameScope() {
        if (!presented_) {
            presenter_.abort_frame();
        }
    }

    ApplicationFrameScope(const ApplicationFrameScope&) = delete;
    ApplicationFrameScope& operator=(const ApplicationFrameScope&) = delete;

    void composite_scene() {
        require_stage(Stage::BeginScene);
        presenter_.composite_scene_to_window();
        stage_ = Stage::CompositeScene;
    }

    void begin_native_overlay() {
        require_stage(Stage::CompositeScene);
        presenter_.begin_native_overlay();
        stage_ = Stage::BeginNativeOverlay;
    }

    void end_native_overlay() {
        require_stage(Stage::BeginNativeOverlay);
        presenter_.end_native_overlay();
        stage_ = Stage::EndNativeOverlay;
    }

    void present() {
        require_stage(Stage::EndNativeOverlay);
        if (presented_) {
            throw std::runtime_error("ApplicationFrameScope present called twice");
        }
        presenter_.present();
        presented_ = true;
        stage_ = Stage::Present;
    }

private:
    enum class Stage { BeginScene, CompositeScene, BeginNativeOverlay, EndNativeOverlay, Present };

    void require_stage(Stage expected) const {
        if (stage_ != expected) {
            throw std::runtime_error("ApplicationFrameScope lifecycle order violation");
        }
    }

    SdlFramePresenter& presenter_;
    Stage              stage_ = Stage::BeginScene;
    bool               presented_ = false;
};

// TRANSITIONAL DEBT: read_binary_file and load_gunits_rows are legacy helpers
// used only by Application constructor for portrait manifest building.
// These belong in a future data-loading layer (d2runtime / d2game), not in
// the application composition root. They are kept here to avoid refactoring
// the full data-loading flow in this milestone.
std::vector<uint8_t> read_binary_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

std::vector<std::map<std::string, std::string>>
load_gunits_rows(const std::filesystem::path& globals_dir) {
    const auto path = globals_dir / "Gunits.dbf";
    try {
        auto data = read_binary_file(path);
        if (data.empty()) {
            throw std::runtime_error("cannot load required Gunits.dbf: " + path.string());
        }
        d2res::DbfReader const reader(std::span<const uint8_t>(data.data(), data.size()));
        return reader.read_records();
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("required portrait manifest source failed: ") +
                                 e.what());
    }
}

} // namespace

Application::Application(AppConfig config)
    : config_(std::move(config)),
      config_root_(resolve_application_config_root(config_.config_root_override)),
      screen_config_store_(std::make_unique<ScreenConfigStore>(config_root_)),
      tuning_(*screen_config_store_) {
    kLog->info("config_root={} mode={}", config_root_.string(),
               config_.config_root_override.empty() ? "runtime" : "source-override");
    tree_layout_editor_ =
        std::make_unique<TreeLayoutEditor>(screen_manager_, *screen_config_store_);

    if (config_.game_root.empty()) {
        throw std::runtime_error("Game root is required for graphical application runtime; "
                                 "set DISCIPLES2_GAME_ROOT or pass a non-empty --game-root");
    }
    debug_sound_catalog_ = std::make_unique<DebugSoundCatalog>(config_.game_root);

    try {
        StartupTimer const timer{"asset runtime construction"};
        asset_runtime_ = std::make_unique<AssetRuntime>(config_.game_root);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Game loader failed: ") + e.what());
    }

    int const window_width =
        static_cast<int>(static_cast<float>(config_.logical_width) * config_.scale);
    int const window_height =
        static_cast<int>(static_cast<float>(config_.logical_height) * config_.scale);

    {
        StartupTimer const timer{"SDL init"};
        sdl_ = std::make_unique<SdlContext>("OpenDis2", window_width, window_height,
                                            config_.fullscreen);
    }
    audio_runtime_ = d2::audio::create_audio_runtime();
    frame_presenter_ = std::make_unique<SdlFramePresenter>(
        sdl_->renderer(),
        UpscaleExtent{.width = config_.logical_width, .height = config_.logical_height});
    upscale_controller_ = std::make_unique<UpscaleController>();
    frame_limiter_enabled_ = !SDL_SetRenderVSync(sdl_->renderer(), 1);
    kLog->info("frame_pacing mode={}", frame_limiter_enabled_ ? "60 FPS limiter" : "SDL vsync");

    {
        SDL_Surface* icon = IMG_Load(D2ENGINE_ICON_FILE);
        if (icon != nullptr) {
            SDL_SetWindowIcon(sdl_->window(), icon);
            SDL_DestroySurface(icon);
        } else {
            kLog->warn("window_icon_not_loaded path={} error={}", D2ENGINE_ICON_FILE,
                       SDL_GetError());
        }
    }

    if (!SDL_SetRenderLogicalPresentation(sdl_->renderer(), config_.logical_width,
                                          config_.logical_height,
                                          SDL_LOGICAL_PRESENTATION_STRETCH)) {
        throw std::runtime_error(std::string("SDL_SetRenderLogicalPresentation failed: ") +
                                 SDL_GetError());
    }

    try {
        StartupTimer const timer{"render asset runtime construction"};
        render_asset_runtime_ =
            std::make_unique<RenderAssetRuntime>(sdl_->renderer(), *asset_runtime_);
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Texture loading failed: ") + e.what());
    }

    {
        StartupTimer const timer{"font init"};
        text_box_renderer_ = std::make_unique<TextBoxRenderer>(sdl_->renderer());
    }
    {
        debug_ui_ = std::make_unique<DebugUiRenderer>(sdl_->window(), sdl_->renderer());
    }
}

void Application::start_with_screen(std::unique_ptr<Screen> screen) {
    if (!running_) {
        screen_manager_.switch_to(std::move(screen));
        return;
    }
    screen_manager_.request_switch_to(std::move(screen));
}

void Application::start_battle_screen(std::function<void()> on_close) {
    auto screen = make_battle_screen(std::move(on_close));
    if (!running_) {
        screen_manager_.switch_to(std::move(screen));
        return;
    }
    screen_manager_.request_push_overlay(std::move(screen));
}
std::unique_ptr<Screen> Application::make_battle_screen(std::function<void()> on_close) {
    ensure_shared_runtime_initialized();
    auto config = screen_config_store_->load_validated("battle_screen",
                                                       BattleScreen::required_layout_nodes());
    validate_required_battle_nodes(config.tree_layout);
    ensure_battle_tuning_initialized(config);
    return std::make_unique<BattleScreen>(
        config_, runtime_context(), tuning_, sdl_->renderer(), std::move(config.tree_layout),
        config.config_path.string(), *tree_layout_editor_, std::move(on_close));
}

void Application::ensure_shared_runtime_initialized() {
    if (context_ != nullptr) {
        return;
    }

    if (game_data_ == nullptr) {
        StartupTimer const timer{"game data load"};
        game_data_ = std::make_unique<GameDataRegistry>(std::filesystem::path(config_.game_root) /
                                                        "Globals");
        if (game_data_->all_units().empty()) {
            throw std::runtime_error("required game data failed: no units loaded from Globals");
        }
    }

    if (portrait_index_ == nullptr) {
        StartupTimer const          timer{"portrait manifest build"};
        const std::filesystem::path globals_dir =
            std::filesystem::path(config_.game_root) / "Globals";
        const auto       gunits_rows = load_gunits_rows(globals_dir);
        PortraitManifest manifest;
        manifest = build_portrait_manifest(asset_runtime_->store(), gunits_rows);
        if (!manifest.warnings.empty()) {
            for (const auto& w : manifest.warnings) {
                kLog->warn("portrait_manifest warning={}", w);
            }
        }
        portrait_index_ = std::make_unique<PortraitManifestIndex>(manifest);
    }

    if (context_ == nullptr) {
        context_ = std::make_unique<AppRuntimeContext>(
            AppRuntimeContext{.assets = *asset_runtime_,
                              .render_assets = *render_asset_runtime_,
                              .game_data = *game_data_,
                              .portraits = *portrait_index_});
    }
}

void Application::ensure_battle_tuning_initialized(const ScreenConfig& config) {
    if (battle_tuning_initialized_) {
        return;
    }
    load_battle_tuning_config(tuning_.state(), config.document, config.config_path);
    tuning_.set_config_path(config.config_path);
    battle_tuning_initialized_ = true;
}

FfAssetStore& Application::store() {
    return asset_runtime().store();
}

const FfAssetStore& Application::store() const {
    return asset_runtime().store();
}

d2::audio::AudioService& Application::audio_service() noexcept {
    return *audio_runtime_;
}

const d2::audio::AudioService& Application::audio_service() const noexcept {
    return *audio_runtime_;
}

d2::audio::DebugAudioPreview& Application::debug_audio_preview() noexcept {
    return *audio_runtime_;
}

const d2::audio::DebugAudioPreview& Application::debug_audio_preview() const noexcept {
    return *audio_runtime_;
}

Application::~Application() {
    if (stdin_state_) {
        stdin_state_->active = false;
    }
    if (stdin_thread_.joinable()) {
        stdin_thread_.detach();
    }
}

void Application::drain_stdin_commands() {
    if (!config_.debug_mode || !stdin_state_) {
        return;
    }
    std::deque<std::string> batch;
    {
        std::lock_guard lock(stdin_state_->mtx);
        batch.swap(stdin_state_->queue);
    }
    for (const auto& line : batch) {
        DebugCommandHandler handler{tuning_.state()};
        const auto          result = handler.execute(line);
        if (!result.output.empty()) {
            std::fputs(result.output.c_str(), stdout);
            std::fflush(stdout);
        }
        if (result.request_quit) {
            should_exit_ = true;
        }
        if (result.request_save) {
            save_visual_config();
        }
        if (result.request_reset) {
            revert_all_visual_config();
        }
    }
}

int Application::run() {
    if (config_.debug_mode) {
        tuning_.set_enabled(true);
        kLog->info("debug_mode_active hint=type_help_for_commands");
        stdin_state_ = std::make_shared<StdinState>();
        stdin_state_->active = true;
        stdin_thread_ = std::thread([state = stdin_state_] {
            std::string line;
            while (std::getline(std::cin, line)) {
                if (!state->active) {
                    break;
                }
                std::lock_guard lock(state->mtx);
                state->queue.push_back(std::move(line));
            }
        });
    }

    running_ = true;
    should_exit_ = false;

    uint64_t       last_ticks = SDL_GetTicks();
    const uint64_t max_delta_ms = 100;

    while (running_ && !should_exit_ && !screen_manager_.quit_requested()) {
        drain_stdin_commands();

        uint64_t const current_ticks = SDL_GetTicks();
        uint64_t       delta_ms = current_ticks - last_ticks;
        last_ticks = current_ticks;
        delta_ms = std::min(delta_ms, max_delta_ms);

        process_events();
        update(static_cast<float>(delta_ms));
        render();
        if (frame_limiter_enabled_) {
            constexpr uint64_t target_frame_ms = 16;
            const uint64_t     frame_ms = SDL_GetTicks() - current_ticks;
            if (frame_ms < target_frame_ms) {
                SDL_Delay(static_cast<Uint32>(target_frame_ms - frame_ms));
            }
        }
    }

    return 0;
}

void Application::process_events() {
    SDL_Event event;
    while (d2engine::SdlContext::poll_event(event)) {
        if (debug_ui_) {
            debug_ui_->process_event(&event);
        }

        if (event.type == SDL_EVENT_QUIT) {
            should_exit_ = true;
            continue;
        }

        if (auto input_event = SdlInputBackend::translate(event, sdl_->renderer())) {
            if (const auto* key = std::get_if<KeyPressed>(&*input_event)) {
                if (key->key == Key::D && has_modifier(key->modifiers, KeyModifier::Ctrl) &&
                    has_modifier(key->modifiers, KeyModifier::Shift)) {
                    toggle_debug_ui();
                    continue;
                }
            }

            // Update global pointer position before dispatching
            if (const auto* ptr_moved = std::get_if<PointerMoved>(&*input_event)) {
                pointer_tracker_.on_move(ptr_moved->x, ptr_moved->y);
            }
            if (const auto* ptr_pressed = std::get_if<PointerPressed>(&*input_event)) {
                pointer_tracker_.on_move(ptr_pressed->x, ptr_pressed->y);
            }

            if (debug_ui_enabled_ && debug_ui_ &&
                !should_forward_to_screen(*input_event, true, debug_ui_->wants_mouse_input(),
                                          debug_ui_->wants_keyboard_input())) {
                continue;
            }

            screen_manager_.handle_input(*input_event);
            screen_manager_.apply_pending_transition();
            refresh_revealed_screen_pointer();

            // Sync cursor kind after any transition or input
            cursor_controller_.set_kind(screen_manager_.cursor_kind());
        }
    }
}

void Application::update(float delta_ms) {
    audio_runtime_->update(delta_ms);
    if (render_asset_runtime_) {
        static_cast<void>(render_asset_runtime_->pump_uploads());
        render_asset_runtime_->pump_reclaim_diagnostics();
    }
    const d2::app::ScreenUpdateContext update_context{
        .real_delta_ms = delta_ms,
        .animation_delta_ms = animation_time_controller_.scale_delta_ms(delta_ms),
    };
    screen_manager_.update(update_context);
    screen_manager_.apply_pending_transition();
    refresh_revealed_screen_pointer();
    cursor_controller_.set_kind(screen_manager_.cursor_kind());
}

void Application::refresh_revealed_screen_pointer() {
    if (auto id = screen_manager_.consume_revealed_screen(); id && pointer_tracker_.valid) {
        if (Screen* revealed = screen_manager_.find_live_screen(*id)) {
            revealed->handle_input(PointerMoved{pointer_tracker_.x, pointer_tracker_.y});
        }
    }
}

void Application::render() {
    frame_presenter_->set_settings(upscale_controller_->settings());
    ApplicationFrameScope frame{*frame_presenter_};

    Renderer2D r{sdl_->renderer()};
    r.set_text_box_renderer(text_box_renderer_.get());

    screen_manager_.render(r);

    if (debug_ui_enabled_) {
        render_tree_layout_selection_outline(r, *tree_layout_editor_);
    }

    frame.composite_scene();
    frame.begin_native_overlay();

    if (debug_ui_ && debug_ui_enabled_) {
        debug_ui_->render(screen_manager_, *tree_layout_editor_, tuning_, *upscale_controller_,
                          frame_presenter_->presentation(), animation_time_controller_,
                          audio_service(), debug_audio_preview(), *debug_sound_catalog_);
    }
    frame.end_native_overlay();
    frame.present();

    render_asset_runtime_->on_frame_presented();
}

void Application::save_visual_config() {
    tuning_.save();
    kLog->info("debug_tuning_saved path={}", tuning_.state().config_file);
}

void Application::toggle_debug_ui() {
    debug_ui_enabled_ = !debug_ui_enabled_;
    kLog->debug("debug_ui state={}", debug_ui_enabled_ ? "on" : "off");
}

void Application::revert_all_visual_config() {
    std::vector<ConfigBinding> bindings;
    for (const auto& [_, entry] : tuning_.state().dirty) {
        bindings.push_back(entry.binding);
    }
    for (const auto& binding : bindings) {
        static_cast<void>(tuning_.state().revert(binding));
    }
    tree_layout_editor_->revert_all();
    kLog->info("debug_tuning_reverted_all");
}

} // namespace d2engine
