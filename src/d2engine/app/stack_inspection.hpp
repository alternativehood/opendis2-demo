#pragma once

#include "../assets/game_data_registry.hpp"
#include "../assets/unit_def.hpp"

#include <d2runtime/AdventureWorldState.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace d2engine {

struct AttackInspectionModel {
    std::string              attack_id;
    std::string              name;
    std::string              description;
    std::string              attack_class;
    std::string              source;
    std::string              reach;
    int                      initiative = 0;
    int                      damage = 0;
    int                      heal = 0;
    int                      power = 0;
    bool                     infinite = false;
    bool                     crit_hit = false;
    std::vector<std::string> ward_ids;
    std::string              alt_attack_id;
};

struct UnitDefInspectionModel {
    std::string              unit_id;
    std::string              name;
    std::string              description;
    std::string              ability_text;
    std::string              race_id;
    int                      subrace = 0;
    std::string              category;
    std::string              branch;
    bool                     size_small = true;
    int                      definition_level = 1;
    int                      base_hp = 0;
    int                      armor = 0;
    int                      regen = 0;
    int                      move = 0;
    int                      scout = 0;
    int                      leadership = 0;
    int                      negotiate = 0;
    int                      xp_killed = 0;
    int                      xp_next = 0;
    std::string              primary_attack_id;
    std::string              secondary_attack_id;
    std::string              base_unit_id;
    std::string              prev_unit_id;
    std::string              dyn_upg1_id;
    std::string              dyn_upg2_id;
    int                      dyn_upg_level = 0;
    std::vector<int>         native_ability_ids;
    std::vector<std::string> native_immunity_ids;
    std::vector<std::string> native_immunity_categories;

    std::optional<AttackInspectionModel> primary_attack;
    std::optional<AttackInspectionModel> secondary_attack;
};

struct UnitInspectionModel {
    int         member_index = -1;
    std::string instance_id;
    std::string type_id;
    bool        is_leader = false;
    std::vector<int>
        formation_cells; // cells this member occupies (derived from positions[n] + unit size)

    // instance resolution
    bool        instance_resolved = false;
    std::string unresolved_instance_id;

    // resolved instance fields (valid only when instance_resolved == true)
    std::string              custom_name;
    int                      serialized_level = 0;
    int                      current_hp = 0;
    int                      xp = 0;
    int                      creation = 0;
    uint8_t                  transformed = 0;
    std::optional<uint8_t>   dynamic_level;
    std::vector<std::string> modifier_ids;

    // definition (may be unresolved)
    bool                                  definition_resolved = false;
    std::optional<UnitDefInspectionModel> definition;
    std::string                           unresolved_type;
};

struct StackInspectionModel {
    std::string             id;
    std::string             group_id;
    std::string             owner;
    std::string             subrace;
    std::string             inside;
    d2runtime::MapCellCoord position;
    int                     move = 0;
    int                     morale = 0;
    int                     battles_won = 0;
    std::string             leader_id;
    bool                    leader_alive = false;
    uint8_t                 leader_alive_raw = 0;

    // Resolved presentation fields (populated in StackInspectionBuilder)
    std::string faction_name;

    std::array<std::optional<std::string>, 6> member_slots;
    std::array<int, 6>                        positions;
    std::vector<UnitInspectionModel>          members;
};

class StackInspectionBuilder {
public:
    StackInspectionBuilder(const d2runtime::AdventureWorldState& world,
                           const GameDataRegistry&               game_data);

    [[nodiscard]] std::optional<StackInspectionModel> build(const std::string& stack_id) const;

private:
    [[nodiscard]] UnitInspectionModel inspect_unit(const std::string& unit_id,
                                                   bool               is_leader) const;

    [[nodiscard]] std::optional<UnitDefInspectionModel>
    inspect_definition(const std::string& type_id) const;

    [[nodiscard]] static std::optional<AttackInspectionModel>
    inspect_attack(const AttackDef* attack);

    const d2runtime::AdventureWorldState& world_;
    const GameDataRegistry&               game_data_;
};

void log_stack_inspection(const StackInspectionModel& model);

[[nodiscard]] std::vector<int> derive_formation_cells(int anchor, bool is_large);

} // namespace d2engine
