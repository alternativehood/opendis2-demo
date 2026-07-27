#pragma once

#include "../battle_state.hpp"

#include <d2engine/assets/game_data_registry.hpp>

#include <string>

namespace d2battle {
namespace detail {

struct UnitHealingResolution {
    std::string unit_id;
    int         hp_before = 0;
    int         hp_after = 0;
    int         requested = 0;
    int         applied = 0;
    int         unused = 0;
};

// Heals a single alive unit up to its max HP. Hard-fails if the unit is not found,
// dead, or its UnitDef is missing.
// Guards: requested_healing >= 0, max_hp > 0, current_hp > 0, current_hp <= max_hp, unit alive.
// Full-health unit returns applied=0, unused=requested_healing (valid no-op).
[[nodiscard]] UnitHealingResolution
heal_alive_unit_up_to_max(BattleState& state, const std::string& unit_id, int requested_healing,
                          const d2engine::GameDataRegistry& game_data);

// Hard-fails if UnitDef is not found in registry.
// TODO: effective-stat resolution (modifiers, dynamic levels) will be expanded separately.
[[nodiscard]] int resolve_unit_max_hp(const BattleUnitState&            unit,
                                      const d2engine::GameDataRegistry& game_data);

struct UnitReviveResolution {
    std::string unit_id;
    int         hp_before = 0;
    int         hp_after = 0;
    int         requested = 0;
    int         applied = 0;
};

// Revives a dead unit with the given HP amount. Hard-fails if the unit is not found,
// not dead, has zero max HP, or has non-empty transient effects. HP cap: min(requested_hp, max_hp).
// Preserves: type_id, serialized_level, dynamic_level, xp, member_index, formation_cell,
// side, name, modifier_ids. Existing empty effects vector preserved.
[[nodiscard]] UnitReviveResolution
revive_dead_unit_with_hp(BattleState& state, const std::string& unit_id, int requested_hp,
                         const d2engine::GameDataRegistry& game_data);

} // namespace detail
} // namespace d2battle
