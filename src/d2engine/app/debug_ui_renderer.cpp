#include "debug_ui_renderer.hpp"
#include "battle_tuning_controller.hpp"
#include "battle_tuning_state.hpp"
#include "adventure_screen.hpp"
#include "screen.hpp"
#include "screen_manager.hpp"
#include "upscale_controller.hpp"

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlrenderer3.h>

#include <d2log/log.hpp>

#include <cstdio>
#include <array>
#include <cmath>
#include <cctype>
#include <sstream>
#include <stdexcept>

namespace d2engine {

namespace {
auto kLog = d2log::get("d2.debug_ui"); // NOLINT(cert-err58-cpp)

d2::audio::AudioBus preview_bus_for_source(const DebugSoundSource source) {
    if (source == DebugSoundSource::Battle || source == DebugSoundSource::Midgard) {
        return d2::audio::AudioBus::Sfx;
    }
    return d2::audio::AudioBus::Ambience;
}

class SdlRenderScaleScope {
public:
    SdlRenderScaleScope(SDL_Renderer* renderer, float scale_x, float scale_y)
        : renderer_(renderer) {
        if (renderer_ == nullptr) {
            throw std::runtime_error("Dear ImGui renderer scale requires a non-null SDL_Renderer");
        }
        if (!std::isfinite(scale_x) || !std::isfinite(scale_y) || scale_x <= 0.0F ||
            scale_y <= 0.0F) {
            throw std::runtime_error("Dear ImGui renderer scale is invalid");
        }
        if (!SDL_GetRenderScale(renderer_, &original_x_, &original_y_)) {
            throw std::runtime_error(std::string{"SDL_GetRenderScale for Dear ImGui failed: "} +
                                     SDL_GetError());
        }
        if (!SDL_SetRenderScale(renderer_, scale_x, scale_y)) {
            throw std::runtime_error(std::string{"SDL_SetRenderScale for Dear ImGui failed: "} +
                                     SDL_GetError());
        }
    }

    ~SdlRenderScaleScope() {
        if (renderer_ != nullptr && !SDL_SetRenderScale(renderer_, original_x_, original_y_)) {
            kLog->error("SDL_SetRenderScale Dear ImGui restore failed: {}", SDL_GetError());
        }
    }

