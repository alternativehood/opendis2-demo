#pragma once

#include "MapCellCoord.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace d2runtime {

enum class AdventureResourceKind : std::uint8_t {
    GoldMine,
    RedMana,
    YellowMana,
    OrangeMana,
    WhiteMana,
    BlueMana,
};

struct AdventureResourceNode {
    std::string id;

    MapCellCoord              position;
    std::vector<MapCellCoord> footprint;

    int                   raw_resource = -1;
    AdventureResourceKind resource_kind{};

    std::string raw_type;
    std::string owner;
    int         ai_priority = 0;
};

} // namespace d2runtime
