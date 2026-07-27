#include "AdventureGroundClassifier.hpp"

namespace d2runtime {

AdventureGroundType classify_adventure_ground(const AdventureTerrainTileDescriptor& descriptor) {
    if (descriptor.material == AdventureTerrainMaterial::Water)
        return AdventureGroundType::Water;
    if (descriptor.is_forest)
        return AdventureGroundType::Forest;
    if (descriptor.material != AdventureTerrainMaterial::Unknown)
        return AdventureGroundType::Plain;
    return AdventureGroundType::Unknown;
}

} // namespace d2runtime
