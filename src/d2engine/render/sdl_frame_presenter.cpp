#include "sdl_frame_presenter.hpp"

#include "fsr_embedded_shader.hpp"

#include <d2log/log.hpp>

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace d2engine {
namespace {

auto kLog = d2log::get("d2.render.upscale"); // NOLINT(cert-err58-cpp)

struct FsrEasuUniforms {
    std::array<uint32_t, 4> con0{};
    std::array<uint32_t, 4> con1{};
    std::array<uint32_t, 4> con2{};
    std::array<uint32_t, 4> con3{};
    std::array<uint32_t, 2> output_size{};
    std::array<uint32_t, 2> padding{};
};
static_assert(sizeof(FsrEasuUniforms) == 80U);

struct FsrRcasUniforms {
    std::array<uint32_t, 4> con0{};
    std::array<uint32_t, 2> output_size{};
    std::array<uint32_t, 2> padding{};
};
static_assert(sizeof(FsrRcasUniforms) == 32U);

[[nodiscard]] uint32_t float_bits(float value) noexcept {
    return std::bit_cast<uint32_t>(value);
}

[[nodiscard]] std::string shader_formats_name(SDL_GPUShaderFormat formats) {
    std::string result;
    const auto  append = [&result](std::string_view name) {
        if (!result.empty()) {
            result += ",";
        }
        result += name;
    };
    if ((formats & SDL_GPU_SHADERFORMAT_SPIRV) != 0U) {
        append("SPIR-V");
    }
    if ((formats & SDL_GPU_SHADERFORMAT_DXIL) != 0U) {
        append("DXIL");
    }
    if ((formats & SDL_GPU_SHADERFORMAT_MSL) != 0U) {
        append("MSL");
    }
    return result.empty() ? "none" : result;
}

[[nodiscard]] const char* shader_format_name(SDL_GPUShaderFormat format) noexcept {
    switch (format) {
    case SDL_GPU_SHADERFORMAT_SPIRV:
        return "SPIR-V";
    case SDL_GPU_SHADERFORMAT_DXIL:
        return "DXIL";
    case SDL_GPU_SHADERFORMAT_MSL:
        return "MSL";
    default:
        return "unknown";
    }
}

[[nodiscard]] std::string property_string(SDL_GPUDevice* device, const char* property) {
    if (device == nullptr) {
        return "unavailable";
    }
    const SDL_PropertiesID properties = SDL_GetGPUDeviceProperties(device);
    if (properties == 0) {
        return "unavailable";
    }
    return SDL_GetStringProperty(properties, property, "unavailable");
}

[[nodiscard]] float rcas_strength(float user_sharpness) noexcept {
    // FsrRcasCon takes a power-of-two strength: 0 is no RCAS, 1 is FSR's maximum.
    if (user_sharpness <= 0.0F) {
        return 0.0F;
    }
    return std::exp2(-8.0F * (1.0F - user_sharpness));
}

class TextureScaleModeScope {
public:
    TextureScaleModeScope(SDL_Texture* texture, SDL_ScaleMode next) : texture_(texture) {
        if (!SDL_GetTextureScaleMode(texture_, &original_)) {
            throw std::runtime_error(std::string("SDL_GetTextureScaleMode failed: ") +
                                     SDL_GetError());
        }
        if (!SDL_SetTextureScaleMode(texture_, next)) {
            throw std::runtime_error(std::string("SDL_SetTextureScaleMode failed: ") +
                                     SDL_GetError());
        }
    }

    ~TextureScaleModeScope() {
        if (texture_ != nullptr && !SDL_SetTextureScaleMode(texture_, original_)) {
            kLog->error("SDL_SetTextureScaleMode restore failed: {}", SDL_GetError());
        }
    }

    TextureScaleModeScope(const TextureScaleModeScope&) = delete;
    TextureScaleModeScope& operator=(const TextureScaleModeScope&) = delete;
    TextureScaleModeScope(TextureScaleModeScope&&) = delete;
    TextureScaleModeScope& operator=(TextureScaleModeScope&&) = delete;

private:
    SDL_Texture*  texture_ = nullptr;
    SDL_ScaleMode original_ = SDL_SCALEMODE_LINEAR;
};

class GpuRenderStateScope {
public:
    GpuRenderStateScope(SDL_Renderer* renderer, SDL_GPURenderState* state) : renderer_(renderer) {
        if (!SDL_SetGPURenderState(renderer_, state)) {
            throw std::runtime_error(std::string("SDL_SetGPURenderState failed: ") +
                                     SDL_GetError());
        }
    }

    ~GpuRenderStateScope() {
        if (!SDL_SetGPURenderState(renderer_, nullptr)) {
            kLog->error("SDL_SetGPURenderState clear failed: {}", SDL_GetError());
        }
    }

    GpuRenderStateScope(const GpuRenderStateScope&) = delete;
    GpuRenderStateScope& operator=(const GpuRenderStateScope&) = delete;
    GpuRenderStateScope(GpuRenderStateScope&&) = delete;
    GpuRenderStateScope& operator=(GpuRenderStateScope&&) = delete;

private:
    SDL_Renderer* renderer_ = nullptr;
};

} // namespace

struct SdlFramePresenter::RendererState {
    SDL_Texture*                    target = nullptr;
    int                             logical_width = 0;
    int                             logical_height = 0;
    SDL_RendererLogicalPresentation logical_mode = SDL_LOGICAL_PRESENTATION_DISABLED;
    bool                            viewport_set = false;
    SDL_Rect                        viewport{};
    bool                            clip_enabled = false;
    SDL_Rect                        clip{};
    float                           scale_x = 1.0F;
    float                           scale_y = 1.0F;
    SDL_BlendMode                   blend_mode = SDL_BLENDMODE_NONE;
    Uint8                           draw_r = 0;
    Uint8                           draw_g = 0;
    Uint8                           draw_b = 0;
    Uint8                           draw_a = 0;
    SDL_TextureAddressMode          address_u = SDL_TEXTURE_ADDRESS_AUTO;
    SDL_TextureAddressMode          address_v = SDL_TEXTURE_ADDRESS_AUTO;
};

SdlFramePresenter::SdlFramePresenter(SDL_Renderer* renderer, UpscaleExtent logical_size)
    : renderer_(renderer) {
    if (renderer_ == nullptr || logical_size.width <= 0 || logical_size.height <= 0) {
        throw std::runtime_error(
            "SdlFramePresenter requires a renderer and positive logical dimensions");
    }
    presentation_.logical_size = logical_size;
    presentation_.scene_pixel_size = logical_size;

    try {
        device_ = SDL_GetGPURendererDevice(renderer_);
        if (device_ == nullptr) {
            fail("SDL_GetGPURendererDevice");
        }
        window_ = SDL_GetRenderWindow(renderer_);
        if (window_ == nullptr) {
            fail("SDL_GetRenderWindow");
        }
        presentation_.renderer_name = renderer_name();
        presentation_.gpu_device_name = gpu_device_name();
        presentation_.gpu_backend = gpu_backend_name();
        create_scene_target();
        create_fsr_states();
        refresh_output_size();

        const int version = SDL_GetVersion();
        kLog->info(
            "startup sdl={}.{}.{} renderer={} gpu_device={} gpu_backend={} shader_formats={} "
            "logical_coordinates={}x{} scene_pixels={}x{} window_coordinates={}x{} "
            "window_pixels={}x{} output_pixels={}x{} pixel_density={:.3f} "
            "sampling_scale={:.3f}x{:.3f} sampling_active={} requested={} effective={}",
            SDL_VERSIONNUM_MAJOR(version), SDL_VERSIONNUM_MINOR(version),
            SDL_VERSIONNUM_MICRO(version), renderer_name(), gpu_device_name(),
            SDL_GetGPUDeviceDriver(device_), shader_formats_name(SDL_GetGPUShaderFormats(device_)),
            presentation_.logical_size.width, presentation_.logical_size.height,
            presentation_.scene_pixel_size.width, presentation_.scene_pixel_size.height,
            presentation_.window_coordinate_size.width, presentation_.window_coordinate_size.height,
            presentation_.window_pixel_size.width, presentation_.window_pixel_size.height,
            presentation_.output_size.width, presentation_.output_size.height,
            presentation_.window_pixel_density, presentation_.scale_x, presentation_.scale_y,
            presentation_.sampling_active, upscale_mode_name(presentation_.requested.mode),
            upscale_mode_name(presentation_.effective_mode));
    } catch (...) {
        release_resources();
        throw;
    }
}

SdlFramePresenter::~SdlFramePresenter() {
    delete saved_window_state_;
    saved_window_state_ = nullptr;
    release_resources();
}

void SdlFramePresenter::set_settings(UpscaleSettings settings) {
    settings.clamp();
    const UpscaleSettings previous = presentation_.requested;
    presentation_.requested = settings;
    if (previous.fsr_sharpness != settings.fsr_sharpness) {
        rcas_uniforms_dirty_ = true;
        kLog->info("sharpness user_value={:.3f}", settings.fsr_sharpness);
    }
    if (previous.mode != settings.mode) {
        kLog->info("requested_mode={}", upscale_mode_name(settings.mode));
    }
    update_effective_mode();
}

void SdlFramePresenter::begin_scene_frame() {
    if (scene_frame_active_) {
        throw std::runtime_error(
            "SdlFramePresenter begin_scene_frame called while a frame is active");
    }
    refresh_output_size();
    require(SDL_SetGPURenderState(renderer_, nullptr), "SDL_SetGPURenderState clear");
    capture_window_state();
    configure_native_target(scene_target_);
    scene_frame_active_ = true;
}

void SdlFramePresenter::composite_scene_to_window() {
    if (!scene_frame_active_) {
        throw std::runtime_error(
            "SdlFramePresenter composite_scene_to_window without a scene frame");
    }
    refresh_output_size();
    configure_native_target(nullptr);
    require(SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE),
            "SDL_SetRenderDrawBlendMode composition");
    require(SDL_SetRenderTextureAddressMode(renderer_, SDL_TEXTURE_ADDRESS_CLAMP,
                                            SDL_TEXTURE_ADDRESS_CLAMP),
            "SDL_SetRenderTextureAddressMode clamp");

    if (presentation_.output_size.width > 0 && presentation_.output_size.height > 0) {
        if (presentation_.effective_mode == UpscaleMode::Fsr1) {
            render_easu();
            render_rcas();
        } else {
            SDL_ScaleMode scale_mode = SDL_SCALEMODE_LINEAR;
            switch (presentation_.effective_mode) {
            case UpscaleMode::Linear:
                scale_mode = SDL_SCALEMODE_LINEAR;
                break;
            case UpscaleMode::Nearest:
                scale_mode = SDL_SCALEMODE_NEAREST;
                break;
            case UpscaleMode::PixelArt:
                scale_mode = SDL_SCALEMODE_PIXELART;
                break;
            case UpscaleMode::Fsr1:
                break;
            }
            TextureScaleModeScope scale_scope{scene_target_, scale_mode};
            require(SDL_RenderTexture(renderer_, scene_target_, nullptr, nullptr),
                    "SDL_RenderTexture scene composition");
            switch (presentation_.effective_mode) {
            case UpscaleMode::Linear:
                ++presentation_.linear_composite_count;
                break;
            case UpscaleMode::Nearest:
                ++presentation_.nearest_composite_count;
                break;
            case UpscaleMode::PixelArt:
                ++presentation_.pixel_art_composite_count;
                break;
            case UpscaleMode::Fsr1:
                break;
            }
        }
    }
    scene_frame_active_ = false;
}

