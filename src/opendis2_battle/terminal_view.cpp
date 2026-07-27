#include "terminal_view.hpp"
#include "attack_presentation_labels.hpp"

#include <d2battle_rules/attack_support.hpp>
#include <d2battle_rules/battle_effect.hpp>
#include <d2battle_rules/battle_formation.hpp>
#include <d2battle_rules/detail/battle_attack_rules.hpp>
#include <d2battle_rules/detail/bundle_support.hpp>

#include <cstddef>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace {

using namespace d2battle;
using namespace d2battle::formation;

enum class HpDeltaKind {
    Damage,
    Heal,
    Revive,
};

struct HpDelta {
    HpDeltaKind            kind;
    const BattleUnitState* before;
    const BattleUnitState* after;
};

[[nodiscard]] std::vector<HpDelta> collect_hp_deltas(const BattleState& before,
                                                     const BattleState& after) {
    std::vector<HpDelta> result;
    for (const auto& ub : before.units) {
        const auto* ua = after.find_unit(ub.id);
        if (!ua) {
            throw std::runtime_error("collect_hp_deltas: unit missing from successor state: " +
                                     ub.id);
        }
        if (ua->current_hp == ub.current_hp && ua->alive == ub.alive)
            continue;
        HpDeltaKind kind;
        if (!ub.alive && ua->alive) {
            kind = HpDeltaKind::Revive;
        } else if (ua->current_hp < ub.current_hp) {
            kind = HpDeltaKind::Damage;
        } else {
            kind = HpDeltaKind::Heal;
        }
        result.push_back({kind, &ub, ua});
    }
    return result;
}

[[nodiscard]] std::string unit_label(const BattleUnitState&            u,
                                     const d2engine::GameDataRegistry& game_data) {
    const auto* udef = game_data.find_unit(u.type_id);
    std::string name = udef ? udef->name : u.type_id;
    if (name.empty())
        name = u.type_id;
    std::string side = (u.side == BattleSide::Party1) ? "P1#" : "P2#";
    return side + std::to_string(u.member_index) + " " + u.name + " (" + name + ")";
}

[[nodiscard]] std::string short_label(const BattleUnitState& u) {
    std::string side = (u.side == BattleSide::Party1) ? "P1#" : "P2#";
    return side + std::to_string(u.member_index);
}

[[nodiscard]] std::string
bundle_label(const d2battle::detail::ResolvedAttackBundleDefinition& bundle) {
    std::ostringstream oss;
    oss << "[" << d2battle_terminal::attack_class_label(bundle.primary_attack.get().attack_class)
        << "/" << d2battle_terminal::attack_reach_label(bundle.primary_attack.get().reach);
    if (bundle.secondary_attack.has_value()) {
        oss << " + "
            << d2battle_terminal::attack_class_label(bundle.secondary_attack->get().attack_class)
            << "/" << d2battle_terminal::attack_reach_label(bundle.secondary_attack->get().reach);
    }
    oss << "]";
    return oss.str();
}

[[nodiscard]] std::string cell_display(const BattleUnitState* u) {
    if (!u)
        return "[  .  ]";
    if (!u->alive)
        return "[DEAD ]";
    std::string tag = short_label(*u);
    if (tag.size() < 5)
        tag += std::string(5 - tag.size(), ' ');
    return "[" + tag + "]";
}

void print_hp_delta_row(std::ostream& out, const BattleUnitState& before,
                        const BattleUnitState& after, const std::string& indent) {
    out << indent << short_label(before) << " " << before.name << " HP " << before.current_hp
        << " -> " << after.current_hp;
    if (!after.alive) {
        out << " DEAD";
    } else if (!before.alive && after.alive) {
        out << " ALIVE";
    }
    out << "\n";
}

void print_delta_section(std::ostream& out, const std::vector<HpDelta>& deltas, HpDeltaKind kind,
                         std::string_view section_header, const std::string& row_indent) {
    bool first = true;
    for (const auto& d : deltas) {
        if (d.kind != kind)
            continue;
        if (first) {
            out << section_header;
            first = false;
        }
        print_hp_delta_row(out, *d.before, *d.after, row_indent);
    }
}

