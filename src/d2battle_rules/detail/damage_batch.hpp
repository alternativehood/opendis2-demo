#pragma once

#include "damage_primitive.hpp"

#include "../battle_state.hpp"

#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace d2battle {
namespace detail {

struct UnitDamageDelta {
    std::string unit_id;
    int         hp_before = 0;
    int         hp_after = 0;
    int         actual_damage = 0;
    bool        killed = false;
};

struct DamageBatchResolution {
    std::vector<UnitDamageDelta> units;
    int                          total_actual_damage = 0;
    int                          affected = 0;
    int                          killed = 0;
};

[[nodiscard]] inline DamageBatchResolution
apply_raw_damage_to_targets(BattleState& state, std::span<const std::string> target_unit_ids,
                            int requested_damage) {
    DamageBatchResolution result;
    for (const auto& uid : target_unit_ids) {
        auto* target = state.find_unit(uid);
        if (!target)
            throw std::runtime_error("apply_raw_damage_to_targets: target not found: " + uid);
        if (!target->alive)
            throw std::runtime_error("apply_raw_damage_to_targets: target is already dead: " + uid);

        auto res = apply_raw_damage(*target, requested_damage);
        result.units.push_back({uid, res.hp_before, res.hp_after, res.actual_damage, res.killed});
        result.total_actual_damage += res.actual_damage;
        ++result.affected;
        if (res.killed)
            ++result.killed;
    }
    return result;
}

} // namespace detail
} // namespace d2battle