    SdlRenderScaleScope(const SdlRenderScaleScope&) = delete;
    SdlRenderScaleScope& operator=(const SdlRenderScaleScope&) = delete;
    SdlRenderScaleScope(SdlRenderScaleScope&&) = delete;
    SdlRenderScaleScope& operator=(SdlRenderScaleScope&&) = delete;

private:
    SDL_Renderer* renderer_ = nullptr;
    float         original_x_ = 1.0F;
    float         original_y_ = 1.0F;
};

[[nodiscard]] std::string imgui_scale_diagnostic(const ImGuiIO& io, SDL_Window* window,
                                                 SDL_Renderer* renderer) {
    int        window_width = 0;
    int        window_height = 0;
    int        pixel_width = 0;
    int        pixel_height = 0;
    int        output_width = 0;
    int        output_height = 0;
    const bool window_size_ok =
        window != nullptr && SDL_GetWindowSize(window, &window_width, &window_height);
    const bool pixel_size_ok =
        window != nullptr && SDL_GetWindowSizeInPixels(window, &pixel_width, &pixel_height);
    const bool output_size_ok =
        renderer != nullptr && SDL_GetRenderOutputSize(renderer, &output_width, &output_height);
    const char* renderer_name = renderer != nullptr ? SDL_GetRendererName(renderer) : nullptr;
    std::ostringstream message;
    message << "Invalid Dear ImGui framebuffer scale; display_size=" << io.DisplaySize.x << "x"
            << io.DisplaySize.y << " framebuffer_scale=" << io.DisplayFramebufferScale.x << "x"
            << io.DisplayFramebufferScale.y << " window_coordinates=";
    if (window_size_ok) {
        message << window_width << "x" << window_height;
    } else {
        message << "<unavailable>";
    }
    message << " window_pixels=";
    if (pixel_size_ok) {
        message << pixel_width << "x" << pixel_height;
    } else {
        message << "<unavailable>";
    }
    message << " renderer_output=";
    if (output_size_ok) {
        message << output_width << "x" << output_height;
    } else {
        message << "<unavailable>";
    }
    message << " renderer=" << (renderer_name != nullptr ? renderer_name : "<unavailable>")
            << " sdl_error=" << SDL_GetError();
    return message.str();
}

void validate_framebuffer_scale(const ImGuiIO& io, SDL_Window* window, SDL_Renderer* renderer) {
    if (!std::isfinite(io.DisplayFramebufferScale.x) ||
        !std::isfinite(io.DisplayFramebufferScale.y) || io.DisplayFramebufferScale.x <= 0.0F ||
        io.DisplayFramebufferScale.y <= 0.0F) {
        throw std::runtime_error(imgui_scale_diagnostic(io, window, renderer));
    }
}

void draw_vec2_edit(const char* label, float* x, float* y, float reset_value = 0.0f) {
    ImGui::PushID(label);
    float v[2] = {*x, *y};
    if (ImGui::InputFloat2(label, v)) {
        *x = v[0];
        *y = v[1];
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("R")) {
        *x = reset_value;
        *y = reset_value;
    }
    ImGui::PopID();
}

void draw_float_edit(const char* label, float* value, float reset_value = 0.0f, float step = 1.0f,
                     float min = 0.0f, float max = 0.0f) {
    ImGui::PushID(label);
    ImGui::SetNextItemWidth(100.0f);
    if (ImGui::InputFloat(label, value, step, step * 10.0f, "%.2f")) {
        if (min < max) {
            *value = std::clamp(*value, min, max);
        }
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("R")) {
        *value = reset_value;
    }
    ImGui::PopID();
}

void draw_int_edit(const char* label, int* value, int reset_value = 0, int step = 1) {
    ImGui::PushID(label);
    ImGui::SetNextItemWidth(100.0f);
    ImGui::InputInt(label, value, step, step * 10);
    ImGui::SameLine();
    if (ImGui::SmallButton("R")) {
        *value = reset_value;
    }
    ImGui::PopID();
}

bool is_live_battle_screen(ScreenManager& sm) {
    for (const auto& ref : sm.live_screens()) {
        if (ref.screen != nullptr && ref.screen->name() == "BattleScreen")
            return true;
    }
    return false;
}

[[nodiscard]] AdventureScreen* find_live_adventure_screen(ScreenManager& screen_manager) {
    for (const auto& ref : screen_manager.live_screens()) {
        if (ref.screen == nullptr)
            continue;
        if (auto* screen = dynamic_cast<AdventureScreen*>(ref.screen))
            return screen;
    }
    return nullptr;
}

[[nodiscard]] const char* movement_mode_name(const d2game::AdventureInteractionMode mode) {
    switch (mode) {
    case d2game::AdventureInteractionMode::Idle:
        return "Idle";
    case d2game::AdventureInteractionMode::StackSelected:
        return "Stack selected";
    case d2game::AdventureInteractionMode::RoutePlanned:
        return "Route planned";
    case d2game::AdventureInteractionMode::Moving:
        return "Moving";
    }
    return "Unknown";
}

const char* debug_sound_load_state_name(const DebugSoundLoadState state) {
    switch (state) {
    case DebugSoundLoadState::NotLoaded:
        return "Not loaded";
    case DebugSoundLoadState::Loaded:
        return "Loaded";
    case DebugSoundLoadState::Missing:
        return "Missing";
    case DebugSoundLoadState::Failed:
        return "Failed";
    }
    return "Unknown";
}

bool contains_case_insensitive(const std::string& text, const std::string_view needle) {
    return std::search(text.begin(), text.end(), needle.begin(), needle.end(),
                       [](const unsigned char left, const unsigned char right) {
                           return std::tolower(left) == std::tolower(right);
                       }) != text.end();
}

} // namespace

DebugUiRenderer::DebugUiRenderer(SDL_Window* window, SDL_Renderer* renderer)
    : window_(window), renderer_(renderer) {
    if (window_ == nullptr || renderer_ == nullptr) {
        kLog->warn("DebugUiRenderer: null window or renderer, skipping init");
        return;
    }

    IMGUI_CHECKVERSION();
    ctx_ = ImGui::CreateContext();
    if (ctx_ == nullptr) {
        kLog->error("DebugUiRenderer: ImGui::CreateContext failed");
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    if (!ImGui_ImplSDL3_InitForSDLRenderer(window_, renderer_)) {
        kLog->error("DebugUiRenderer: ImGui_ImplSDL3_InitForSDLRenderer failed");
        ImGui::DestroyContext(ctx_);
        ctx_ = nullptr;
        return;
    }
    if (!ImGui_ImplSDLRenderer3_Init(renderer_)) {
        kLog->error("DebugUiRenderer: ImGui_ImplSDLRenderer3_Init failed");
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext(ctx_);
        ctx_ = nullptr;
        return;
    }

    initialized_ = true;
    kLog->info("DebugUiRenderer initialized");
}

DebugUiRenderer::~DebugUiRenderer() {
    if (initialized_) {
        ImGui_ImplSDLRenderer3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
    }
    if (ctx_ != nullptr) {
        ImGui::DestroyContext(ctx_);
    }
}

void DebugUiRenderer::set_game_window_opacity(float opacity) {
    opacity = std::clamp(opacity, 0.10f, 1.00f);
    if (window_ == nullptr || !SDL_SetWindowOpacity(window_, opacity)) {
        window_opacity_supported_ = false;
        if (!window_opacity_failure_logged_) {
            kLog->warn("SDL_SetWindowOpacity is unsupported: {}", SDL_GetError());
            window_opacity_failure_logged_ = true;
        }
        return;
    }
    game_window_opacity_ = opacity;
}

// cppcheck-suppress unusedFunction
bool DebugUiRenderer::process_event(const SDL_Event* event) {
    if (!initialized_)
        return false;
    return ImGui_ImplSDL3_ProcessEvent(event);
}

// cppcheck-suppress unusedFunction
bool DebugUiRenderer::wants_mouse_input() const {
    if (!initialized_)
        return false;
    return ImGui::GetIO().WantCaptureMouse;
}

// cppcheck-suppress unusedFunction
bool DebugUiRenderer::wants_keyboard_input() const {
    if (!initialized_)
        return false;
    return ImGui::GetIO().WantCaptureKeyboard;
}

void DebugUiRenderer::render(ScreenManager& screen_manager, TreeLayoutEditor& tree_editor,
                             BattleTuningController&           battle_tuning,
                             UpscaleController&                upscale_controller,
                             const UpscalePresentation&        upscale_presentation,
                             d2::app::AnimationTimeController& animation_time_controller,
                             d2::audio::AudioService&          audio_service,
                             d2::audio::DebugAudioPreview&     debug_audio_preview,
                             DebugSoundCatalog&                debug_sound_catalog) {
    if (!initialized_)
        return;

    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowSize(ImVec2(460, 600), ImGuiCond_FirstUseEver);
    ImGui::Begin("Debug Tools", nullptr, ImGuiWindowFlags_NoSavedSettings);
    if (!tree_editor.targets().empty() &&
        ImGui::CollapsingHeader("Tree Layout Editor", ImGuiTreeNodeFlags_SpanAvailWidth)) {
        ImGui::PushID("tree_layout");
        render_tree_layout_contents(tree_editor);
        ImGui::PopID();
    }
    if (ImGui::CollapsingHeader("Animation Time", ImGuiTreeNodeFlags_SpanAvailWidth)) {
        ImGui::PushID("animation_time");
        render_animation_time_contents(animation_time_controller);
        ImGui::PopID();
    }
    if (ImGui::CollapsingHeader("Audio Preview", ImGuiTreeNodeFlags_SpanAvailWidth)) {
        ImGui::PushID("audio_preview");
        render_audio_preview_contents(audio_service, debug_audio_preview, debug_sound_catalog);
        ImGui::PopID();
    }
    if (ImGui::CollapsingHeader("Render Output", ImGuiTreeNodeFlags_SpanAvailWidth)) {
        ImGui::PushID("render_output");
        render_output_contents(upscale_controller, upscale_presentation);
        ImGui::PopID();
    }
    if (battle_tuning.enabled() && is_live_battle_screen(screen_manager) &&
        ImGui::CollapsingHeader("Battle Tuning", ImGuiTreeNodeFlags_SpanAvailWidth)) {
        ImGui::PushID("battle_tuning");
        render_battle_tuning_contents(battle_tuning);
        ImGui::PopID();
    }
    if (auto* adventure_screen = find_live_adventure_screen(screen_manager);
        adventure_screen != nullptr &&
        ImGui::CollapsingHeader("Movement", ImGuiTreeNodeFlags_SpanAvailWidth)) {
        ImGui::PushID("movement");
        render_movement_contents(*adventure_screen);
        ImGui::PopID();
    }
    if (auto* adventure_screen = find_live_adventure_screen(screen_manager);
        adventure_screen != nullptr &&
        ImGui::CollapsingHeader("Adventure Rendering", ImGuiTreeNodeFlags_SpanAvailWidth)) {
        ImGui::PushID("adventure_rendering");
        if (ImGui::Button("Toggle banners"))
            adventure_screen->debug_toggle_banners();
        ImGui::Text("Banners: %s",
                    adventure_screen->debug_banners_visible() ? "visible" : "hidden");
        ImGui::PopID();
    }
    ImGui::End();

    ImGui::Render();
    const ImGuiIO& io = ImGui::GetIO();
    validate_framebuffer_scale(io, window_, renderer_);
    if (imgui_framebuffer_scale_x_ != io.DisplayFramebufferScale.x ||
        imgui_framebuffer_scale_y_ != io.DisplayFramebufferScale.y) {
        kLog->info("framebuffer_scale old={:.3f}x{:.3f} new={:.3f}x{:.3f}",
                   imgui_framebuffer_scale_x_, imgui_framebuffer_scale_y_,
                   io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
    }
    imgui_display_width_ = io.DisplaySize.x;
    imgui_display_height_ = io.DisplaySize.y;
    imgui_framebuffer_scale_x_ = io.DisplayFramebufferScale.x;
    imgui_framebuffer_scale_y_ = io.DisplayFramebufferScale.y;
    imgui_renderer_scale_x_ = io.DisplayFramebufferScale.x;
    imgui_renderer_scale_y_ = io.DisplayFramebufferScale.y;
    SdlRenderScaleScope scale_scope{renderer_, io.DisplayFramebufferScale.x,
                                    io.DisplayFramebufferScale.y};
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer_);
}

void DebugUiRenderer::render_movement_contents(AdventureScreen& adventure_screen) {
    const auto snapshot = adventure_screen.movement_debug_snapshot();
    ImGui::Text("Mode: %s", movement_mode_name(snapshot.mode));
    ImGui::Text("Selected stack: %s",
                snapshot.selected_stack_id ? snapshot.selected_stack_id->c_str() : "none");
    ImGui::Text("Current move points: %s",
                snapshot.current_movement_points
                    ? std::to_string(*snapshot.current_movement_points).c_str()
                    : "-");
    ImGui::Text("Reset value: %s", snapshot.reset_movement_points
                                       ? std::to_string(*snapshot.reset_movement_points).c_str()
                                       : "-");

    const bool can_edit = snapshot.can_edit();
    if (!can_edit)
        ImGui::BeginDisabled();
    if (ImGui::Button("Reset move points"))
        static_cast<void>(adventure_screen.debug_reset_selected_stack_movement_points());
    if (ImGui::Button("Free move points"))
        static_cast<void>(adventure_screen.debug_grant_selected_stack_free_movement_points());
    if (!can_edit)
        ImGui::EndDisabled();
    if (ImGui::Button(adventure_screen.debug_follow_unit_enabled() ? "Stop following"
                                                                   : "Follow unit"))
        adventure_screen.debug_toggle_follow_unit();
    ImGui::TextUnformatted("Free move points = 1048576");
}

void DebugUiRenderer::render_audio_preview_contents(
    d2::audio::AudioService& audio_service, d2::audio::DebugAudioPreview& debug_audio_preview,
    DebugSoundCatalog& catalog) {
    static constexpr std::array<const char*, d2::audio::kAudioBusCount> kBusNames{
        "Master", "Music", "Ambience", "SFX", "UI", "Voice"};
    for (std::size_t index = 0; index < d2::audio::kAudioBusCount; ++index) {
        const auto bus = static_cast<d2::audio::AudioBus>(index);
        float      gain = audio_service.bus_gain(bus);
        if (ImGui::SliderFloat(kBusNames[index], &gain, 0.0F, 1.0F, "%.2f")) {
            audio_service.set_bus_gain(bus, gain);
        }
    }
    if (ImGui::Button("Reset gains")) {
        for (std::size_t index = 0; index < d2::audio::kAudioBusCount; ++index) {
            audio_service.set_bus_gain(static_cast<d2::audio::AudioBus>(index), 1.0F);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop all")) {
        audio_service.stop_all();
    }

    ImGui::Separator();
    const auto  banks = catalog.banks();
    const char* source_name =
        banks[static_cast<std::size_t>(audio_source_index_)].display_name.c_str();
    if (ImGui::BeginCombo("Source", source_name)) {
        for (std::size_t index = 0; index < kDebugSoundSourceCount; ++index) {
            const bool selected = audio_source_index_ == static_cast<int>(index);
            if (ImGui::Selectable(banks[index].display_name.c_str(), selected)) {
                audio_source_index_ = static_cast<int>(index);
                selected_audio_sound_.clear();
                static_cast<void>(catalog.ensure_loaded(static_cast<DebugSoundSource>(index)));
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    const auto            source = static_cast<DebugSoundSource>(audio_source_index_);
    const auto            preview_bus = preview_bus_for_source(source);
    const DebugSoundBank& bank = catalog.ensure_loaded(source);
    ImGui::Text("Status: %s", debug_sound_load_state_name(bank.state));
    ImGui::Text("Sounds: %zu", bank.sounds.size());
    if (!bank.error.empty()) {
        ImGui::TextWrapped("%s", bank.error.c_str());
    }
    ImGui::InputText("Filter", audio_sound_filter_.data(), audio_sound_filter_.size());

    const std::string_view              filter{audio_sound_filter_.data()};
    std::vector<const DebugSoundEntry*> filtered;
    filtered.reserve(bank.sounds.size());
    for (const DebugSoundEntry& sound : bank.sounds) {
        if (filter.empty() || contains_case_insensitive(sound.logical_name, filter)) {
            filtered.push_back(&sound);
        }
    }
    ImGui::TextUnformatted("Sound:");
    if (ImGui::BeginChild("sounds", ImVec2(0.0F, 180.0F), ImGuiChildFlags_Borders)) {
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(filtered.size()));
        while (clipper.Step()) {
            for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index) {
                const DebugSoundEntry& sound = *filtered[static_cast<std::size_t>(index)];
                const bool             selected = selected_audio_sound_ == sound.logical_name;
                if (ImGui::Selectable(sound.logical_name.c_str(), selected)) {
                    selected_audio_sound_ = sound.logical_name;
                }
            }
        }
    }
    ImGui::EndChild();

    const auto selected =
        std::find_if(bank.sounds.begin(), bank.sounds.end(), [this](const DebugSoundEntry& sound) {
            return sound.logical_name == selected_audio_sound_;
        });
    if (selected != bank.sounds.end()) {
        ImGui::Text("Selected sound: %s", selected->logical_name.c_str());
        ImGui::Text("Payload size: %llu", static_cast<unsigned long long>(selected->payload_size));
    } else {
        ImGui::TextUnformatted("Selected sound: <none>");
    }
    const auto status = debug_audio_preview.preview_status();
    const bool can_play = status.state != d2::audio::DebugAudioPreviewState::Unavailable &&
                          bank.state == DebugSoundLoadState::Loaded &&
                          selected != bank.sounds.end();
    if (!can_play)
        ImGui::BeginDisabled();
    if (ImGui::Button("Play")) {
        try {
            DebugEncodedSound encoded = catalog.load_encoded_sound(source, selected_audio_sound_);
            audio_preview_error_.clear();
            static_cast<void>(
                debug_audio_preview.play_preview({.display_name = std::move(encoded.logical_name),
                                                  .encoded_payload = std::move(encoded.payload),
                                                  .bus = preview_bus,
                                                  .loop = audio_preview_loop_}));
        } catch (const std::exception& exception) {
            audio_preview_error_ = exception.what();
        }
    }
    if (!can_play)
        ImGui::EndDisabled();
    ImGui::SameLine();
    const bool can_stop = status.state == d2::audio::DebugAudioPreviewState::Playing ||
                          status.state == d2::audio::DebugAudioPreviewState::Failed;
    if (!can_stop)
        ImGui::BeginDisabled();
    if (ImGui::Button("Stop"))
        debug_audio_preview.stop_preview();
    if (!can_stop)
        ImGui::EndDisabled();
    ImGui::SameLine();
    if (status.state == d2::audio::DebugAudioPreviewState::Unavailable)
        ImGui::BeginDisabled();
    ImGui::Checkbox("Loop", &audio_preview_loop_);
    if (status.state == d2::audio::DebugAudioPreviewState::Unavailable)
        ImGui::EndDisabled();

    const auto state_name = [](const d2::audio::DebugAudioPreviewState value) {
        switch (value) {
        case d2::audio::DebugAudioPreviewState::Unavailable:
            return "Unavailable";
        case d2::audio::DebugAudioPreviewState::Stopped:
            return "Stopped";
        case d2::audio::DebugAudioPreviewState::Playing:
            return "Playing";
        case d2::audio::DebugAudioPreviewState::Failed:
            return "Failed";
        }
        return "Unknown";
    };
    ImGui::TextUnformatted("Backend: SDL3_mixer");
    ImGui::Text("State: %s", state_name(status.state));
    ImGui::Text("Current sound: %s",
                status.current_name.empty() ? "<none>" : status.current_name.c_str());
    ImGui::Text("Looping: %s", status.loop ? "Yes" : "No");
    ImGui::Text("Preview bus: %s", preview_bus == d2::audio::AudioBus::Sfx ? "SFX" : "Ambience");
    if (status.state == d2::audio::DebugAudioPreviewState::Unavailable)
        ImGui::TextWrapped("Backend unavailable: %s", status.message.c_str());
    else if (!audio_preview_error_.empty())
        ImGui::TextWrapped("Playback error: %s", audio_preview_error_.c_str());
    else if (!status.message.empty())
        ImGui::TextWrapped("Playback error: %s", status.message.c_str());
}

void DebugUiRenderer::render_animation_time_contents(d2::app::AnimationTimeController& controller) {
    float speed = controller.speed();
    ImGui::TextUnformatted("Animation speed");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::SliderFloat("##animation_speed", &speed,
                           d2::app::AnimationTimeController::kMinimumSpeed,
                           d2::app::AnimationTimeController::kMaximumSpeed, "%.2f\xC3\x97",
                           ImGuiSliderFlags_Logarithmic)) {
        controller.set_speed(speed);
    }
    bool paused = controller.paused();
    if (ImGui::Checkbox("Pause animations", &paused))
        controller.set_paused(paused);
    const auto preset = [&controller](const char* label, float value) {
        if (ImGui::Button(label))
            controller.set_speed(value);
    };
    preset("0.25\xC3\x97", 0.25F);
    ImGui::SameLine();
    preset("0.50\xC3\x97", 0.50F);
    ImGui::SameLine();
    preset("1.00\xC3\x97", 1.00F);
    ImGui::SameLine();
    preset("2.00\xC3\x97", 2.00F);
    ImGui::SameLine();
    preset("4.00\xC3\x97", 4.00F);
    if (ImGui::Button("Reset to default"))
        controller.reset();
    if (controller.paused()) {
        ImGui::Text("Animation time: paused");
        ImGui::Text("Selected speed: %.2f\xC3\x97", static_cast<double>(controller.speed()));
    } else {
        ImGui::Text("Current speed: %.2f\xC3\x97", static_cast<double>(controller.speed()));
    }
}

void DebugUiRenderer::render_output_contents(UpscaleController&         upscale_controller,
                                             const UpscalePresentation& presentation) {
    static constexpr std::array<UpscaleMode, 4> kModes{
        UpscaleMode::Linear,
        UpscaleMode::Nearest,
        UpscaleMode::PixelArt,
        UpscaleMode::Fsr1,
    };

    int current_mode = 0;
    for (std::size_t index = 0; index < kModes.size(); ++index) {
        if (kModes[index] == presentation.requested.mode) {
            current_mode = static_cast<int>(index);
            break;
        }
    }
    const char* mode_names[] = {
        upscale_mode_name(UpscaleMode::Linear).data(),
        upscale_mode_name(UpscaleMode::Nearest).data(),
        upscale_mode_name(UpscaleMode::PixelArt).data(),
        upscale_mode_name(UpscaleMode::Fsr1).data(),
    };
    if (ImGui::Combo("Upscale", &current_mode, mode_names, static_cast<int>(kModes.size()))) {
        upscale_controller.set_mode(kModes[static_cast<std::size_t>(current_mode)]);
    }

    ImGui::Text("Effective: %s", upscale_mode_name(presentation.effective_mode).data());
    if (presentation.fallback_reason.has_value()) {
        ImGui::TextColored(ImVec4(1.0F, 0.75F, 0.2F, 1.0F), "FSR fallback: %s",
                           upscale_fallback_reason_name(*presentation.fallback_reason).data());
    }
    ImGui::Text("Logical coordinates: %d x %d", presentation.logical_size.width,
                presentation.logical_size.height);
    ImGui::Text("Scene pixels: %d x %d", presentation.scene_pixel_size.width,
                presentation.scene_pixel_size.height);
    ImGui::Text("Window coordinates: %d x %d", presentation.window_coordinate_size.width,
                presentation.window_coordinate_size.height);
    ImGui::Text("Window pixels: %d x %d", presentation.window_pixel_size.width,
                presentation.window_pixel_size.height);
    ImGui::Text("Output pixels: %d x %d", presentation.output_size.width,
                presentation.output_size.height);
    ImGui::Text("Pixel density: %.2f", static_cast<double>(presentation.window_pixel_density));
    ImGui::Text("Sampling ratio: %.3f x %.3f", static_cast<double>(presentation.scale_x),
                static_cast<double>(presentation.scale_y));
    ImGui::Text("Whole-frame sampling: %s", presentation.sampling_active ? "active" : "inactive");
    if (!presentation.sampling_active) {
        ImGui::TextColored(ImVec4(1.0F, 0.5F, 0.2F, 1.0F),
                           "Upscaling inactive: scene and output are both %d x %d.",
                           presentation.scene_pixel_size.width,
                           presentation.scene_pixel_size.height);
        ImGui::TextWrapped("Linear, Nearest, and Pixel Art are performing a 1:1 copy.");
    }
    if (presentation.scale_x == std::round(presentation.scale_x) &&
        presentation.scale_y == std::round(presentation.scale_y)) {
        ImGui::TextWrapped("At an exact integer ratio Pixel Art may match Nearest. Resize to a "
                           "non-integer ratio to compare their edge handling.");
    }
    ImGui::Text("SDL renderer: %s", presentation.renderer_name.c_str());
    ImGui::Text("GPU device: %s", presentation.gpu_device_name.c_str());
    ImGui::Text("GPU backend: %s", presentation.gpu_backend.c_str());

    if (presentation.requested.mode == UpscaleMode::Fsr1) {
        float sharpness = presentation.requested.fsr_sharpness;
        if (ImGui::SliderFloat("FSR sharpness", &sharpness, 0.0F, 1.0F, "%.2f")) {
            upscale_controller.set_fsr_sharpness(sharpness);
        }
    }
    if (ImGui::Button("Reset Render Output")) {
        upscale_controller.reset();
    }

    if (ImGui::CollapsingHeader("Diagnostics")) {
        ImGui::Text("ImGui Display Size: %.0f x %.0f", static_cast<double>(imgui_display_width_),
                    static_cast<double>(imgui_display_height_));
        ImGui::Text("ImGui Framebuffer Scale: %.2f x %.2f",
                    static_cast<double>(imgui_framebuffer_scale_x_),
                    static_cast<double>(imgui_framebuffer_scale_y_));
        ImGui::Text("ImGui Framebuffer Size: %.0f x %.0f",
                    static_cast<double>(imgui_display_width_ * imgui_framebuffer_scale_x_),
                    static_cast<double>(imgui_display_height_ * imgui_framebuffer_scale_y_));
        ImGui::Text("SDL Renderer Scale During UI: %.2f x %.2f",
                    static_cast<double>(imgui_renderer_scale_x_),
                    static_cast<double>(imgui_renderer_scale_y_));
        const int imgui_width =
            static_cast<int>(std::lround(imgui_display_width_ * imgui_framebuffer_scale_x_));
        const int imgui_height =
            static_cast<int>(std::lround(imgui_display_height_ * imgui_framebuffer_scale_y_));
        if (std::abs(imgui_width - presentation.output_size.width) > 1 ||
            std::abs(imgui_height - presentation.output_size.height) > 1) {
            ImGui::TextColored(ImVec4(1.0F, 0.75F, 0.2F, 1.0F),
                               "ImGui framebuffer dimensions do not match SDL output:");
            ImGui::Text("ImGui=%d x %d SDL=%d x %d", imgui_width, imgui_height,
                        presentation.output_size.width, presentation.output_size.height);
        }
        ImGui::Separator();
        ImGui::Text("Linear composites: %llu",
                    static_cast<unsigned long long>(presentation.linear_composite_count));
        ImGui::Text("Nearest composites: %llu",
                    static_cast<unsigned long long>(presentation.nearest_composite_count));
        ImGui::Text("Pixel Art composites: %llu",
                    static_cast<unsigned long long>(presentation.pixel_art_composite_count));
        ImGui::Text("EASU passes: %llu",
                    static_cast<unsigned long long>(presentation.easu_pass_count));
        ImGui::Text("RCAS passes: %llu",
                    static_cast<unsigned long long>(presentation.rcas_pass_count));
    }

    render_game_window_opacity_panel();
}

void DebugUiRenderer::render_tree_layout_contents(TreeLayoutEditor& tree_editor) {
    render_screen_selector(tree_editor);

    if (tree_editor.selected_screen() != nullptr) {
        render_node_selector(tree_editor);
        ImGui::Separator();
        render_node_properties(tree_editor);
    }
}

void DebugUiRenderer::render_screen_selector(TreeLayoutEditor& tree_editor) {
    const auto targets = tree_editor.targets();
    if (targets.empty()) {
        ImGui::Text("No screens available");
        return;
    }

    std::optional<ScreenInstanceId> current = tree_editor.selected_screen_id();

    for (const auto& ref : targets) {
        std::string label = std::string(ref.screen->name()) + " #" + std::to_string(ref.id);
        bool        is_selected = current.has_value() && *current == ref.id;
        if (is_selected) {
            ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "%s", label.c_str());
        } else if (ImGui::SmallButton(label.c_str())) {
            tree_editor.select_screen(ref.id);
        }
    }
}

void DebugUiRenderer::render_node_selector(TreeLayoutEditor& tree_editor) {
    Screen* screen = tree_editor.selected_screen();
    if (screen == nullptr)
        return;

    ImGui::Text("Config: %s", screen->config_source().c_str());
    ImGui::Separator();

    const auto paths = screen->tree_layout().paths();
    if (paths.empty()) {
        ImGui::Text("(empty tree)");
        return;
    }

    std::optional<std::string> current = tree_editor.selected_node_path();

    for (const auto& path : paths) {
        bool is_selected = current.has_value() && *current == path;
        if (is_selected) {
            ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "%s", path.c_str());
        } else if (ImGui::Selectable(path.c_str())) {
            tree_editor.select_node(path);
        }
    }
}

void DebugUiRenderer::render_node_properties(TreeLayoutEditor& tree_editor) {
    Screen* screen = tree_editor.selected_screen();
    if (screen == nullptr)
        return;

    auto node_path = tree_editor.selected_node_path();
    if (!node_path.has_value()) {
        ImGui::Text("(no node selected)");
        return;
    }

    auto opt = screen->tree_layout().node(*node_path);
    if (!opt.has_value()) {
        ImGui::Text("(node not found)");
        return;
    }

    TreeNode n = *opt;
    float    fx = n.x;
    float    fy = n.y;
    float    fw = n.w;
    float    fh = n.h;
    float    fa = n.alpha;
    int      level = n.level;

    draw_vec2_edit("Position", &fx, &fy);
    draw_vec2_edit("Size", &fw, &fh);
    draw_float_edit("Alpha", &fa, 1.0f, 0.05f, 0.0f, 1.0f);
    draw_int_edit("Level", &level, 0, 1);

    if (fx != n.x || fy != n.y || fw != n.w || fh != n.h || fa != n.alpha || level != n.level) {
        TreeNode updated = n;
        updated.x = fx;
        updated.y = fy;
        updated.w = fw;
        updated.h = fh;
        updated.alpha = fa;
        updated.level = level;
        tree_editor.update_selected_node(updated);
    }

    ImGui::Separator();

    const bool dirty = tree_editor.is_dirty();
    if (dirty) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Dirty");
    } else {
        ImGui::Text("Clean");
    }