void SdlFramePresenter::begin_native_overlay() {
    if (scene_frame_active_ || saved_window_state_ == nullptr || native_overlay_active_) {
        throw std::runtime_error(
            "SdlFramePresenter begin_native_overlay outside composition lifecycle");
    }
    configure_native_target(nullptr);
    require(SDL_SetGPURenderState(renderer_, nullptr),
            "SDL_SetGPURenderState native overlay clear");
    native_overlay_active_ = true;
}

void SdlFramePresenter::end_native_overlay() {
    if (!native_overlay_active_) {
        throw std::runtime_error(
            "SdlFramePresenter end_native_overlay without begin_native_overlay");
    }
    require(SDL_SetGPURenderState(renderer_, nullptr), "SDL_SetGPURenderState post-overlay clear");
    restore_window_state();
    native_overlay_active_ = false;
}

void SdlFramePresenter::present() {
    if (scene_frame_active_ || native_overlay_active_ || saved_window_state_ != nullptr) {
        throw std::runtime_error("SdlFramePresenter present before renderer state restoration");
    }
    require(SDL_RenderPresent(renderer_), "SDL_RenderPresent");
}

void SdlFramePresenter::abort_frame() noexcept {
    if (renderer_ != nullptr && !SDL_SetGPURenderState(renderer_, nullptr)) {
        kLog->error("SDL_SetGPURenderState abort clear failed: {}", SDL_GetError());
    }
    if (saved_window_state_ != nullptr) {
        try {
            restore_window_state();
        } catch (const std::exception& error) {
            kLog->error("SdlFramePresenter abort restore failed: {}", error.what());
        }
    }
    scene_frame_active_ = false;
    native_overlay_active_ = false;
}

