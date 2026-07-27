#pragma once

#include "rect.hpp"

namespace d2engine {

class Camera2D {
public:
    explicit Camera2D(float scale) : scale_(scale) {}

    [[nodiscard]] Rect to_screen(Rect r) const {
        return Rect{.x = r.x * scale_, .y = r.y * scale_, .w = r.w * scale_, .h = r.h * scale_};
    }

private:
    float scale_;
};

} // namespace d2engine
