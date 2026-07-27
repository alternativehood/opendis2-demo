#pragma once

#include "upscale_settings.hpp"

#include <SDL3/SDL.h>

#include <cstddef>
#include <string>

// cppcheck-suppress syntaxError
#if !SDL_VERSION_ATLEAST(3, 4, 0)
#error "OpenDis2 whole-frame upscaling requires SDL 3.4.0 or newer"
#endif

namespace d2engine {

class SdlFramePresenter {
public:
    SdlFramePresenter(SDL_Renderer* renderer, UpscaleExtent logical_size);
    ~SdlFramePresenter();

    SdlFramePresenter(const SdlFramePresenter&) = delete;
    SdlFramePresenter& operator=(const SdlFramePresenter&) = delete;
    SdlFramePresenter(SdlFramePresenter&&) = delete;
    SdlFramePresenter& operator=(SdlFramePresenter&&) = delete;

    void set_settings(UpscaleSettings settings);

    void begin_scene_frame();
    void composite_scene_to_window();
    void begin_native_overlay();
    void end_native_overlay();
    void present();
    void abort_frame() noexcept;

    [[nodiscard]] const UpscalePresentation& presentation() const noexcept { return presentation_; }
    [[nodiscard]] SDL_Texture*               scene_target() const noexcept { return scene_target_; }
    [[nodiscard]] std::string                renderer_name() const;
    [[nodiscard]] std::string                gpu_device_name() const;
    [[nodiscard]] std::string                gpu_backend_name() const;
    [[nodiscard]] bool                       fsr_states_ready() const noexcept {
        return easu_state_ != nullptr && rcas_state_ != nullptr;
    }
    [[nodiscard]] std::size_t output_resource_generation() const noexcept {
        return output_resource_generation_;
    }

private:
    struct RendererState;

    void              refresh_output_size();
    void              refresh_runtime_dimensions();
    void              recreate_output_target();
    void              create_scene_target();
    void              create_fsr_states();
    void              release_resources() noexcept;
    void              update_effective_mode();
    void              update_fsr_uniforms();
    void              render_easu();
    void              render_rcas();
    void              configure_native_target(SDL_Texture* target);
    void              capture_window_state();
    void              restore_window_state();
    void              require(bool result, const char* operation) const;
    [[noreturn]] void fail(const char* operation) const;

    SDL_Renderer*       renderer_ = nullptr;
    SDL_Window*         window_ = nullptr;
    SDL_GPUDevice*      device_ = nullptr;
    SDL_Texture*        scene_target_ = nullptr;
    SDL_Texture*        easu_target_ = nullptr;
    SDL_GPUShader*      easu_shader_ = nullptr;
    SDL_GPUShader*      rcas_shader_ = nullptr;
    SDL_GPURenderState* easu_state_ = nullptr;
    SDL_GPURenderState* rcas_state_ = nullptr;

    UpscalePresentation presentation_;
    RendererState*      saved_window_state_ = nullptr;
    SDL_GPUShaderFormat shader_format_ = SDL_GPU_SHADERFORMAT_INVALID;
    bool                easu_uniforms_dirty_ = true;
    bool                rcas_uniforms_dirty_ = true;
    bool                scene_frame_active_ = false;
    bool                native_overlay_active_ = false;
    std::size_t         output_resource_generation_ = 0;
    UpscaleExtent       fallback_output_size_{.width = -1, .height = -1};
};

} // namespace d2engine
