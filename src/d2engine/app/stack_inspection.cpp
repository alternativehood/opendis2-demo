#include "stack_inspection.hpp"

#include <d2log/log.hpp>

#include <algorithm>
#include <array>
#include <sstream>
#include <string>
#include <string_view>

namespace d2engine {

namespace {

auto kLog = d2log::get("d2.inspect"); // NOLINT(cert-err58-cpp)

std::string category_name(UnitCategory cat) {
    using enum UnitCategory;
    switch (cat) {
    case Soldier:
        return "Soldier";
    case Noble:
        return "Noble";
    case Leader:
        return "Leader";
    case Summon:
        return "Summon";
    case Illusion:
        return "Illusion";
    case NeutralLeader:
        return "NeutralLeader";
    case NeutralSoldier:
        return "NeutralSoldier";
    case Guardian:
        return "Guardian";
    case Unknown:
        return "Unknown";
    }
    return "Unknown";
}

std::string branch_name(UnitBranch b) {
    using enum UnitBranch;
    switch (b) {
    case Fighter:
        return "Fighter";
    case Archer:
        return "Archer";
    case Mage:
        return "Mage";
    case Special:
        return "Special";
    case Sideshow:
        return "Sideshow";
    case Hero:
        return "Hero";
    case Noble:
        return "Noble";
    case Summon:
        return "Summon";
    case Unknown:
        return "Unknown";
    }
    return "Unknown";
}

std::string attack_class_name(AttackClass c) {
    using enum AttackClass;
    switch (c) {
    case Damage:
        return "Damage";
    case Drain:
        return "Drain";
    case Paralyze:
        return "Paralyze";
    case Heal:
        return "Heal";
    case Fear:
        return "Fear";
    case BoostDamage:
        return "BoostDamage";
    case Petrify:
        return "Petrify";
    case LowerDamage:
        return "LowerDamage";
    case LowerInitiative:
        return "LowerInitiative";
    case Poison:
        return "Poison";
    case Frostbite:
        return "Frostbite";
    case Revive:
        return "Revive";
    case DrainOverflow:
        return "DrainOverflow";
    case Cure:
        return "Cure";
    case Summon:
        return "Summon";
    case DrainLevel:
        return "DrainLevel";
    case GiveAttack:
        return "GiveAttack";
    case Doppelganger:
        return "Doppelganger";
    case TransformSelf:
        return "TransformSelf";
    case TransformOther:
        return "TransformOther";
    case Blister:
        return "Blister";
    case BestowWards:
        return "BestowWards";
    case Shatter:
        return "Shatter";
    case Unknown:
        return "Unknown";
    }
    return "Unknown";
}

std::string source_name(AttackSource s) {
    using enum AttackSource;
    switch (s) {
    case Weapon:
        return "Weapon";
    case Mind:
        return "Mind";
    case Life:
        return "Life";
    case Death:
        return "Death";
    case Fire:
        return "Fire";
    case Water:
        return "Water";
    case Earth:
        return "Earth";
    case Air:
        return "Air";
    case Unknown:
        return "Unknown";
    }
    return "Unknown";
}

std::string reach_name(AttackReach r) {
    using enum AttackReach;
    switch (r) {
    case All:
        return "All";
    case Any:
        return "Any";
    case Adjacent:
        return "Adjacent";
    case Unknown:
        return "Unknown";
    }
    return "Unknown";
}

} // namespace

std::vector<int> derive_formation_cells(int anchor, bool is_large) {
    std::vector<int> cells;
    if (anchor < 0 || anchor > 5)
        return cells;
    if (is_large) {
        const int row_start = (anchor / 2) * 2;
        cells.push_back(row_start);
        cells.push_back(row_start + 1);
    } else {
        cells.push_back(anchor);
    }
    return cells;
}

StackInspectionBuilder::StackInspectionBuilder(const d2runtime::AdventureWorldState& world,
                                               const GameDataRegistry&               game_data)
    : world_(world), game_data_(game_data) {}

std::optional<StackInspectionModel>
StackInspectionBuilder::build(const std::string& stack_id) const {
    const auto* stack = world_.find_stack(stack_id);
    if (stack == nullptr) {
        return std::nullopt;
    }

    StackInspectionModel model;
    model.id = stack->id;
    model.group_id = stack->group_id;
    model.owner = stack->owner;
    model.subrace = stack->subrace;
    model.inside = stack->inside;
    model.position = stack->position;
    model.move = stack->move;
    model.morale = stack->morale;
    model.battles_won = stack->battles_won;
    model.leader_id = stack->leader_id;
    model.leader_alive = stack->leader_alive != 0;
    model.leader_alive_raw = stack->leader_alive;

    // Resolve faction from the stack's explicit subrace affiliation.
    // Prefer race_id directly (newer format), fall back to subrace type (legacy).
    if (!stack->subrace.empty()) {
        const auto* sr = world_.find_subrace(stack->subrace);
        if (sr != nullptr) {
            const RaceDef* race = nullptr;
            if (!sr->race_id.empty()) {
                race = game_data_.find_race(sr->race_id);
            }
            if (race == nullptr) {
                race = game_data_.find_race_by_type(sr->subrace);
            }
            if (race != nullptr && !race->name.empty()) {
                model.faction_name = race->name;
            }
        }
        if (model.faction_name.empty()) {
            kLog->warn("faction_unresolved stack={} subrace={}", stack->id, stack->subrace);
        }
    }

    // positions[member_index] = formation cell (derived convenience view)
    model.positions = stack->group.positions;

    // Member slots (6-slot semantic table, preserves empty slots)
    for (std::size_t i = 0; i < 6; ++i) {
        const auto& member_id = stack->group.members[i];
        if (member_id.has_value() && !member_id->empty()) {
            model.member_slots[i] = *member_id;
        }
    }

    // Members (non-empty slots only)
    for (std::size_t i = 0; i < 6; ++i) {
        const auto& member_id = stack->group.members[i];
        if (!member_id.has_value() || member_id->empty()) {
            continue;
        }
        bool is_leader = (stack->leader_id == *member_id);
        auto unit_model = inspect_unit(*member_id, is_leader);
        unit_model.member_index = static_cast<int>(i);

        unit_model.formation_cells = derive_formation_cells(
            stack->group.positions[static_cast<std::size_t>(i)],
            unit_model.definition_resolved && unit_model.definition.has_value() &&
                !unit_model.definition->size_small);

        model.members.push_back(std::move(unit_model));
    }

    return model;
}

UnitInspectionModel StackInspectionBuilder::inspect_unit(const std::string& unit_id,
                                                         bool               is_leader) const {
    UnitInspectionModel model;
    model.instance_id = unit_id;
    model.is_leader = is_leader;

    const auto* unit = world_.find_unit(unit_id);
    if (unit == nullptr) {
        model.unresolved_instance_id = unit_id;
        return model;
    }

    model.instance_resolved = true;
    model.type_id = unit->type_id;
    model.custom_name = unit->name;
    model.serialized_level = unit->serialized_level;
    model.current_hp = unit->current_hp;
    model.xp = unit->xp;
    model.creation = unit->creation;
    model.transformed = unit->transformed;
    model.dynamic_level = unit->dynamic_level;
    model.modifier_ids = unit->modifier_ids;

    // Resolve definition
    auto def = inspect_definition(unit->type_id);
    if (def.has_value()) {
        model.definition_resolved = true;
        model.definition = std::move(def);
    } else {
        model.unresolved_type = unit->type_id;
    }

    return model;
}

std::optional<UnitDefInspectionModel>
StackInspectionBuilder::inspect_definition(const std::string& type_id) const {
    const auto* unit_def = game_data_.find_unit(type_id);
    if (unit_def == nullptr) {
        return std::nullopt;
    }

    UnitDefInspectionModel def;
    def.unit_id = unit_def->unit_id;
    def.name = unit_def->name;
    def.description = unit_def->description;
    def.ability_text = unit_def->ability_text;
    def.race_id = unit_def->race_id;
    def.subrace = unit_def->subrace;
    def.category = category_name(unit_def->category);
    def.branch = branch_name(unit_def->branch);
    def.size_small = unit_def->size_small;
    def.definition_level = unit_def->level;
    def.base_hp = unit_def->hit_points;
    def.armor = unit_def->armor;
    def.regen = unit_def->regen;
    def.move = unit_def->move;
    def.scout = unit_def->scout;
    def.leadership = unit_def->leadership;
    def.negotiate = unit_def->negotiate;
    def.xp_killed = unit_def->xp_killed;
    def.xp_next = unit_def->xp_next;
    def.primary_attack_id = unit_def->primary_attack_id;
    def.secondary_attack_id = unit_def->secondary_attack_id;
    def.base_unit_id = unit_def->base_unit_id;
    def.prev_unit_id = unit_def->prev_unit_id;
    def.dyn_upg1_id = unit_def->dyn_upg1_id;
    def.dyn_upg2_id = unit_def->dyn_upg2_id;
    def.dyn_upg_level = unit_def->dyn_upg_level;

    def.native_ability_ids = unit_def->native_ability_ids;
    def.native_immunity_ids = unit_def->native_immunity_ids;
    def.native_immunity_categories = unit_def->native_immunity_categories;

    def.primary_attack = inspect_attack(unit_def->primary_attack);
    def.secondary_attack = inspect_attack(unit_def->secondary_attack);

    return def;
}

std::optional<AttackInspectionModel>
StackInspectionBuilder::inspect_attack(const AttackDef* attack) {
    if (attack == nullptr) {
        return std::nullopt;
    }

    AttackInspectionModel model;
    model.attack_id = attack->attack_id;
    model.name = attack->name;
    model.description = attack->description;
    model.attack_class = attack_class_name(attack->attack_class);
    model.source = source_name(attack->source);
    model.reach = reach_name(attack->reach);
    model.initiative = attack->initiative;
    model.damage = attack->damage;
    model.heal = attack->heal;
    model.power = attack->power;
    model.infinite = attack->infinite;
    model.crit_hit = attack->crit_hit;
    model.ward_ids = attack->ward_ids;
    model.alt_attack_id = attack->alt_attack_id;
    return model;
}

void log_stack_inspection(const StackInspectionModel& model) {
    std::ostringstream out;
    out << "stack_inspection begin\n";
    out << "  stack\n";
    out << "    id=" << model.id << "\n";
    out << "    group_id=" << model.group_id << "\n";
    out << "    owner=" << model.owner << "\n";
    out << "    subrace=" << model.subrace << "\n";
    out << "    inside=" << model.inside << "\n";
    out << "    position=(" << model.position.x << "," << model.position.y << ")\n";
    out << "    movement=" << model.move << "\n";
    out << "    morale=" << model.morale << "\n";
    out << "    battles_won=" << model.battles_won << "\n";
    out << "    leader_id=" << model.leader_id << "\n";
    out << "    leader_alive=" << (model.leader_alive ? "true" : "false") << "\n";
    out << "    leader_alive_raw=" << static_cast<int>(model.leader_alive_raw) << "\n";

    out << "  member_slots\n";
    for (std::size_t i = 0; i < 6; ++i) {
        if (model.member_slots[i].has_value()) {
            out << "    slot[" << i << "]=" << *model.member_slots[i] << "\n";
        } else {
            out << "    slot[" << i << "]=<empty>\n";
        }
    }

    out << "  positions[member]=cell\n";
    for (std::size_t mi = 0; mi < model.positions.size(); ++mi) {
        out << "    member[" << mi << "] -> cell[" << model.positions[mi] << "]\n";
    }

    for (const auto& member : model.members) {
        out << "  member[" << member.member_index << "]\n";
        out << "    instance_id=" << member.instance_id << "\n";

        if (!member.instance_resolved) {
            out << "    instance <unresolved id=" << member.unresolved_instance_id << ">\n";
            out << "  member[" << member.member_index << "] end\n";
            continue;
        }

        out << "    type_id=" << member.type_id << "\n";
        out << "    leader=" << (member.is_leader ? "true" : "false") << "\n";
        out << "    formation_cells=[";
        for (std::size_t fc = 0; fc < member.formation_cells.size(); ++fc) {
            if (fc > 0)
                out << ",";
            out << member.formation_cells[fc];
        }
        out << "]\n";

        out << "    instance\n";
        out << "      custom_name=" << member.custom_name << "\n";
        out << "      serialized_level=" << member.serialized_level << "\n";
        out << "      current_hp=" << member.current_hp << "\n";
        out << "      xp=" << member.xp << "\n";
        out << "      creation=" << member.creation << "\n";
        out << "      transformed=" << static_cast<int>(member.transformed) << "\n";
        if (member.dynamic_level.has_value()) {
            out << "      dynamic_level=" << static_cast<int>(*member.dynamic_level) << "\n";
        } else {
            out << "      dynamic_level=<absent>\n";
        }
        out << "      modifiers=[";
        for (std::size_t mi = 0; mi < member.modifier_ids.size(); ++mi) {
            if (mi > 0)
                out << ",";
            out << member.modifier_ids[mi];
        }
        out << "]\n";

        if (member.definition_resolved && member.definition.has_value()) {
            const auto& def = *member.definition;
            out << "    definition\n";
            out << "      unit_id=" << def.unit_id << "\n";
            out << "      name=" << def.name << "\n";
            out << "      description=" << def.description << "\n";
            out << "      ability_text=" << def.ability_text << "\n";
            out << "      race_id=" << def.race_id << "\n";
            out << "      subrace=" << def.subrace << "\n";
            out << "      category=" << def.category << "\n";
            out << "      branch=" << def.branch << "\n";
            out << "      size_small=" << (def.size_small ? "true" : "false") << "\n";
            out << "      definition_level=" << def.definition_level << "\n";
            out << "      base_hp=" << def.base_hp << "\n";
            out << "      armor=" << def.armor << "\n";
            out << "      regen=" << def.regen << "\n";
            out << "      move=" << def.move << "\n";
            out << "      scout=" << def.scout << "\n";
            out << "      leadership=" << def.leadership << "\n";
            out << "      negotiate=" << def.negotiate << "\n";
            out << "      xp_killed=" << def.xp_killed << "\n";
            out << "      xp_next=" << def.xp_next << "\n";
            out << "      primary_attack_id=" << def.primary_attack_id << "\n";
            out << "      secondary_attack_id=" << def.secondary_attack_id << "\n";
            out << "      base_unit_id=" << def.base_unit_id << "\n";
            out << "      prev_unit_id=" << def.prev_unit_id << "\n";
            out << "      dyn_upg1_id=" << def.dyn_upg1_id << "\n";
            out << "      dyn_upg2_id=" << def.dyn_upg2_id << "\n";
            out << "      dyn_upg_level=" << def.dyn_upg_level << "\n";

            if (!def.native_ability_ids.empty()) {
                out << "      native_ability_ids=[";
                for (std::size_t ai = 0; ai < def.native_ability_ids.size(); ++ai) {
                    if (ai > 0)
                        out << ",";
                    out << def.native_ability_ids[ai];
                }
                out << "]\n";
            }
            if (!def.native_immunity_ids.empty()) {
                out << "      native_immunity_ids=[";
                for (std::size_t ii = 0; ii < def.native_immunity_ids.size(); ++ii) {
                    if (ii > 0)
                        out << ",";
                    out << def.native_immunity_ids[ii];
                }
                out << "]\n";
            }
            if (!def.native_immunity_categories.empty()) {
                out << "      native_immunity_categories=[";
                for (std::size_t ic = 0; ic < def.native_immunity_categories.size(); ++ic) {
                    if (ic > 0)
                        out << ",";
                    out << def.native_immunity_categories[ic];
                }
                out << "]\n";
            }

            if (def.primary_attack.has_value()) {
                const auto& a = *def.primary_attack;
                out << "    primary_attack\n";
                out << "      attack_id=" << a.attack_id << "\n";
                out << "      name=" << a.name << "\n";
                out << "      description=" << a.description << "\n";
                out << "      class=" << a.attack_class << "\n";
                out << "      source=" << a.source << "\n";
                out << "      reach=" << a.reach << "\n";
                out << "      initiative=" << a.initiative << "\n";
                out << "      damage=" << a.damage << "\n";
                out << "      heal=" << a.heal << "\n";
                out << "      power=" << a.power << "\n";
                out << "      infinite=" << (a.infinite ? "true" : "false") << "\n";
                out << "      crit_hit=" << (a.crit_hit ? "true" : "false") << "\n";
                out << "      alt_attack_id=" << a.alt_attack_id << "\n";
                if (!a.ward_ids.empty()) {
                    out << "      wards=[";
                    for (std::size_t wi = 0; wi < a.ward_ids.size(); ++wi) {
                        if (wi > 0)
                            out << ",";
                        out << a.ward_ids[wi];
                    }
                    out << "]\n";
                }
            } else {
                out << "    primary_attack_id=" << def.primary_attack_id << " <unresolved>\n";
            }

            if (def.secondary_attack.has_value()) {
                const auto& a = *def.secondary_attack;
                out << "    secondary_attack\n";
                out << "      attack_id=" << a.attack_id << "\n";
                out << "      name=" << a.name << "\n";
                out << "      description=" << a.description << "\n";
                out << "      class=" << a.attack_class << "\n";
                out << "      source=" << a.source << "\n";
                out << "      reach=" << a.reach << "\n";
                out << "      initiative=" << a.initiative << "\n";
                out << "      damage=" << a.damage << "\n";
                out << "      heal=" << a.heal << "\n";
                out << "      power=" << a.power << "\n";
                out << "      infinite=" << (a.infinite ? "true" : "false") << "\n";
                out << "      crit_hit=" << (a.crit_hit ? "true" : "false") << "\n";
                out << "      alt_attack_id=" << a.alt_attack_id << "\n";
                if (!a.ward_ids.empty()) {
                    out << "      wards=[";
                    for (std::size_t wi = 0; wi < a.ward_ids.size(); ++wi) {
                        if (wi > 0)
                            out << ",";
                        out << a.ward_ids[wi];
                    }
                    out << "]\n";
                }
            } else if (!def.secondary_attack_id.empty()) {
                out << "    secondary_attack_id=" << def.secondary_attack_id << " <unresolved>\n";
            }
        } else {
            out << "    definition <unresolved type=" << member.unresolved_type << ">\n";
        }
    }

    out << "stack_inspection end\n";

    kLog->info("{}", out.str());
}

} // namespace d2engine
