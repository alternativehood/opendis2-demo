#pragma once

namespace d2engine {

struct PointerTracker {
    int  x = 0;
    int  y = 0;
    bool valid = false;

    void on_move(int px, int py) {
        x = px;
        y = py;
        valid = true;
    }
};

} // namespace d2engine
