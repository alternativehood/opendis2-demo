#include "battle_formation.hpp"

#include <stdexcept>
#include <string>

namespace d2battle {
namespace formation {

CellPosition cell_to_position(int cell) {
    switch (cell) {
    case 0:
        return {FormationRank::Front, FormationRow::Top};
    case 1:
        return {FormationRank::Back, FormationRow::Top};
    case 2:
        return {FormationRank::Front, FormationRow::Middle};
    case 3:
        return {FormationRank::Back, FormationRow::Middle};
    case 4:
        return {FormationRank::Front, FormationRow::Bottom};
    case 5:
        return {FormationRank::Back, FormationRow::Bottom};
    default:
        throw std::runtime_error("cell_to_position: invalid cell " + std::to_string(cell));
    }
}

const char* rank_name(FormationRank r) {
    return r == FormationRank::Front ? "Front" : "Back";
}

const char* row_name(FormationRow r) {
    switch (r) {
    case FormationRow::Top:
        return "Top";
    case FormationRow::Middle:
        return "Middle";
    case FormationRow::Bottom:
        return "Bottom";
    }
    return "?";
}

const BattleUnitState* unit_at_position(const BattleState& state, BattleSide side,
                                        FormationRank rank, FormationRow row) {
    const auto& s = state.side(side);
    for (int c = 0; c < 6; ++c) {
        CellPosition pos = cell_to_position(c);
        if (pos.rank == rank && pos.row == row) {
            int mi = s.cell_members[static_cast<std::size_t>(c)];
            if (mi >= 0 && s.members[static_cast<std::size_t>(mi)].has_value())
                return state.find_unit(*s.members[static_cast<std::size_t>(mi)]);
        }
    }
    return nullptr;
}

} // namespace formation
} // namespace d2battle
