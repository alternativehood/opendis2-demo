#include <gtest/gtest.h>

#include "d2engine/input/input_event.hpp"
#include "d2engine/input/sdl_input_backend.hpp"

#include <SDL3/SDL.h>

#include <optional>
#include <string>

namespace d2engine {

namespace {

class SdlRendererFixture : public ::testing::Test {
protected:
    void SetUp() override {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            GTEST_SKIP() << "SDL video init failed: " << SDL_GetError();
        }
        window_ = SDL_CreateWindow("opendis2_test_coords", 800, 600, SDL_WINDOW_HIDDEN);
        if (window_ == nullptr) {
            SDL_Quit();
            GTEST_SKIP() << "SDL window creation failed: " << SDL_GetError();
        }
        renderer_ = SDL_CreateRenderer(window_, nullptr);
        if (renderer_ == nullptr) {
            SDL_DestroyWindow(window_);
            window_ = nullptr;
            SDL_Quit();
            GTEST_SKIP() << "SDL renderer creation failed: " << SDL_GetError();
        }

        if (!SDL_SetRenderLogicalPresentation(renderer_, 400, 300,
                                              SDL_LOGICAL_PRESENTATION_STRETCH)) {
            SDL_DestroyRenderer(renderer_);
            renderer_ = nullptr;
            SDL_DestroyWindow(window_);
            window_ = nullptr;
            SDL_Quit();
            GTEST_SKIP() << "SDL logical presentation failed: " << SDL_GetError();
        }
    }

    void TearDown() override {
        if (renderer_) {
            SDL_DestroyRenderer(renderer_);
            renderer_ = nullptr;
        }
        if (window_) {
            SDL_DestroyWindow(window_);
            window_ = nullptr;
        }
        SDL_Quit();
    }

    SDL_Window*   window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
};

} // namespace

TEST_F(SdlRendererFixture, PointerPressedConvertsWindowToLogicalCoordinates) {
    // Window (200, 150) → logical (100, 75) with 2x scale
    SDL_Event event;
    event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = 200.0f;
    event.button.y = 150.0f;

    auto result = SdlInputBackend::translate(event, renderer_);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<PointerPressed>(*result));
    const auto& pressed = std::get<PointerPressed>(*result);
    EXPECT_EQ(pressed.button, PointerButton::Left);
    EXPECT_EQ(pressed.x, 100);
    EXPECT_EQ(pressed.y, 75);
}

TEST_F(SdlRendererFixture, PointerMovedConvertsWindowToLogicalCoordinates) {
    SDL_Event event;
    event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.x = 400.0f;
    event.motion.y = 300.0f;

    auto result = SdlInputBackend::translate(event, renderer_);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<PointerMoved>(*result));
    const auto& moved = std::get<PointerMoved>(*result);
    EXPECT_EQ(moved.x, 200);
    EXPECT_EQ(moved.y, 150);
}

TEST_F(SdlRendererFixture, PointerReleasedConvertsWindowToLogicalCoordinates) {
    SDL_Event event;
    event.type = SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.button = SDL_BUTTON_RIGHT;
    event.button.x = 100.0f;
    event.button.y = 50.0f;

    auto result = SdlInputBackend::translate(event, renderer_);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<PointerReleased>(*result));
    const auto& released = std::get<PointerReleased>(*result);
    EXPECT_EQ(released.button, PointerButton::Right);
    EXPECT_EQ(released.x, 50);
    EXPECT_EQ(released.y, 25);
}

TEST_F(SdlRendererFixture, NullRendererReturnsNulloptForPointerPress) {
    SDL_Event event;
    event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = 100.0f;
    event.button.y = 200.0f;

    EXPECT_FALSE(SdlInputBackend::translate(event, nullptr).has_value());
}

TEST_F(SdlRendererFixture, UnknownMouseButtonReturnsNullopt) {
    SDL_Event event;
    event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.button = 99;

    EXPECT_FALSE(SdlInputBackend::translate(event, renderer_).has_value());
}

TEST_F(SdlRendererFixture, KeyboardStillWorksWithoutRenderer) {
    SDL_Event event;
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.key = SDLK_ESCAPE;
    event.key.repeat = false;
    event.key.mod = 0;

    auto result = SdlInputBackend::translate(event, nullptr);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<KeyPressed>(*result));
    EXPECT_EQ(std::get<KeyPressed>(*result).key, Key::Escape);
}

} // namespace d2engine