void SdlFramePresenter::refresh_output_size() {
    refresh_runtime_dimensions();
    int width = 0;
    int height = 0;
    require(SDL_GetRenderOutputSize(renderer_, &width, &height), "SDL_GetRenderOutputSize");
    const UpscaleExtent current{.width = width, .height = height};
    presentation_.scale_x = static_cast<float>(current.width) /
                            static_cast<float>(presentation_.scene_pixel_size.width);
    presentation_.scale_y = static_cast<float>(current.height) /
                            static_cast<float>(presentation_.scene_pixel_size.height);
    presentation_.sampling_active = current.width != presentation_.scene_pixel_size.width ||
                                    current.height != presentation_.scene_pixel_size.height;
    const bool size_changed = current.width != presentation_.output_size.width ||
                              current.height != presentation_.output_size.height;
    if (size_changed) {
        presentation_.output_size = current;
        recreate_output_target();
        easu_uniforms_dirty_ = true;
        rcas_uniforms_dirty_ = true;
        kLog->info("output_size_recreated output={}x{} generation={}", current.width,
                   current.height, output_resource_generation_);
        update_effective_mode();
    }
}

void SdlFramePresenter::refresh_runtime_dimensions() {
    int window_width = 0;
    int window_height = 0;
    require(SDL_GetWindowSize(window_, &window_width, &window_height), "SDL_GetWindowSize");
    presentation_.window_coordinate_size = {.width = window_width, .height = window_height};
    int pixel_width = 0;
    int pixel_height = 0;
    require(SDL_GetWindowSizeInPixels(window_, &pixel_width, &pixel_height),
            "SDL_GetWindowSizeInPixels");
    presentation_.window_pixel_size = {.width = pixel_width, .height = pixel_height};
    const float density = SDL_GetWindowPixelDensity(window_);
    if (density <= 0.0F) {
        fail("SDL_GetWindowPixelDensity");
    }
    presentation_.window_pixel_density = density;
}

