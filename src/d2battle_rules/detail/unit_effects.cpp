#include "unit_effects.hpp"

#include <algorithm>
#include <stdexcept>

namespace d2battle {
namespace detail {

const PetrifiedEffect* find_petrified_effect(const BattleUnitState& unit) {
    for (const auto& e : unit.effects) {
        if (auto* p = std::get_if<PetrifiedEffect>(&e))
            return p;
    }
    return nullptr;
}

bool is_petrified(const BattleUnitState& unit) {
    return find_petrified_effect(unit) != nullptr;
}

void apply_or_refresh_petrified(BattleUnitState& target, const std::string& source_actor_id,
                                const std::string& source_attack_id,
                                std::uint32_t      activation_skips) {

    if (source_actor_id.empty()) {
        throw std::runtime_error("apply_or_refresh_petrified: source_actor_id empty for target " +
                                 target.id);
    }
    if (source_attack_id.empty()) {
        throw std::runtime_error("apply_or_refresh_petrified: source_attack_id empty for target " +
                                 target.id);
    }
    if (activation_skips == 0) {
        throw std::runtime_error("apply_or_refresh_petrified: activation_skips=0 for target " +
                                 target.id + " source=" + source_actor_id);
    }
    if (!target.alive)
        throw std::runtime_error("apply_or_refresh_petrified: target is dead: " + target.id);

    for (auto& e : target.effects) {
        if (auto* p = std::get_if<PetrifiedEffect>(&e)) {
            p->source_actor_id = source_actor_id;
            p->source_attack_id = source_attack_id;
            p->remaining_activation_skips = activation_skips;
            return;
        }
    }

    target.effects.emplace_back(std::in_place_type<PetrifiedEffect>, source_actor_id,
                                source_attack_id, activation_skips);
}

void consume_one_petrified_activation_skip(BattleUnitState& unit) {

    auto it = std::find_if(unit.effects.begin(), unit.effects.end(), [](const auto& e) {
        return std::holds_alternative<PetrifiedEffect>(e);
    });

    if (it == unit.effects.end()) {
        throw std::runtime_error(
            "consume_one_petrified_activation_skip: unit has no PetrifiedEffect: " + unit.id);
    }

    auto& pet = std::get<PetrifiedEffect>(*it);
    if (pet.remaining_activation_skips == 0) {
        throw std::runtime_error(
            "consume_one_petrified_activation_skip: remaining_activation_skips already zero: " +
            unit.id);
    }

    --pet.remaining_activation_skips;

    if (pet.remaining_activation_skips == 0)
        unit.effects.erase(it);
}

void clear_transient_effects_on_death(BattleUnitState& unit) {
    if (unit.effects.empty())
        return;

    unit.effects.clear();
}

bool is_curable_negative_effect(const BattleUnitEffectState& effect) {
    return std::holds_alternative<PetrifiedEffect>(effect);
}

std::size_t remove_curable_negative_effects(BattleUnitState& unit) {
    if (!unit.alive)
        throw std::runtime_error("remove_curable_negative_effects: unit is dead: " + unit.id);

    auto before = unit.effects.size();
    auto it = std::remove_if(unit.effects.begin(), unit.effects.end(), is_curable_negative_effect);
    unit.effects.erase(it, unit.effects.end());
    return before - unit.effects.size();
}

} // namespace detail
} // namespace d2battle
