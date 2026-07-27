#pragma once

#include <cstdint>
#include <variant>

namespace d2engine {

enum class Key : uint8_t {
    Escape,
    Q,
    B,
    Left,
    Right,
    Up,
    Down,
    Space,
    P,
    R,
    S,
    O,
    D,
    L,
    I,
    Z,
    Backspace,
    Key1,
    Key2,
    Key3,
    Key4,
    Key5,
    Key6,
    A,
    W,
    E,
    U,
    J,
    Tab,
    Minus,
    Equals,
};

enum class KeyModifier : uint8_t {
    None = 0,
    Ctrl = 1 << 0,
    Shift = 1 << 1,
    Alt = 1 << 2,
    Gui = 1 << 3,
};

constexpr KeyModifier operator|(KeyModifier a, KeyModifier b) {
    return static_cast<KeyModifier>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

constexpr bool has_modifier(KeyModifier value, KeyModifier mask) {
    return (static_cast<uint8_t>(value) & static_cast<uint8_t>(mask)) != 0;
}

enum class PointerButton : uint8_t {
    Left,
    Right,
    Middle,
};

struct KeyPressed {
    Key         key;
    bool        repeat = false;
    KeyModifier modifiers = KeyModifier::None;
};

struct KeyReleased {
    Key         key;
    KeyModifier modifiers = KeyModifier::None;
};

struct PointerPressed {
    PointerButton button;
    int           x = 0;
    int           y = 0;
};

struct PointerReleased {
    PointerButton button;
    int           x = 0;
    int           y = 0;
};

struct PointerMoved {
    int x = 0;
    int y = 0;
};

using InputEvent =
    std::variant<KeyPressed, KeyReleased, PointerPressed, PointerReleased, PointerMoved>;

[[nodiscard]] inline bool is_pointer_event(const InputEvent& event) noexcept {
    return std::holds_alternative<PointerMoved>(event) ||
           std::holds_alternative<PointerPressed>(event) ||
           std::holds_alternative<PointerReleased>(event);
}

[[nodiscard]] inline bool is_keyboard_event(const InputEvent& event) noexcept {
    return std::holds_alternative<KeyPressed>(event) || std::holds_alternative<KeyReleased>(event);
}

// Returns true when a translated InputEvent should be forwarded to ScreenManager
// given the current debug UI state.
[[nodiscard]] inline bool should_forward_to_screen(const InputEvent& event, bool debug_ui_visible,
                                                   bool wants_mouse, bool wants_keyboard) noexcept {
    if (!debug_ui_visible)
        return true;
    if (is_pointer_event(event) && wants_mouse)
        return false;
    if (is_keyboard_event(event) && wants_keyboard)
        return false;
    return true;
}

} // namespace d2engine
