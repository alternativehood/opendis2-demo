#pragma once

#include "app_config.hpp"
#include "app_runtime_context.hpp"
#include "animation_time_controller.hpp"
#include "battle_tuning_controller.hpp"
#include "screen.hpp"
#include "screen_config_store.hpp"
#include "screen_manager.hpp"
#include "tree_layout_editor.hpp"

#include "../audio/audio_service.hpp"
#include "../audio/debug_audio_preview.hpp"
#include "../audio/audio_runtime.hpp"
#include "../assets/debug_sound_catalog.hpp"
#include "../assets/portrait_manifest_index.hpp"
#include "../input/input_event.hpp"
#include "../platform/sdl_context.hpp"
#include "../render/text/text_box_renderer.hpp"

#include "cursor_controller.hpp"
#include "pointer_tracker.hpp"

#include <SDL3/SDL.h>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>

namespace d2engine {

class DebugUiRenderer;
class AssetRuntime;
class GameDataRegistry;
class FfAssetStore;
class RenderAssetRuntime;
class SdlFramePresenter;
class UpscaleController;

struct StdinState;

class Application {
public:
    explicit Application(AppConfig config);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    int run();

    // Adventure mode support: override the initial screen after construction.
    // Skip this if using the battle-viewer path (creates BattleScreen automatically).
    void start_with_screen(std::unique_ptr<Screen> screen);
    void start_battle_screen(std::function<void()> on_close = nullptr);

    [[nodiscard]] AssetRuntime& asset_runtime() {
        assert(asset_runtime_ != nullptr);
        return *asset_runtime_;
    }
    [[nodiscard]] const AssetRuntime& asset_runtime() const {
        assert(asset_runtime_ != nullptr);
        return *asset_runtime_;
    }
    [[nodiscard]] RenderAssetRuntime& render_assets() {
        assert(render_asset_runtime_ != nullptr);
        return *render_asset_runtime_;
    }
    [[nodiscard]] const RenderAssetRuntime& render_assets() const {
        assert(render_asset_runtime_ != nullptr);
        return *render_asset_runtime_;
    }
    [[nodiscard]] AppRuntimeContext& runtime_context() {
        if (context_ == nullptr) {
            throw std::logic_error(
                "Application runtime context requested before shared runtime initialization");
        }
        return *context_;
    }
    [[nodiscard]] const AppRuntimeContext& runtime_context() const {
        if (context_ == nullptr) {
            throw std::logic_error(
                "Application runtime context requested before shared runtime initialization");
        }
        return *context_;
    }
    [[nodiscard]] bool shared_runtime_initialized() const noexcept { return context_ != nullptr; }
    void               ensure_shared_runtime_initialized();

    // Transitional direct access for low-level preparation paths.
    [[nodiscard]] FfAssetStore&       store();
    [[nodiscard]] const FfAssetStore& store() const;

    // SDL renderer (used by adventure mode terrain texture upload).
    [[nodiscard]] SDL_Renderer* renderer() { return sdl_->renderer(); }

    // Switch the root screen (for AdventureStartupScreen → AdventureScreen transition).
    void switch_screen(std::unique_ptr<Screen> screen) {
        screen_manager_.request_switch_to(std::move(screen));
    }

    // External quit trigger (for adventure launcher or headless hooks).
    void request_quit() { screen_manager_.request_quit(); }

    // Modal overlay support.
    void push_overlay_screen(std::unique_ptr<Screen> overlay) {
        screen_manager_.request_push_overlay(std::move(overlay));
    }
    void pop_overlay_screen() { screen_manager_.request_pop_overlay(); }

    void setup_cursors(SDL_Cursor* default_cursor, SDL_Cursor* select_unit) {
        cursor_controller_.set_cursors(default_cursor, select_unit);
    }
    [[nodiscard]] bool activate_cursor() { return cursor_controller_.activate(); }

    [[nodiscard]] ScreenConfigStore&       screen_config_store() { return *screen_config_store_; }
    [[nodiscard]] d2::audio::AudioService& audio_service() noexcept;
    [[nodiscard]] const d2::audio::AudioService&      audio_service() const noexcept;
    [[nodiscard]] d2::audio::DebugAudioPreview&       debug_audio_preview() noexcept;
    [[nodiscard]] const d2::audio::DebugAudioPreview& debug_audio_preview() const noexcept;

private:
    [[nodiscard]] std::unique_ptr<Screen>
         make_battle_screen(std::function<void()> request_adventure = nullptr);
    void process_events();
    void update(float delta_ms);
    void render();
    void refresh_revealed_screen_pointer();
    void drain_stdin_commands();

    void save_visual_config();
    void revert_all_visual_config();
    void toggle_debug_ui();
    void ensure_battle_tuning_initialized(const ScreenConfig& config);

    AppConfig                              config_;
    std::unique_ptr<SdlContext>            sdl_;
    std::unique_ptr<SdlFramePresenter>     frame_presenter_;
    std::unique_ptr<UpscaleController>     upscale_controller_;
    std::unique_ptr<AssetRuntime>          asset_runtime_;
    std::unique_ptr<RenderAssetRuntime>    render_asset_runtime_;
    std::unique_ptr<GameDataRegistry>      game_data_;
    std::unique_ptr<PortraitManifestIndex> portrait_index_;
    std::unique_ptr<TextBoxRenderer>       text_box_renderer_;
    std::unique_ptr<DebugUiRenderer>       debug_ui_;
    std::filesystem::path                  config_root_;
    std::unique_ptr<AppRuntimeContext>     context_;

    std::unique_ptr<d2::audio::AudioRuntime> audio_runtime_;
    std::unique_ptr<DebugSoundCatalog>       debug_sound_catalog_;

    ScreenManager                      screen_manager_;
    CursorController                   cursor_controller_;
    PointerTracker                     pointer_tracker_;
    std::unique_ptr<ScreenConfigStore> screen_config_store_;
    BattleTuningController             tuning_;
    d2::app::AnimationTimeController   animation_time_controller_;
    std::unique_ptr<TreeLayoutEditor>  tree_layout_editor_;

    bool running_ = false;
    bool should_exit_ = false;
    bool frame_limiter_enabled_ = false;
    bool debug_ui_enabled_ = false;
    bool battle_tuning_initialized_ = false;

    std::shared_ptr<StdinState> stdin_state_;
    std::thread                 stdin_thread_;
};

} // namespace d2engine