void print_hp_deltas(std::ostream& out, const BattleState& before, const BattleState& after,
                     const d2engine::GameDataRegistry& /*game_data*/) {
    auto deltas = collect_hp_deltas(before, after);
    if (deltas.empty())
        return;
    print_delta_section(out, deltas, HpDeltaKind::Damage, "  DAMAGE:\n", "    ");
    bool has_heal = false;
    bool has_revive = false;
    for (const auto& d : deltas) {
        if (d.kind == HpDeltaKind::Heal)
            has_heal = true;
        if (d.kind == HpDeltaKind::Revive)
            has_revive = true;
    }
    if (has_heal) {
        out << "\n";
        print_delta_section(out, deltas, HpDeltaKind::Heal, "  HEAL:\n", "    ");
    }
    if (has_revive) {
        out << "\n";
        print_delta_section(out, deltas, HpDeltaKind::Revive, "  REVIVED:\n", "    ");
    }
}

void print_result_hp_deltas(std::ostream& out, const BattleState& before, const BattleState& after,
                            const d2engine::GameDataRegistry& /*game_data*/) {
    auto deltas = collect_hp_deltas(before, after);
    if (deltas.empty())
        return;
    bool has_damage = false;
    bool has_heal = false;
    bool has_revive = false;
    for (const auto& d : deltas) {
        if (d.kind == HpDeltaKind::Damage)
            has_damage = true;
        if (d.kind == HpDeltaKind::Heal)
            has_heal = true;
        if (d.kind == HpDeltaKind::Revive)
            has_revive = true;
    }
    if (has_damage) {
        print_delta_section(out, deltas, HpDeltaKind::Damage, "DAMAGE:\n", "  ");
    }
    if (has_heal) {
        if (has_damage)
            out << "\n";
        print_delta_section(out, deltas, HpDeltaKind::Heal, "HEAL:\n", "  ");
    }
    // Revived units must NOT also appear in HEAL section
    if (has_revive) {
        if (has_damage || has_heal)
            out << "\n";
        print_delta_section(out, deltas, HpDeltaKind::Revive, "REVIVED:\n", "  ");
    }
}

enum class EffectDeltaKind { Applied, Removed, Refreshed };

struct EffectDelta {
    EffectDeltaKind        kind;
    const BattleUnitState* unit;
    BattleUnitEffectState  before_effect;
    BattleUnitEffectState  after_effect;
};

[[nodiscard]] std::size_t effect_variant_index(const BattleUnitEffectState& e) {
    return e.index();
}

[[nodiscard]] bool effects_are_equal_content(const BattleUnitEffectState& a,
                                             const BattleUnitEffectState& b) {
    return a == b;
}

[[nodiscard]] std::vector<EffectDelta> collect_effect_deltas(const BattleState& before,
                                                             const BattleState& after) {
    std::vector<EffectDelta> result;
    for (const auto& ub : before.units) {
        const auto* ua = after.find_unit(ub.id);
        if (!ua) {
            throw std::runtime_error("collect_effect_deltas: unit missing from successor state: " +
                                     ub.id);
        }

        for (const auto& eb : ub.effects) {
            auto vi_before = effect_variant_index(eb);
            bool found_in_after = false;
            for (const auto& ea : ua->effects) {
                if (effect_variant_index(ea) == vi_before) {
                    found_in_after = true;
                    if (!effects_are_equal_content(eb, ea)) {
                        result.push_back({EffectDeltaKind::Refreshed, ua, eb, ea});
                    }
                    break;
                }
            }
            if (!found_in_after) {
                result.push_back({EffectDeltaKind::Removed, &ub, eb, eb});
            }
        }

        for (const auto& ea : ua->effects) {
            auto vi_after = effect_variant_index(ea);
            bool found_in_before = false;
            for (const auto& eb : ub.effects) {
                if (effect_variant_index(eb) == vi_after) {
                    found_in_before = true;
                    break;
                }
            }
            if (!found_in_before) {
                result.push_back({EffectDeltaKind::Applied, ua, ea, ea});
            }
        }
    }
    return result;
}

[[nodiscard]] std::string effect_status_text(const BattleUnitEffectState& e) {
    if (std::holds_alternative<PetrifiedEffect>(e))
        return "PETRIFIED";
    return "?";
}