void SdlFramePresenter::recreate_output_target() {
    if (easu_target_ != nullptr) {
        SDL_DestroyTexture(easu_target_);
        easu_target_ = nullptr;
    }
    if (presentation_.output_size.width <= 0 || presentation_.output_size.height <= 0) {
        return;
    }
    easu_target_ =
        SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET,
                          presentation_.output_size.width, presentation_.output_size.height);
    if (easu_target_ == nullptr) {
        fail("SDL_CreateTexture EASU output target");
    }
    require(SDL_SetTextureBlendMode(easu_target_, SDL_BLENDMODE_NONE),
            "SDL_SetTextureBlendMode EASU output target");
    require(SDL_SetTextureScaleMode(easu_target_, SDL_SCALEMODE_LINEAR),
            "SDL_SetTextureScaleMode EASU output target");
    ++output_resource_generation_;
}

void SdlFramePresenter::create_scene_target() {
    scene_target_ =
        SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET,
                          presentation_.logical_size.width, presentation_.logical_size.height);
    if (scene_target_ == nullptr) {
        fail("SDL_CreateTexture logical scene target");
    }
    require(SDL_SetTextureBlendMode(scene_target_, SDL_BLENDMODE_NONE),
            "SDL_SetTextureBlendMode logical scene target");
    require(SDL_SetTextureScaleMode(scene_target_, SDL_SCALEMODE_LINEAR),
            "SDL_SetTextureScaleMode logical scene target");
}

