#include "battle_status.hpp"

#include <algorithm>
#include <d2log/log.hpp>

namespace d2battle {
namespace detail {

namespace {
auto kLog = d2log::get("d2battle.status");
} // namespace

void normalize_battle_status(BattleState& state) {
    bool p1_alive = std::any_of(state.units.begin(), state.units.end(), [](const auto& u) {
        return u.alive && u.side == BattleSide::Party1;
    });
    bool p2_alive = std::any_of(state.units.begin(), state.units.end(), [](const auto& u) {
        return u.alive && u.side == BattleSide::Party2;
    });

    if (!p1_alive || !p2_alive) {
        state.status = BattleStatus::Finished;
        state.winner = std::nullopt;
        if (p1_alive && !p2_alive) {
            state.winner = BattleSide::Party1;
        } else if (!p1_alive && p2_alive) {
            state.winner = BattleSide::Party2;
        }
        D2_LOG_DEBUG(kLog, "battle finished: winner={}",
                     state.winner.has_value()
                         ? (*state.winner == BattleSide::Party1 ? "Party1" : "Party2")
                         : "draw");
    } else {
        state.status = BattleStatus::InProgress;
        state.winner = std::nullopt;
    }
}

} // namespace detail
} // namespace d2battle
