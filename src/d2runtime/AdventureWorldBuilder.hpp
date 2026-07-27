#pragma once

#include "AdventureWorldState.hpp"

namespace d2scenario {
struct ScenarioTemplate;
struct SgTerrainGrid;
} // namespace d2scenario

namespace d2runtime {

class AdventureWorldBuilder {
public:
    AdventureWorldBuildResult build(const d2scenario::ScenarioTemplate& scenario);
};

[[nodiscard]] AdventureTerrainGrid normalize_raw_sg_terrain(const d2scenario::SgTerrainGrid& raw);

} // namespace d2runtime