void SdlFramePresenter::create_fsr_states() {
    const SDL_GPUShaderFormat supported = SDL_GetGPUShaderFormats(device_);
    if (supported == SDL_GPU_SHADERFORMAT_INVALID) {
        fail("SDL_GetGPUShaderFormats");
    }

    if ((supported & SDL_GPU_SHADERFORMAT_SPIRV) != 0U) {
        shader_format_ = SDL_GPU_SHADERFORMAT_SPIRV;
    } else if ((supported & SDL_GPU_SHADERFORMAT_DXIL) != 0U) {
        shader_format_ = SDL_GPU_SHADERFORMAT_DXIL;
    } else if ((supported & SDL_GPU_SHADERFORMAT_MSL) != 0U) {
        shader_format_ = SDL_GPU_SHADERFORMAT_MSL;
    } else {
        throw std::runtime_error("No embedded FSR shader format is supported by GPU renderer");
    }

    const auto descriptor_diagnostic = [this](const char*                     operation,
                                              const EmbeddedShaderDescriptor& embedded) {
        const char*        entrypoint = embedded.entrypoint;
        std::ostringstream message;
        message << operation << "\npass=" << fsr_pass_name(embedded.pass)
                << "\nshader_format=" << shader_format_name(embedded.format) << "\nentrypoint="
                << (entrypoint == nullptr ? "<null>"
                                          : (entrypoint[0] == '\0' ? "<empty>" : entrypoint))
                << "\ncode_size=" << embedded.code.size() << "\nrenderer=" << renderer_name()
                << "\ngpu_device=" << gpu_device_name() << "\ngpu_backend=";
        const char* backend = SDL_GetGPUDeviceDriver(device_);
        message << (backend != nullptr && backend[0] != '\0' ? backend : "<empty>")
                << "\nsdl_error=";
        const char* sdl_error = SDL_GetError();
        message << (sdl_error != nullptr && sdl_error[0] != '\0' ? sdl_error : "<empty>");
        return message.str();
    };
    const auto create_shader = [this,
                                &descriptor_diagnostic](const EmbeddedShaderDescriptor& embedded) {
        if (embedded.code.data() == nullptr) {
            throw std::runtime_error(
                descriptor_diagnostic("Invalid embedded FSR shader descriptor", embedded));
        }
        if (embedded.code.empty()) {
            throw std::runtime_error(
                descriptor_diagnostic("Invalid embedded FSR shader descriptor", embedded));
        }
        if (embedded.entrypoint == nullptr || embedded.entrypoint[0] == '\0') {
            throw std::runtime_error(
                descriptor_diagnostic("Invalid embedded FSR shader descriptor", embedded));
        }
        const SDL_GPUShaderCreateInfo info{
            .code_size = embedded.code.size(),
            .code = embedded.code.data(),
            .entrypoint = embedded.entrypoint,
            .format = embedded.format,
            .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
            .num_samplers = embedded.num_samplers,
            .num_storage_textures = 0,
            .num_storage_buffers = 0,
            .num_uniform_buffers = embedded.num_uniform_buffers,
            .props = 0,
        };
        SDL_GPUShader* shader = SDL_CreateGPUShader(device_, &info);
        if (shader == nullptr) {
            throw std::runtime_error(descriptor_diagnostic("SDL_CreateGPUShader failed", embedded));
        }
        return shader;
    };
    const auto& easu = embedded_fsr_shader(FsrPass::Easu, shader_format_);
    const auto& rcas = embedded_fsr_shader(FsrPass::Rcas, shader_format_);
    easu_shader_ = create_shader(easu);
    rcas_shader_ = create_shader(rcas);

    const auto create_state = [this](SDL_GPUShader* shader, const char* pass) {
        const SDL_GPURenderStateCreateInfo info{
            .fragment_shader = shader,
            .num_sampler_bindings = 0,
            .sampler_bindings = nullptr,
            .num_storage_textures = 0,
            .storage_textures = nullptr,
            .num_storage_buffers = 0,
            .storage_buffers = nullptr,
            .props = 0,
        };
        SDL_GPURenderState* state = SDL_CreateGPURenderState(renderer_, &info);
        if (state == nullptr) {
            fail(pass);
        }
        return state;
    };
    easu_state_ = create_state(easu_shader_, "SDL_CreateGPURenderState EASU");
    rcas_state_ = create_state(rcas_shader_, "SDL_CreateGPURenderState RCAS");
    const char* backend = SDL_GetGPUDeviceDriver(device_);
    kLog->info("fsr_states_ready shader_format={} gpu_backend={} easu_entrypoint={} "
               "rcas_entrypoint={}",
               shader_format_name(shader_format_),
               backend != nullptr && backend[0] != '\0' ? backend : "<empty>", easu.entrypoint,
               rcas.entrypoint);
}

