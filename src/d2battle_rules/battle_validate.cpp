#include "battle_validate.hpp"
#include "battle_effect.hpp"

#include <algorithm>
#include <set>
#include <stdexcept>
#include <string>

namespace d2battle {

void validate_battle_state(const BattleState& state) {
    if (state.party1.source_stack_id.empty()) {
        throw std::runtime_error("validate: Party1 source_stack_id empty");
    }
    if (state.party2.source_stack_id.empty()) {
        throw std::runtime_error("validate: Party2 source_stack_id empty");
    }
    if (state.party1.source_stack_id == state.party2.source_stack_id) {
        throw std::runtime_error("validate: stack identity conflict: " +
                                 state.party1.source_stack_id);
    }

    if (state.units.empty()) {
        throw std::runtime_error("validate: no units");
    }

    if (state.winner.has_value() && !state.is_terminal()) {
        throw std::runtime_error("validate: winner set but not Finished");
    }

    std::set<std::string> unit_ids;
    for (const auto& u : state.units) {
        if (u.id.empty()) {
            throw std::runtime_error("validate: unit has empty id");
        }
        if (!unit_ids.insert(u.id).second) {
            throw std::runtime_error("validate: duplicate unit id: " + u.id);
        }
    }

    auto validate_side = [&](BattleSide side) {
        const auto& s = state.side(side);
        for (std::size_t slot = 0; slot < 6; ++slot) {
            const auto& member = s.members[slot];
            if (!member.has_value()) {
                if (s.positions[slot] >= 0) {
                    throw std::runtime_error(
                        "validate: side " + std::to_string(static_cast<int>(side)) +
                        " empty slot " + std::to_string(slot) + " has position anchor");
                }
                continue;
            }
            if (unit_ids.count(*member) == 0) {
                throw std::runtime_error(
                    "validate: side " + std::to_string(static_cast<int>(side)) + " slot " +
                    std::to_string(slot) + " references unknown unit: " + *member);
            }
            if (s.positions[slot] < -1 || s.positions[slot] >= 6) {
                throw std::runtime_error("validate: side " +
                                         std::to_string(static_cast<int>(side)) + " slot " +
                                         std::to_string(slot) + " invalid position " +
                                         std::to_string(s.positions[slot]));
            }

            const auto* unit = state.find_unit(*member);
            if (!unit) {
                throw std::runtime_error("validate: side slot " + std::to_string(slot) +
                                         " unit not found: " + *member);
            }
            if (unit->side != side) {
                throw std::runtime_error("validate: unit " + unit->id +
                                         " side=" + std::to_string(static_cast<int>(unit->side)) +
                                         " but referenced by side " +
                                         std::to_string(static_cast<int>(side)));
            }
            if (static_cast<std::size_t>(unit->member_index) != slot) {
                throw std::runtime_error("validate: unit " + unit->id +
                                         " member_index=" + std::to_string(unit->member_index) +
                                         " != slot " + std::to_string(slot));
            }

            int canonical_anchor = -1;
            for (int c = 0; c < 6; ++c) {
                if (s.cell_members[static_cast<std::size_t>(c)] == static_cast<int>(slot)) {
                    canonical_anchor = c;
                    break;
                }
            }
            if (canonical_anchor < 0) {
                throw std::runtime_error("validate: unit " + unit->id + " slot " +
                                         std::to_string(slot) + " not in cell_members");
            }
            if (s.positions[slot] != canonical_anchor) {
                throw std::runtime_error(
                    "validate: unit " + unit->id + " positions[" + std::to_string(slot) +
                    "]=" + std::to_string(s.positions[slot]) + " != canonical anchor cell " +
                    std::to_string(canonical_anchor));
            }
            if (unit->formation_cell != canonical_anchor) {
                throw std::runtime_error("validate: unit " + unit->id +
                                         " formation_cell=" + std::to_string(unit->formation_cell) +
                                         " != canonical anchor cell " +
                                         std::to_string(canonical_anchor));
            }
        }

        for (int c = 0; c < 6; ++c) {
            int mi = s.cell_members[static_cast<std::size_t>(c)];
            if (mi < -1 || mi >= 6) {
                throw std::runtime_error("validate: invalid cell_members[" + std::to_string(c) +
                                         "]=" + std::to_string(mi));
            }
            if (mi >= 0) {
                if (!s.members[static_cast<std::size_t>(mi)].has_value()) {
                    throw std::runtime_error("validate: cell " + std::to_string(c) +
                                             " references empty member slot " + std::to_string(mi));
                }
            }
        }
    };

    validate_side(BattleSide::Party1);
    validate_side(BattleSide::Party2);

    for (const auto& u : state.units) {
        bool found = false;
        for (int s = 0; s <= 1; ++s) {
            BattleSide  bs = (s == 0) ? BattleSide::Party1 : BattleSide::Party2;
            const auto& ss = state.side(bs);
            for (std::size_t slot = 0; slot < 6; ++slot) {
                if (ss.members[slot].has_value() && *ss.members[slot] == u.id) {
                    if (found) {
                        throw std::runtime_error("validate: unit " + u.id +
                                                 " in multiple member slots");
                    }
                    found = true;
                }
            }
        }
        if (!found) {
            throw std::runtime_error("validate: orphan unit: " + u.id);
        }

        if (u.alive && u.current_hp <= 0) {
            throw std::runtime_error("validate: unit " + u.id + " alive but hp=0");
        }
        if (!u.alive && u.current_hp > 0) {
            throw std::runtime_error("validate: unit " + u.id +
                                     " dead but hp=" + std::to_string(u.current_hp));
        }
        if (u.formation_cell < -1 || u.formation_cell >= 6) {
            throw std::runtime_error("validate: unit " + u.id +
                                     " invalid formation_cell=" + std::to_string(u.formation_cell));
        }
        if (u.member_index < 0 || u.member_index >= 6) {
            throw std::runtime_error("validate: unit " + u.id +
                                     " invalid member_index=" + std::to_string(u.member_index));
        }

        if (!u.alive && !u.effects.empty()) {
            throw std::runtime_error("validate: dead unit " + u.id + " has " +
                                     std::to_string(u.effects.size()) + " transient effects");
        }

        bool had_petrify = false;
        for (const auto& e : u.effects) {
            if (auto* p = std::get_if<PetrifiedEffect>(&e)) {
                if (had_petrify)
                    throw std::runtime_error("validate: unit " + u.id +
                                             " has duplicate PetrifiedEffect");
                had_petrify = true;
                if (p->remaining_activation_skips == 0)
                    throw std::runtime_error("validate: unit " + u.id +
                                             " PetrifiedEffect remaining_activation_skips=0");
                if (p->source_actor_id.empty())
                    throw std::runtime_error("validate: unit " + u.id +
                                             " PetrifiedEffect source_actor_id empty");
                if (p->source_attack_id.empty())
                    throw std::runtime_error("validate: unit " + u.id +
                                             " PetrifiedEffect source_attack_id empty");
                const auto* src = state.find_unit(p->source_actor_id);
                if (!src)
                    throw std::runtime_error(
                        "validate: unit " + u.id +
                        " PetrifiedEffect source_actor not found: " + p->source_actor_id);
            }
        }
    }

    for (int s = 0; s <= 1; ++s) {
        BattleSide  bs = (s == 0) ? BattleSide::Party1 : BattleSide::Party2;
        const auto& ss = state.side(bs);
        if (!ss.leader_id.empty()) {
            const auto* leader = state.find_unit(ss.leader_id);
            if (leader) {
                std::uint8_t expected = leader->alive ? 1 : 0;
                if (ss.leader_alive != expected) {
                    throw std::runtime_error("validate: side " + std::to_string(s) +
                                             " leader_alive=" + std::to_string(ss.leader_alive) +
                                             " but leader " + ss.leader_id +
                                             " alive=" + (leader->alive ? "true" : "false"));
                }
            }
        }
    }

    if (state.status == BattleStatus::InProgress) {
        if (state.round_state.turn_order.empty()) {
            throw std::runtime_error("validate: InProgress turn_order empty");
        }
        if (state.round_state.current_turn_index >= state.round_state.turn_order.size()) {
            throw std::runtime_error("validate: current_turn_index out of range");
        }
        if (state.round_state.round_number == 0) {
            throw std::runtime_error("validate: InProgress round_number=0");
        }

        std::set<std::string> to_ids;
        for (const auto& entry : state.round_state.turn_order) {
            if (unit_ids.count(entry.unit_id) == 0) {
                throw std::runtime_error("validate: turn_order contains unknown unit: " +
                                         entry.unit_id);
            }
            if (!to_ids.insert(entry.unit_id).second) {
                throw std::runtime_error("validate: duplicate in turn_order: " + entry.unit_id);
            }
        }

        const auto* actor = state.current_actor();
        if (!actor) {
            throw std::runtime_error("validate: InProgress no current actor");
        }
        if (!actor->alive) {
            throw std::runtime_error("validate: current actor dead: " + actor->id);
        }

        bool p1_alive = std::any_of(state.units.begin(), state.units.end(), [](const auto& u) {
            return u.alive && u.side == BattleSide::Party1;
        });
        bool p2_alive = std::any_of(state.units.begin(), state.units.end(), [](const auto& u) {
            return u.alive && u.side == BattleSide::Party2;
        });
        if (!p1_alive) {
            throw std::runtime_error("validate: InProgress Party1 has no alive units");
        }
        if (!p2_alive) {
            throw std::runtime_error("validate: InProgress Party2 has no alive units");
        }
    }

    if (state.is_terminal()) {
        bool p1_alive = std::any_of(state.units.begin(), state.units.end(), [](const auto& u) {
            return u.alive && u.side == BattleSide::Party1;
        });
        bool p2_alive = std::any_of(state.units.begin(), state.units.end(), [](const auto& u) {
            return u.alive && u.side == BattleSide::Party2;
        });

        if (p1_alive && p2_alive) {
            throw std::runtime_error("validate: Finished both sides have alive units");
        }

        if (!p1_alive && !p2_alive) {
            if (state.winner.has_value()) {
                throw std::runtime_error(
                    "validate: Finished both dead requires winner=nullopt (draw)");
            }
            return;
        }

        if (p1_alive && !p2_alive) {
            if (!state.winner.has_value()) {
                throw std::runtime_error("validate: Finished Party1 wins but no winner");
            }
            if (*state.winner != BattleSide::Party1) {
                throw std::runtime_error("validate: winner Party2 but only Party1 alive");
            }
        }
        if (!p1_alive && p2_alive) {
            if (!state.winner.has_value()) {
                throw std::runtime_error("validate: Finished Party2 wins but no winner");
            }
            if (*state.winner != BattleSide::Party2) {
                throw std::runtime_error("validate: winner Party1 but only Party2 alive");
            }
        }
    }

    std::size_t expected_idx = 0;
    auto        check_order = [&](BattleSide side) {
        const auto& s = state.side(side);
        for (std::size_t slot = 0; slot < 6; ++slot) {
            if (s.members[slot].has_value()) {
                if (expected_idx >= state.units.size()) {
                    throw std::runtime_error("validate: unit order exhausted too early");
                }
                if (state.units[expected_idx].id != *s.members[slot]) {
                    throw std::runtime_error(
                        "validate: unit order mismatch at index " + std::to_string(expected_idx) +
                        " expected " + *s.members[slot] + " got " + state.units[expected_idx].id);
                }
                ++expected_idx;
            }
        }
    };
    check_order(BattleSide::Party1);
    check_order(BattleSide::Party2);
    if (expected_idx != state.units.size()) {
        throw std::runtime_error("validate: unit order has extra units beyond canonical slots");
    }
}

} // namespace d2battle