void print_effect_delta_section(std::ostream& out, const std::vector<EffectDelta>& deltas,
                                EffectDeltaKind kind, const std::string& section_header,
                                const std::string& indent) {
    bool first = true;
    for (const auto& d : deltas) {
        if (d.kind != kind)
            continue;
        if (first) {
            out << section_header;
            first = false;
        }
        const auto& display_effect =
            (kind == EffectDeltaKind::Removed) ? d.before_effect : d.after_effect;
        out << indent << short_label(*d.unit) << " " << d.unit->name << " "
            << effect_status_text(display_effect) << "\n";
    }
}

} // namespace

std::string hp_bar(int current, int max, int width) {
    if (max <= 0)
        max = 1;
    int filled = (current * width) / max;
    if (filled < 0)
        filled = 0;
    if (filled > width)
        filled = width;
    std::string bar;
    for (int i = 0; i < filled; ++i)
        bar += '#';
    for (int i = filled; i < width; ++i)
        bar += '.';
    return bar;
}

// cppcheck-suppress unusedFunction
void print_formation_board(std::ostream& out, const BattleState& state,
                           const d2engine::GameDataRegistry& /*game_data*/) {
    auto p1_back_top =
        unit_at_position(state, BattleSide::Party1, FormationRank::Back, FormationRow::Top);
    auto p1_back_mid =
        unit_at_position(state, BattleSide::Party1, FormationRank::Back, FormationRow::Middle);
    auto p1_back_bot =
        unit_at_position(state, BattleSide::Party1, FormationRank::Back, FormationRow::Bottom);
    auto p1_front_top =
        unit_at_position(state, BattleSide::Party1, FormationRank::Front, FormationRow::Top);
    auto p1_front_mid =
        unit_at_position(state, BattleSide::Party1, FormationRank::Front, FormationRow::Middle);
    auto p1_front_bot =
        unit_at_position(state, BattleSide::Party1, FormationRank::Front, FormationRow::Bottom);

    auto p2_front_top =
        unit_at_position(state, BattleSide::Party2, FormationRank::Front, FormationRow::Top);
    auto p2_front_mid =
        unit_at_position(state, BattleSide::Party2, FormationRank::Front, FormationRow::Middle);
    auto p2_front_bot =
        unit_at_position(state, BattleSide::Party2, FormationRank::Front, FormationRow::Bottom);
    auto p2_back_top =
        unit_at_position(state, BattleSide::Party2, FormationRank::Back, FormationRow::Top);
    auto p2_back_mid =
        unit_at_position(state, BattleSide::Party2, FormationRank::Back, FormationRow::Middle);
    auto p2_back_bot =
        unit_at_position(state, BattleSide::Party2, FormationRank::Back, FormationRow::Bottom);

    out << "\n";
    out << "          PARTY 1                         PARTY 2\n";
    out << "     BACK       FRONT               FRONT       BACK\n";
    out << "  " << cell_display(p1_back_top) << "  " << cell_display(p1_front_top)
        << "                     " << cell_display(p2_front_top) << "  "
        << cell_display(p2_back_top) << "\n";
    out << "  " << cell_display(p1_back_mid) << "  " << cell_display(p1_front_mid)
        << "                     " << cell_display(p2_front_mid) << "  "
        << cell_display(p2_back_mid) << "\n";
    out << "  " << cell_display(p1_back_bot) << "  " << cell_display(p1_front_bot)
        << "                     " << cell_display(p2_front_bot) << "  "
        << cell_display(p2_back_bot) << "\n";
    out << "\n";
}

void print_unit_legend(std::ostream& out, const BattleState& state,
                       const d2engine::GameDataRegistry& game_data) {
    const auto* actor = state.current_actor();
    out << "UNITS\n\n";
    for (const auto& u : state.units) {
        std::string label = unit_label(u, game_data);
        const auto* udef = game_data.find_unit(u.type_id);
        int         max_hp = udef ? udef->hit_points : u.current_hp;
        std::string bar = hp_bar(u.current_hp, max_hp);

        std::string marker;
        if (!u.alive) {
            marker = "XX ";
        } else if (actor && u.id == actor->id) {
            marker = ">> ";
        } else {
            marker = "   ";
        }

        out << marker << label << " HP " << u.current_hp << "/" << max_hp << " [" << bar << "]";
        if (!u.alive) {
            out << " DEAD";
        }
        for (const auto& e : u.effects) {
            out << " " << effect_status_text(e);
        }
        out << "\n";
    }
    out << "\n";
}

