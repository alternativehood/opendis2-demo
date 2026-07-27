#include "battle_initiative.hpp"

#include <d2log/log.hpp>

#include <stdexcept>

namespace d2battle {

namespace {
auto kLog = d2log::get("d2battle.initiative");
} // namespace

int effective_initiative(const BattleState& state, const std::string& unit_id,
                         const d2engine::GameDataRegistry& game_data) {
    const auto* unit = state.find_unit(unit_id);
    if (!unit) {
        D2_LOG_ERROR(kLog, "effective_initiative: unit {} not found", unit_id);
        throw std::runtime_error("effective_initiative: unknown unit " + unit_id);
    }

    const auto* udef = game_data.find_unit(unit->type_id);
    if (!udef) {
        D2_LOG_ERROR(kLog, "effective_initiative: type {} not found", unit->type_id);
        throw std::runtime_error("effective_initiative: unknown type " + unit->type_id);
    }

    const d2engine::AttackDef* attack = udef->primary_attack;
    if (!attack)
        attack = udef->secondary_attack;

    int         base = 0;
    std::string attack_id = "none";
    if (attack) {
        base = attack->initiative;
        attack_id = attack->attack_id;
    } else {
        D2_LOG_WARN(kLog, "effective_initiative: unit {} has no attacks, defaulting 0", unit_id);
    }

    D2_LOG_DEBUG(kLog,
                 "effective_initiative: unit={} type={} attack={} base={} modifier=0 effective={}",
                 unit_id, unit->type_id, attack_id, base, base);

    return base;
}

} // namespace d2battle
