#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

namespace d2runtime {

enum class AdventureGroundType : uint8_t {
    Plain = 0,
    Forest = 1,
    Exceptional = 2,
    Water = 3,
    Mountain = 4,
    Hill = 5,
    Unknown = 255
};

[[nodiscard]] constexpr AdventureGroundType adventure_ground_type_from_id(int raw_id) {
    switch (raw_id) {
    case 0:
        return AdventureGroundType::Plain;
    case 1:
        return AdventureGroundType::Forest;
    case 2:
        return AdventureGroundType::Exceptional;
    case 3:
        return AdventureGroundType::Water;
    case 4:
        return AdventureGroundType::Mountain;
    case 5:
        return AdventureGroundType::Hill;
    default:
        return AdventureGroundType::Unknown;
    }
}

} // namespace d2runtime
