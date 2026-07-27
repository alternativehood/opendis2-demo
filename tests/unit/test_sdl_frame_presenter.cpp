#include <gtest/gtest.h>

#include "d2engine/input/input_event.hpp"
#include "d2engine/input/sdl_input_backend.hpp"
#include "d2engine/platform/sdl_context.hpp"
#include "d2engine/render/fsr_embedded_shader.hpp"
#include "d2engine/render/sdl_frame_presenter.hpp"

#include <SDL3/SDL.h>

#include <array>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace d2engine {
namespace {

struct PixelStats {
    std::size_t   pixel_count = 0;
    std::size_t   opaque_pixel_count = 0;
    std::size_t   non_black_pixel_count = 0;
    std::size_t   non_transparent_pixel_count = 0;
    std::uint64_t byte_sum = 0;
};

[[nodiscard]] PixelStats pixel_stats(const SDL_Surface& surface) {
    PixelStats  stats;
    auto* const readable_surface = const_cast<SDL_Surface*>(&surface);
    for (int y = 0; y < surface.h; ++y) {
        for (int x = 0; x < surface.w; ++x) {
            Uint8 red = 0;
            Uint8 green = 0;
            Uint8 blue = 0;
            Uint8 alpha = 0;
            if (!SDL_ReadSurfacePixel(readable_surface, x, y, &red, &green, &blue, &alpha)) {
                throw std::runtime_error(std::string{"SDL_ReadSurfacePixel failed: "} +
                                         SDL_GetError());
            }
            ++stats.pixel_count;
            stats.opaque_pixel_count += alpha == 255U ? 1U : 0U;
            stats.non_black_pixel_count += (red != 0U || green != 0U || blue != 0U) ? 1U : 0U;
            stats.non_transparent_pixel_count += alpha != 0U ? 1U : 0U;
            stats.byte_sum += static_cast<std::uint64_t>(red) + static_cast<std::uint64_t>(green) +
                              static_cast<std::uint64_t>(blue) + static_cast<std::uint64_t>(alpha);
        }
    }
    return stats;
}

[[nodiscard]] std::size_t differing_pixel_count(const SDL_Surface& lhs, const SDL_Surface& rhs) {
    if (lhs.w != rhs.w || lhs.h != rhs.h) {
        throw std::runtime_error("Cannot compare surfaces with different dimensions");
    }
    auto* const readable_lhs = const_cast<SDL_Surface*>(&lhs);
    auto* const readable_rhs = const_cast<SDL_Surface*>(&rhs);
    std::size_t count = 0;
    for (int y = 0; y < lhs.h; ++y) {
        for (int x = 0; x < lhs.w; ++x) {
            Uint8 lhs_red = 0;
            Uint8 lhs_green = 0;
            Uint8 lhs_blue = 0;
            Uint8 lhs_alpha = 0;
            Uint8 rhs_red = 0;
            Uint8 rhs_green = 0;
            Uint8 rhs_blue = 0;
            Uint8 rhs_alpha = 0;
            if (!SDL_ReadSurfacePixel(readable_lhs, x, y, &lhs_red, &lhs_green, &lhs_blue,
                                      &lhs_alpha) ||
                !SDL_ReadSurfacePixel(readable_rhs, x, y, &rhs_red, &rhs_green, &rhs_blue,
                                      &rhs_alpha)) {
                throw std::runtime_error(std::string{"SDL_ReadSurfacePixel failed: "} +
                                         SDL_GetError());
            }
            count += lhs_red != rhs_red || lhs_green != rhs_green || lhs_blue != rhs_blue ||
                             lhs_alpha != rhs_alpha
                         ? 1U
                         : 0U;
        }
    }
    return count;
}

class SdlFramePresenterTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            GTEST_SKIP() << "SDL_Init failed: " << SDL_GetError();
        }
        window_ = SDL_CreateWindow("opendis2_frame_presenter", 16, 16, SDL_WINDOW_HIDDEN);
        if (window_ == nullptr) {
            SDL_Quit();
            GTEST_SKIP() << "SDL_CreateWindow failed: " << SDL_GetError();
        }
        renderer_ = SDL_CreateGPURenderer(nullptr, window_);
        if (renderer_ == nullptr || SDL_GetGPURendererDevice(renderer_) == nullptr) {
            const std::string error = SDL_GetError();
            if (renderer_ != nullptr) {
                SDL_DestroyRenderer(renderer_);
                renderer_ = nullptr;
            }
            SDL_DestroyWindow(window_);
            window_ = nullptr;
            SDL_Quit();
            GTEST_SKIP() << "SDL GPU renderer unavailable: " << error;
        }
        ASSERT_TRUE(
            SDL_SetRenderLogicalPresentation(renderer_, 2, 2, SDL_LOGICAL_PRESENTATION_STRETCH));
        try {
            presenter_ = std::make_unique<SdlFramePresenter>(renderer_, UpscaleExtent{2, 2});
        } catch (const std::exception& error) {
            FAIL() << "GPU renderer created but FSR presenter failed: " << error.what();
        }
    }

    void TearDown() override {
        presenter_.reset();
        if (renderer_ != nullptr) {
            SDL_DestroyRenderer(renderer_);
        }
        if (window_ != nullptr) {
            SDL_DestroyWindow(window_);
        }
        SDL_Quit();
    }

    void draw_test_scene() {
        presenter_->begin_scene_frame();
        const SDL_FRect top_left{0.0F, 0.0F, 1.0F, 1.0F};
        const SDL_FRect top_right{1.0F, 0.0F, 1.0F, 1.0F};
        const SDL_FRect bottom_left{0.0F, 1.0F, 1.0F, 1.0F};
        const SDL_FRect bottom_right{1.0F, 1.0F, 1.0F, 1.0F};
        ASSERT_TRUE(SDL_SetRenderDrawColor(renderer_, 255, 0, 0, 255));
        ASSERT_TRUE(SDL_RenderFillRect(renderer_, &top_left));
        ASSERT_TRUE(SDL_SetRenderDrawColor(renderer_, 0, 255, 0, 255));
        ASSERT_TRUE(SDL_RenderFillRect(renderer_, &top_right));
        ASSERT_TRUE(SDL_SetRenderDrawColor(renderer_, 0, 0, 255, 255));
        ASSERT_TRUE(SDL_RenderFillRect(renderer_, &bottom_left));
        ASSERT_TRUE(SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255));
        ASSERT_TRUE(SDL_RenderFillRect(renderer_, &bottom_right));
    }

    [[nodiscard]] std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> capture() const {
        return {SDL_RenderReadPixels(renderer_, nullptr), SDL_DestroySurface};
    }

    void finish_frame() {
        presenter_->composite_scene_to_window();
        presenter_->begin_native_overlay();
        presenter_->end_native_overlay();
    }

    SDL_Window*                        window_ = nullptr;
    SDL_Renderer*                      renderer_ = nullptr;
    std::unique_ptr<SdlFramePresenter> presenter_;
};

} // namespace

