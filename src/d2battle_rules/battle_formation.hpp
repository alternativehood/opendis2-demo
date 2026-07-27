#pragma once

#include "battle_side.hpp"
#include "battle_state.hpp"

#include <cstdint>
#include <string>

namespace d2battle {
namespace formation {

enum class FormationRow : std::uint8_t { Top = 0, Middle = 1, Bottom = 2 };
enum class FormationRank : std::uint8_t { Front = 0, Back = 1 };

struct CellPosition {
    FormationRank rank;
    FormationRow  row;
    bool          operator==(const CellPosition&) const = default;
};

[[nodiscard]] CellPosition cell_to_position(int cell);

[[nodiscard]] const char* rank_name(FormationRank r);
[[nodiscard]] const char* row_name(FormationRow r);

[[nodiscard]] const BattleUnitState* unit_at_position(const BattleState& state, BattleSide side,
                                                      FormationRank rank, FormationRow row);

} // namespace formation
} // namespace d2battle