// cppcheck-suppress unusedFunction
void print_turn_order(std::ostream& out, const BattleState& state,
                      const d2engine::GameDataRegistry& game_data) {
    if (state.status != BattleStatus::InProgress) {
        return;
    }

    out << "TURN ORDER — ROUND " << state.round_state.round_number << "\n\n";

    for (std::size_t i = 0; i < state.round_state.turn_order.size(); ++i) {
        const auto& entry = state.round_state.turn_order[i];
        const auto* u = state.find_unit(entry.unit_id);
        if (!u)
            continue;
        std::string label = unit_label(*u, game_data);

        if (i == state.round_state.current_turn_index) {
            out << ">> ";
        } else {
            out << "   ";
        }

        out << (i + 1) << ". " << label << "  INIT " << entry.effective_initiative;
        if (i < state.round_state.current_turn_index) {
            out << "  DONE";
        }
        out << "\n";
    }
    out << "\n";
}

void print_actions_menu(std::ostream& out, const BattleState& state,
                        const std::vector<BattleActionOutcome>& outcomes,
                        const d2engine::GameDataRegistry&       game_data) {
    if (outcomes.empty()) {
        out << "No valid actions available.\n\n";
        return;
    }

    out << "ACTIONS\n\n";
    for (std::size_t i = 0; i < outcomes.size(); ++i) {
        const auto* skp = std::get_if<SkipActivationAction>(&outcomes[i].action);
        if (skp) {
            const auto* actor = state.find_unit(skp->actor_id);
            if (!actor) {
                throw std::runtime_error("print_actions_menu: forced actor missing: " +
                                         skp->actor_id);
            }

            out << "[" << i << "] " << unit_label(*actor, game_data) << "\n";
            out << "    -> FORCED SKIP (PETRIFIED)\n";
            out << "    preview:\n";

            auto deltas = collect_effect_deltas(state, outcomes[i].outcome);
            print_effect_delta_section(out, deltas, EffectDeltaKind::Removed,
                                       "  EFFECTS CONSUMED:\n", "    ");
            out << "\n";
            continue;
        }

        const auto* atk = std::get_if<AttackAction>(&outcomes[i].action);
        if (!atk)
            continue;

        const auto* actor = state.find_unit(atk->actor_id);
        if (!actor)
            continue;

        const auto* udef = game_data.find_unit(actor->type_id);

        out << "[" << i << "] " << unit_label(*actor, game_data) << "\n";

        if (std::holds_alternative<AllEnemyUnitsTarget>(atk->target)) {
            out << "    -> ALL ENEMIES\n";
        } else if (std::holds_alternative<AllAlliedUnitsTarget>(atk->target)) {
            out << "    -> ALL ALLIES\n";
        } else if (auto* ut = std::get_if<UnitTarget>(&atk->target)) {
            const auto* target = state.find_unit(ut->unit_id);
            const auto* target_after = outcomes[i].outcome.find_unit(ut->unit_id);
            if (target && target_after) {
                out << "    -> " << unit_label(*target, game_data) << "\n";
            }
        }

        out << "    attack=";
        if (udef) {
            auto bundle_support = d2battle::analyze_attack_bundle(*udef);
            if (!bundle_support.supported) {
                throw std::runtime_error(
                    "print_actions_menu: AttackAction for unsupported bundle: actor=" +
                    atk->actor_id);
            }
            auto bundle = d2battle::detail::require_supported_attack_bundle(*udef);
            out << bundle_label(bundle);
        }
        out << "\n";

        out << "    preview:\n";
        print_hp_deltas(out, state, outcomes[i].outcome, game_data);

        auto deltas = collect_effect_deltas(state, outcomes[i].outcome);
        print_effect_delta_section(out, deltas, EffectDeltaKind::Applied, "  EFFECTS APPLIED:\n",
                                   "    ");
        print_effect_delta_section(out, deltas, EffectDeltaKind::Refreshed,
                                   "  EFFECTS REFRESHED:\n", "    ");
        print_effect_delta_section(out, deltas, EffectDeltaKind::Removed, "  EFFECTS REMOVED:\n",
                                   "    ");

        out << "\n";
    }
}

