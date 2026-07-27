#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>

namespace d2engine {

// Which side's identity feeds into direction resolution.
// source_side: use the source unit's side ('a'/'d') as the basis.
// target_team_side: use the opposite team's side (i.e. opposite of source) as the basis.
enum class DirectionBasis : std::uint8_t {
    SourceSide,
    TargetTeamSide,
};

// One clip specification: which animation family and direction to load.
// direction == '\0' means "side_preferred": resolve using the basis side's render direction
// ('A'/'D'). When direction_by_side has a value, it takes precedence over direction:
//   present side key -> use that direction char
//   absent side key  -> no overlay for that side (empty clip, skip spawn)
// direction_basis determines which side key ('a' vs 'd') is used for resolution.
struct UnitAttackFxIntentConfig {
    std::string                         family;
    char                                direction{};
    std::optional<std::map<char, char>> direction_by_side;
    DirectionBasis                      direction_basis = DirectionBasis::SourceSide;
};

// Per-unit-type explicit attack FX intent: overrides the generic HEFF/TUCH fallback.
struct UnitAttackVisualIntentEntry {
    std::optional<UnitAttackFxIntentConfig> source_attack_overlay;
    std::optional<UnitAttackFxIntentConfig> team_attack_overlay;
    std::optional<UnitAttackFxIntentConfig> target_damage_fx;
};

using UnitAttackVisualIntentMap = std::unordered_map<std::string, UnitAttackVisualIntentEntry>;

} // namespace d2engine
