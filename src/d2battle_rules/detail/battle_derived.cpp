#include "battle_derived.hpp"

#include <algorithm>

namespace d2battle {
namespace detail {

void normalize_derived_side_state(BattleState& state) {
    for (int s = 0; s <= 1; ++s) {
        BattleSide side = (s == 0) ? BattleSide::Party1 : BattleSide::Party2;
        auto&      ss = state.side(side);
        if (!ss.leader_id.empty()) {
            const auto* leader = state.find_unit(ss.leader_id);
            ss.leader_alive = (leader && leader->alive) ? 1 : 0;
        }
    }
}

} // namespace detail
} // namespace d2battle
