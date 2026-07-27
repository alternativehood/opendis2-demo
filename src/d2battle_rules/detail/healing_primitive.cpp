#include "healing_primitive.hpp"

#include <d2engine/assets/game_data_registry.hpp>

#include <algorithm>
#include <stdexcept>

namespace d2battle {
namespace detail {

int resolve_unit_max_hp(const BattleUnitState& unit, const d2engine::GameDataRegistry& game_data) {
    const auto* udef = game_data.find_unit(unit.type_id);
    if (!udef) {
        throw std::runtime_error("resolve_unit_max_hp: UnitDef not found for unit " + unit.id +
                                 " type " + unit.type_id);
    }
    return udef->hit_points;
}

UnitHealingResolution heal_alive_unit_up_to_max(BattleState& state, const std::string& unit_id,
                                                int                               requested_healing,
                                                const d2engine::GameDataRegistry& game_data) {
    if (requested_healing < 0) {
        throw std::runtime_error("heal_alive_unit_up_to_max: requested_healing < 0 for unit " +
                                 unit_id);
    }

    auto* unit = state.find_unit(unit_id);
    if (!unit)
        throw std::runtime_error("heal_alive_unit_up_to_max: unit not found: " + unit_id);
    if (!unit->alive)
        throw std::runtime_error("heal_alive_unit_up_to_max: unit is dead: " + unit_id);
    if (unit->current_hp <= 0) {
        throw std::runtime_error(
            "heal_alive_unit_up_to_max: unit current_hp <= 0 for alive unit: " + unit_id);
    }

    int max_hp = resolve_unit_max_hp(*unit, game_data);
    if (max_hp <= 0)
        throw std::runtime_error("heal_alive_unit_up_to_max: max_hp <= 0 for unit " + unit_id);
    if (unit->current_hp > max_hp) {
        throw std::runtime_error("heal_alive_unit_up_to_max: current_hp > max_hp for unit " +
                                 unit_id);
    }

    UnitHealingResolution result;
    result.unit_id = unit_id;
    result.hp_before = unit->current_hp;
    result.requested = requested_healing;

    int missing = max_hp - unit->current_hp;
    int heal_amount = std::min(requested_healing, missing);

    if (heal_amount > 0) {
        unit->current_hp += heal_amount;
    }
    result.hp_after = unit->current_hp;
    result.applied = heal_amount;
    result.unused = requested_healing - heal_amount;

    return result;
}

UnitReviveResolution revive_dead_unit_with_hp(BattleState& state, const std::string& unit_id,
                                              int                               requested_hp,
                                              const d2engine::GameDataRegistry& game_data) {
    if (requested_hp <= 0)
        throw std::runtime_error("revive_dead_unit_with_hp: requested_hp <= 0 for unit " + unit_id);

    auto* unit = state.find_unit(unit_id);
    if (!unit)
        throw std::runtime_error("revive_dead_unit_with_hp: unit not found: " + unit_id);
    if (unit->alive)
        throw std::runtime_error("revive_dead_unit_with_hp: unit is alive: " + unit_id);
    if (unit->current_hp != 0) {
        throw std::runtime_error("revive_dead_unit_with_hp: current_hp != 0 for dead unit " +
                                 unit_id);
    }

    if (!unit->effects.empty()) {
        throw std::runtime_error("revive_dead_unit_with_hp: dead unit has transient effects: " +
                                 unit_id);
    }

    int max_hp = resolve_unit_max_hp(*unit, game_data);
    if (max_hp <= 0)
        throw std::runtime_error("revive_dead_unit_with_hp: max_hp <= 0 for unit " + unit_id);

    UnitReviveResolution result;
    result.unit_id = unit_id;
    result.hp_before = unit->current_hp;
    result.requested = requested_hp;

    int revived_hp = std::min(requested_hp, max_hp);
    unit->current_hp = revived_hp;
    unit->alive = true;

    for (auto* sidestate : {&state.party1, &state.party2}) {
        if (sidestate->leader_id == unit_id && sidestate->leader_alive != 1)
            sidestate->leader_alive = 1;
    }

    result.hp_after = unit->current_hp;
    result.applied = revived_hp;

    return result;
}

} // namespace detail
} // namespace d2battle
