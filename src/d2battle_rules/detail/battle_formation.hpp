#pragma once

#include "../battle_action_validate.hpp"
#include "../battle_formation.hpp"

#include <set>
#include <vector>

namespace d2battle {
namespace detail {
namespace formation {

using d2battle::formation::cell_to_position;
using d2battle::formation::CellPosition;
using d2battle::formation::FormationRank;
using d2battle::formation::FormationRow;

struct AdjacentValidationResult {
    ActionValidationError error;

    FormationRank          active_attacker_rank;
    FormationRank          active_target_rank;
    std::set<FormationRow> actor_rows;
    std::set<FormationRow> target_rows;

    [[nodiscard]] bool valid() const { return error == ActionValidationError::None; }
};

[[nodiscard]] std::set<int> alive_members_in_rank(const BattleSideState& side,
                                                  const BattleState& state, FormationRank rank);

[[nodiscard]] FormationRank active_rank(const BattleSideState& side, const BattleState& state);

[[nodiscard]] std::set<FormationRow> occupied_rows_in_rank(const BattleUnitState& unit,
                                                           const BattleSideState& side_state,
                                                           FormationRank          rank);

[[nodiscard]] bool adjacent_rows_reachable(FormationRow attacker_row, FormationRow target_row);

[[nodiscard]] AdjacentValidationResult validate_adjacent_target(const BattleState&     state,
                                                                const BattleUnitState& actor,
                                                                const BattleUnitState& target);

} // namespace formation
} // namespace detail
} // namespace d2battle