    if (ImGui::Button("Save Tree")) {
        tree_editor.save();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload Tree")) {
        tree_editor.reload();
    }
    if (ImGui::Button("Revert Node")) {
        tree_editor.revert_selected();
    }
    ImGui::SameLine();
    if (ImGui::Button("Revert All Tree Changes")) {
        tree_editor.revert_all();
    }
}

void DebugUiRenderer::render_game_window_opacity_panel() {
    ImGui::Separator();
    ImGui::Text("Game Window Opacity");
    if (!window_opacity_supported_) {
        ImGui::TextDisabled("unsupported");
        return;
    }
    float opacity = game_window_opacity_;
    if (ImGui::SliderFloat("##window_opacity", &opacity, 0.10f, 1.00f, "%.2f")) {
        set_game_window_opacity(opacity);
    }
    if (ImGui::Button("50% Window")) {
        set_game_window_opacity(0.50f);
    }
    ImGui::SameLine();
    if (ImGui::Button("Opaque Window")) {
        set_game_window_opacity(1.00f);
    }
}

void DebugUiRenderer::render_battle_tuning_contents(BattleTuningController& battle_tuning) {
    auto& state = battle_tuning.state();

    const bool has_dirty = !state.dirty.empty();
    if (has_dirty) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Dirty");
        ImGui::SameLine();
        if (ImGui::Button("Save")) {
            battle_tuning.save();
        }
        ImGui::SameLine();
        if (ImGui::Button("Revert")) {
            battle_tuning.revert_selected();
        }
        ImGui::SameLine();
        if (ImGui::Button("Revert All")) {
            battle_tuning.revert_all();
        }
    } else {
        ImGui::Text("Clean");
    }

    ImGui::Separator();

    const DebugRenderableItem* item = state.selected_item();
    if (item == nullptr) {
        ImGui::Text("No item selected.");
        return;
    }

    ImGui::Text("ID: %s", item->stable_id.c_str());
    ImGui::Text("Kind: %s", item->kind.c_str());
    if (!item->render_side.empty())
        ImGui::Text("Side: %s", item->render_side.c_str());

    ImGui::Separator();

    // Only VisualPlacement properties (TreeLayout properties handled in universal panel)
    if (!item->binding.has_value()) {
        ImGui::Text("(read-only)");
        return;
    }
    const auto binding = to_config_binding(*item->binding);
    if (!binding.has_value()) {
        ImGui::Text("(unresolved)");
        return;
    }
    if (binding->owner_kind == BindingOwnerKind::TreeLayout) {
        ImGui::Text("TreeLayout node — edit in Tree Layout Editor panel");
        return;
    }

    ImGui::Text("Path: %s", binding->display_path.c_str());

    VisualPlacementValue value = state.placement(*binding);

    float fx = value.x;
    float fy = value.y;
    float fsx = value.scale_x;
    float fsy = value.scale_y;
    int   level_v = value.level;
    int   frame_delay = value.frame_delay;
    float fa = value.alpha;

    draw_vec2_edit("Offset", &fx, &fy);
    draw_vec2_edit("Scale", &fsx, &fsy, 1.0f);
    draw_float_edit("Alpha", &fa, 1.0f, 0.05f, 0.0f, 1.0f);
    draw_int_edit("Level", &level_v, 0, 1);
    draw_int_edit("FrameDelay", &frame_delay, 0, 1);

    VisualPlacementValue updated = value;
    updated.x = fx;
    updated.y = fy;
    updated.scale_x = fsx;
    updated.scale_y = fsy;
    updated.alpha = fa;
    updated.level = level_v;
    updated.frame_delay = frame_delay;

    if (updated.x != value.x || updated.y != value.y || updated.scale_x != value.scale_x ||
        updated.scale_y != value.scale_y || updated.alpha != value.alpha ||
        updated.level != value.level || updated.frame_delay != value.frame_delay) {
        battle_tuning.update_placement(*binding, updated);
    }
}

} // namespace d2engine
