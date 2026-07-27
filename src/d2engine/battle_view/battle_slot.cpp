#include "battle_slot.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace d2engine {

std::optional<BattleSlotCoord> parse_position_string(const std::string& s) {
    // Expected format: <SIDE>_<DEPTH>_<LANE>
    // e.g. "A_FRONT_0", "D_BACK_2", "D_CENTER_1"
    if (s.size() < 3) {
        return std::nullopt;
    }

    BattleSide side;
    if (s[0] == 'A' || s[0] == 'a') {
        side = BattleSide::Attacker;
    } else if (s[0] == 'D' || s[0] == 'd') {
        side = BattleSide::Defender;
    } else {
        return std::nullopt;
    }

    if (s[1] != '_') {
        return std::nullopt;
    }

    const std::size_t depth_start = 2;
    const std::size_t depth_end = s.find('_', depth_start);
    if (depth_end == std::string::npos || depth_end + 2 > s.size()) {
        return std::nullopt;
    }

    const std::string depth_str = s.substr(depth_start, depth_end - depth_start);
    BattleDepth       depth;
    if (depth_str == "FRONT" || depth_str == "front" || depth_str == "Front") {
        depth = BattleDepth::Front;
    } else if (depth_str == "BACK" || depth_str == "back" || depth_str == "Back") {
        depth = BattleDepth::Back;
    } else if (depth_str == "CENTER" || depth_str == "center" || depth_str == "Center") {
        depth = BattleDepth::Center;
    } else {
        return std::nullopt;
    }

    const std::string lane_str = s.substr(depth_end + 1);
    if (lane_str.size() != 1 || (std::isdigit(static_cast<unsigned char>(lane_str[0])) == 0)) {
        return std::nullopt;
    }
    const int lane = lane_str[0] - '0';
    if (lane < 0 || lane > 2) {
        return std::nullopt;
    }

    return BattleSlotCoord{.side = side, .lane = lane, .depth = depth};
}

void validate_unit_slot(BattleSlotCoord coord, bool is_large, std::string_view unit_type,
                        std::string_view alias) {
    if (is_large && !is_center_slot(coord)) {
        std::ostringstream msg;
        msg << "invalid_large_slot slot=" << slot_coord_to_string(coord)
            << " expected=CENTER is_large=true";
        if (!unit_type.empty())
            msg << " unit_type=" << unit_type;
        if (!alias.empty())
            msg << " alias=" << alias;
        throw std::invalid_argument(msg.str());
    }
    if (!is_large && is_center_slot(coord)) {
        std::ostringstream msg;
        msg << "invalid_small_slot slot=" << slot_coord_to_string(coord)
            << " expected=FRONT_OR_BACK is_large=false";
        if (!unit_type.empty())
            msg << " unit_type=" << unit_type;
        if (!alias.empty())
            msg << " alias=" << alias;
        throw std::invalid_argument(msg.str());
    }
}

} // namespace d2engine
