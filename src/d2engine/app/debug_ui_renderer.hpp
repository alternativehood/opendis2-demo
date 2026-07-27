#pragma once

// Dev UI policy:
// Dear ImGui is the preferred UI layer for interactive tuning panels.
// Keep tuning state and edit/apply/save logic in controllers/model classes.
// Do not add new hand-written SDL widget systems or ad-hoc overlay panels;
// expose new controls through ImGui on top of existing tuning APIs.

#include "tree_layout_editor.hpp"
#include "debug_tuning_types.hpp"
#include "animation_time_controller.hpp"
#include "../audio/audio_service.hpp"
#include "../audio/debug_audio_preview.hpp"
#include "../assets/debug_sound_catalog.hpp"
#include "../render/upscale_settings.hpp"

#include <SDL3/SDL.h>

#include <cstdint>
#include <array>
#include <string>
#include <vector>

struct ImGuiContext;

namespace d2engine {

class BattleTuningController;
struct BattleTuningState;
class ScreenManager;
class AdventureScreen;
struct LiveScreenRef;
class UpscaleController;

class DebugUiRenderer {
public:
    DebugUiRenderer(SDL_Window* window, SDL_Renderer* renderer);
    ~DebugUiRenderer();

    DebugUiRenderer(const DebugUiRenderer&) = delete;
    DebugUiRenderer& operator=(const DebugUiRenderer&) = delete;
    DebugUiRenderer(DebugUiRenderer&&) = delete;
    DebugUiRenderer& operator=(DebugUiRenderer&&) = delete;

    bool process_event(const SDL_Event* event);

    [[nodiscard]] bool wants_mouse_input() const;
    [[nodiscard]] bool wants_keyboard_input() const;

    void render(ScreenManager& screen_manager, TreeLayoutEditor& tree_editor,
                BattleTuningController& battle_tuning, UpscaleController& upscale_controller,
                const UpscalePresentation&        upscale_presentation,
                d2::app::AnimationTimeController& animation_time_controller,
                d2::audio::AudioService&          audio_service,
                d2::audio::DebugAudioPreview&     debug_audio_preview,
                DebugSoundCatalog&                debug_sound_catalog);

private:
    void render_tree_layout_contents(TreeLayoutEditor& tree_editor);
    void render_screen_selector(TreeLayoutEditor& tree_editor);
    void render_node_selector(TreeLayoutEditor& tree_editor);
    void render_node_properties(TreeLayoutEditor& tree_editor);
    void render_battle_tuning_contents(BattleTuningController& battle_tuning);
    void render_movement_contents(AdventureScreen& adventure_screen);
    void render_output_contents(UpscaleController&         upscale_controller,
                                const UpscalePresentation& presentation);
    void render_animation_time_contents(d2::app::AnimationTimeController& controller);
    void render_audio_preview_contents(d2::audio::AudioService&      audio_service,
                                       d2::audio::DebugAudioPreview& debug_audio_preview,
                                       DebugSoundCatalog&            catalog);

    // Game window opacity controls
    void render_game_window_opacity_panel();

    void set_game_window_opacity(float opacity);

    ImGuiContext*         ctx_ = nullptr;
    SDL_Window*           window_ = nullptr;
    SDL_Renderer*         renderer_ = nullptr;
    float                 game_window_opacity_ = 1.0f;
    float                 imgui_display_width_ = 0.0F;
    float                 imgui_display_height_ = 0.0F;
    float                 imgui_framebuffer_scale_x_ = 1.0F;
    float                 imgui_framebuffer_scale_y_ = 1.0F;
    float                 imgui_renderer_scale_x_ = 1.0F;
    float                 imgui_renderer_scale_y_ = 1.0F;
    bool                  window_opacity_supported_ = true;
    bool                  window_opacity_failure_logged_ = false;
    bool                  initialized_ = false;
    int                   audio_source_index_ = 0;
    std::string           selected_audio_sound_;
    std::array<char, 128> audio_sound_filter_{};
    bool                  audio_preview_loop_ = false;
    std::string           audio_preview_error_;
};

} // namespace d2engine
