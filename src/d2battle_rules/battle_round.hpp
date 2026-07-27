#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace d2battle {

struct BattleTurnEntry {
    std::string unit_id;
    int         effective_initiative = 0;
    int         tie_break_key = 0;

    bool operator==(const BattleTurnEntry&) const = default;
};

struct BattleRoundState {
    std::uint32_t                round_number = 0;
    std::vector<BattleTurnEntry> turn_order;
    std::size_t                  current_turn_index = 0;

    bool operator==(const BattleRoundState&) const = default;
};

} // namespace d2battle
