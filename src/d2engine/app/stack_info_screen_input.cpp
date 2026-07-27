#include "stack_info_screen_input.hpp"

namespace d2engine {

std::optional<StackInfoAction> StackInfoScreenInputHandler::handle(const InputEvent& event) {
    if (const auto* key = std::get_if<KeyPressed>(&event)) {
        if (key->key == Key::Escape) {
            return StackInfoCancel{};
        }
    }

    return std::nullopt;
}

} // namespace d2engine
