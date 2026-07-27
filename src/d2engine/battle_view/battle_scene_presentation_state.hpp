#pragma once

#include "battle_ids.hpp"

namespace d2engine {

struct BattleScenePresentationState {
    bool           background_visible = true;
    bool           frame_visible = true;
    int            layer_cycle = 0;
    UnitInstanceId info_left_unit;
    UnitInstanceId info_right_unit;

    void toggle_layers() noexcept {
        layer_cycle = (layer_cycle + 1) % 3;
        background_visible = layer_cycle != 2;
        frame_visible = layer_cycle == 0;
    }
};

} // namespace d2engine
