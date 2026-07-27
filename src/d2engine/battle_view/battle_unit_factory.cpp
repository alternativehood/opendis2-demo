#include "battle_unit_factory.hpp"

#include "animation_catalog.hpp"
#include "animation_role.hpp"

#include "../assets/game_data_registry.hpp"
#include "../assets/unit_def.hpp"

#include <d2log/log.hpp>

#include <cctype>
#include <stdexcept>
#include <string>

namespace d2engine {

namespace {

std::string unit_id_from_type(const std::string& type) {
    std::string unit_id = "g000";
    for (char const c : type) {
        unit_id += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return unit_id;
}

std::string type_from_unit_id(const std::string& unit_id) {
    if (unit_id.size() <= 4u) {
        return {};
    }
    std::string type = unit_id.substr(4);
    for (char& c : type) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return type;
}

} // namespace

UnitCreationData BattleUnitFactory::create_unit(const UnitCreationRequest& request) {
    UnitCreationData result;
    result.debug_alias = request.alias;
    result.unit_type = request.unit_type;

    const auto opt_coord = parse_position_string(request.slot);
    if (!opt_coord.has_value()) {
        result.diagnostics.push_back("invalid slot: " + request.slot);
        return result;
    }
    result.coord = *opt_coord;
    result.slot_name = request.slot;

    const BattleSlotCoord& coord = result.coord;
    const std::string&     unit_type = request.unit_type;
    const char             direction = (coord.side == BattleSide::Attacker) ? 'A' : 'D';
    result.direction = direction;

    const std::string unit_id = unit_id_from_type(unit_type);
    const UnitDef*    def = registry_.find_unit(unit_id);
    result.is_large = (def != nullptr && !def->size_small);
    result.max_hp = request.max_hp.value_or(def != nullptr ? def->hit_points : 0);
    result.current_hp = request.hp.value_or(result.max_hp);
    result.display_name = (def != nullptr && !def->name.empty()) ? def->name : unit_type;

    try {
        validate_unit_slot(coord, result.is_large, unit_type, request.alias);
    } catch (const std::invalid_argument& e) {
        result.diagnostics.emplace_back(e.what());
        return result;
    }

    std::string animation_unit_type = unit_type;

    UnitStateClip idle_bundle = catalog_.unit_state_bundle(animation_unit_type, "IDLE", direction);
    if (idle_bundle.is_empty() && def != nullptr && !def->base_unit_id.empty()) {
        const std::string base_type = type_from_unit_id(def->base_unit_id);
        if (!base_type.empty() && base_type != animation_unit_type) {
            UnitStateClip base_idle = catalog_.unit_state_bundle(base_type, "IDLE", direction);
            if (!base_idle.is_empty()) {
                animation_unit_type = base_type;
                idle_bundle = std::move(base_idle);
            }
        }
    }
    if (idle_bundle.is_empty()) {
        result.diagnostics.push_back("no idle animation for " + unit_type + " dir=" + direction);
        return result;
    }
    idle_bundle.clip.loop_policy = true;
    result.animation_unit_type = animation_unit_type;

    result.roles.idle = std::move(idle_bundle);
    result.roles.hit = catalog_.unit_state_bundle(animation_unit_type, "HHIT", direction);
    result.roles.attack = catalog_.unit_state_bundle(animation_unit_type, "HMOV", direction);
    result.roles.heff = catalog_.battle_effect_bundle(animation_unit_type, "HEFF", direction);
    result.roles.tuch = catalog_.battle_effect_bundle(animation_unit_type, "TUCH", direction);
    result.roles.death = catalog_.unit_state_bundle(animation_unit_type, "STIL", direction);

    const auto intent_it = intent_map_.find(unit_type);
    if (intent_it != intent_map_.end()) {
        const auto& entry = intent_it->second;
        const char  raw_side = (direction == 'A') ? 'a' : 'd';
        auto        resolve_dir = [&](const UnitAttackFxIntentConfig& cfg) -> char {
            const char side_key = (cfg.direction_basis == DirectionBasis::TargetTeamSide)
                                      ? ((raw_side == 'a') ? 'd' : 'a')
                                      : raw_side;
            if (cfg.direction_by_side.has_value()) {
                const auto it = cfg.direction_by_side->find(side_key);
                return (it != cfg.direction_by_side->end()) ? it->second : '\0';
            }
            if (cfg.direction == '\0') {
                return (side_key == 'a') ? 'A' : 'D';
            }
            return cfg.direction;
        };

        UnitAttackFxOverride ovr;
        if (entry.source_attack_overlay) {
            const char src_dir = resolve_dir(*entry.source_attack_overlay);
            if (src_dir != '\0') {
                ovr.source_attack_overlay = catalog_.battle_effect_bundle(
                    animation_unit_type, entry.source_attack_overlay->family, src_dir);
            }
        }
        if (entry.team_attack_overlay) {
            const char team_dir = resolve_dir(*entry.team_attack_overlay);
            if (team_dir != '\0') {
                ovr.team_attack_overlay = catalog_.battle_effect_bundle(
                    animation_unit_type, entry.team_attack_overlay->family, team_dir);
            }
        }
        if (entry.target_damage_fx) {
            const char td_dir = entry.target_damage_fx->direction;
            ovr.target_damage_fx = catalog_.battle_effect_bundle(
                animation_unit_type, entry.target_damage_fx->family, td_dir);
        }
        if (!ovr.source_attack_overlay.is_empty() || !ovr.team_attack_overlay.is_empty() ||
            !ovr.target_damage_fx.is_empty()) {
            result.roles.attack_fx_override = std::move(ovr);
        }
    }

    if (def != nullptr) {
        result.lifecycle.death.death_fx_name = def->death_battle_ff_animation;
        result.lifecycle.death.death_fx = catalog_.battle_effect(def->death_battle_ff_animation);
        result.lifecycle.death.corpse_sprite_base = def->bones_battle_ff_sprite_base;
        // Corpse frames are OPT images, not animations. Load them through the catalog so
        // DeathBody tracks retain ImageMap canvas/foot metadata for tree/debug tuning.
        result.lifecycle.death.corpse = catalog_.image_sequence(def->bones_battle_ff_sprite_base);
    }
    result.lifecycle.revive.small_fx_name = "REVIVEANIMS";
    result.lifecycle.revive.small_fx = catalog_.battle_effect("REVIVEANIMS");
    result.lifecycle.revive.large_fx_name = "REVIVEANIML";
    result.lifecycle.revive.large_fx = catalog_.battle_effect("REVIVEANIML");
    result.lifecycle.death.stil_clip = result.roles.death;

    result.success = true;
    return result;
}

} // namespace d2engine
