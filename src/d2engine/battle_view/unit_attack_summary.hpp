#pragma once

#include "../assets/attack_def.hpp"
#include "../assets/game_data_registry.hpp"
#include "unit_id_helpers.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace d2engine {

enum class AttackType { Melee, Ranged, Aoe, Unknown };
enum class TargetShape { SingleTarget, AllEnemyUnits, Unknown };
enum class PostEffectPresence { None, Present, Unknown };

struct HitPostEffectSummary {
    PostEffectPresence presence = PostEffectPresence::Unknown;
    std::string        kind;
    std::string        raw_id_or_name;
};

struct DamageRange {
    int min = 0;
    int max = 0;
};

struct UnitAttackSummary {
    std::string                unit_type;
    std::string                display_name;
    bool                       size_small = true;
    AttackType                 attack_type = AttackType::Unknown;
    TargetShape                target_shape = TargetShape::Unknown;
    std::optional<int>         damage;
    std::optional<DamageRange> damage_range;
    HitPostEffectSummary       post_effect;
    std::vector<std::string>   warnings;
};

class UnitAttackSummaryExtractor {
public:
    static UnitAttackSummary extract(const UnitDef* unit_def,
                                     const GameDataRegistry& /*unused*/ /*unused*/) {
        UnitAttackSummary summary;

        if (unit_def == nullptr) {
            summary.warnings.emplace_back("UnitDef is null; all fields set to Unknown");
            return summary;
        }

        summary.unit_type = unit_type_from_resource_unit_id(unit_def->unit_id);
        summary.display_name = unit_def->name;
        summary.size_small = unit_def->size_small;

        const AttackDef* attack = unit_def->primary_attack;
        if (attack == nullptr) {
            summary.attack_type = AttackType::Unknown;
            summary.target_shape = TargetShape::Unknown;
            summary.post_effect.presence = PostEffectPresence::Unknown;
            summary.warnings.emplace_back(
                "UnitDef has no primary attack; all fields set to Unknown");
            return summary;
        }

        switch (attack->reach) {
        case AttackReach::Adjacent:
            summary.attack_type = AttackType::Melee;
            summary.target_shape = TargetShape::SingleTarget;
            break;
        case AttackReach::Any:
            summary.attack_type = AttackType::Ranged;
            summary.target_shape = TargetShape::SingleTarget;
            break;
        case AttackReach::All:
            summary.attack_type = AttackType::Aoe;
            summary.target_shape = TargetShape::AllEnemyUnits;
            break;
        case AttackReach::Unknown:
            summary.attack_type = AttackType::Unknown;
            summary.target_shape = TargetShape::Unknown;
            summary.warnings.emplace_back(
                "AttackReach is Unknown; attack_type and target_shape set to Unknown");
            break;
        }

        summary.damage = attack->damage;

        switch (attack->attack_class) {
        case AttackClass::Heal:
            summary.post_effect.presence = PostEffectPresence::None;
            summary.post_effect.kind = "heal";
            summary.warnings.emplace_back(
                "Heal/support action; damage event generation requires manual handling");
            // heal is not a damage attack — clear damage to signal this
            summary.damage.reset();
            break;
        case AttackClass::Poison:
            summary.post_effect.presence = PostEffectPresence::Present;
            summary.post_effect.kind = "poison";
            break;
        case AttackClass::Paralyze:
            summary.post_effect.presence = PostEffectPresence::Present;
            summary.post_effect.kind = "paralyze";
            break;
        case AttackClass::Drain:
        case AttackClass::DrainOverflow:
        case AttackClass::DrainLevel:
            summary.post_effect.presence = PostEffectPresence::Present;
            summary.post_effect.kind = "drain";
            break;
        case AttackClass::BoostDamage:
            summary.post_effect.presence = PostEffectPresence::Present;
            summary.post_effect.kind = "buff";
            break;
        case AttackClass::LowerDamage:
        case AttackClass::LowerInitiative:
            summary.post_effect.presence = PostEffectPresence::Present;
            summary.post_effect.kind = "debuff";
            break;
        case AttackClass::Fear:
            summary.post_effect.presence = PostEffectPresence::Present;
            summary.post_effect.kind = "fear";
            break;
        case AttackClass::Petrify:
            summary.post_effect.presence = PostEffectPresence::Present;
            summary.post_effect.kind = "petrify";
            break;
        case AttackClass::Frostbite:
            summary.post_effect.presence = PostEffectPresence::Present;
            summary.post_effect.kind = "frostbite";
            break;
        case AttackClass::Revive:
            summary.post_effect.presence = PostEffectPresence::Present;
            summary.post_effect.kind = "revive";
            break;
        case AttackClass::Cure:
            summary.post_effect.presence = PostEffectPresence::Present;
            summary.post_effect.kind = "cure";
            break;
        case AttackClass::Summon:
            summary.post_effect.presence = PostEffectPresence::Present;
            summary.post_effect.kind = "summon";
            break;
        case AttackClass::GiveAttack:
            summary.post_effect.presence = PostEffectPresence::Present;
            summary.post_effect.kind = "give_attack";
            break;
        case AttackClass::Doppelganger:
            summary.post_effect.presence = PostEffectPresence::Present;
            summary.post_effect.kind = "doppelganger";
            break;
        case AttackClass::TransformSelf:
            summary.post_effect.presence = PostEffectPresence::Present;
            summary.post_effect.kind = "transform_self";
            break;
        case AttackClass::TransformOther:
            summary.post_effect.presence = PostEffectPresence::Present;
            summary.post_effect.kind = "transform_other";
            break;
        case AttackClass::Blister:
            summary.post_effect.presence = PostEffectPresence::Present;
            summary.post_effect.kind = "blister";
            break;
        case AttackClass::BestowWards:
            summary.post_effect.presence = PostEffectPresence::Present;
            summary.post_effect.kind = "bestow_wards";
            break;
        case AttackClass::Shatter:
            summary.post_effect.presence = PostEffectPresence::Present;
            summary.post_effect.kind = "shatter";
            break;
        case AttackClass::Damage:
            summary.post_effect.presence = PostEffectPresence::None;
            break;
        case AttackClass::Unknown:
            summary.post_effect.presence = PostEffectPresence::Unknown;
            break;
        }

        summary.post_effect.raw_id_or_name = attack->attack_id;

        return summary;
    }
};

