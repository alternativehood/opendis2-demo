#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace d2engine {

enum class AttackClass : std::uint8_t {
    Damage,
    Drain,
    Paralyze,
    Heal,
    Fear,
    BoostDamage,
    Petrify,
    LowerDamage,
    LowerInitiative,
    Poison,
    Frostbite,
    Revive,
    DrainOverflow,
    Cure,
    Summon,
    DrainLevel,
    GiveAttack,
    Doppelganger,
    TransformSelf,
    TransformOther,
    Blister,
    BestowWards,
    Shatter,
    Unknown
};

enum class AttackSource : std::uint8_t {
    Weapon,
    Mind,
    Life,
    Death,
    Fire,
    Water,
    Earth,
    Air,
    Unknown
};

enum class AttackReach : std::uint8_t { All, Any, Adjacent, Unknown };

struct AttackDef {
    std::string              attack_id;
    std::string              name;
    std::string              description;
    AttackClass              attack_class = AttackClass::Unknown;
    AttackSource             source = AttackSource::Unknown;
    AttackReach              reach = AttackReach::Unknown;
    int                      initiative = 0;
    int                      damage = 0;
    int                      heal = 0;
    int                      power = 0;
    bool                     infinite = false;
    bool                     crit_hit = false;
    std::vector<std::string> ward_ids;
    std::string              alt_attack_id;
};

} // namespace d2engine
