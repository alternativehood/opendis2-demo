#include <gtest/gtest.h>

#include "d2engine/render/text/text_box_renderer.hpp"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace d2engine {
namespace {

class TextBoxRendererTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            GTEST_SKIP() << "SDL_Init failed: " << SDL_GetError();
        }
        sdl_initialized_ = true;
        if (!TTF_Init()) {
            GTEST_SKIP() << "TTF_Init failed: " << SDL_GetError();
        }
        ttf_initialized_ = true;
        window_ = SDL_CreateWindow("text-box-test", kWidth, kHeight, SDL_WINDOW_HIDDEN);
        if (window_ == nullptr) {
            GTEST_SKIP() << "SDL_CreateWindow failed: " << SDL_GetError();
        }
        renderer_ = SDL_CreateRenderer(window_, nullptr);
        if (renderer_ == nullptr) {
            GTEST_SKIP() << "SDL_CreateRenderer failed: " << SDL_GetError();
        }
        SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
        SDL_RenderClear(renderer_);
    }

    void TearDown() override {
        if (renderer_ != nullptr) {
            SDL_DestroyRenderer(renderer_);
        }
        if (window_ != nullptr) {
            SDL_DestroyWindow(window_);
        }
        if (ttf_initialized_) {
            TTF_Quit();
        }
        if (sdl_initialized_) {
            SDL_Quit();
        }
    }

    struct Bounds {
        int min_x = std::numeric_limits<int>::max();
        int min_y = std::numeric_limits<int>::max();
        int max_x = std::numeric_limits<int>::min();
        int max_y = std::numeric_limits<int>::min();

        [[nodiscard]] bool empty() const { return min_x > max_x || min_y > max_y; }
    };

    [[nodiscard]] Bounds rendered_bounds() const {
        std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> pixels(
            SDL_RenderReadPixels(renderer_, nullptr), SDL_DestroySurface);
        EXPECT_NE(pixels.get(), nullptr);
        Bounds bounds;
        if (pixels == nullptr) {
            return bounds;
        }
        for (int y = 0; y < pixels->h; ++y) {
            for (int x = 0; x < pixels->w; ++x) {
                std::uint8_t r = 0;
                std::uint8_t g = 0;
                std::uint8_t b = 0;
                std::uint8_t a = 0;
                if (!SDL_ReadSurfacePixel(pixels.get(), x, y, &r, &g, &b, &a)) {
                    continue;
                }
                if (a != 0 && (r < 245 || g < 245 || b < 245)) {
                    bounds.min_x = std::min(bounds.min_x, x);
                    bounds.min_y = std::min(bounds.min_y, y);
                    bounds.max_x = std::max(bounds.max_x, x);
                    bounds.max_y = std::max(bounds.max_y, y);
                }
            }
        }
        return bounds;
    }

    void clear() const {
        SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
        SDL_RenderClear(renderer_);
    }

    static constexpr int kWidth = 240;
    static constexpr int kHeight = 120;

    SDL_Window*   window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    bool          sdl_initialized_ = false;
    bool          ttf_initialized_ = false;
};

TEST_F(TextBoxRendererTest, CenteredSingleLineTextStaysInsideRect) {
    TextBoxRenderer text(renderer_);
    const Rect      rect{.x = 100.0f, .y = 50.0f, .w = 80.0f, .h = 20.0f};

    text.draw(TextBox{.rect = rect,
                      .text = "60/100",
                      .style = {.font_size = 12.0f,
                                .color = Color{.r = 0, .g = 0, .b = 0, .a = 255},
                                .align = TextAlign::Center,
                                .valign = TextVAlign::Middle,
                                .overflow = TextOverflowMode::Clip}});

    const Bounds bounds = rendered_bounds();
    ASSERT_FALSE(bounds.empty());
    EXPECT_GE(bounds.min_x, static_cast<int>(rect.x));
    EXPECT_GE(bounds.min_y, static_cast<int>(rect.y));
    EXPECT_LT(bounds.max_x, static_cast<int>(rect.right()));
    EXPECT_LT(bounds.max_y, static_cast<int>(rect.bottom()));
    EXPECT_GT(bounds.min_x, static_cast<int>(rect.x) + 8);
    const int text_center = (bounds.min_x + bounds.max_x) / 2;
    EXPECT_NEAR(text_center, static_cast<int>(rect.x + rect.w * 0.5f), 16);
}

