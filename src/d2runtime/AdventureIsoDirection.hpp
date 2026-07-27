#pragma once

#include <cstdint>
#include <stdexcept>

namespace d2runtime {

enum class AdventureIsoDirection : uint8_t {
    D0 = 0,
    D1 = 1,
    D2 = 2,
    D3 = 3,
    D4 = 4,
    D5 = 5,
    D6 = 6,
    D7 = 7
};

inline constexpr int direction_index(AdventureIsoDirection d) {
    return static_cast<int>(d);
}

inline constexpr AdventureIsoDirection direction_from_index(int idx) {
    if (idx < 0 || idx > 7)
        throw std::out_of_range("AdventureIsoDirection index out of range");
    return static_cast<AdventureIsoDirection>(idx);
}

} // namespace d2runtime
