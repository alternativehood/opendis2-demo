#pragma once

#include "AdventureSurfacePlacement.hpp"
#include "MapCellCoord.hpp"

#include <string>
#include <vector>

namespace d2runtime {

struct AdventureRuin {
    std::string               id;
    std::string               title;
    std::string               description;
    std::string               cash;
    std::string               item_id;
    std::string               looter_id;
    int                       ai_priority = 0;
    int                       image = -1;
    MapCellCoord              position;
    AdventureSurfacePlacement placement = AdventureSurfacePlacement::Land;
    std::vector<std::string>  defender_unit_ids;
    std::vector<int>          formation_positions;
    std::vector<MapCellCoord> footprint;
};

} // namespace d2runtime