TEST_F(TextBoxRendererTest, WrappedTextRespectsWidth) {
    TextBoxRenderer text(renderer_);
    const Rect      rect{.x = 10.0f, .y = 10.0f, .w = 64.0f, .h = 80.0f};

    text.draw(TextBox{.rect = rect,
                      .text = "long wrapped text stays narrow",
                      .style = {.font_size = 12.0f,
                                .color = Color{.r = 0, .g = 0, .b = 0, .a = 255},
                                .wrap = TextWrapMode::Word,
                                .overflow = TextOverflowMode::Clip}});

    const Bounds bounds = rendered_bounds();
    ASSERT_FALSE(bounds.empty());
    EXPECT_GE(bounds.min_x, static_cast<int>(rect.x));
    EXPECT_LT(bounds.max_x, static_cast<int>(rect.right()));
}

TEST_F(TextBoxRendererTest, EllipsisOverflowCurrentlyClipsInsideRect) {
    TextBoxRenderer text(renderer_);
    const Rect      rect{.x = 20.0f, .y = 20.0f, .w = 48.0f, .h = 20.0f};

    text.draw(TextBox{.rect = rect,
                      .text = "this is too long",
                      .style = {.font_size = 12.0f,
                                .color = Color{.r = 0, .g = 0, .b = 0, .a = 255},
                                .overflow = TextOverflowMode::Ellipsis}});

    const Bounds bounds = rendered_bounds();
    ASSERT_FALSE(bounds.empty());
    EXPECT_GE(bounds.min_x, static_cast<int>(rect.x));
    EXPECT_LT(bounds.max_x, static_cast<int>(rect.right()));
}

TEST_F(TextBoxRendererTest, UnsupportedFontFaceFailsClearly) {
    TextBoxRenderer text(renderer_);
    EXPECT_THROW(text.draw(TextBox{.rect = {.x = 0.0f, .y = 0.0f, .w = 80.0f, .h = 20.0f},
                                   .text = "x",
                                   .style = {.font_face = "No Such Face"}}),
                 std::runtime_error);
}

TEST_F(TextBoxRendererTest, ShrinkToFitCacheKeyIncludesHeight) {
    // Two boxes with same text/width but different heights must not share a cache entry
    // because the shrink-to-fit algorithm selects font size based on both dimensions.
    TextBoxRenderer text(renderer_);
    const Rect      tall_rect{.x = 10.0f, .y = 10.0f, .w = 120.0f, .h = 80.0f};
    const Rect      short_rect{.x = 10.0f, .y = 100.0f, .w = 120.0f, .h = 16.0f};

    text.draw(TextBox{.rect = tall_rect,
                      .text = "Tall box shrink",
                      .style = {.font_size = 24.0f,
                                .color = Color{.r = 0, .g = 0, .b = 0, .a = 255},
                                .overflow = TextOverflowMode::ShrinkToFit}});

    const Bounds tall_bounds = rendered_bounds();
    ASSERT_FALSE(tall_bounds.empty());

    clear();

    text.draw(TextBox{.rect = short_rect,
                      .text = "Tall box shrink",
                      .style = {.font_size = 24.0f,
                                .color = Color{.r = 0, .g = 0, .b = 0, .a = 255},
                                .overflow = TextOverflowMode::ShrinkToFit}});

    const Bounds short_bounds = rendered_bounds();
    ASSERT_FALSE(short_bounds.empty());

    // The tall box should have taller rendered height than what fits in short rect.
    // If cache incorrectly ignored height, the second draw would reuse tall texture.
    EXPECT_LE(short_bounds.max_y - short_bounds.min_y + 1, static_cast<int>(short_rect.h));
}

} // namespace
} // namespace d2engine
