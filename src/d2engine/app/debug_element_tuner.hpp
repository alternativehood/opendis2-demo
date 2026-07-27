#pragma once

#include <d2log/log.hpp>

namespace d2engine {

struct DebugElementTuner {
    const char* label = "element";
    float       x_offset = 0.0f;
    float       y_offset = 0.0f;
    float       scale_x = 1.0f;
    float       scale_y = 1.0f;

    void move_x(float delta) {
        x_offset += delta;
        print();
    }

    void move_y(float delta) {
        y_offset += delta;
        print();
    }

    void scale(float ratio) {
        scale_x += scale_x * ratio;
        scale_y += scale_y * ratio;
        print();
    }

    void print() const {
        d2log::get("d2.debug")
            ->debug("element_tuner label={} scale_x={:.4f} scale_y={:.4f} x={:.0f} y={:.0f}", label,
                    static_cast<double>(scale_x), static_cast<double>(scale_y),
                    static_cast<double>(x_offset), static_cast<double>(y_offset));
    }
};

} // namespace d2engine