void print_selected_action(std::ostream& out, const BattleState& before, const BattleAction& action,
                           const BattleState& after, const d2engine::GameDataRegistry& game_data) {
    const auto* skp = std::get_if<SkipActivationAction>(&action);
    if (skp) {
        const auto* actor = before.find_unit(skp->actor_id);
        if (!actor) {
            throw std::runtime_error("print_selected_action: forced actor missing: " +
                                     skp->actor_id);
        }

        out << ">>> FORCED ACTION\n\n";
        out << unit_label(*actor, game_data) << "\n";
        out << "skips activation: PETRIFIED\n\n";

        auto deltas = collect_effect_deltas(before, after);
        print_effect_delta_section(out, deltas, EffectDeltaKind::Removed, "EFFECTS CONSUMED:\n",
                                   "  ");
        out << "\n";
        return;
    }

    const auto* atk = std::get_if<AttackAction>(&action);
    if (!atk)
        return;

    const auto* actor = before.find_unit(atk->actor_id);
    if (!actor)
        return;

    const auto* udef = game_data.find_unit(actor->type_id);

    out << ">>> SELECTED ACTION\n\n";
    out << unit_label(*actor, game_data) << "\n";
    out << "uses";
    if (udef) {
        auto bundle_support = d2battle::analyze_attack_bundle(*udef);
        if (!bundle_support.supported) {
            throw std::runtime_error(
                "print_selected_action: AttackAction for unsupported bundle: actor=" +
                atk->actor_id);
        }
        auto bundle = d2battle::detail::require_supported_attack_bundle(*udef);
        out << " " << bundle_label(bundle);
    }
    out << "\n";

    if (std::holds_alternative<AllEnemyUnitsTarget>(atk->target)) {
        out << "on ALL ENEMIES\n\n";
    } else if (std::holds_alternative<AllAlliedUnitsTarget>(atk->target)) {
        out << "on ALL ALLIES\n\n";
    } else if (auto* ut = std::get_if<UnitTarget>(&atk->target)) {
        const auto* target = before.find_unit(ut->unit_id);
        if (target) {
            out << "on " << unit_label(*target, game_data) << "\n\n";
        }
    }

    print_result_hp_deltas(out, before, after, game_data);

    auto deltas = collect_effect_deltas(before, after);
    print_effect_delta_section(out, deltas, EffectDeltaKind::Applied, "EFFECTS APPLIED:\n", "  ");
    print_effect_delta_section(out, deltas, EffectDeltaKind::Refreshed, "EFFECTS REFRESHED:\n",
                               "  ");
    // EFFECTS REMOVED for AttackAction, EFFECTS CONSUMED for SkipActivationAction
    if (std::holds_alternative<SkipActivationAction>(action)) {
        print_effect_delta_section(out, deltas, EffectDeltaKind::Removed, "EFFECTS CONSUMED:\n",
                                   "  ");
    } else {
        print_effect_delta_section(out, deltas, EffectDeltaKind::Removed, "EFFECTS REMOVED:\n",
                                   "  ");
    }

    out << "\n";
}

// cppcheck-suppress unusedFunction
void print_battle_finished(std::ostream& out, const BattleState& state,
                           const d2engine::GameDataRegistry& game_data) {
    out << "========================================\n";
    out << "BATTLE FINISHED\n\n";
    if (state.winner.has_value()) {
        out << "WINNER: " << (*state.winner == BattleSide::Party1 ? "PARTY 1" : "PARTY 2")
            << "\n\n";
    } else {
        out << "RESULT: DRAW\n\n";
    }

    for (const auto& u : state.units) {
        const auto* udef = game_data.find_unit(u.type_id);
        int         max_hp = udef ? udef->hit_points : u.current_hp;
        std::string bar = hp_bar(u.current_hp, max_hp);
        std::string label = unit_label(u, game_data);
        out << label << "  HP " << u.current_hp << "/" << max_hp << " [" << bar << "]";
        if (!u.alive)
            out << " DEAD";
        out << "\n";
    }
    out << "========================================\n";
}