TEST(SdlContextStartup, NonTransparentWindowCreatesGpuRendererAndExposesDevice) {
    try {
        SdlContext context{"opendis2_gpu_startup", 16, 16, false};
        EXPECT_EQ(SDL_GetWindowFlags(context.window()) & SDL_WINDOW_TRANSPARENT, 0U);
        ASSERT_NE(context.renderer(), nullptr);
        EXPECT_NE(SDL_GetGPURendererDevice(context.renderer()), nullptr);
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        if (message.starts_with("SDL_Init failed:") ||
            message.starts_with("SDL_CreateWindow failed:") ||
            message.starts_with("SDL_CreateGPURenderer failed:")) {
            GTEST_SKIP() << "SDL video/GPU renderer unavailable: " << message;
        }
        FAIL() << "SdlContext startup failed: " << message;
    }
}

TEST(FsrEmbeddedShader, UsesTheEntrypointForEachCompiledPayload) {
    struct ExpectedDescriptor {
        FsrPass             pass;
        SDL_GPUShaderFormat format;
        const char*         entrypoint;
    };
    constexpr std::array kExpected{
        ExpectedDescriptor{
            .pass = FsrPass::Easu, .format = SDL_GPU_SHADERFORMAT_SPIRV, .entrypoint = "main"},
        ExpectedDescriptor{
            .pass = FsrPass::Rcas, .format = SDL_GPU_SHADERFORMAT_SPIRV, .entrypoint = "main"},
        ExpectedDescriptor{
            .pass = FsrPass::Easu, .format = SDL_GPU_SHADERFORMAT_DXIL, .entrypoint = "main"},
        ExpectedDescriptor{
            .pass = FsrPass::Rcas, .format = SDL_GPU_SHADERFORMAT_DXIL, .entrypoint = "main"},
        ExpectedDescriptor{
            .pass = FsrPass::Easu, .format = SDL_GPU_SHADERFORMAT_MSL, .entrypoint = "main0"},
        ExpectedDescriptor{
            .pass = FsrPass::Rcas, .format = SDL_GPU_SHADERFORMAT_MSL, .entrypoint = "main0"},
    };

    for (const auto& expected : kExpected) {
        const auto& descriptor = embedded_fsr_shader(expected.pass, expected.format);
        EXPECT_EQ(descriptor.pass, expected.pass);
        EXPECT_EQ(descriptor.format, expected.format);
        EXPECT_STREQ(descriptor.entrypoint, expected.entrypoint);
        EXPECT_FALSE(descriptor.code.empty());
        EXPECT_EQ(descriptor.num_samplers, 1U);
        EXPECT_EQ(descriptor.num_uniform_buffers, 1U);
    }
}

