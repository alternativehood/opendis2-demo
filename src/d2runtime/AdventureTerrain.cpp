#include "AdventureTerrain.hpp"

namespace d2runtime {

const char* terrain_border_kind_name(AdventureTerrainBorderKind kind) {
    switch (kind) {
    case AdventureTerrainBorderKind::None:
        return "none";
    case AdventureTerrainBorderKind::Drawable:
        return "drawable";
    case AdventureTerrainBorderKind::NonDrawableShape16:
        return "non_drawable_shape_16";
    }
    return "none";
}

} // namespace d2runtime
