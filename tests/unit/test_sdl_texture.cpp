#include <gtest/gtest.h>
#include "d2engine/render/sdl_texture.hpp"
#include <SDL3/SDL.h>

#include <cstring>
#include <vector>

using namespace d2engine;

TEST(SdlTexture, CreateWithNullRendererReturnsNull) {
    SdlTexture const tex = create_sdl_texture(nullptr, 1, 1, nullptr, 4);
    EXPECT_EQ(tex.get(), nullptr);
}

class SdlTextureTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            GTEST_SKIP() << "SDL_Init failed: " << SDL_GetError();
        }
        window_ = SDL_CreateWindow("test", 1, 1, SDL_WINDOW_HIDDEN);
        if (window_ == nullptr) {
            SDL_Quit();
            GTEST_SKIP() << "SDL_CreateWindow failed: " << SDL_GetError();
        }
        renderer_ = SDL_CreateRenderer(window_, nullptr);
        if (renderer_ == nullptr) {
            SDL_DestroyWindow(window_);
            SDL_Quit();
            GTEST_SKIP() << "SDL_CreateRenderer failed: " << SDL_GetError();
        }
    }

    void TearDown() override {
        if (renderer_ != nullptr) {
            SDL_DestroyRenderer(renderer_);
        }
        if (window_ != nullptr) {
            SDL_DestroyWindow(window_);
        }
        SDL_Quit();
    }

    SDL_Window*   window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
};

TEST_F(SdlTextureTest, CreateValidTexture) {
    std::vector<uint8_t> pixels = {255, 0, 255, 255};
    SdlTexture const     tex = create_sdl_texture(renderer_, 1, 1, pixels.data(), 4);
    EXPECT_NE(tex.get(), nullptr);
}

TEST_F(SdlTextureTest, DestroyOnScopeExit) {
    {
        std::vector<uint8_t> pixels = {255, 0, 255, 255};
        SdlTexture const     tex = create_sdl_texture(renderer_, 1, 1, pixels.data(), 4);
        EXPECT_NE(tex.get(), nullptr);
    }
    // Texture should be destroyed when tex goes out of scope
    SUCCEED();
}

TEST_F(SdlTextureTest, MoveSemantics) {
    std::vector<uint8_t> pixels = {255, 0, 255, 255};
    SdlTexture           tex1 = create_sdl_texture(renderer_, 1, 1, pixels.data(), 4);
    EXPECT_NE(tex1.get(), nullptr);

    SdlTexture const tex2 = std::move(tex1);
    // cppcheck-suppress accessMoved
    // NOLINTNEXTLINE(clang-analyzer-cplusplus.Move) — intentional test of moved-from state
    EXPECT_EQ(tex1.get(), nullptr);
    EXPECT_NE(tex2.get(), nullptr);
}

// ─── rgba_buffer_from_sdl_surface tests ──────────────────────────────────────

TEST_F(SdlTextureTest, RgbaBufferFromSdlSurface_NullSurfaceReturnsEmpty) {
    auto buf = rgba_buffer_from_sdl_surface(nullptr);
    EXPECT_EQ(buf.width, 0u);
    EXPECT_EQ(buf.height, 0u);
    EXPECT_TRUE(buf.rgba.empty());
}

TEST_F(SdlTextureTest, RgbaBufferFromSdlSurface_Rgba32) {
    SDL_Surface* surface = SDL_CreateSurface(2, 2, SDL_PIXELFORMAT_RGBA32);
    ASSERT_NE(surface, nullptr);

    std::vector<uint8_t> pixels = {255, 0, 0,   255, 0,   255, 0,   255,
                                   0,   0, 255, 255, 128, 128, 128, 255};
    SDL_LockSurface(surface);
    std::memcpy(surface->pixels, pixels.data(), pixels.size());
    SDL_UnlockSurface(surface);

    auto buf = rgba_buffer_from_sdl_surface(surface);
    SDL_DestroySurface(surface);

    ASSERT_EQ(buf.width, 2u);
    ASSERT_EQ(buf.height, 2u);
    ASSERT_EQ(buf.rgba.size(), 16u);
    EXPECT_EQ(buf.rgba[0], 255);
    EXPECT_EQ(buf.rgba[1], 0);
    EXPECT_EQ(buf.rgba[2], 0);
    EXPECT_EQ(buf.rgba[3], 255);
    EXPECT_EQ(buf.rgba[4], 0);
    EXPECT_EQ(buf.rgba[5], 255);
    EXPECT_EQ(buf.rgba[6], 0);
    EXPECT_EQ(buf.rgba[7], 255);
    EXPECT_EQ(buf.rgba[8], 0);
    EXPECT_EQ(buf.rgba[9], 0);
    EXPECT_EQ(buf.rgba[10], 255);
    EXPECT_EQ(buf.rgba[11], 255);
    EXPECT_EQ(buf.rgba[12], 128);
    EXPECT_EQ(buf.rgba[13], 128);
    EXPECT_EQ(buf.rgba[14], 128);
    EXPECT_EQ(buf.rgba[15], 255);
}

