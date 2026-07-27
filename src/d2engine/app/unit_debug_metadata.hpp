#pragma once

#include "../battle_view/battle_ids.hpp"

#include <string>
#include <vector>

namespace d2engine {

struct UnitDebugMetadata {
    UnitInstanceId unit_id;
    std::string    unit_type;
};

} // namespace d2engine