void SdlFramePresenter::release_resources() noexcept {
    if (renderer_ != nullptr) {
        if (!SDL_SetGPURenderState(renderer_, nullptr)) {
            kLog->error("SDL_SetGPURenderState shutdown clear failed: {}", SDL_GetError());
        }
    }
    if (easu_state_ != nullptr) {
        SDL_DestroyGPURenderState(easu_state_);
        easu_state_ = nullptr;
    }
    if (rcas_state_ != nullptr) {
        SDL_DestroyGPURenderState(rcas_state_);
        rcas_state_ = nullptr;
    }
    if (device_ != nullptr && easu_shader_ != nullptr) {
        SDL_ReleaseGPUShader(device_, easu_shader_);
        easu_shader_ = nullptr;
    }
    if (device_ != nullptr && rcas_shader_ != nullptr) {
        SDL_ReleaseGPUShader(device_, rcas_shader_);
        rcas_shader_ = nullptr;
    }
    if (easu_target_ != nullptr) {
        SDL_DestroyTexture(easu_target_);
        easu_target_ = nullptr;
    }
    if (scene_target_ != nullptr) {
        SDL_DestroyTexture(scene_target_);
        scene_target_ = nullptr;
    }
}

void SdlFramePresenter::update_effective_mode() {
    const UpscaleMode before = presentation_.effective_mode;
    presentation_.effective_mode = resolve_effective_upscale_mode(
        presentation_.requested.mode, presentation_.logical_size, presentation_.output_size);
    presentation_.fallback_reason.reset();
    if (presentation_.requested.mode == UpscaleMode::Fsr1 &&
        presentation_.effective_mode != UpscaleMode::Fsr1) {
        presentation_.fallback_reason =
            presentation_.output_size.width < presentation_.logical_size.width ||
                    presentation_.output_size.height < presentation_.logical_size.height
                ? UpscaleFallbackReason::OutputSmallerThanScene
                : UpscaleFallbackReason::IdenticalDimensions;
    }
    if (before != presentation_.effective_mode) {
        kLog->info("effective_mode={} requested={} output={}x{}",
                   upscale_mode_name(presentation_.effective_mode),
                   upscale_mode_name(presentation_.requested.mode), presentation_.output_size.width,
                   presentation_.output_size.height);
    }
    if (presentation_.requested.mode == UpscaleMode::Fsr1) {
        if (presentation_.effective_mode == UpscaleMode::Linear) {
            if (fallback_output_size_.width != presentation_.output_size.width ||
                fallback_output_size_.height != presentation_.output_size.height) {
                kLog->warn("fsr_fallback reason={} logical={}x{} output={}x{}",
                           upscale_fallback_reason_name(*presentation_.fallback_reason),
                           presentation_.logical_size.width, presentation_.logical_size.height,
                           presentation_.output_size.width, presentation_.output_size.height);
                fallback_output_size_ = presentation_.output_size;
            }
        } else if (fallback_output_size_.width >= 0) {
            kLog->info("fsr_downscale_fallback_restored output={}x{}",
                       presentation_.output_size.width, presentation_.output_size.height);
            fallback_output_size_ = {.width = -1, .height = -1};
        }
    } else {
        fallback_output_size_ = {.width = -1, .height = -1};
    }
}

