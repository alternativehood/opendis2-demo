// JSON Schema validation for battle_screen.json:
// Schema handles structural checks (top-level keys, required fields, types).
// C++ typed parsers handle semantic validation (asset existence, role/profile resolution).

#include "battle_tuning_state.hpp"
#include "config_schemas.hpp"

#include "../battle_view/battle_render_tree_contract.hpp"

#include <d2log/log.hpp>
#include <nlohmann/json.hpp>
#include <nlohmann/json-schema.hpp>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>

namespace d2engine {

namespace {

[[nodiscard]] double r2(float v) {
    return std::round(static_cast<double>(v) * 100.0) / 100.0;
}

[[nodiscard]] std::optional<BindingRole> load_role(std::string_view key) {
    if (key == "idle")
        return BindingRole::UnitIdle;
    if (key == "attack")
        return BindingRole::UnitAttack;
    if (key == "hit")
        return BindingRole::UnitHit;
    if (key == "base")
        return BindingRole::UnitBase;
    if (key == "source")
        return BindingRole::Source;
    if (key == "target")
        return BindingRole::Target;
    if (key == "target_team")
        return BindingRole::TargetTeam;
    if (key == "global")
        return BindingRole::Global;
    if (key == "corpse")
        return BindingRole::Corpse;
    if (key == "death_fx")
        return BindingRole::DeathFx;
    if (key == "revive_small")
        return BindingRole::ReviveSmall;
    if (key == "revive_large")
        return BindingRole::ReviveLarge;
    if (key == "background")
        return BindingRole::Background;
    if (key == "combat_frame")
        return BindingRole::CombatFrame;
    if (key == "selection_marker")
        return BindingRole::SelectionMarker;
    if (key == "target_marker")
        return BindingRole::TargetMarker;
    if (key == "unit")
        return BindingRole::Unit;
    return std::nullopt;
}

[[nodiscard]] bool is_supported_top_level_key(std::string_view key) {
    for (const std::string_view supported :
         {"battle_effect_default_profiles", "battle_effect_profiles", "debug_overlay_style", "font",
          "layout_metrics", "position_levels", "render_tree", "scene_layout", "sprite_profiles",
          "unit_attack_visual_intents", "unit_lifecycle_profiles",
          "unit_visual_layer_default_profiles", "unit_visual_layer_profiles",
          "unit_visual_profiles"}) {
        if (key == supported) {
            return true;
        }
    }
    return false;
}

void validate_supported_top_level_keys(const nlohmann::json& json) {
    for (const auto& [key, ignored] : json.items()) {
        static_cast<void>(ignored);
        if (!is_supported_top_level_key(key)) {
            d2log::get("d2.app")->critical("unsupported_config_key key={}", key);
            throw std::runtime_error("battle_screen config: unsupported key '" + key + "'");
        }
    }
}

} // namespace

void to_json(nlohmann::json& j, const VisualPlacementValue& v) {
    j = nlohmann::json{{"x", r2(v.x)},
                       {"y", r2(v.y)},
                       {"scale_x", r2(v.scale_x)},
                       {"scale_y", r2(v.scale_y)},
                       {"alpha", r2(v.alpha)},
                       {"level", v.level},
                       {"frame_delay", v.frame_delay}};
}

void from_json(const nlohmann::json& j, VisualPlacementValue& v) {
    v.x = j.value("x", v.x);
    v.y = j.value("y", v.y);
    v.scale_x = j.value("scale_x", v.scale_x);
    v.scale_y = j.value("scale_y", v.scale_y);
    v.alpha = j.value("alpha", v.alpha);
    v.level = j.value("level", v.level);
    v.frame_delay = j.value("frame_delay", v.frame_delay);
}

void write_placement_to_json(nlohmann::json& json, const ConfigBinding& b,
                             const VisualPlacementValue& v) {
    switch (b.owner_kind) {
    case BindingOwnerKind::UnitVisualProfile:
        if (b.side.empty()) {
            json["unit_visual_profiles"][b.tree_path][role_json_key(b.role)] = v;
        } else {
            json["unit_visual_profiles"][b.tree_path][role_json_key(b.role)][b.side] = v;
        }
        break;
    case BindingOwnerKind::UnitVisualLayerProfile: {
        const auto        colon = b.tree_path.rfind(':');
        const std::string unit_id =
            (colon != std::string::npos) ? b.tree_path.substr(0, colon) : b.tree_path;
        const std::string slot_key =
            (colon != std::string::npos) ? b.tree_path.substr(colon + 1) : "";
        if (b.side.empty()) {
            json["unit_visual_layer_profiles"][unit_id][role_json_key(b.role)][slot_key] = v;
        } else {
            json["unit_visual_layer_profiles"][unit_id][role_json_key(b.role)][slot_key][b.side] =
                v;
        }
        break;
    }
    case BindingOwnerKind::UnitVisualLayerDefaultProfile:
        if (b.side.empty()) {
            json["unit_visual_layer_default_profiles"][role_json_key(b.role)][b.tree_path] = v;
        } else {
            json["unit_visual_layer_default_profiles"][role_json_key(b.role)][b.tree_path][b.side] =
                v;
        }
        break;
    case BindingOwnerKind::EffectProfile:
        json["battle_effect_profiles"][b.tree_path][role_json_key(b.role)] = v;
        break;
    case BindingOwnerKind::EffectDefaultProfile: {
        nlohmann::json ev;
        ev["x"] = r2(v.x);
        ev["y"] = r2(v.y);
        ev["scale_x"] = r2(v.scale_x);
        ev["scale_y"] = r2(v.scale_y);
        ev["level"] = v.level;
        if (b.side.empty()) {
            json["battle_effect_default_profiles"][b.tree_path] = ev;
        } else {
            json["battle_effect_default_profiles"][b.tree_path][b.side] = ev;
        }
        break;
    }
    case BindingOwnerKind::SpriteProfile:
        json["sprite_profiles"][b.tree_path] = v;
        break;
    case BindingOwnerKind::LifecycleProfile:
        json["unit_lifecycle_profiles"][b.tree_path][role_json_key(b.role)] = v;
        break;
    case BindingOwnerKind::SceneLayout:
        json["scene_layout"][role_json_key(b.role)] = v;
        break;
    case BindingOwnerKind::TreeLayout:
        break;
    case BindingOwnerKind::PositionLevel:
        json["position_levels"][b.tree_path] = v.level;
        break;
    }
}

void load_battle_tuning_config(BattleTuningState& state, const nlohmann::json& json,
                               const std::filesystem::path& config_path) {
    state.config_file = config_path.string();

    // Schema validation: structural checks before semantic parsing
    try {
        nlohmann::json_schema::json_validator validator;
        nlohmann::json schema_json = nlohmann::json::parse(d2engine::schemas::battle_screen());
        validator.set_root_schema(schema_json);
        validator.validate(json);
    } catch (const std::exception& e) {
        d2log::get("d2.app")->critical("config_schema_error error={}", e.what());
        throw std::runtime_error(std::string("battle_screen config schema: ") + e.what());
    }

    validate_supported_top_level_keys(json);

    if (json.contains("font") && json["font"].is_object()) {
        state.font_face = json["font"].value("face", state.font_face);
    }

    // Unit visual profiles (per-unit-type, per-animation-role, optional side-specific overrides)
    if (json.contains("unit_visual_profiles") && json["unit_visual_profiles"].is_object()) {
        for (const auto& [uid, roles_obj] : json["unit_visual_profiles"].items()) {
            if (!roles_obj.is_object())
                continue;
            for (const auto& [rkey, val] : roles_obj.items()) {
                const auto role = load_role(rkey);
                if (!role || !val.is_object())
                    continue;
                ConfigBinding const binding{
                    .config_file = state.config_file,
                    .owner_kind = BindingOwnerKind::UnitVisualProfile,
                    .tree_path = uid,
                    .role = *role,
                    .display_path =
                        std::string("unit_visual_profiles.").append(uid).append(".").append(rkey)};
                VisualPlacementValue pv;
                from_json(val, pv);
                static_cast<void>(state.set_placement(binding, pv));
                for (const char* const side_key : {"a", "d"}) {
                    if (!val.contains(side_key) || !val[side_key].is_object())
                        continue;
                    ConfigBinding sb = binding;
                    sb.side = side_key;
                    sb.display_path = binding.display_path + "." + side_key;
                    VisualPlacementValue sv;
                    from_json(val[side_key], sv);
                    static_cast<void>(state.set_placement(sb, sv));
                }
            }
        }
    }

    // Per-layer unit visual profiles
    if (json.contains("unit_visual_layer_profiles") &&
        json["unit_visual_layer_profiles"].is_object()) {
        for (const auto& [uid, roles_obj] : json["unit_visual_layer_profiles"].items()) {
            if (!roles_obj.is_object())
                continue;
            for (const auto& [rkey, slots_obj] : roles_obj.items()) {
                const auto role = load_role(rkey);
                if (!role || !slots_obj.is_object())
                    continue;
                for (const auto& [slot_key, val] : slots_obj.items()) {
                    if (!val.is_object())
                        continue;
                    std::string owner_id = uid;
                    owner_id.append(":").append(slot_key);
                    std::string display_path = "unit_visual_layer_profiles.";
                    display_path.append(uid).append(".").append(rkey).append(".").append(slot_key);
                    ConfigBinding const  binding{.config_file = state.config_file,
                                                 .owner_kind =
                                                     BindingOwnerKind::UnitVisualLayerProfile,
                                                 .tree_path = owner_id,
                                                 .role = *role,
                                                 .display_path = display_path};
                    VisualPlacementValue pv;
                    from_json(val, pv);
                    static_cast<void>(state.set_placement(binding, pv));
                    for (const char* const side_key : {"a", "d"}) {
                        if (!val.contains(side_key) || !val[side_key].is_object())
                            continue;
                        ConfigBinding sb = binding;
                        sb.side = side_key;
                        sb.display_path = binding.display_path + "." + side_key;
                        VisualPlacementValue sv;
                        from_json(val[side_key], sv);
                        static_cast<void>(state.set_placement(sb, sv));
                    }
                }
            }
        }
    }

    // Typed effect profiles
    if (json.contains("battle_effect_profiles") && json["battle_effect_profiles"].is_object()) {
        for (const auto& [eid, roles_obj] : json["battle_effect_profiles"].items()) {
            if (!roles_obj.is_object())
                continue;
            for (const auto& [rkey, val] : roles_obj.items()) {
                const auto role = load_role(rkey);
                if (!role || !val.is_object())
                    continue;
                ConfigBinding binding{.config_file = state.config_file,
                                      .owner_kind = BindingOwnerKind::EffectProfile,
                                      .tree_path = eid,
                                      .role = *role};
                binding.display_path = "battle_effect_profiles.";
                binding.display_path.append(eid).append(".").append(rkey);
                VisualPlacementValue pv;
                from_json(val, pv);
                static_cast<void>(state.set_placement(binding, pv));
            }
        }
    }

    // Typed lifecycle profiles
    if (json.contains("unit_lifecycle_profiles") && json["unit_lifecycle_profiles"].is_object()) {
        for (const auto& [pid, roles_obj] : json["unit_lifecycle_profiles"].items()) {
            if (!roles_obj.is_object())
                continue;
            for (const auto& [rkey, val] : roles_obj.items()) {
                const auto role = load_role(rkey);
                if (!role || !val.is_object())
                    continue;
                ConfigBinding binding{.config_file = state.config_file,
                                      .owner_kind = BindingOwnerKind::LifecycleProfile,
                                      .tree_path = pid,
                                      .role = *role};
                binding.display_path = "unit_lifecycle_profiles.";
                binding.display_path.append(pid).append(".").append(rkey);
                VisualPlacementValue pv;
                from_json(val, pv);
                static_cast<void>(state.set_placement(binding, pv));
            }
        }
    }

    // Shared sprite profiles
    if (json.contains("sprite_profiles") && json["sprite_profiles"].is_object()) {
        for (const auto& [sid, val] : json["sprite_profiles"].items()) {
            if (!val.is_object())
                continue;
            ConfigBinding binding{.config_file = state.config_file,
                                  .owner_kind = BindingOwnerKind::SpriteProfile,
                                  .tree_path = sid,
                                  .role = BindingRole::Global};
            binding.display_path = "sprite_profiles." + sid;
            VisualPlacementValue pv;
            from_json(val, pv);
            static_cast<void>(state.set_placement(binding, pv));
        }
    }

    // Scene layout
    if (json.contains("scene_layout") && json["scene_layout"].is_object()) {
        for (const auto& [rkey, val] : json["scene_layout"].items()) {
            const auto role = load_role(rkey);
            if (!role || !val.is_object())
                continue;
            ConfigBinding const  binding{.config_file = state.config_file,
                                         .owner_kind = BindingOwnerKind::SceneLayout,
                                         .tree_path = "scene",
                                         .role = *role,
                                         .display_path = "scene_layout." + rkey};
            VisualPlacementValue pv;
            from_json(val, pv);
            static_cast<void>(state.set_placement(binding, pv));
        }
    }

    // Position levels
    if (json.contains("position_levels") && json["position_levels"].is_object()) {
        for (const auto& [pos_key, val] : json["position_levels"].items()) {
            if (!val.is_number_integer())
                continue;
            const ConfigBinding binding{.config_file = state.config_file,
                                        .owner_kind = BindingOwnerKind::PositionLevel,
                                        .tree_path = pos_key,
                                        .role = BindingRole::Unit,
                                        .display_path = "position_levels." + pos_key};
            static_cast<void>(
                state.set_placement(binding, VisualPlacementValue{.level = val.get<int>()}));
        }
    }

    // Shared layer-default profiles
    if (json.contains("unit_visual_layer_default_profiles") &&
        json["unit_visual_layer_default_profiles"].is_object()) {
        for (const auto& [rkey, slots_obj] : json["unit_visual_layer_default_profiles"].items()) {
            const auto role = load_role(rkey);
            if (!role || !slots_obj.is_object())
                continue;
            for (const auto& [slot_key, val] : slots_obj.items()) {
                if (!val.is_object())
                    continue;
                ConfigBinding const binding{
                    .config_file = state.config_file,
                    .owner_kind = BindingOwnerKind::UnitVisualLayerDefaultProfile,
                    .tree_path = slot_key,
                    .role = *role,
                    .display_path = [&] {
                        std::string path = "unit_visual_layer_default_profiles.";
                        path.append(rkey).append(".").append(slot_key);
                        return path;
                    }()};
                if (val.contains("x") || val.contains("alpha") || val.contains("scale_x") ||
                    val.contains("level") || val.contains("frame_delay")) {
                    VisualPlacementValue pv;
                    from_json(val, pv);
                    static_cast<void>(state.set_placement(binding, pv));
                }
                for (const char* const side_key : {"a", "d"}) {
                    if (!val.contains(side_key) || !val[side_key].is_object())
                        continue;
                    ConfigBinding sb = binding;
                    sb.side = side_key;
                    sb.display_path = binding.display_path + "." + side_key;
                    VisualPlacementValue sv;
                    from_json(val[side_key], sv);
                    static_cast<void>(state.set_placement(sb, sv));
                }
            }
        }
    }

    // Shared effect-default profiles
    if (json.contains("battle_effect_default_profiles") &&
        json["battle_effect_default_profiles"].is_object()) {
        const auto infer_role = [](std::string_view id) -> std::optional<BindingRole> {
            if (id == "source_attack_overlay" || id == "source_cast")
                return BindingRole::Source;
            if (id == "target_damage")
                return BindingRole::Target;
            if (id == "team_attack" || id == "target_team")
                return BindingRole::TargetTeam;
            return std::nullopt;
        };
        for (const auto& [owner_id, val] : json["battle_effect_default_profiles"].items()) {
            if (!val.is_object())
                continue;
            const auto role = infer_role(owner_id);
            if (!role)
                continue;
            ConfigBinding const binding{.config_file = state.config_file,
                                        .owner_kind = BindingOwnerKind::EffectDefaultProfile,
                                        .tree_path = owner_id,
                                        .role = *role,
                                        .display_path =
                                            "battle_effect_default_profiles." + owner_id};
            if (val.contains("x")) {
                VisualPlacementValue pv;
                from_json(val, pv);
                static_cast<void>(state.set_placement(binding, pv));
            }
            for (const char* const side_key : {"a", "d"}) {
                if (!val.contains(side_key) || !val[side_key].is_object())
                    continue;
                ConfigBinding sb = binding;
                sb.side = side_key;
                sb.display_path = binding.display_path + "." + side_key;
                VisualPlacementValue sv;
                from_json(val[side_key], sv);
                static_cast<void>(state.set_placement(sb, sv));
            }
        }
    }

    // Layout metrics — reference canvas size, slot hitbox, battlefield rect
    if (json.contains("layout_metrics") && json["layout_metrics"].is_object()) {
        const auto& lm = json["layout_metrics"];
        if (lm.contains("reference_size") && lm["reference_size"].is_object()) {
            state.layout_metrics.ref_w =
                lm["reference_size"].value("w", state.layout_metrics.ref_w);
            state.layout_metrics.ref_h =
                lm["reference_size"].value("h", state.layout_metrics.ref_h);
        }
        if (lm.contains("slot_hitbox") && lm["slot_hitbox"].is_object()) {
            state.layout_metrics.slot_w = lm["slot_hitbox"].value("w", state.layout_metrics.slot_w);
            state.layout_metrics.slot_h = lm["slot_hitbox"].value("h", state.layout_metrics.slot_h);
        }
        if (lm.contains("battlefield_reference_rect") &&
            lm["battlefield_reference_rect"].is_object()) {
            const auto& br = lm["battlefield_reference_rect"];
            state.layout_metrics.battlefield_rect.x =
                br.value("x", state.layout_metrics.battlefield_rect.x);
            state.layout_metrics.battlefield_rect.y =
                br.value("y", state.layout_metrics.battlefield_rect.y);
            state.layout_metrics.battlefield_rect.w =
                br.value("w", state.layout_metrics.battlefield_rect.w);
            state.layout_metrics.battlefield_rect.h =
                br.value("h", state.layout_metrics.battlefield_rect.h);
        }
    }

    // Debug overlay style — colours and radii
    if (json.contains("debug_overlay_style") && json["debug_overlay_style"].is_object()) {
        const auto& ds = json["debug_overlay_style"];
        state.debug_overlay_style.slot_dot_radius =
            ds.value("slot_dot_radius", state.debug_overlay_style.slot_dot_radius);
        state.debug_overlay_style.center_dot_radius =
            ds.value("center_dot_radius", state.debug_overlay_style.center_dot_radius);
        state.debug_overlay_style.mount_marker_radius =
            ds.value("mount_marker_radius", state.debug_overlay_style.mount_marker_radius);
        if (ds.contains("label_offset") && ds["label_offset"].is_object()) {
            state.debug_overlay_style.label_offset_x =
                ds["label_offset"].value("x", state.debug_overlay_style.label_offset_x);
            state.debug_overlay_style.label_offset_y =
                ds["label_offset"].value("y", state.debug_overlay_style.label_offset_y);
        }
        auto load_color = [&](const char* key, Color& out) {
            if (!ds.contains("colors") || !ds["colors"].is_object())
                return;
            if (!ds["colors"].contains(key) || !ds["colors"][key].is_object())
                return;
            const auto& c = ds["colors"][key];
            out.r = static_cast<uint8_t>(c.value("r", static_cast<int>(out.r)));
            out.g = static_cast<uint8_t>(c.value("g", static_cast<int>(out.g)));
            out.b = static_cast<uint8_t>(c.value("b", static_cast<int>(out.b)));
            out.a = static_cast<uint8_t>(c.value("a", static_cast<int>(out.a)));
        };
        load_color("attacker_back", state.debug_overlay_style.slot_attacker_back);
        load_color("attacker_front", state.debug_overlay_style.slot_attacker_front);
        load_color("defender_back", state.debug_overlay_style.slot_defender_back);
        load_color("defender_front", state.debug_overlay_style.slot_defender_front);
        load_color("center", state.debug_overlay_style.center_color);
        load_color("lane_line", state.debug_overlay_style.lane_line);
        load_color("depth_line", state.debug_overlay_style.depth_line);
        load_color("mount", state.debug_overlay_style.mount_color);
    }

    // Per-unit attack visual intents (read-only; not stored as placements)
    state.attack_visual_intents.clear();
    if (json.contains("unit_attack_visual_intents") &&
        json["unit_attack_visual_intents"].is_object()) {
        for (const auto& [unit_type, entry_obj] : json["unit_attack_visual_intents"].items()) {
            if (!entry_obj.is_object())
                continue;
            UnitAttackVisualIntentEntry entry;
            auto warn_partial_dbs = [](const char* key, const std::map<char, char>& dbs) {
                if (dbs.size() == 1u) {
                    const char present = dbs.begin()->first;
                    const char missing = (present == 'a') ? 'd' : 'a';
                    d2log::get("d2.app")->warn(
                        "partial_direction_by_side key={} present_side={} missing_side={}", key,
                        present, missing);
                }
            };
            auto load_fx = [&](const char* key) -> std::optional<UnitAttackFxIntentConfig> {
                if (!entry_obj.contains(key) || !entry_obj[key].is_object())
                    return std::nullopt;
                const auto& obj = entry_obj[key];
                if (!obj.contains("family") || !obj["family"].is_string())
                    return std::nullopt;
                UnitAttackFxIntentConfig cfg;
                cfg.family = obj["family"].get<std::string>();
                cfg.direction = '\0';
                cfg.direction_basis = DirectionBasis::SourceSide;
                if (obj.contains("direction_basis") && obj["direction_basis"].is_string()) {
                    const std::string basis_str = obj["direction_basis"].get<std::string>();
                    if (basis_str == "target_team_side") {
                        cfg.direction_basis = DirectionBasis::TargetTeamSide;
                    }
                }
                if (obj.contains("direction_by_side") && obj["direction_by_side"].is_object()) {
                    cfg.direction_by_side.emplace();
                    for (const auto& [side_str, dir_val] : obj["direction_by_side"].items()) {
                        if (side_str.empty() || !dir_val.is_string())
                            continue;
                        const std::string dv = dir_val.get<std::string>();
                        if (!dv.empty()) {
                            (*cfg.direction_by_side)[side_str[0]] = dv[0];
                        }
                    }
                    warn_partial_dbs(key, *cfg.direction_by_side);
                } else if (obj.contains("direction") && obj["direction"].is_string()) {
                    const std::string dir_str = obj["direction"].get<std::string>();
                    if (dir_str.empty())
                        return std::nullopt;
                    cfg.direction = (dir_str == "side_preferred") ? '\0' : dir_str[0];
                } else {
                    return std::nullopt;
                }
                return cfg;
            };
            entry.source_attack_overlay = load_fx("source_attack_overlay");
            // Hard guard: partial direction_by_side on source_attack_overlay is a config bug
            if (entry.source_attack_overlay.has_value() &&
                entry.source_attack_overlay->direction_by_side.has_value() &&
                entry.source_attack_overlay->direction_by_side->size() == 1u) {
                d2log::get("d2.app")->critical(
                    "partial_source_attack_overlay unit_type={} "
                    "hint=use_direction_side_preferred_or_add_both_sides",
                    unit_type);
                throw std::runtime_error(
                    "battle_screen config: partial direction_by_side on source_attack_overlay for "
                    "unit '" +
                    unit_type + "'; add both sides or use direction_side_preferred");
            }
            entry.team_attack_overlay = load_fx("team_attack_overlay");
            entry.target_damage_fx = load_fx("target_damage_fx");
            state.attack_visual_intents.emplace(unit_type, std::move(entry));
        }
    }

    state.mark_saved();
}

void write_battle_tuning_config(nlohmann::json& json, const BattleTuningState& state) {
    for (const char* sec :
         {"battle_effect_profiles", "battle_effect_default_profiles", "unit_visual_profiles",
          "unit_visual_layer_profiles", "unit_visual_layer_default_profiles",
          "unit_lifecycle_profiles", "sprite_profiles", "scene_layout", "position_levels"}) {
        json.erase(sec);
    }
    if (!state.attack_visual_intents.empty()) {
        json.erase("unit_attack_visual_intents");
    }

    json["font"]["face"] = state.font_face;

    const VisualPlacementValue kIdentity{};
    for (const auto& [key, binding] : state.binding_registry) {
        const auto it = state.placements.find(key);
        if (it == state.placements.end())
            continue;
        if (binding.owner_kind == BindingOwnerKind::UnitVisualLayerProfile &&
            it->second == kIdentity) {
            continue;
        }
        write_placement_to_json(json, binding, it->second);
    }

    for (const auto& [pos_key, lvl] : state.position_levels) {
        json["position_levels"][pos_key] = lvl;
    }

    for (const auto& [unit_type, entry] : state.attack_visual_intents) {
        auto save_fx = [&](const char* key, const std::optional<UnitAttackFxIntentConfig>& fx) {
            if (!fx)
                return;
            auto& obj = json["unit_attack_visual_intents"][unit_type][key];
            obj["family"] = fx->family;
            constexpr auto default_basis_for_key = DirectionBasis::SourceSide;
            if (fx->direction_basis != default_basis_for_key) {
                obj["direction_basis"] = (fx->direction_basis == DirectionBasis::TargetTeamSide)
                                             ? "target_team_side"
                                             : "source_side";
            }
            if (fx->direction_by_side.has_value()) {
                auto& dbs = obj["direction_by_side"];
                dbs = nlohmann::json::object();
                for (const auto& [side_ch, dir_ch] : *fx->direction_by_side) {
                    dbs[std::string(1, side_ch)] = std::string(1, dir_ch);
                }
            } else {
                const std::string dir_val =
                    (fx->direction == '\0') ? "side_preferred" : std::string(1, fx->direction);
                obj["direction"] = dir_val;
            }
        };
        save_fx("source_attack_overlay", entry.source_attack_overlay);
        save_fx("team_attack_overlay", entry.team_attack_overlay);
        save_fx("target_damage_fx", entry.target_damage_fx);
    }
}

} // namespace d2engine
