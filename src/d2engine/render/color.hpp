#pragma once

#include <cstdint>

namespace d2engine {

struct Color {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 0;

    [[nodiscard]] bool operator==(const Color&) const = default;
};

} // namespace d2engine
