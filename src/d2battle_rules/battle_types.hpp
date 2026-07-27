#pragma once

#include <cstdint>
#include <optional>

namespace d2battle {

enum class BattleSide : std::uint8_t { Party1 = 0, Party2 = 1 };

[[nodiscard]] inline constexpr BattleSide opposite_side(BattleSide side) {
    return (side == BattleSide::Party1) ? BattleSide::Party2 : BattleSide::Party1;
}

enum class BattleStatus : std::uint8_t { InProgress, Finished };

} // namespace d2battle
