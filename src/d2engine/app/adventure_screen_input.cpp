#include "adventure_screen_input.hpp"

namespace d2engine {

std::optional<AdventureUiAction> AdventureScreenInputHandler::handle(const InputEvent& event) {
    if (const auto* key = std::get_if<KeyPressed>(&event)) {
        if (key->key == Key::Escape || key->key == Key::Q) {
            return AdventureCancel{};
        }
        if (key->key == Key::B) {
            return AdventureOpenDebugBattle{};
        }
        const int pan = 64;
        if (key->key == Key::Left) {
            return AdventurePanCamera{-pan, 0};
        }
        if (key->key == Key::Right) {
            return AdventurePanCamera{pan, 0};
        }
        if (key->key == Key::Up) {
            return AdventurePanCamera{0, -pan};
        }
        if (key->key == Key::Down) {
            return AdventurePanCamera{0, pan};
        }
        if (key->key == Key::Equals) {
            return AdventureZoomIn{};
        }
        if (key->key == Key::Minus) {
            return AdventureZoomOut{};
        }
    }

    if (const auto* ptr = std::get_if<PointerPressed>(&event)) {
        if (ptr->button == PointerButton::Right) {
            return AdventureInspectAt{ptr->x, ptr->y};
        }
        if (ptr->button == PointerButton::Left) {
            return AdventureSelectAt{ptr->x, ptr->y};
        }
    }

    if (const auto* ptr = std::get_if<PointerMoved>(&event)) {
        return AdventurePointerAt{ptr->x, ptr->y};
    }

    return std::nullopt;
}

} // namespace d2engine
