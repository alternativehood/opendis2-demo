#include "battle_scenario_runtime.hpp"

#include "battle_scene.hpp"
#include "battle_unit.hpp"
#include "visual_track.hpp"

#include <sstream>
#include <stdexcept>

namespace d2engine {

namespace {

BattleSlotCoord parse_slot_or_throw(const std::string& slot_name, const std::string& envelope_id,
                                    const std::string& step_id, const std::string& sequence_id,
                                    const std::string& scenario_path) {
    auto opt = parse_position_string(slot_name);
    if (!opt.has_value()) {
        std::ostringstream msg;
        msg << "scenario=" << scenario_path;
        if (!sequence_id.empty())
            msg << " seq=" << sequence_id;
        if (!step_id.empty())
            msg << " step=" << step_id;
        if (!envelope_id.empty())
            msg << " envelope=" << envelope_id;
        msg << " invalid slot: " << slot_name;
        throw std::runtime_error(msg.str());
    }
    return *opt;
}

} // namespace

BattleScenarioRuntime::BattleScenarioRuntime(BattleScene&                scene,
                                             IBattlePresentationSink&    presentation,
                                             IBattleUnitCreationService& factory)
    : scene_(scene), presentation_(presentation), factory_(factory) {}

void BattleScenarioRuntime::ensure_unit_footprint_free(BattleSlotCoord coord, bool is_large,
                                                       std::string_view unit_type,
                                                       std::string_view alias) const {
    const auto occupant_info = [&](BattleSlotCoord slot) -> std::string {
        auto it = slot_occupant_.find(slot);
        if (it == slot_occupant_.end())
            return {};
        const UnitInstanceId occupant_id = it->second;
        auto                 type_it = unit_id_to_unit_type_.find(occupant_id);
        const std::string    occupant_type =
            type_it != unit_id_to_unit_type_.end() ? type_it->second : "?";
        return " occupant_id=" + std::to_string(occupant_id.value) +
               " occupant_type=" + occupant_type;
    };

    if (has_slot_occupant(coord)) {
        std::string fp_list;
        if (is_large) {
            for (const auto& fp : large_footprint_slots(coord))
                fp_list += " " + slot_coord_to_string(fp);
        }
        throw std::runtime_error("footprint_conflict slot=" + slot_coord_to_string(coord) +
                                 " alias=" + std::string{alias} +
                                 " unit_type=" + std::string{unit_type} + " footprint=[" + fp_list +
                                 "]" + occupant_info(coord));
    }
    // Large unit: identity slot is CENTER, footprint is FRONT+BACK — all three must be free.
    if (is_large) {
        for (const auto& fp : large_footprint_slots(coord)) {
            if (has_slot_occupant(fp)) {
                std::string fp_list;
                for (const auto& s : large_footprint_slots(coord))
                    fp_list += " " + slot_coord_to_string(s);
                throw std::runtime_error(
                    "footprint_conflict slot=" + slot_coord_to_string(coord) +
                    " alias=" + std::string{alias} + " unit_type=" + std::string{unit_type} +
                    " footprint=[" + fp_list + "]" + " occupied_slot=" + slot_coord_to_string(fp) +
                    occupant_info(fp));
            }
        }
    }
}

BattleScenarioRuntime::CreateResult
BattleScenarioRuntime::apply_unit_impl(const std::string& alias, const UnitCreationData& creation) {
    // Checks first — no mutations until both pass.
    try {
        validate_unit_slot(creation.coord, creation.is_large, creation.unit_type, alias);
    } catch (const std::invalid_argument& e) {
        throw std::runtime_error(e.what());
    }
    ensure_unit_footprint_free(creation.coord, creation.is_large, creation.unit_type, alias);

    // Mutations start here.
    const UnitVisualProfileId visual_profile_id = presentation_.add_visual_profile(creation.roles);
    const UnitLifecycleVisualProfileId lifecycle_profile_id =
        presentation_.add_lifecycle_profile(creation.unit_type, creation.lifecycle);

    BattleUnit unit;
    unit.coord = creation.coord;
    unit.direction = creation.direction;
    unit.is_large = creation.is_large;
    unit.unit_type = creation.unit_type;
    unit.animation_unit_type = creation.animation_unit_type;
    unit.display_name = creation.display_name.empty() ? creation.unit_type : creation.display_name;
    unit.current_hp = creation.current_hp;
    unit.max_hp = creation.max_hp;
    unit.visual_profile_id = visual_profile_id;
    unit.lifecycle_profile_id = lifecycle_profile_id;

    const auto* roles = presentation_.unit_profiles().roles(visual_profile_id);

    VisualTrack base_track;
    base_track.kind = TrackKind::Base;
    base_track.layer = TrackRenderLayer::Base;
    base_track.anchor = AnchorPolicy::UnitCanvasFoot;
    base_track.effect_role = BindingRole::UnitIdle;

    if (roles != nullptr) {
        base_track.layered_player = {.clip = &roles->idle.clip, .elapsed_ms = 0, .looping = true};
    }
    const AnimationSequence* drv = roles != nullptr ? driver_sequence(roles->idle.clip) : nullptr;
    if (drv != nullptr) {
        AnimationSequence seq = *drv;
        seq.is_looping = true;
        base_track.player.load(std::move(seq));
    }
    base_track.player.play();
    unit.tracks.push_back(std::move(base_track));

    scene_.add_unit(std::move(unit));

    const auto&          added_unit = scene_.units().back();
    const UnitInstanceId unit_id = added_unit.unit_instance_id;
    const VisualEntityId entity_id = added_unit.id;

    alias_to_unit_id_.emplace(alias, unit_id);
    unit_id_to_entity_id_.emplace(unit_id, entity_id);
    unit_id_to_unit_type_.emplace(unit_id, creation.unit_type);
    // Identity slot (CENTER for large, FRONT/BACK for small) and footprint slots (FRONT+BACK for
    // large) — stored together in the occupancy index.
    slot_occupant_.emplace(creation.coord, unit_id);
    if (creation.is_large) {
        for (const auto& fp : large_footprint_slots(creation.coord)) {
            slot_occupant_.emplace(fp, unit_id);
        }
    }
    unit_id_to_coord_.emplace(unit_id, creation.coord);
    alive_units_.insert(unit_id);
    retreated_aliases_.erase(alias);

    presentation_.on_unit_created(unit_id);

    return CreateResult{.unit_id = unit_id,
                        .entity_id = entity_id,
                        .unit_type = creation.unit_type,
                        .animation_unit_type = creation.animation_unit_type,
                        .coord = creation.coord,
                        .debug_alias = alias};
}

BattleScenarioRuntime::CreateResult BattleScenarioRuntime::apply_unit_created(
    const ScenarioUnitCreated& event, const std::string& envelope_id, const std::string& step_id,
    const std::string& sequence_id, const std::string& scenario_path) {
    if (has_alias(event.alias)) {
        throw std::runtime_error(make_error("duplicate alias: " + event.alias, envelope_id, step_id,
                                            sequence_id, scenario_path, "UnitCreated")
                                     .detail);
    }

    // Slot parse only — footprint conflict check is in apply_unit_impl (SOT).
    parse_slot_or_throw(event.slot, envelope_id, step_id, sequence_id, scenario_path);

    auto creation = factory_.create_unit(UnitCreationRequest{.alias = event.alias,
                                                             .unit_type = event.unit_type,
                                                             .slot = event.slot,
                                                             .hp = event.hp,
                                                             .max_hp = event.max_hp,
                                                             .status = event.status});
    if (!creation.success) {
        std::string diag;
        for (const auto& d : creation.diagnostics) {
            if (!diag.empty())
                diag += "; ";
            diag += d;
        }
        throw std::runtime_error(make_error("unit creation failed: " + diag, envelope_id, step_id,
                                            sequence_id, scenario_path, "UnitCreated")
                                     .detail);
    }
    if (creation.max_hp <= 0) {
        throw std::runtime_error(make_error("alias " + event.alias + " max_hp must be > 0",
                                            envelope_id, step_id, sequence_id, scenario_path,
                                            "UnitCreated")
                                     .detail);
    }
    if (creation.current_hp < 0 || creation.current_hp > creation.max_hp) {
        throw std::runtime_error(make_error("alias " + event.alias + " hp must be within [0," +
                                                std::to_string(creation.max_hp) + "]",
                                            envelope_id, step_id, sequence_id, scenario_path,
                                            "UnitCreated")
                                     .detail);
    }

    return apply_unit_impl(event.alias, creation);
}

BattleScenarioRuntime::CreateResult BattleScenarioRuntime::apply_unit_from_data(
    const std::string& alias, const UnitCreationData& data, const std::string& envelope_id,
    const std::string& step_id, const std::string& sequence_id, const std::string& scenario_path) {
    if (has_alias(alias)) {
        throw std::runtime_error(make_error("duplicate alias: " + alias, envelope_id, step_id,
                                            sequence_id, scenario_path, "UnitCreated")
                                     .detail);
    }
    if (data.max_hp <= 0) {
        throw std::runtime_error(make_error("alias " + alias + " max_hp must be > 0", envelope_id,
                                            step_id, sequence_id, scenario_path, "UnitCreated")
                                     .detail);
    }
    if (data.current_hp < 0 || data.current_hp > data.max_hp) {
        throw std::runtime_error(make_error("alias " + alias + " hp must be within [0," +
                                                std::to_string(data.max_hp) + "]",
                                            envelope_id, step_id, sequence_id, scenario_path,
                                            "UnitCreated")
                                     .detail);
    }
    // Footprint conflict check is in apply_unit_impl (SOT).
    return apply_unit_impl(alias, data);
}

BattleScenarioRuntime::RetreatResult BattleScenarioRuntime::apply_unit_retreated(
    const ScenarioUnitRetreated& event, const std::string& envelope_id, const std::string& step_id,
    const std::string& sequence_id, const std::string& scenario_path) {
    auto it = alias_to_unit_id_.find(event.unit);
    if (it == alias_to_unit_id_.end()) {
        throw std::runtime_error(make_error("unknown alias: " + event.unit, envelope_id, step_id,
                                            sequence_id, scenario_path, "UnitRetreated")
                                     .detail);
    }

    const UnitInstanceId unit_id = it->second;
    auto                 entity_it = unit_id_to_entity_id_.find(unit_id);
    if (entity_it == unit_id_to_entity_id_.end()) {
        throw std::runtime_error(make_error("no entity for alias: " + event.unit, envelope_id,
                                            step_id, sequence_id, scenario_path, "UnitRetreated")
                                     .detail);
    }

    const VisualEntityId entity_id = entity_it->second;

    scene_.remove_unit(entity_id);
    presentation_.on_unit_retreated(unit_id);

    {
        auto coord_it = unit_id_to_coord_.find(unit_id);
        if (coord_it != unit_id_to_coord_.end()) {
            const BattleSlotCoord coord = coord_it->second;
            slot_occupant_.erase(coord);
            if (is_center_slot(coord)) {
                for (const auto& fp : large_footprint_slots(coord)) {
                    slot_occupant_.erase(fp);
                }
            }
            unit_id_to_coord_.erase(coord_it);
        }
    }
    alias_to_unit_id_.erase(it);
    unit_id_to_entity_id_.erase(unit_id);
    unit_id_to_unit_type_.erase(unit_id);
    alive_units_.erase(unit_id);
    retreated_aliases_.insert(event.unit);

    return RetreatResult{.unit_id = unit_id};
}

UnitInstanceId BattleScenarioRuntime::resolve(const std::string& alias,
                                              const std::string& envelope_id,
                                              const std::string& step_id,
                                              const std::string& sequence_id,
                                              const std::string& scenario_path) const {
    if (retreated_aliases_.contains(alias)) {
        throw std::runtime_error(
            make_error("alias '" + alias + "' has retreated and is no longer in runtime",
                       envelope_id, step_id, sequence_id, scenario_path)
                .detail);
    }
    auto it = alias_to_unit_id_.find(alias);
    if (it == alias_to_unit_id_.end()) {
        throw std::runtime_error(
            make_error("unknown alias: " + alias, envelope_id, step_id, sequence_id, scenario_path)
                .detail);
    }
    return it->second;
}

std::vector<UnitInstanceId>
BattleScenarioRuntime::resolve_targets(const std::vector<std::string>& aliases,
                                       const std::string& envelope_id, const std::string& step_id,
                                       const std::string& sequence_id,
                                       const std::string& scenario_path) const {
    std::vector<UnitInstanceId> result;
    result.reserve(aliases.size());
    for (const auto& a : aliases) {
        result.push_back(resolve(a, envelope_id, step_id, sequence_id, scenario_path));
    }
    return result;
}

void BattleScenarioRuntime::validate_unit_hp(UnitInstanceId id, int hp, const std::string& alias,
                                             const std::string& event_type, int min_hp) const {
    const auto  entity_it = unit_id_to_entity_id_.find(id);
    const auto* unit = entity_it != unit_id_to_entity_id_.end()
                           ? scene_.try_unit_by_id(entity_it->second)
                           : nullptr;
    if (unit == nullptr) {
        throw std::runtime_error(event_type + " alias " + alias +
                                 " has no live BattleUnit for HP update");
    }
    if (unit->max_hp <= 0) {
        throw std::runtime_error(event_type + " alias " + alias + " has invalid max_hp");
    }
    if (hp < min_hp || hp > unit->max_hp) {
        throw std::runtime_error(event_type + " alias " + alias + " hp must be within [" +
                                 std::to_string(min_hp) + "," + std::to_string(unit->max_hp) + "]");
    }
}

bool BattleScenarioRuntime::has_alias(const std::string& alias) const {
    return alias_to_unit_id_.contains(alias);
}

bool BattleScenarioRuntime::has_slot_occupant(const BattleSlotCoord& coord) const {
    return slot_occupant_.contains(coord);
}

std::optional<UnitInstanceId> BattleScenarioRuntime::find_by_alias(const std::string& alias) const {
    auto it = alias_to_unit_id_.find(alias);
    if (it == alias_to_unit_id_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<std::pair<UnitInstanceId, std::string>>
BattleScenarioRuntime::active_unit_metadata() const {
    std::vector<std::pair<UnitInstanceId, std::string>> result;
    result.reserve(unit_id_to_unit_type_.size());
    for (const auto& [id, type] : unit_id_to_unit_type_) {
        result.emplace_back(id, type);
    }
    return result;
}

ScenarioRuntimeError
BattleScenarioRuntime::make_error(const std::string& detail, const std::string& envelope_id,
                                  const std::string& step_id, const std::string& sequence_id,
                                  const std::string& scenario_path, const std::string& event_type) {
    ScenarioRuntimeError err;
    err.scenario_path = scenario_path;
    err.sequence_id = sequence_id;
    err.step_id = step_id;
    err.envelope_id = envelope_id;
    err.event_type = event_type;
    err.detail = detail;
    return err;
}

} // namespace d2engine
