#pragma once

#include "MapCellCoord.hpp"

#include <string>
#include <vector>

namespace d2runtime {

struct AdventureCity {
    std::string               id;
    std::string               name_txt_id;
    std::string               description_txt_id;
    std::string               owner_id;
    std::string               subrace_id;
    std::string               stack_id;
    std::string               group_id;
    MapCellCoord              position;
    int                       ai_priority = 0;
    int                       size = 0;
    std::vector<MapCellCoord> footprint;
};

} // namespace d2runtime
