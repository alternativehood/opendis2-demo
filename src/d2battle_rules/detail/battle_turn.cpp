#include "battle_turn.hpp"
#include "../battle_initiative.hpp"

#include <d2log/log.hpp>

#include <algorithm>
#include <vector>

namespace d2battle {
namespace detail {

namespace {
auto kLog = d2log::get("d2battle.turn");
} // namespace

std::vector<BattleTurnEntry> build_turn_order(const BattleState&                state,
                                              const d2engine::GameDataRegistry& game_data) {
    std::vector<InitiativeCandidate> candidates;
    for (const auto& u : state.units) {
        if (!u.alive)
            continue;
        InitiativeCandidate cand;
        cand.unit_id = u.id;
        cand.initiative = effective_initiative(state, u.id, game_data);
        cand.side = u.side;
        cand.member_index = u.member_index;
        candidates.push_back(cand);
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const InitiativeCandidate& a, const InitiativeCandidate& b) {
                  if (a.initiative != b.initiative)
                      return a.initiative > b.initiative;
                  if (a.side != b.side)
                      return static_cast<int>(a.side) < static_cast<int>(b.side);
                  return a.member_index < b.member_index;
              });

    D2_LOG_DEBUG(kLog, "=== ROUND {} TURN ORDER ===", state.round_state.round_number);
    std::vector<BattleTurnEntry> order;
    order.reserve(candidates.size());
    for (std::size_t i = 0; i < candidates.size(); ++i) {
        const auto& c = candidates[i];
        int         tie_key = static_cast<int>(c.side) * 100 + c.member_index;
        order.push_back({c.unit_id, c.initiative, tie_key});
        D2_LOG_DEBUG(kLog, "  [{}] unit={} initiative={} tie_key={}", i, c.unit_id, c.initiative,
                     tie_key);
    }
    return order;
}

void begin_round(BattleState& state, std::uint32_t round_number,
                 const d2engine::GameDataRegistry& game_data) {
    state.round_state.round_number = round_number;
    state.round_state.turn_order = build_turn_order(state, game_data);
    state.round_state.current_turn_index = 0;
}

void advance_turn(BattleState& state, const d2engine::GameDataRegistry& game_data) {
    if (state.status != BattleStatus::InProgress)
        return;

    auto&       rs = state.round_state;
    const auto* prev_actor = state.current_actor();
    auto        prev_round = rs.round_number;

    rs.current_turn_index++;

    while (rs.current_turn_index < rs.turn_order.size()) {
        const auto& entry = rs.turn_order[rs.current_turn_index];
        const auto* unit = state.find_unit(entry.unit_id);
        if (unit && unit->alive)
            break;
        rs.current_turn_index++;
    }

    if (rs.current_turn_index >= rs.turn_order.size()) {
        D2_LOG_DEBUG(kLog, "round {} exhausted, begin round {}", rs.round_number,
                     rs.round_number + 1);
        begin_round(state, rs.round_number + 1, game_data);
        while (rs.current_turn_index < rs.turn_order.size()) {
            const auto& entry = rs.turn_order[rs.current_turn_index];
            const auto* unit = state.find_unit(entry.unit_id);
            if (unit && unit->alive)
                break;
            rs.current_turn_index++;
        }
    }

    const auto* next_actor = state.current_actor();
    bool        round_advanced = (rs.round_number != prev_round);
    D2_LOG_DEBUG(
        kLog,
        "turn advance: prev_round={} new_round={} round_advanced={} prev_actor={} next_actor={}",
        prev_round, rs.round_number, round_advanced, prev_actor ? prev_actor->id.c_str() : "none",
        next_actor ? next_actor->id.c_str() : "none");
}

} // namespace detail
} // namespace d2battle
