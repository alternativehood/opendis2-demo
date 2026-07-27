#include "battle_bootstrap.hpp"
#include "battle_validate.hpp"
#include "detail/battle_derived.hpp"
#include "detail/battle_status.hpp"
#include "detail/battle_turn.hpp"

#include <d2log/log.hpp>

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace d2battle {

namespace {

auto kLog = d2log::get("d2battle.bootstrap");

void log_attack_def(const d2engine::AttackDef& atk, const char* label) {
    D2_LOG_INFO(kLog,
                "    {}: id={} class={} source={} reach={} init={} dmg={} heal={} power={} "
                "infinite={} crit={} alt={} wards=[{}]",
                label, atk.attack_id, static_cast<int>(atk.attack_class),
                static_cast<int>(atk.source), static_cast<int>(atk.reach), atk.initiative,
                atk.damage, atk.heal, atk.power, atk.infinite, atk.crit_hit, atk.alt_attack_id,
                [&]() {
                    std::string w;
                    for (std::size_t i = 0; i < atk.ward_ids.size(); ++i) {
                        if (i > 0)
                            w += ",";
                        w += atk.ward_ids[i];
                    }
                    return w;
                }());
}

void bootstrap_log_side(BattleSide side, const BattleSideState& sstate) {
    D2_LOG_INFO(
        kLog,
        "  [{}] stack_id={} owner={} subrace={} leader={} leader_alive={} morale={} battles_won={}",
        static_cast<int>(side) == 0 ? "Party1" : "Party2", sstate.source_stack_id, sstate.owner,
        sstate.subrace, sstate.leader_id, static_cast<int>(sstate.leader_alive), sstate.morale,
        sstate.battles_won);
    D2_LOG_INFO(kLog,
                "  [{}] member_count={} banner={} tome={} battle1={} battle2={} artifact1={} "
                "artifact2={} boots={}",
                static_cast<int>(side) == 0 ? "Party1" : "Party2", sstate.member_count(),
                sstate.banner, sstate.tome, sstate.battle1, sstate.battle2, sstate.artifact1,
                sstate.artifact2, sstate.boots);
    D2_LOG_INFO(kLog, "  [{}] cell_members={{{}}}",
                static_cast<int>(side) == 0 ? "Party1" : "Party2", [&]() {
                    std::string s;
                    for (int i = 0; i < 6; ++i) {
                        if (i > 0)
                            s += ",";
                        s += std::to_string(sstate.cell_members[static_cast<std::size_t>(i)]);
                    }
                    return s;
                }());
}

void bootstrap_log_unit(const BattleUnitState& unit, const d2engine::GameDataRegistry& game_data) {
    D2_LOG_INFO(kLog, "  unit id={} type={} side={} member={} cell={} hp={} xp={} alive={}",
                unit.id, unit.type_id, static_cast<int>(unit.side), unit.member_index,
                unit.formation_cell, unit.current_hp, unit.xp, unit.alive);
    D2_LOG_INFO(kLog, "    level={} dynamic_level={} creation={} name='{}' transformed={}",
                unit.serialized_level,
                unit.dynamic_level.has_value() ? std::to_string(*unit.dynamic_level) : "none",
                unit.creation, unit.name, static_cast<int>(unit.transformed));

    if (!unit.modifier_ids.empty()) {
        std::string mids;
        for (std::size_t i = 0; i < unit.modifier_ids.size(); ++i) {
            if (i > 0)
                mids += ",";
            mids += unit.modifier_ids[i];
        }
        D2_LOG_INFO(kLog, "    modifier_ids=[{}]", mids);
    }

    const auto* udef = game_data.find_unit(unit.type_id);
    if (udef) {
        D2_LOG_INFO(kLog, "    unit_def: hp_base={} armor={} regen={} atck_twice={}",
                    udef->hit_points, udef->armor, udef->regen, udef->atck_twice);
        if (!udef->native_ability_ids.empty()) {
            std::string s;
            for (std::size_t i = 0; i < udef->native_ability_ids.size(); ++i) {
                if (i > 0)
                    s += ",";
                s += std::to_string(udef->native_ability_ids[i]);
            }
            D2_LOG_INFO(kLog, "    native_ability_ids=[{}]", s);
        }
        if (!udef->native_immunity_ids.empty()) {
            std::string s;
            for (std::size_t i = 0; i < udef->native_immunity_ids.size(); ++i) {
                if (i > 0)
                    s += ",";
                s += udef->native_immunity_ids[i];
            }
            D2_LOG_INFO(kLog, "    native_immunity_ids=[{}]", s);
        }
        if (!udef->native_immunity_categories.empty()) {
            std::string s;
            for (std::size_t i = 0; i < udef->native_immunity_categories.size(); ++i) {
                if (i > 0)
                    s += ",";
                s += udef->native_immunity_categories[i];
            }
            D2_LOG_INFO(kLog, "    native_immunity_categories=[{}]", s);
        }
        if (udef->primary_attack)
            log_attack_def(*udef->primary_attack, "prim_atk");
        if (udef->secondary_attack)
            log_attack_def(*udef->secondary_attack, "sec_atk");
    } else {
        D2_LOG_ERROR(kLog, "    unit_def: NOT FOUND in GameDataRegistry");
        throw std::runtime_error("bootstrap_battle: unit type not found: " + unit.type_id);
    }
}

void append_side_units(BattleState& state, const d2runtime::AdventureWorldState& world,
                       BattleSide side, const d2engine::GameDataRegistry& game_data) {
    auto& sstate = state.side(side);
    for (std::size_t slot = 0; slot < 6; ++slot) {
        const auto& member = sstate.members[slot];
        if (!member.has_value())
            continue;

        const auto* wunit = world.find_unit(*member);
        if (!wunit)
            throw std::runtime_error("bootstrap_battle: unit not found: " + *member);

        BattleUnitState bunit;
        bunit.id = wunit->id;
        bunit.type_id = wunit->type_id;
        bunit.serialized_level = wunit->serialized_level;
        bunit.modifier_ids = wunit->modifier_ids;
        bunit.creation = wunit->creation;
        bunit.name = wunit->name;
        bunit.transformed = wunit->transformed;
        bunit.dynamic_level = wunit->dynamic_level;
        bunit.current_hp = wunit->current_hp;
        bunit.xp = wunit->xp;
        bunit.side = side;
        bunit.member_index = static_cast<int>(slot);
        bunit.alive = (wunit->current_hp > 0);
        bunit.formation_cell = sstate.positions[slot];

        bootstrap_log_unit(bunit, game_data);
        state.units.push_back(std::move(bunit));
    }
}

} // namespace

BattleState bootstrap_battle(const d2runtime::AdventureStack&      party1_stack,
                             const d2runtime::AdventureStack&      party2_stack,
                             const d2runtime::AdventureWorldState& world,
                             const d2engine::GameDataRegistry&     game_data) {
    D2_LOG_INFO(kLog, "=== BOOTSTRAP BATTLE ===");
    D2_LOG_INFO(kLog, "Party1 stack_id={} coord=({},{})", party1_stack.id, party1_stack.position.x,
                party1_stack.position.y);
    D2_LOG_INFO(kLog, "Party2 stack_id={} coord=({},{})", party2_stack.id, party2_stack.position.x,
                party2_stack.position.y);

    BattleState state;

    auto build_side = [&](const d2runtime::AdventureStack& stack) -> BattleSideState {
        BattleSideState s;
        s.source_stack_id = stack.id;
        s.owner = stack.owner;
        s.subrace = stack.subrace;
        s.leader_id = stack.leader_id;
        s.leader_alive = stack.leader_alive;
        s.morale = stack.morale;
        s.battles_won = stack.battles_won;
        s.banner = stack.banner;
        s.tome = stack.tome;
        s.battle1 = stack.battle1;
        s.battle2 = stack.battle2;
        s.artifact1 = stack.artifact1;
        s.artifact2 = stack.artifact2;
        s.boots = stack.boots;

        for (std::size_t i = 0; i < stack.group.members.size(); ++i) {
            const auto& member = stack.group.members[i];
            if (!member.has_value())
                continue;
            s.members[i] = member;
            if (!world.find_unit(*member)) {
                D2_LOG_ERROR(kLog, "stack {} member[{}] {} not found in world units", stack.id, i,
                             *member);
                throw std::runtime_error("bootstrap_battle: dangling unit reference: stack=" +
                                         stack.id + " unit=" + *member);
            }
        }

        for (int i = 0; i < 6; ++i) {
            s.positions[static_cast<std::size_t>(i)] =
                stack.group.positions[static_cast<std::size_t>(i)];
            s.cell_members[static_cast<std::size_t>(i)] =
                stack.group.cell_members[static_cast<std::size_t>(i)];
        }

        return s;
    };

    state.party1 = build_side(party1_stack);
    state.party2 = build_side(party2_stack);

    bootstrap_log_side(BattleSide::Party1, state.party1);
    bootstrap_log_side(BattleSide::Party2, state.party2);

    append_side_units(state, world, BattleSide::Party1, game_data);
    append_side_units(state, world, BattleSide::Party2, game_data);

    detail::normalize_derived_side_state(state);
    detail::normalize_battle_status(state);

    if (state.status == BattleStatus::InProgress) {
        detail::begin_round(state, 1, game_data);

        D2_LOG_INFO(kLog, "Bootstrap complete: {} units, {} alive, round={}", state.units.size(),
                    std::count_if(state.units.begin(), state.units.end(),
                                  [](const auto& u) { return u.alive; }),
                    state.round_state.round_number);

        const auto* actor = state.current_actor();
        if (actor)
            D2_LOG_INFO(kLog, "Current actor: {} ({})", actor->id, actor->type_id);
    } else {
        D2_LOG_INFO(kLog, "Bootstrap: battle already terminal, status=Finished");
    }

    validate_battle_state(state);
    return state;
}

BattleState bootstrap_battle_from_stack_ids(const d2runtime::AdventureWorldState& world,
                                            const std::string&                    party1_stack_id,
                                            const std::string&                    party2_stack_id,
                                            const d2engine::GameDataRegistry&     game_data) {
    const auto* p1 = world.find_stack(party1_stack_id);
    const auto* p2 = world.find_stack(party2_stack_id);

    if (!p1) {
        throw std::runtime_error("bootstrap_battle_from_stack_ids: stack not found: " +
                                 party1_stack_id);
    }
    if (!p2) {
        throw std::runtime_error("bootstrap_battle_from_stack_ids: stack not found: " +
                                 party2_stack_id);
    }

    return bootstrap_battle(*p1, *p2, world, game_data);
}

} // namespace d2battle