void SdlFramePresenter::update_fsr_uniforms() {
    if (presentation_.output_size.width <= 0 || presentation_.output_size.height <= 0) {
        return;
    }
    if (easu_uniforms_dirty_) {
        const float     input_width = static_cast<float>(presentation_.logical_size.width);
        const float     input_height = static_cast<float>(presentation_.logical_size.height);
        const float     output_width = static_cast<float>(presentation_.output_size.width);
        const float     output_height = static_cast<float>(presentation_.output_size.height);
        FsrEasuUniforms uniforms{};
        uniforms.con0 = {float_bits(input_width / output_width),
                         float_bits(input_height / output_height),
                         float_bits(0.5F * input_width / output_width - 0.5F),
                         float_bits(0.5F * input_height / output_height - 0.5F)};
        uniforms.con1 = {float_bits(1.0F / input_width), float_bits(1.0F / input_height),
                         float_bits(1.0F / input_width), float_bits(-1.0F / input_height)};
        uniforms.con2 = {float_bits(-1.0F / input_width), float_bits(2.0F / input_height),
                         float_bits(1.0F / input_width), float_bits(2.0F / input_height)};
        uniforms.con3 = {float_bits(0.0F), float_bits(4.0F / input_height), 0U, 0U};
        uniforms.output_size = {static_cast<uint32_t>(presentation_.output_size.width),
                                static_cast<uint32_t>(presentation_.output_size.height)};
        require(SDL_SetGPURenderStateFragmentUniforms(easu_state_, 0, &uniforms, sizeof(uniforms)),
                "SDL_SetGPURenderStateFragmentUniforms EASU");
        easu_uniforms_dirty_ = false;
    }
    if (rcas_uniforms_dirty_) {
        FsrRcasUniforms uniforms{};
        uniforms.con0[0] = float_bits(rcas_strength(presentation_.requested.fsr_sharpness));
        uniforms.output_size = {static_cast<uint32_t>(presentation_.output_size.width),
                                static_cast<uint32_t>(presentation_.output_size.height)};
        require(SDL_SetGPURenderStateFragmentUniforms(rcas_state_, 0, &uniforms, sizeof(uniforms)),
                "SDL_SetGPURenderStateFragmentUniforms RCAS");
        rcas_uniforms_dirty_ = false;
    }
}

void SdlFramePresenter::render_easu() {
    if (easu_target_ == nullptr) {
        throw std::runtime_error("FSR EASU output target is unavailable");
    }
    update_fsr_uniforms();
    configure_native_target(easu_target_);
    require(SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 0), "SDL_SetRenderDrawColor EASU");
    require(SDL_RenderClear(renderer_), "SDL_RenderClear EASU");
    TextureScaleModeScope scale_scope{scene_target_, SDL_SCALEMODE_LINEAR};
    GpuRenderStateScope   state_scope{renderer_, easu_state_};
    require(SDL_RenderTexture(renderer_, scene_target_, nullptr, nullptr),
            "SDL_RenderTexture EASU");
    ++presentation_.easu_pass_count;
}

void SdlFramePresenter::render_rcas() {
    configure_native_target(nullptr);
    require(SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255), "SDL_SetRenderDrawColor RCAS");
    require(SDL_RenderClear(renderer_), "SDL_RenderClear RCAS");
    TextureScaleModeScope scale_scope{easu_target_, SDL_SCALEMODE_LINEAR};
    GpuRenderStateScope   state_scope{renderer_, rcas_state_};
    require(SDL_RenderTexture(renderer_, easu_target_, nullptr, nullptr), "SDL_RenderTexture RCAS");
    ++presentation_.rcas_pass_count;
}

void SdlFramePresenter::configure_native_target(SDL_Texture* target) {
    require(SDL_SetRenderTarget(renderer_, target), "SDL_SetRenderTarget native target");
    require(SDL_SetRenderLogicalPresentation(renderer_, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED),
            "SDL_SetRenderLogicalPresentation disabled");
    require(SDL_SetRenderViewport(renderer_, nullptr), "SDL_SetRenderViewport native target");
    require(SDL_SetRenderClipRect(renderer_, nullptr), "SDL_SetRenderClipRect native target");
    require(SDL_SetRenderScale(renderer_, 1.0F, 1.0F), "SDL_SetRenderScale native target");
}

