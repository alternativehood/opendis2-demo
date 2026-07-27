#pragma once

namespace d2engine {

struct Rect {
    float x = 0.0F;
    float y = 0.0F;
    float w = 0.0F;
    float h = 0.0F;

    [[nodiscard]] float right() const { return x + w; }
    [[nodiscard]] float bottom() const { return y + h; }

    [[nodiscard]] bool operator==(const Rect&) const = default;
};

} // namespace d2engine
