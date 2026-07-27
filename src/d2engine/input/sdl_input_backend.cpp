#include "sdl_input_backend.hpp"

#include <SDL3/SDL.h>

#include <optional>
#include <utility>

namespace d2engine {

namespace {

struct LogicalPointerPosition {
    float x = 0.0f;
    float y = 0.0f;
};

std::optional<LogicalPointerPosition> map_pointer_coordinates(SDL_Renderer* renderer,
                                                              float window_x, float window_y) {
    if (renderer == nullptr) {
        return std::nullopt;
    }
    float lx = 0.0f;
    float ly = 0.0f;
    if (!SDL_RenderCoordinatesFromWindow(renderer, window_x, window_y, &lx, &ly)) {
        return std::nullopt;
    }
    return LogicalPointerPosition{.x = lx, .y = ly};
}

KeyModifier translate_mod(SDL_Keymod mod) {
    KeyModifier out = KeyModifier::None;
    if ((mod & SDL_KMOD_CTRL) != 0u)
        out = out | KeyModifier::Ctrl;
    if ((mod & SDL_KMOD_SHIFT) != 0u)
        out = out | KeyModifier::Shift;
    if ((mod & SDL_KMOD_ALT) != 0u)
        out = out | KeyModifier::Alt;
    if ((mod & SDL_KMOD_GUI) != 0u)
        out = out | KeyModifier::Gui;
    return out;
}

std::optional<Key> translate_key(SDL_Keycode sdl_key) {
    switch (sdl_key) {
    case SDLK_ESCAPE:
        return Key::Escape;
    case SDLK_Q:
        return Key::Q;
    case SDLK_B:
        return Key::B;
    case SDLK_LEFT:
        return Key::Left;
    case SDLK_RIGHT:
        return Key::Right;
    case SDLK_UP:
        return Key::Up;
    case SDLK_DOWN:
        return Key::Down;
    case SDLK_SPACE:
        return Key::Space;
    case SDLK_P:
        return Key::P;
    case SDLK_R:
        return Key::R;
    case SDLK_S:
        return Key::S;
    case SDLK_O:
        return Key::O;
    case SDLK_D:
        return Key::D;
    case SDLK_L:
        return Key::L;
    case SDLK_I:
        return Key::I;
    case SDLK_Z:
        return Key::Z;
    case SDLK_BACKSPACE:
        return Key::Backspace;
    case SDLK_1:
        return Key::Key1;
    case SDLK_2:
        return Key::Key2;
    case SDLK_3:
        return Key::Key3;
    case SDLK_4:
        return Key::Key4;
    case SDLK_5:
        return Key::Key5;
    case SDLK_6:
        return Key::Key6;
    case SDLK_A:
        return Key::A;
    case SDLK_W:
        return Key::W;
    case SDLK_E:
        return Key::E;
    case SDLK_U:
        return Key::U;
    case SDLK_J:
        return Key::J;
    case SDLK_TAB:
        return Key::Tab;
    case SDLK_MINUS:
        return Key::Minus;
    case SDLK_EQUALS:
        return Key::Equals;
    default:
        return std::nullopt;
    }
}

std::optional<PointerButton> translate_button(uint8_t sdl_button) {
    switch (sdl_button) {
    case SDL_BUTTON_LEFT:
        return PointerButton::Left;
    case SDL_BUTTON_RIGHT:
        return PointerButton::Right;
    case SDL_BUTTON_MIDDLE:
        return PointerButton::Middle;
    default:
        return std::nullopt;
    }
}

} // namespace

std::optional<InputEvent> SdlInputBackend::translate(const SDL_Event& event,
                                                     SDL_Renderer*    renderer) {
    switch (event.type) {
    case SDL_EVENT_KEY_DOWN: {
        auto key = translate_key(event.key.key);
        if (!key.has_value())
            return std::nullopt;
        return InputEvent{KeyPressed{*key, event.key.repeat, translate_mod(event.key.mod)}};
    }
    case SDL_EVENT_KEY_UP: {
        auto key = translate_key(event.key.key);
        if (!key.has_value())
            return std::nullopt;
        return InputEvent{KeyReleased{*key, translate_mod(event.key.mod)}};
    }
    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
        auto button = translate_button(event.button.button);
        if (!button.has_value())
            return std::nullopt;
        auto coords = map_pointer_coordinates(renderer, event.button.x, event.button.y);
        if (!coords.has_value())
            return std::nullopt;
        return InputEvent{
            PointerPressed{*button, static_cast<int>(coords->x), static_cast<int>(coords->y)}};
    }
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        auto button = translate_button(event.button.button);
        if (!button.has_value())
            return std::nullopt;
        auto coords = map_pointer_coordinates(renderer, event.button.x, event.button.y);
        if (!coords.has_value())
            return std::nullopt;
        return InputEvent{
            PointerReleased{*button, static_cast<int>(coords->x), static_cast<int>(coords->y)}};
    }
    case SDL_EVENT_MOUSE_MOTION: {
        auto coords = map_pointer_coordinates(renderer, event.motion.x, event.motion.y);
        if (!coords.has_value())
            return std::nullopt;
        return InputEvent{PointerMoved{static_cast<int>(coords->x), static_cast<int>(coords->y)}};
    }
    default:
        return std::nullopt;
    }
}

} // namespace d2engine
