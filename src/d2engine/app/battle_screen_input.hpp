#pragma once

#include "../battle_view/battle_debug_binding.hpp"
#include "../input/input_event.hpp"
#include "battle_screen_action.hpp"

#include <optional>
#include <span>
#include <string>

namespace d2engine {

struct BattleScreenInputContext {
    bool                                 tuning_enabled = false;
    std::span<const DebugRenderableItem> selectable_items;
};

class BattleScreenInputHandler {
public:
    [[nodiscard]] static std::optional<BattleScreenAction>
    handle(const InputEvent& event, const BattleScreenInputContext& context);

    [[nodiscard]] static std::optional<BattleScreenAction>
    handle_pointer_click(int logical_x, int logical_y,
                         const std::span<const DebugRenderableItem>& items);
};

} // namespace d2engine