inline std::string format_unit_attack_summary_comment(const UnitAttackSummary& summary,
                                                      const std::string&       alias,
                                                      const std::string&       slot) {
    if (summary.unit_type.empty() && summary.display_name.empty()) {
        return {};
    }

    auto type_str = [](AttackType t) -> std::string_view {
        switch (t) {
        case AttackType::Melee:
            return "melee";
        case AttackType::Ranged:
            return "ranged";
        case AttackType::Aoe:
            return "aoe";
        case AttackType::Unknown:
            return "unknown";
        }
    };

    auto shape_str = [](TargetShape s) -> std::string_view {
        switch (s) {
        case TargetShape::SingleTarget:
            return "single_target";
        case TargetShape::AllEnemyUnits:
            return "all_enemy_units";
        case TargetShape::Unknown:
            return "unknown";
        }
    };

    auto effect_str = [](PostEffectPresence p) -> std::string_view {
        switch (p) {
        case PostEffectPresence::None:
            return "none";
        case PostEffectPresence::Present:
            return "present";
        case PostEffectPresence::Unknown:
            return "unknown";
        }
    };

    auto hint = [](AttackType at, TargetShape ts) -> std::string_view {
        if (at == AttackType::Aoe || ts == TargetShape::AllEnemyUnits) {
            return "AttackStarted.targets should include all currently present enemy units.";
        }
        if (at == AttackType::Melee && ts == TargetShape::SingleTarget) {
            return "AttackStarted.targets should contain exactly one enemy unit. Prefer a "
                   "front-row enemy target.";
        }
        if (at == AttackType::Ranged && ts == TargetShape::SingleTarget) {
            return "AttackStarted.targets should contain exactly one enemy unit. Any present enemy "
                   "unit is acceptable.";
        }
        return "Do not auto-generate attack events without manual confirmation.";
    };

    auto post_vis_avail = [](PostEffectPresence p) -> std::string_view {
        switch (p) {
        case PostEffectPresence::Present:
            return "available";
        case PostEffectPresence::None:
            return "none";
        case PostEffectPresence::Unknown:
            return "unknown";
        }
    };

    auto post_hint = [](PostEffectPresence p) -> std::string_view {
        switch (p) {
        case PostEffectPresence::Present:
            return "Post-effect exists; generated scenario should add/comment post-effect handling "
                   "when supported.";
        case PostEffectPresence::None:
            return "No post-effect event needed.";
        case PostEffectPresence::Unknown:
            return "Post-effect behavior unknown; manual confirmation required.";
        }
    };

    auto resource_unit_id = resource_unit_id_from_unit_type(summary.unit_type);

    std::string result;
    result += "// - alias: ";
    result += alias;
    result += "\n//   unit_type: ";
    result += summary.unit_type;
    result += "\n//   resource_unit_id: ";
    result += resource_unit_id;
    result += "\n//   name: ";
    result += summary.display_name;
    result += "\n//   slot: ";
    result += slot;
    result += "\n//   size: ";
    result += summary.size_small ? "small" : "large";
    if (!summary.size_small) {
        // For large units the slot should be CENTER; derive footprint by name substitution.
        std::string fp_front = slot;
        std::string fp_back = slot;
        auto        rpl = [](std::string& s, const std::string& from, const std::string& to) {
            const auto pos = s.find(from);
            if (pos != std::string::npos)
                s.replace(pos, from.size(), to);
        };
        rpl(fp_front, "_CENTER_", "_FRONT_");
        rpl(fp_back, "_CENTER_", "_BACK_");
        result += "\n//   footprint: " + fp_front + " + " + fp_back;
    }
    result += "\n//   attack:\n//     type: ";
    result += type_str(summary.attack_type);
    result += "\n//     target_shape: ";
    result += shape_str(summary.target_shape);
    result += "\n//     damage: ";
    if (summary.damage.has_value()) {
        result += std::to_string(*summary.damage);
    } else {
        result += "unknown";
    }
    result += "\n//     hit_post_effect: ";
    result += effect_str(summary.post_effect.presence);
    if (summary.post_effect.presence == PostEffectPresence::Present) {
        if (!summary.post_effect.kind.empty()) {
            result += "\n//     post_effect_kind: ";
            result += summary.post_effect.kind;
        }
        if (!summary.post_effect.raw_id_or_name.empty()) {
            result += "\n//     post_effect_raw: ";
            result += summary.post_effect.raw_id_or_name;
        }
    }
    result += "\n//   visual:\n"
              "//     source_attack_effect: unknown"
              "\n//     target_impact_effect: unknown"
              "\n//     team_impact_effect: unknown"
              "\n//     projectile_effect: unknown"
              "\n//     post_effect_visual: ";
    result += post_vis_avail(summary.post_effect.presence);
    result += "\n//   event_generation_hint:\n//     ";
    result += hint(summary.attack_type, summary.target_shape);
    result += "\n//     ";
    result += post_hint(summary.post_effect.presence);

    if (!summary.warnings.empty()) {
        result += "\n//   warnings:";
        for (const auto& w : summary.warnings) {
            result += "\n//     - ";
            result += w;
        }
    }

    result += '\n';
    return result;
}

} // namespace d2engine
