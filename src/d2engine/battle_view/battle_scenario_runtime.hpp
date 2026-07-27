#pragma once

#include "battle_ids.hpp"
#include "battle_scenario_data.hpp"
#include "battle_slot.hpp"
#include "i_battle_presentation_sink.hpp"
#include "i_battle_unit_creation_service.hpp"
#include "unit_creation_types.hpp"

#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace d2engine {

class BattleScene;

struct ScenarioRuntimeError {
    std::string scenario_path;
    std::string sequence_id;
    std::string step_id;
    std::string envelope_id;
    std::string event_type;
    std::string detail;
};

class BattleScenarioRuntime {
public:
    struct CreateResult {
        UnitInstanceId  unit_id;
        VisualEntityId  entity_id;
        std::string     unit_type;
        std::string     animation_unit_type;
        BattleSlotCoord coord;
        std::string     debug_alias;
    };

    struct RetreatResult {
        UnitInstanceId unit_id;
    };

    BattleScenarioRuntime(BattleScene& scene, IBattlePresentationSink& presentation,
                          IBattleUnitCreationService& factory);

    CreateResult apply_unit_created(const ScenarioUnitCreated& event,
                                    const std::string&         envelope_id = {},
                                    const std::string&         step_id = {},
                                    const std::string&         sequence_id = {},
                                    const std::string&         scenario_path = {});

    // Replay pre-built unit data without calling the factory (used during transactional commit).
    CreateResult apply_unit_from_data(const std::string& alias, const UnitCreationData& data,
                                      const std::string& envelope_id = {},
                                      const std::string& step_id = {},
                                      const std::string& sequence_id = {},
                                      const std::string& scenario_path = {});

    RetreatResult apply_unit_retreated(const ScenarioUnitRetreated& event,
                                       const std::string&           envelope_id = {},
                                       const std::string&           step_id = {},
                                       const std::string&           sequence_id = {},
                                       const std::string&           scenario_path = {});

    UnitInstanceId resolve(const std::string& alias, const std::string& envelope_id = {},
                           const std::string& step_id = {}, const std::string& sequence_id = {},
                           const std::string& scenario_path = {}) const;

    std::vector<UnitInstanceId> resolve_targets(const std::vector<std::string>& aliases,
                                                const std::string&              envelope_id = {},
                                                const std::string&              step_id = {},
                                                const std::string&              sequence_id = {},
                                                const std::string& scenario_path = {}) const;

    void validate_unit_hp(UnitInstanceId id, int hp, const std::string& alias,
                          const std::string& event_type, int min_hp = 0) const;

    bool                          has_alias(const std::string& alias) const;
    bool                          has_slot_occupant(const BattleSlotCoord& coord) const;
    std::optional<UnitInstanceId> find_by_alias(const std::string& alias) const;

    // Returns debug metadata for all active units (unit_id, unit_type) pairs.
    // Used by Application to keep the HUD/debug overlay in sync with dynamic
    // UnitCreated/UnitRetreated during scenario playback.
    [[nodiscard]] std::vector<std::pair<UnitInstanceId, std::string>> active_unit_metadata() const;

private:
    [[nodiscard]] static ScenarioRuntimeError
    make_error(const std::string& detail, const std::string& envelope_id,
               const std::string& step_id, const std::string& sequence_id,
               const std::string& scenario_path, const std::string& event_type = {});

    // Shared implementation: build validated unit in scene+presentation.
    CreateResult apply_unit_impl(const std::string& alias, const UnitCreationData& creation);

    // Throws std::runtime_error if coord or its footprint slots are already occupied.
    // Source of truth for all placement conflict checks — callers must not duplicate this.
    void ensure_unit_footprint_free(BattleSlotCoord coord, bool is_large,
                                    std::string_view unit_type, std::string_view alias) const;

    BattleScene&                scene_;
    IBattlePresentationSink&    presentation_;
    IBattleUnitCreationService& factory_;

    std::unordered_map<std::string, UnitInstanceId>     alias_to_unit_id_;
    std::unordered_map<UnitInstanceId, VisualEntityId>  unit_id_to_entity_id_;
    std::unordered_map<UnitInstanceId, std::string>     unit_id_to_unit_type_;
    std::unordered_map<BattleSlotCoord, UnitInstanceId> slot_occupant_;
    std::unordered_map<UnitInstanceId, BattleSlotCoord> unit_id_to_coord_;
    std::unordered_set<UnitInstanceId>                  alive_units_;
    std::unordered_set<std::string>                     retreated_aliases_;
};

} // namespace d2engine
