#pragma once

#include <string>
#include <vector>

namespace d2engine {

struct RaceDef {
    std::string              race_id;
    std::string              name;
    int                      race_type = 0;
    bool                     playable = false;
    int                      regen_heal = 0;
    std::string              guardian_unit_id;
    std::vector<std::string> leader_unit_ids;
    std::vector<std::string> soldier_unit_ids;
};

} // namespace d2engine
