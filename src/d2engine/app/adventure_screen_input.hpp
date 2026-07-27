#pragma once

#include "../input/input_event.hpp"

#include <cstdint>
#include <optional>
#include <variant>

namespace d2engine {

struct AdventureCancel {};
struct AdventureOpenDebugBattle {};
struct AdventurePanCamera {
    int dx = 0;
    int dy = 0;
};
struct AdventureInspectAt {
    int x = 0;
    int y = 0;
};
struct AdventureSelectAt {
    int x = 0;
    int y = 0;
};
struct AdventurePointerAt {
    int x = 0;
    int y = 0;
};
struct AdventureZoomIn {};
struct AdventureZoomOut {};

using AdventureUiAction =
    std::variant<AdventureCancel, AdventureOpenDebugBattle, AdventurePanCamera, AdventureInspectAt,
                 AdventureSelectAt, AdventurePointerAt, AdventureZoomIn, AdventureZoomOut>;

class AdventureScreenInputHandler {
public:
    [[nodiscard]] static std::optional<AdventureUiAction> handle(const InputEvent& event);
};

} // namespace d2engine
