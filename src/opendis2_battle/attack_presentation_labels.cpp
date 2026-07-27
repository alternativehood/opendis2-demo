#include "attack_presentation_labels.hpp"

#include <string_view>

namespace d2battle_terminal {

std::string_view attack_class_label(d2engine::AttackClass value) {
    using d2engine::AttackClass;
    switch (value) {
    case AttackClass::Damage:
        return "Damage";
    case AttackClass::Drain:
        return "Drain";
    case AttackClass::Paralyze:
        return "Paralyze";
    case AttackClass::Heal:
        return "Heal";
    case AttackClass::Fear:
        return "Fear";
    case AttackClass::BoostDamage:
        return "BoostDamage";
    case AttackClass::Petrify:
        return "Petrify";
    case AttackClass::LowerDamage:
        return "LowerDamage";
    case AttackClass::LowerInitiative:
        return "LowerInitiative";
    case AttackClass::Poison:
        return "Poison";
    case AttackClass::Frostbite:
        return "Frostbite";
    case AttackClass::Revive:
        return "Revive";
    case AttackClass::DrainOverflow:
        return "DrainOverflow";
    case AttackClass::Cure:
        return "Cure";
    case AttackClass::Summon:
        return "Summon";
    case AttackClass::DrainLevel:
        return "DrainLevel";
    case AttackClass::GiveAttack:
        return "GiveAttack";
    case AttackClass::Doppelganger:
        return "Doppelganger";
    case AttackClass::TransformSelf:
        return "TransformSelf";
    case AttackClass::TransformOther:
        return "TransformOther";
    case AttackClass::Blister:
        return "Blister";
    case AttackClass::BestowWards:
        return "BestowWards";
    case AttackClass::Shatter:
        return "Shatter";
    case AttackClass::Unknown:
        return "Unknown";
    }
    return "Unknown";
}

std::string_view attack_reach_label(d2engine::AttackReach value) {
    using d2engine::AttackReach;
    switch (value) {
    case AttackReach::All:
        return "All";
    case AttackReach::Any:
        return "Any";
    case AttackReach::Adjacent:
        return "Adjacent";
    case AttackReach::Unknown:
        return "Unknown";
    }
    return "Unknown";
}
} // namespace d2battle_terminal
