#pragma once

#include "../render/rect.hpp"
#include "../render/vec2.hpp"

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace d2engine {

enum class BattleSide : uint8_t { Attacker, Defender };
// Center = logical anchor for large (2-slot) units; footprint = Front + Back of same lane.
enum class BattleDepth : uint8_t { Front, Back, Center };

struct BattleSlotCoord {
    BattleSide  side = BattleSide::Attacker;
    int         lane = 0; // 0, 1, or 2 (top to bottom)
    BattleDepth depth = BattleDepth::Front;

    auto operator<=>(const BattleSlotCoord&) const = default;
};

struct BattleSlotVisual {
    BattleSlotCoord coord;
    Vec2            position;     // top-left corner in screen space
    Rect            debug_bounds; // full slot rect for debug rendering
};

// ── Slot classification helpers ────────────────────────────────────────────

[[nodiscard]] inline bool is_center_slot(BattleSlotCoord c) noexcept {
    return c.depth == BattleDepth::Center;
}

// Given a CENTER coord, returns the FRONT slot of the same side/lane.
[[nodiscard]] inline BattleSlotCoord same_lane_front(BattleSlotCoord center) noexcept {
    return {.side = center.side, .lane = center.lane, .depth = BattleDepth::Front};
}
// Given a CENTER coord, returns the BACK slot of the same side/lane.
[[nodiscard]] inline BattleSlotCoord same_lane_back(BattleSlotCoord center) noexcept {
    return {.side = center.side, .lane = center.lane, .depth = BattleDepth::Back};
}

// Returns the two footprint slots (FRONT, BACK) for a large unit anchored at CENTER.
[[nodiscard]] inline std::array<BattleSlotCoord, 2>
large_footprint_slots(BattleSlotCoord center) noexcept {
    return {same_lane_front(center), same_lane_back(center)};
}

// Validate that the slot type matches the unit size.
// Throws std::invalid_argument with a diagnostic message on mismatch.
void validate_unit_slot(BattleSlotCoord coord, bool is_large, std::string_view unit_type = {},
                        std::string_view alias = {});

// ── Slot string helpers ────────────────────────────────────────────────────

// Format as "A_FRONT_0", "D_BACK_2", "D_CENTER_1", etc.
[[nodiscard]] inline std::string slot_coord_to_string(BattleSlotCoord c) {
    const char  side_ch = (c.side == BattleSide::Attacker) ? 'A' : 'D';
    const char* depth_str = "CENTER";
    if (c.depth == BattleDepth::Front) {
        depth_str = "FRONT";
    } else if (c.depth == BattleDepth::Back) {
        depth_str = "BACK";
    }
    return std::string{side_ch} + '_' + depth_str + '_' + std::to_string(c.lane);
}

// Parse position strings like "A_FRONT_0", "D_BACK_2", "D_CENTER_1".
// Returns std::nullopt on invalid input.
[[nodiscard]] std::optional<d2engine::BattleSlotCoord> parse_position_string(const std::string& s);

} // namespace d2engine

template <> struct std::hash<d2engine::BattleSlotCoord> {
    std::size_t operator()(d2engine::BattleSlotCoord c) const noexcept {
        auto h1 = std::hash<std::uint8_t>{}(static_cast<std::uint8_t>(c.side));
        auto h2 = std::hash<int>{}(c.lane);
        auto h3 = std::hash<std::uint8_t>{}(static_cast<std::uint8_t>(c.depth));
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};
