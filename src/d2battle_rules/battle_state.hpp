#pragma once

#include "battle_round.hpp"
#include "battle_side.hpp"
#include "battle_types.hpp"
#include "battle_unit.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace d2battle {

struct BattleState {
    BattleStatus              status = BattleStatus::InProgress;
    std::optional<BattleSide> winner;

    BattleSideState party1;
    BattleSideState party2;

    std::vector<BattleUnitState> units;

    BattleRoundState round_state;

    [[nodiscard]] const BattleSideState& side(BattleSide s) const {
        return (s == BattleSide::Party1) ? party1 : party2;
    }

    [[nodiscard]] BattleSideState& side(BattleSide s) {
        return (s == BattleSide::Party1) ? party1 : party2;
    }

    [[nodiscard]] const BattleUnitState* find_unit(const std::string& unit_id) const {
        for (const auto& u : units) {
            if (u.id == unit_id)
                return &u;
        }
        return nullptr;
    }

    [[nodiscard]] BattleUnitState* find_unit(const std::string& unit_id) {
        for (auto& u : units) {
            if (u.id == unit_id)
                return &u;
        }
        return nullptr;
    }

    [[nodiscard]] const BattleUnitState* current_actor() const {
        if (status != BattleStatus::InProgress)
            return nullptr;
        if (round_state.current_turn_index >= round_state.turn_order.size())
            return nullptr;
        const auto& entry = round_state.turn_order[round_state.current_turn_index];
        return find_unit(entry.unit_id);
    }

    [[nodiscard]] bool is_terminal() const { return status == BattleStatus::Finished; }

    bool operator==(const BattleState&) const = default;
};

} // namespace d2battle
