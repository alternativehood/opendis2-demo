#include "battle_formation.hpp"

#include "../battle_formation.hpp"

#include <algorithm>
#include <set>

namespace d2battle {
namespace detail {
namespace formation {

using d2battle::formation::cell_to_position;

std::set<int> alive_members_in_rank(const BattleSideState& side, const BattleState& state,
                                    FormationRank rank) {
    std::set<int> result;
    for (int c = 0; c < 6; ++c) {
        int mi = side.cell_members[static_cast<std::size_t>(c)];
        if (mi < 0)
            continue;
        CellPosition pos = cell_to_position(c);
        if (pos.rank != rank)
            continue;
        const auto* unit = state.find_unit(*side.members[static_cast<std::size_t>(mi)]);
        if (unit && unit->alive)
            result.insert(mi);
    }
    return result;
}

FormationRank active_rank(const BattleSideState& side, const BattleState& state) {
    auto front_alive = alive_members_in_rank(side, state, FormationRank::Front);
    return front_alive.empty() ? FormationRank::Back : FormationRank::Front;
}

std::set<FormationRow> occupied_rows_in_rank(const BattleUnitState& unit,
                                             const BattleSideState& side_state,
                                             FormationRank          rank) {
    std::set<FormationRow> rows;
    int                    slot = unit.member_index;
    for (int c = 0; c < 6; ++c) {
        if (side_state.cell_members[static_cast<std::size_t>(c)] == slot) {
            CellPosition pos = cell_to_position(c);
            if (pos.rank == rank)
                rows.insert(pos.row);
        }
    }
    return rows;
}

bool adjacent_rows_reachable(FormationRow attacker_row, FormationRow target_row) {
    switch (attacker_row) {
    case FormationRow::Top:
        return target_row == FormationRow::Top || target_row == FormationRow::Middle;
    case FormationRow::Middle:
        return true;
    case FormationRow::Bottom:
        return target_row == FormationRow::Middle || target_row == FormationRow::Bottom;
    }
    return false;
}

AdjacentValidationResult validate_adjacent_target(const BattleState&     state,
                                                  const BattleUnitState& actor,
                                                  const BattleUnitState& target) {
    AdjacentValidationResult result{};
    result.error = ActionValidationError::None;

    const auto& attacker_side = state.side(actor.side);
    const auto& defender_side = state.side(target.side);

    result.active_attacker_rank = active_rank(attacker_side, state);
    result.active_target_rank = active_rank(defender_side, state);

    result.actor_rows = occupied_rows_in_rank(actor, attacker_side, result.active_attacker_rank);

    if (result.actor_rows.empty()) {
        result.error = ActionValidationError::ActorNotOnActiveMeleeRank;
        return result;
    }

    result.target_rows = occupied_rows_in_rank(target, defender_side, result.active_target_rank);

    if (result.target_rows.empty()) {
        result.error = ActionValidationError::TargetProtectedByFrontRank;
        return result;
    }

    for (const auto& ar : result.actor_rows) {
        for (const auto& tr : result.target_rows) {
            if (adjacent_rows_reachable(ar, tr)) {
                result.error = ActionValidationError::None;
                return result;
            }
        }
    }

    result.error = ActionValidationError::TargetOutOfAdjacentReach;
    return result;
}

} // namespace formation
} // namespace detail
} // namespace d2battle