TEST(FsrEmbeddedShader, MslPayloadExportsTheConfiguredFragmentEntrypoint) {
    for (const FsrPass pass : {FsrPass::Easu, FsrPass::Rcas}) {
        const auto& descriptor = embedded_fsr_shader(pass, SDL_GPU_SHADERFORMAT_MSL);
        ASSERT_NE(descriptor.entrypoint, nullptr);
        ASSERT_FALSE(descriptor.code.empty());
        EXPECT_STREQ(descriptor.entrypoint, "main0");

        const std::string_view source{reinterpret_cast<const char*>(descriptor.code.data()),
                                      descriptor.code.size()};
        const std::string      declaration =
            std::string{"fragment main0_out "} + descriptor.entrypoint + "(";
        EXPECT_NE(source.find(declaration), std::string_view::npos)
            << fsr_pass_name(pass) << " MSL source does not export " << declaration;
    }
}

TEST(FsrEmbeddedShader, RcasDeclaresItsPrimaryTextureForEveryMslPayload) {
    const auto& descriptor = embedded_fsr_shader(FsrPass::Rcas, SDL_GPU_SHADERFORMAT_MSL);
    ASSERT_FALSE(descriptor.code.empty());
    const std::string_view source{reinterpret_cast<const char*>(descriptor.code.data()),
                                  descriptor.code.size()};
    EXPECT_NE(source.find("texture2d<float> u_texture [[texture(0)]]"), std::string_view::npos);
}

TEST_F(SdlFramePresenterTest, NearestProducesExactEnlargedColorBlocks) {
    presenter_->set_settings({.mode = UpscaleMode::Nearest});
    draw_test_scene();
    finish_frame();

    auto pixels = capture();
    ASSERT_NE(pixels, nullptr);
    const int sample_x = pixels->w / 4;
    const int sample_y = pixels->h / 4;
    Uint8     red = 0;
    Uint8     green = 0;
    Uint8     blue = 0;
    Uint8     alpha = 0;
    ASSERT_TRUE(
        SDL_ReadSurfacePixel(pixels.get(), sample_x, sample_y, &red, &green, &blue, &alpha));
    EXPECT_EQ(red, 255U);
    EXPECT_EQ(green, 0U);
    EXPECT_EQ(blue, 0U);
    EXPECT_EQ(alpha, 255U);
    presenter_->present();
}

