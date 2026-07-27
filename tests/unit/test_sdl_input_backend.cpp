#include <gtest/gtest.h>

#include "d2engine/input/input_event.hpp"
#include "d2engine/input/sdl_input_backend.hpp"

#include <SDL3/SDL.h>

#include <optional>

namespace d2engine {

// ── SDL → InputEvent translation tests ─────────────────────────────────
//
// These tests verify the pure SDL-to-InputEvent mapping.
// Pointer coordinate conversion requires an SDL renderer; keyboard
// translation works without one.

TEST(SdlInputBackend, KeyDownTranslatesToKeyPressed) {
    SDL_Event event;
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.key = SDLK_ESCAPE;
    event.key.repeat = false;
    event.key.mod = 0;

    auto result = SdlInputBackend::translate(event, nullptr);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<KeyPressed>(*result));
    EXPECT_EQ(std::get<KeyPressed>(*result).key, Key::Escape);
    EXPECT_FALSE(std::get<KeyPressed>(*result).repeat);
}

TEST(SdlInputBackend, KeyUpTranslatesToKeyReleased) {
    SDL_Event event;
    event.type = SDL_EVENT_KEY_UP;
    event.key.key = SDLK_SPACE;
    event.key.mod = 0;

    auto result = SdlInputBackend::translate(event, nullptr);
    ASSERT_TRUE(result.has_value());
    ASSERT_TRUE(std::holds_alternative<KeyReleased>(*result));
    EXPECT_EQ(std::get<KeyReleased>(*result).key, Key::Space);
}

TEST(SdlInputBackend, ModifierCtrlTranslatesCorrectly) {
    SDL_Event event;
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.key = SDLK_S;
    event.key.repeat = false;
    event.key.mod = SDL_KMOD_CTRL;

    auto result = SdlInputBackend::translate(event, nullptr);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(has_modifier(std::get<KeyPressed>(*result).modifiers, KeyModifier::Ctrl));
    EXPECT_FALSE(has_modifier(std::get<KeyPressed>(*result).modifiers, KeyModifier::Shift));
    EXPECT_FALSE(has_modifier(std::get<KeyPressed>(*result).modifiers, KeyModifier::Alt));
    EXPECT_FALSE(has_modifier(std::get<KeyPressed>(*result).modifiers, KeyModifier::Gui));
}

TEST(SdlInputBackend, ModifierShiftTranslatesCorrectly) {
    SDL_Event event;
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.key = SDLK_D;
    event.key.repeat = false;
    event.key.mod = SDL_KMOD_SHIFT;

    auto result = SdlInputBackend::translate(event, nullptr);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(has_modifier(std::get<KeyPressed>(*result).modifiers, KeyModifier::Shift));
}

TEST(SdlInputBackend, ModifierAltTranslatesCorrectly) {
    SDL_Event event;
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.key = SDLK_A;
    event.key.repeat = false;
    event.key.mod = SDL_KMOD_ALT;

    auto result = SdlInputBackend::translate(event, nullptr);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(has_modifier(std::get<KeyPressed>(*result).modifiers, KeyModifier::Alt));
}

TEST(SdlInputBackend, ModifierGuiTranslatesCorrectly) {
    SDL_Event event;
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.key = SDLK_LEFT;
    event.key.repeat = false;
    event.key.mod = SDL_KMOD_GUI;

    auto result = SdlInputBackend::translate(event, nullptr);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(has_modifier(std::get<KeyPressed>(*result).modifiers, KeyModifier::Gui));
}

TEST(SdlInputBackend, ModifierCombinationTranslatesCorrectly) {
    SDL_Event event;
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.key = SDLK_Z;
    event.key.repeat = false;
    event.key.mod = static_cast<SDL_Keymod>(SDL_KMOD_CTRL | SDL_KMOD_SHIFT);

    auto result = SdlInputBackend::translate(event, nullptr);
    ASSERT_TRUE(result.has_value());
    const auto& pressed = std::get<KeyPressed>(*result);
    EXPECT_TRUE(has_modifier(pressed.modifiers, KeyModifier::Ctrl));
    EXPECT_TRUE(has_modifier(pressed.modifiers, KeyModifier::Shift));
}

TEST(SdlInputBackend, PointerPressWithNoRendererReturnsNullopt) {
    SDL_Event event;
    event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = 100;
    event.button.y = 200;

    // No renderer → cannot convert coordinates → nullopt
    EXPECT_FALSE(SdlInputBackend::translate(event, nullptr).has_value());
}

TEST(SdlInputBackend, PointerReleaseWithNoRendererReturnsNullopt) {
    SDL_Event event;
    event.type = SDL_EVENT_MOUSE_BUTTON_UP;
    event.button.button = SDL_BUTTON_RIGHT;

    EXPECT_FALSE(SdlInputBackend::translate(event, nullptr).has_value());
}

TEST(SdlInputBackend, PointerMovedWithNoRendererReturnsNullopt) {
    SDL_Event event;
    event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.x = 50;
    event.motion.y = 75;

    EXPECT_FALSE(SdlInputBackend::translate(event, nullptr).has_value());
}

TEST(SdlInputBackend, UnknownMouseButtonReturnsNullopt) {
    SDL_Event event;
    event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.button = 42; // not SDL_BUTTON_LEFT/RIGHT/MIDDLE

    EXPECT_FALSE(SdlInputBackend::translate(event, nullptr).has_value());
}

TEST(SdlInputBackend, UnknownKeyReturnsNullopt) {
    SDL_Event event;
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.key = SDLK_F1; // not in our Key enum

    EXPECT_FALSE(SdlInputBackend::translate(event, nullptr).has_value());
}

TEST(SdlInputBackend, UnknownEventTypeReturnsNullopt) {
    SDL_Event event;
    event.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED; // arbitrary unhandled event

    EXPECT_FALSE(SdlInputBackend::translate(event, nullptr).has_value());
}

} // namespace d2engine
