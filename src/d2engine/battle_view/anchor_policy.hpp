#pragma once

#include <cstdint>

namespace d2engine {

enum class AnchorPolicy : std::uint8_t {
    UnitFoot,
    UnitCanvasFoot,
    TeamCentroid,
    OppositeTeamCentroid,
    LaneMidpoint,
    OppositeLaneMidpoint,
    BattlefieldReferenceRect,
    ScreenRect,
    WorldPoint,
};

} // namespace d2engine
