#pragma once

#include "attack_def.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace d2engine {

struct ResourceCost {
    int gold = 0;
    int runic = 0;
    int yellow = 0;
    int infernal = 0;
    int grove = 0;
};

ResourceCost parse_resource_cost(std::string_view s);

enum class UnitCategory : std::uint8_t {
    Soldier,
    Noble,
    Leader,
    Summon,
    Illusion,
    NeutralLeader,
    NeutralSoldier,
    Guardian,
    Unknown
};

enum class UnitBranch : std::uint8_t {
    Fighter,
    Archer,
    Mage,
    Special,
    Sideshow,
    Hero,
    Noble,
    Summon,
    Unknown
};

struct DynUpgradeDef {
    std::string  upgrade_id;
    int          hit_points = 0;
    int          armor = 0;
    int          regen = 0;
    int          damage = 0;
    int          heal = 0;
    int          power = 0;
    int          xp_killed = 0;
    int          xp_next = 0;
    int          initiative = 0;
    int          move = 0;
    int          negotiate = 0;
    ResourceCost enroll_cost;
    ResourceCost revive_cost;
    ResourceCost heal_cost;
    ResourceCost training_cost;
};

struct UnitDef {
    // identity / text
    std::string unit_id;
    std::string name;
    std::string description;
    std::string ability_text;
    std::string race_id;
    int         subrace = 0;

    // classification
    UnitCategory category = UnitCategory::Unknown;
    UnitBranch   branch = UnitBranch::Unknown;
    bool         size_small = true;
    bool         sex_male = true;
    bool         atck_twice = false;
    bool         water_only = false;

    // stats
    int level = 1;
    int hit_points = 0;
    int armor = 0;
    int regen = 0;
    int move = 0;
    int scout = 0;
    int leadership = 0;
    int negotiate = 0;
    int xp_killed = 0;
    int xp_next = 0;

    // costs
    ResourceCost enroll_cost;
    ResourceCost enroll_building_cost;
    ResourceCost revive_cost;
    ResourceCost heal_cost;
    ResourceCost training_cost;

    // raw link IDs
    std::string primary_attack_id;
    std::string secondary_attack_id;
    std::string prev_unit_id;
    std::string base_unit_id;
    std::string upgrade_building_id;
    std::string dyn_upg1_id;
    std::string dyn_upg2_id;
    int         dyn_upg_level = 0;

    // resolved pointers (null if not found in registry)
    const AttackDef*     primary_attack = nullptr;
    const AttackDef*     secondary_attack = nullptr;
    const DynUpgradeDef* dyn_upg1 = nullptr;
    const DynUpgradeDef* dyn_upg2 = nullptr;

    // death / bones (Battle.ff)
    int         death_anim_id = 1;
    std::string death_anim_label;            // "L_HUMAN", "L_UNDEAD", …
    std::string death_battle_ff_animation;   // "DEATH_HUMAN_S13", … — full death sequence
    std::string bones_battle_ff_sprite_base; // e.g. "DEAD_HUMAN_SA" — base for OPT frames +00/+01

    // native traits
    std::vector<int>         native_ability_ids;         // M_ABILITY values (GMabi)
    std::vector<std::string> native_immunity_ids;        // IMMUNITY int as string (Gimmu)
    std::vector<std::string> native_immunity_categories; // IMMUNITY int as string (GimmuC)
};

} // namespace d2engine