TEST_F(SdlTextureTest, RgbaBufferFromSdlSurface_FormatConversionBgra32) {
    // Create a 1x1 BGRA32 surface with known pixel data
    SDL_Surface* surface = SDL_CreateSurface(1, 1, SDL_PIXELFORMAT_BGRA32);
    ASSERT_NE(surface, nullptr);

    // BGRA pixel bytes: B=255, G=0, R=0, A=255 (blue)
    std::vector<uint8_t> pixel = {255, 0, 0, 255};
    SDL_LockSurface(surface);
    std::memcpy(surface->pixels, pixel.data(), 4);
    SDL_UnlockSurface(surface);

    auto buf = rgba_buffer_from_sdl_surface(surface);
    SDL_DestroySurface(surface);

    ASSERT_EQ(buf.width, 1u);
    ASSERT_EQ(buf.height, 1u);
    // After format conversion: RGBA32 should have R=0, G=0, B=255, A=255
    EXPECT_EQ(buf.rgba[0], 0);
    EXPECT_EQ(buf.rgba[1], 0);
    EXPECT_EQ(buf.rgba[2], 255);
    EXPECT_EQ(buf.rgba[3], 255);
}

TEST_F(SdlTextureTest, RgbaBufferFromSdlSurface_Rgb24ToRgba32) {
    // Create a 1x1 RGB24 surface (3 bytes/pixel, no alpha)
    SDL_Surface* surface = SDL_CreateSurface(1, 1, SDL_PIXELFORMAT_RGB24);
    ASSERT_NE(surface, nullptr);

    // RGB byte: R=255, G=128, B=0
    std::vector<uint8_t> pixel = {255, 128, 0};
    SDL_LockSurface(surface);
    std::memcpy(surface->pixels, pixel.data(), 3);
    SDL_UnlockSurface(surface);

    auto buf = rgba_buffer_from_sdl_surface(surface);
    SDL_DestroySurface(surface);

    ASSERT_EQ(buf.width, 1u);
    ASSERT_EQ(buf.height, 1u);
    // After format conversion: R=255, G=128, B=0, A=255 (full alpha)
    EXPECT_EQ(buf.rgba[0], 255);
    EXPECT_EQ(buf.rgba[1], 128);
    EXPECT_EQ(buf.rgba[2], 0);
    EXPECT_EQ(buf.rgba[3], 255);
}

// ─── create_sdl_texture row-by-row copy test ─────────────────────────────────

TEST_F(SdlTextureTest, CreateSdlTexture_RowByRowCopyRenderAndReadback) {
    // Create a 2x2 streaming texture via create_sdl_texture with known pixel data
    std::vector<uint8_t> pixels = {255, 0, 0,   255, 0,   255, 0,   255,
                                   0,   0, 255, 255, 128, 128, 128, 255};
    SdlTexture           tex = create_sdl_texture(renderer_, 2, 2, pixels.data(), 8);
    ASSERT_NE(tex.get(), nullptr);

    // Render to a target texture to read back pixels
    SDL_Texture* target =
        SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET, 2, 2);
    if (target == nullptr) {
        GTEST_SKIP() << "Target texture not supported: " << SDL_GetError();
    }

    SDL_SetRenderTarget(renderer_, target);
    SDL_RenderTexture(renderer_, tex.get(), nullptr, nullptr);

    SDL_Surface* readback = SDL_RenderReadPixels(renderer_, nullptr);
    ASSERT_NE(readback, nullptr);

    // Verify pixels from the readback surface
    SDL_LockSurface(readback);
    const auto* data = static_cast<const uint8_t*>(readback->pixels);
    EXPECT_EQ(data[0], 255);
    EXPECT_EQ(data[1], 0);
    EXPECT_EQ(data[2], 0);
    EXPECT_EQ(data[3], 255);
    EXPECT_EQ(data[4], 0);
    EXPECT_EQ(data[5], 255);
    EXPECT_EQ(data[6], 0);
    EXPECT_EQ(data[7], 255);
    EXPECT_EQ(data[8], 0);
    EXPECT_EQ(data[9], 0);
    EXPECT_EQ(data[10], 255);
    EXPECT_EQ(data[11], 255);
    EXPECT_EQ(data[12], 128);
    EXPECT_EQ(data[13], 128);
    EXPECT_EQ(data[14], 128);
    EXPECT_EQ(data[15], 255);
    SDL_UnlockSurface(readback);

    SDL_DestroySurface(readback);
    SDL_SetRenderTarget(renderer_, nullptr);
    SDL_DestroyTexture(target);
}