TEST_F(SdlFramePresenterTest, LinearDiffersFromNearestAtABlockBoundary) {
    presenter_->set_settings({.mode = UpscaleMode::Nearest});
    draw_test_scene();
    finish_frame();
    auto nearest = capture();
    ASSERT_NE(nearest, nullptr);

    presenter_->present();
    presenter_->set_settings({.mode = UpscaleMode::Linear});
    draw_test_scene();
    finish_frame();
    auto linear = capture();
    ASSERT_NE(linear, nullptr);

    const int sample_x = linear->w / 2 - 1;
    const int sample_y = linear->h / 4;
    Uint8     nearest_red = 0;
    Uint8     nearest_green = 0;
    Uint8     nearest_blue = 0;
    Uint8     nearest_alpha = 0;
    Uint8     linear_red = 0;
    Uint8     linear_green = 0;
    Uint8     linear_blue = 0;
    Uint8     linear_alpha = 0;
    ASSERT_TRUE(SDL_ReadSurfacePixel(nearest.get(), sample_x, sample_y, &nearest_red,
                                     &nearest_green, &nearest_blue, &nearest_alpha));
    ASSERT_TRUE(SDL_ReadSurfacePixel(linear.get(), sample_x, sample_y, &linear_red, &linear_green,
                                     &linear_blue, &linear_alpha));
    EXPECT_NE(nearest_red, linear_red);
    EXPECT_NE(nearest_green, linear_green);
    presenter_->present();
}

TEST_F(SdlFramePresenterTest, ModeSwitchingKeepsLogicalInputMappingAndFixedTarget) {
    const std::size_t generation = presenter_->output_resource_generation();
    for (const UpscaleMode mode :
         {UpscaleMode::PixelArt, UpscaleMode::Fsr1, UpscaleMode::Nearest, UpscaleMode::Linear}) {
        presenter_->set_settings({.mode = mode});
        draw_test_scene();
        finish_frame();
        EXPECT_EQ(SDL_GetRenderTarget(renderer_), nullptr);
        int                             logical_width = 0;
        int                             logical_height = 0;
        SDL_RendererLogicalPresentation logical_mode = SDL_LOGICAL_PRESENTATION_DISABLED;
        ASSERT_TRUE(SDL_GetRenderLogicalPresentation(renderer_, &logical_width, &logical_height,
                                                     &logical_mode));
        EXPECT_EQ(logical_width, 2);
        EXPECT_EQ(logical_height, 2);
        EXPECT_EQ(logical_mode, SDL_LOGICAL_PRESENTATION_STRETCH);
        EXPECT_EQ(presenter_->presentation().logical_size.width, 2);
        EXPECT_EQ(presenter_->presentation().logical_size.height, 2);
        presenter_->present();
    }
    EXPECT_EQ(presenter_->output_resource_generation(), generation);

    SDL_Event event{};
    event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.x = 8.0F;
    event.motion.y = 8.0F;
    const auto input = SdlInputBackend::translate(event, renderer_);
    ASSERT_TRUE(input.has_value());
    ASSERT_TRUE(std::holds_alternative<PointerMoved>(*input));
    EXPECT_EQ(std::get<PointerMoved>(*input).x, 1);
    EXPECT_EQ(std::get<PointerMoved>(*input).y, 1);
}

TEST_F(SdlFramePresenterTest, FsrUsesBothStatesAndSharpnessDoesNotRecreateTargets) {
    ASSERT_TRUE(presenter_->fsr_states_ready());
    presenter_->set_settings({.mode = UpscaleMode::Fsr1, .fsr_sharpness = 0.0F});
    draw_test_scene();
    finish_frame();
    auto no_sharpening = capture();
    ASSERT_NE(no_sharpening, nullptr);
    const PixelStats no_sharpening_stats = pixel_stats(*no_sharpening);
    EXPECT_GT(no_sharpening_stats.non_black_pixel_count, 0U);
    EXPECT_GT(no_sharpening_stats.non_transparent_pixel_count, 0U);
    EXPECT_EQ(presenter_->presentation().easu_pass_count, 1U);
    EXPECT_EQ(presenter_->presentation().rcas_pass_count, 1U);
    presenter_->present();

    const std::size_t generation = presenter_->output_resource_generation();
    presenter_->set_settings({.mode = UpscaleMode::Fsr1, .fsr_sharpness = 1.0F});
    draw_test_scene();
    finish_frame();
    auto maximum_sharpening = capture();
    ASSERT_NE(maximum_sharpening, nullptr);
    const PixelStats maximum_sharpening_stats = pixel_stats(*maximum_sharpening);
    EXPECT_GT(maximum_sharpening_stats.non_black_pixel_count, 0U);
    EXPECT_GT(maximum_sharpening_stats.non_transparent_pixel_count, 0U);
    EXPECT_GT(differing_pixel_count(*no_sharpening, *maximum_sharpening), 0U);
    EXPECT_EQ(presenter_->output_resource_generation(), generation);
    EXPECT_EQ(presenter_->presentation().effective_mode, UpscaleMode::Fsr1);
    EXPECT_EQ(presenter_->presentation().easu_pass_count, 2U);
    EXPECT_EQ(presenter_->presentation().rcas_pass_count, 2U);
    presenter_->present();
}