void SdlFramePresenter::capture_window_state() {
    if (saved_window_state_ != nullptr) {
        throw std::runtime_error("SdlFramePresenter window state captured twice");
    }
    auto state = std::make_unique<RendererState>();
    state->target = SDL_GetRenderTarget(renderer_);
    require(SDL_GetRenderLogicalPresentation(renderer_, &state->logical_width,
                                             &state->logical_height, &state->logical_mode),
            "SDL_GetRenderLogicalPresentation");
    state->viewport_set = SDL_RenderViewportSet(renderer_);
    require(SDL_GetRenderViewport(renderer_, &state->viewport), "SDL_GetRenderViewport");
    state->clip_enabled = SDL_RenderClipEnabled(renderer_);
    require(SDL_GetRenderClipRect(renderer_, &state->clip), "SDL_GetRenderClipRect");
    require(SDL_GetRenderScale(renderer_, &state->scale_x, &state->scale_y), "SDL_GetRenderScale");
    require(SDL_GetRenderDrawBlendMode(renderer_, &state->blend_mode),
            "SDL_GetRenderDrawBlendMode");
    require(SDL_GetRenderDrawColor(renderer_, &state->draw_r, &state->draw_g, &state->draw_b,
                                   &state->draw_a),
            "SDL_GetRenderDrawColor");
    require(SDL_GetRenderTextureAddressMode(renderer_, &state->address_u, &state->address_v),
            "SDL_GetRenderTextureAddressMode");
    saved_window_state_ = state.release();
}

void SdlFramePresenter::restore_window_state() {
    if (saved_window_state_ == nullptr) {
        throw std::runtime_error("SdlFramePresenter window state was not captured");
    }
    std::unique_ptr<RendererState> state{saved_window_state_};
    saved_window_state_ = nullptr;
    require(SDL_SetRenderTarget(renderer_, state->target), "SDL_SetRenderTarget restore");
    require(SDL_SetRenderLogicalPresentation(renderer_, state->logical_width, state->logical_height,
                                             state->logical_mode),
            "SDL_SetRenderLogicalPresentation restore");
    require(SDL_SetRenderViewport(renderer_, state->viewport_set ? &state->viewport : nullptr),
            "SDL_SetRenderViewport restore");
    require(SDL_SetRenderClipRect(renderer_, state->clip_enabled ? &state->clip : nullptr),
            "SDL_SetRenderClipRect restore");
    require(SDL_SetRenderScale(renderer_, state->scale_x, state->scale_y),
            "SDL_SetRenderScale restore");
    require(SDL_SetRenderDrawBlendMode(renderer_, state->blend_mode),
            "SDL_SetRenderDrawBlendMode restore");
    require(SDL_SetRenderDrawColor(renderer_, state->draw_r, state->draw_g, state->draw_b,
                                   state->draw_a),
            "SDL_SetRenderDrawColor restore");
    require(SDL_SetRenderTextureAddressMode(renderer_, state->address_u, state->address_v),
            "SDL_SetRenderTextureAddressMode restore");
}

void SdlFramePresenter::require(bool result, const char* operation) const {
    if (!result) {
        fail(operation);
    }
}

[[noreturn]] void SdlFramePresenter::fail(const char* operation) const {
    std::ostringstream message;
    message << operation << " failed; renderer=" << renderer_name()
            << " gpu_device=" << gpu_device_name() << " gpu_backend=" << gpu_backend_name()
            << "; SDL error=" << SDL_GetError();
    throw std::runtime_error(message.str());
}

std::string SdlFramePresenter::renderer_name() const {
    const char* name = renderer_ != nullptr ? SDL_GetRendererName(renderer_) : nullptr;
    return name != nullptr ? name : "unavailable";
}

std::string SdlFramePresenter::gpu_device_name() const {
    return property_string(device_, SDL_PROP_GPU_DEVICE_NAME_STRING);
}

std::string SdlFramePresenter::gpu_backend_name() const {
    const char* backend = device_ != nullptr ? SDL_GetGPUDeviceDriver(device_) : nullptr;
    return backend != nullptr && backend[0] != '\0' ? backend : "unavailable";
}

} // namespace d2engine
