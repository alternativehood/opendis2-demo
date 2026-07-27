#pragma once

#include "AdventureSurfacePlacement.hpp"
#include "MapCellCoord.hpp"

#include <string>
#include <vector>

namespace d2runtime {

enum class AdventureTreasurePlacement {
    Land,
    Water,
};

struct AdventureTreasure {
    std::string                id;
    std::string                looter_id;
    std::vector<std::string>   item_ids;
    MapCellCoord               position;
    int                        image = -1;
    AdventureTreasurePlacement placement = AdventureTreasurePlacement::Land;
    std::vector<MapCellCoord>  footprint;
};

} // namespace d2runtime