TEST_F(SdlFramePresenterTest, MetalCreatesEasuAndRcasShadersAndRenderStatesFromMsl) {
    SDL_GPUDevice* device = SDL_GetGPURendererDevice(renderer_);
    ASSERT_NE(device, nullptr);
    const char* backend = SDL_GetGPUDeviceDriver(device);
    if (backend == nullptr || std::strcmp(backend, "metal") != 0) {
        GTEST_SKIP() << "Metal GPU backend unavailable: "
                     << (backend != nullptr ? backend : "<null>");
    }
    if ((SDL_GetGPUShaderFormats(device) & SDL_GPU_SHADERFORMAT_MSL) == 0U) {
        GTEST_SKIP() << "Metal GPU renderer does not report MSL shader support";
    }

    const auto create_shader = [device](const EmbeddedShaderDescriptor& descriptor) {
        const SDL_GPUShaderCreateInfo info{
            .code_size = descriptor.code.size(),
            .code = descriptor.code.data(),
            .entrypoint = descriptor.entrypoint,
            .format = descriptor.format,
            .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
            .num_samplers = descriptor.num_samplers,
            .num_storage_textures = 0,
            .num_storage_buffers = 0,
            .num_uniform_buffers = descriptor.num_uniform_buffers,
            .props = 0,
        };
        return SDL_CreateGPUShader(device, &info);
    };
    const auto create_state = [this](SDL_GPUShader* shader) {
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
        return SDL_CreateGPURenderState(renderer_, &info);
    };

    const auto&    easu = embedded_fsr_shader(FsrPass::Easu, SDL_GPU_SHADERFORMAT_MSL);
    SDL_GPUShader* easu_shader = create_shader(easu);
    ASSERT_NE(easu_shader, nullptr) << "EASU MSL shader creation failed: " << SDL_GetError();
    const auto&    rcas = embedded_fsr_shader(FsrPass::Rcas, SDL_GPU_SHADERFORMAT_MSL);
    SDL_GPUShader* rcas_shader = create_shader(rcas);
    if (rcas_shader == nullptr) {
        SDL_ReleaseGPUShader(device, easu_shader);
        FAIL() << "RCAS MSL shader creation failed: " << SDL_GetError();
    }

    SDL_GPURenderState* easu_state = create_state(easu_shader);
    if (easu_state == nullptr) {
        SDL_ReleaseGPUShader(device, rcas_shader);
        SDL_ReleaseGPUShader(device, easu_shader);
        FAIL() << "EASU MSL render-state creation failed: " << SDL_GetError();
    }
    SDL_GPURenderState* rcas_state = create_state(rcas_shader);
    if (rcas_state == nullptr) {
        SDL_DestroyGPURenderState(easu_state);
        SDL_ReleaseGPUShader(device, rcas_shader);
        SDL_ReleaseGPUShader(device, easu_shader);
        FAIL() << "RCAS MSL render-state creation failed: " << SDL_GetError();
    }

    SDL_DestroyGPURenderState(rcas_state);
    SDL_DestroyGPURenderState(easu_state);
    SDL_ReleaseGPUShader(device, rcas_shader);
    SDL_ReleaseGPUShader(device, easu_shader);
}

} // namespace d2engine
