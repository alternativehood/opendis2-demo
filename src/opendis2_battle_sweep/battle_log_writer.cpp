#include "battle_log_writer.hpp"
#include "battle_simulator.hpp"

#include <opendis2_battle/attack_presentation_labels.hpp>
#include <opendis2_battle/terminal_view.hpp>

#include <d2battle_rules/battle_effect.hpp>
#include <d2battle_rules/battle_fingerprint.hpp>

#include <cstdint>
#include <string_view>
#include <variant>

namespace d2battle_sweep {

namespace {

[[nodiscard]] std::string unit_label(const d2battle::BattleUnitState&  u,
                                     const d2engine::GameDataRegistry& game_data) {
    const auto* udef = game_data.find_unit(u.type_id);
    std::string name = udef ? udef->name : u.type_id;
    if (name.empty())
        name = u.type_id;
    std::string side = (u.side == d2battle::BattleSide::Party1) ? "P1#" : "P2#";
    return side + std::to_string(u.member_index) + " " + u.name + " (" + name + ")";
}

[[nodiscard]] std::string effect_status_text(const d2battle::BattleUnitEffectState& e) {
    if (std::holds_alternative<d2battle::PetrifiedEffect>(e))
        return "PETRIFIED";
    return "?";
}

[[nodiscard]] std::string status_string(BattleRunStatus s) {
    switch (s) {
    case BattleRunStatus::Finished:
        return "FINISHED";
    case BattleRunStatus::AbortedNoValidActions:
        return "ABORTED_NO_VALID_ACTIONS";
    case BattleRunStatus::AbortedActionLimit:
        return "ABORTED_ACTION_LIMIT";
    case BattleRunStatus::AbortedRoundLimit:
        return "ABORTED_ROUND_LIMIT";
    case BattleRunStatus::AbortedRuleException:
        return "ABORTED_RULE_EXCEPTION";
    case BattleRunStatus::AbortedBootstrapFailure:
        return "ABORTED_BOOTSTRAP_FAILURE";
    case BattleRunStatus::AbortedLogFailure:
        return "ABORTED_LOG_FAILURE";
    }
    return "UNKNOWN";
}

} // namespace

void write_attack_bundle_block(std::ostream& out, const d2engine::UnitDef& /*udef*/,
                               const d2battle::AttackBundleSupport& support) {
    auto write_component = [&](const char* label, const d2battle::AttackComponentSupport& comp) {
        out << "  " << label << ":\n";
        out << "    present=" << (comp.present ? "yes" : "no") << "\n";
        out << "    id=" << comp.attack_id << "\n";
        if (!comp.present) {
            out << "    definition_status=NOT_PRESENT\n";
            out << "    class=\n";
            out << "    reach=\n";
            out << "    class_supported=no\n";
            out << "    reach_supported=no\n";
            return;
        }
        if (comp.definition) {
            out << "    definition_status=FOUND\n";
            out << "    class="
                << d2battle_terminal::attack_class_label(comp.definition->attack_class) << "\n";
            out << "    reach=" << d2battle_terminal::attack_reach_label(comp.definition->reach)
                << "\n";
        } else {
            out << "    definition_status=MISSING\n";
            out << "    class=\n";
            out << "    reach=\n";
        }
        out << "    class_supported=" << (comp.class_supported ? "yes" : "no") << "\n";
        out << "    reach_supported=" << (comp.reach_supported ? "yes" : "no") << "\n";
    };

    out << "attack_bundle:\n";
    out << "  support=" << (support.supported ? "SUPPORTED" : "UNSUPPORTED") << "\n";
    out << "  error=" << d2battle::to_string(support.error) << "\n";

    write_component("primary", support.primary);
    write_component("secondary", support.secondary);
}

BattleLogWriter::BattleLogWriter(const std::filesystem::path& filepath)
    : file_(filepath, std::ios::binary), out_(file_) {}

BattleLogWriter::~BattleLogWriter() {
    if (file_.is_open())
        file_.close();
}

void BattleLogWriter::write_header(const std::string& scenario_path, std::uint64_t global_seed,
                                   std::uint64_t battle_seed, std::string_view sequence,
                                   const std::string& party1_stack_id,
                                   const std::string& party2_stack_id) {
    out_ << "=== BATTLE SWEEP CASE ===\n\n";
    out_ << "scenario=" << scenario_path << "\n";
    out_ << "global_seed=" << global_seed << "\n";
    out_ << "battle_seed=" << battle_seed << "\n";
    out_ << "sequence=" << sequence << "\n\n";
    out_ << "party1_stack_id=" << party1_stack_id << "\n";
    out_ << "party2_stack_id=" << party2_stack_id << "\n\n";
}

void BattleLogWriter::write_party_detail(const d2battle::BattleState&      state,
                                         d2battle::BattleSide              side,
                                         const d2engine::GameDataRegistry& game_data) {
    const auto* side_label = (side == d2battle::BattleSide::Party1) ? "PARTY 1" : "PARTY 2";
    const auto& sstate = state.side(side);

    out_ << side_label << ":\n";
    out_ << "  source_stack_id=" << sstate.source_stack_id << "\n";
    out_ << "  owner=" << sstate.owner << "\n";
    out_ << "  subrace=" << sstate.subrace << "\n";
    out_ << "  leader_id=" << sstate.leader_id << "\n";
    out_ << "  leader_alive=" << static_cast<int>(sstate.leader_alive) << "\n";
    out_ << "  morale=" << sstate.morale << "\n";
    out_ << "  battles_won=" << sstate.battles_won << "\n";
    out_ << "  banner=" << sstate.banner << "\n";
    out_ << "  tome=" << sstate.tome << "\n";
    out_ << "  battle1=" << sstate.battle1 << "\n";
    out_ << "  battle2=" << sstate.battle2 << "\n";
    out_ << "  artifact1=" << sstate.artifact1 << "\n";
    out_ << "  artifact2=" << sstate.artifact2 << "\n";
    out_ << "  boots=" << sstate.boots << "\n";
    out_ << "  cell_members={";
    for (int i = 0; i < 6; ++i) {
        if (i > 0)
            out_ << ",";
        out_ << sstate.cell_members[static_cast<std::size_t>(i)];
    }
    out_ << "}\n";

    for (const auto& u : state.units) {
        if (u.side != side)
            continue;
        const auto* udef = game_data.find_unit(u.type_id);
        int         max_hp = udef ? udef->hit_points : u.current_hp;
        out_ << "\n  member:\n";
        out_ << "    instance_id=" << u.id << "\n";
        out_ << "    type_id=" << u.type_id << "\n";
        out_ << "    display_name=" << u.name << "\n";
        if (udef && !udef->name.empty())
            out_ << "    unit_def_name=" << udef->name << "\n";
        out_ << "    member_slot=" << u.member_index << "\n";
        out_ << "    formation_cell=" << u.formation_cell << "\n";
        out_ << "    HP=" << u.current_hp << "/" << max_hp << "\n";
        out_ << "    XP=" << u.xp << "\n";
        out_ << "    alive=" << (u.alive ? "yes" : "no") << "\n";
        if (!u.effects.empty()) {
            out_ << "    effects:";
            for (const auto& e : u.effects)
                out_ << " " << effect_status_text(e);
            out_ << "\n";
        }
        if (udef) {
            if (udef->primary_attack) {
                out_ << "    primary_attack: id=" << udef->primary_attack->attack_id << " class="
                     << d2battle_terminal::attack_class_label(udef->primary_attack->attack_class)
                     << " reach="
                     << d2battle_terminal::attack_reach_label(udef->primary_attack->reach) << "\n";
            }
            if (udef->secondary_attack) {
                out_ << "    secondary_attack: id=" << udef->secondary_attack->attack_id
                     << " class="
                     << d2battle_terminal::attack_class_label(udef->secondary_attack->attack_class)
                     << " reach="
                     << d2battle_terminal::attack_reach_label(udef->secondary_attack->reach)
                     << "\n";
            }
        }
    }
    out_ << "\n";
}

void BattleLogWriter::write_initial_fingerprint(const std::string& fingerprint) {
    out_ << "initial_state_fingerprint=" << fingerprint << "\n\n";
}

void BattleLogWriter::write_turn_header(std::uint32_t round, std::size_t turn_index,
                                        const d2battle::BattleState&      state,
                                        const d2engine::GameDataRegistry& game_data) {
    out_ << "=== ROUND " << round << " TURN " << turn_index << " ===\n\n";

    const auto* actor = state.current_actor();
    if (actor) {
        out_ << "current_actor=" << unit_label(*actor, game_data) << "\n";
    }

    std::string fp = d2battle::compute_fingerprint(state);
    out_ << "current_state_fingerprint=" << fp << "\n\n";
}

void BattleLogWriter::write_outcomes(const std::vector<d2battle::BattleActionOutcome>& outcomes,
                                     const d2battle::BattleState&                      state,
                                     const d2engine::GameDataRegistry&                 game_data) {
    out_ << "all_valid_outcomes count=" << outcomes.size() << "\n";
    for (std::size_t i = 0; i < outcomes.size(); ++i) {
        const auto& o = outcomes[i];
        out_ << "outcome[" << i << "]: ";

        if (auto* skp = std::get_if<d2battle::SkipActivationAction>(&o.action)) {
            const auto* actor = state.find_unit(skp->actor_id);
            out_ << "SkipActivation actor=" << skp->actor_id;
            if (actor)
                out_ << " (" << unit_label(*actor, game_data) << ")";
            out_ << "\n";
        } else if (auto* atk = std::get_if<d2battle::AttackAction>(&o.action)) {
            const auto* actor = state.find_unit(atk->actor_id);
            out_ << "Attack actor=" << atk->actor_id;
            if (actor)
                out_ << " (" << unit_label(*actor, game_data) << ")";
            if (std::holds_alternative<d2battle::AllEnemyUnitsTarget>(atk->target)) {
                out_ << " target=ALL_ENEMIES";
            } else if (std::holds_alternative<d2battle::AllAlliedUnitsTarget>(atk->target)) {
                out_ << " target=ALL_ALLIES";
            } else if (auto* ut = std::get_if<d2battle::UnitTarget>(&atk->target)) {
                out_ << " target=" << ut->unit_id;
            }
            out_ << "\n";
        }
    }
    out_ << "\n";
}

void BattleLogWriter::write_random_selection(std::size_t candidate_count,
                                             std::size_t selected_index) {
    out_ << "selection_policy=RANDOM_UNIFORM\n";
    out_ << "candidate_count=" << candidate_count << "\n";
    out_ << "selected_index=" << selected_index << "\n\n";
}

void BattleLogWriter::write_forced_selection() {
    out_ << "selection_policy=FORCED\n";
    out_ << "candidate_count=1\n";
    out_ << "selected_index=0\n\n";
}

void BattleLogWriter::write_selected_action(const d2battle::BattleState&      before,
                                            const d2battle::BattleAction&     action,
                                            const d2battle::BattleState&      after,
                                            const d2engine::GameDataRegistry& game_data) {
    print_selected_action(out_, before, action, after, game_data);
}

void BattleLogWriter::write_successor_deltas(const d2battle::BattleState&      before,
                                             const d2battle::BattleState&      after,
                                             const d2engine::GameDataRegistry& game_data) {
    const auto* actor_before = before.current_actor();
    if (actor_before) {
        const auto* actor_after = after.find_unit(actor_before->id);
        if (actor_after) {
            if (!actor_after->alive && actor_before->alive) {
                out_ << "current_actor_died=yes\n";
            }
        }
    }

    const auto* actor_after = after.current_actor();
    if (actor_after) {
        out_ << "next_actor=" << actor_after->id << " (" << unit_label(*actor_after, game_data)
             << ")\n";
    }

    std::string fp = d2battle::compute_fingerprint(after);
    out_ << "successor_fingerprint=" << fp << "\n\n";
}

void BattleLogWriter::write_finished(std::optional<d2battle::BattleSide> winner,
                                     std::uint32_t rounds, std::uint64_t actions,
                                     const std::string& fingerprint) {
    out_ << "========================================\n";
    out_ << "BATTLE FINISHED\n";
    out_ << "========================================\n\n";
    out_ << "status=FINISHED\n";
    if (winner.has_value()) {
        out_ << "winner=" << (*winner == d2battle::BattleSide::Party1 ? "PARTY_1" : "PARTY_2")
             << "\n";
    } else {
        out_ << "winner=DRAW\n";
    }
    out_ << "rounds=" << rounds << "\n";
    out_ << "actions_applied=" << actions << "\n";
    out_ << "final_state_fingerprint=" << fingerprint << "\n";
    out_ << "========================================\n";
}

void BattleLogWriter::write_aborted(BattleRunStatus status, std::string_view reason,
                                    std::string_view diagnostic, std::uint32_t round,
                                    std::uint64_t actions, std::string_view current_actor_id,
                                    std::string_view current_actor_label,
                                    std::string_view fingerprint) {
    out_ << "========================================\n";
    out_ << "BATTLE ABORTED\n";
    out_ << "========================================\n\n";
    out_ << "status=" << status_string(status) << "\n";
    out_ << "reason=" << reason << "\n";
    out_ << "diagnostic=" << (diagnostic.empty() ? "<none>" : diagnostic) << "\n";
    out_ << "round=" << round << "\n";
    out_ << "actions_applied=" << actions << "\n";
    out_ << "current_actor=" << current_actor_id << "\n";
    if (!current_actor_label.empty())
        out_ << "current_actor_label=" << current_actor_label << "\n";
    out_ << "state_fingerprint=" << fingerprint << "\n";
    out_ << "========================================\n";
}

void BattleLogWriter::write_fatal_invariant(const std::string&                reason,
                                            const std::string&                party1_stack,
                                            const std::string&                party2_stack,
                                            const d2battle::BattleState&      state,
                                            const d2engine::GameDataRegistry& game_data) {
    out_ << "==================================================\n";
    out_ << "FATAL BATTLE INVARIANT\n\n";
    out_ << "reason=" << reason << "\n";
    out_ << "party1_stack=" << party1_stack << "\n";
    out_ << "party2_stack=" << party2_stack << "\n";

    if (state.status == d2battle::BattleStatus::InProgress) {
        out_ << "round=" << state.round_state.round_number << "\n";
        out_ << "turn_index=" << state.round_state.current_turn_index << "\n";
    }

    const auto* actor = state.current_actor();
    if (actor) {
        out_ << "current_actor=" << actor->id << "\n";
        out_ << "current_actor_label=" << unit_label(*actor, game_data) << "\n";
        out_ << "current_actor_type=" << actor->type_id << "\n";

        const auto* udef = game_data.find_unit(actor->type_id);
        if (udef) {
            out_ << "unit_def_status=FOUND\n";
            out_ << "current_actor_unit_name=" << udef->name << "\n";
        } else {
            out_ << "unit_def_status=MISSING\n";
        }

        if (udef) {
            auto bundle_support = d2battle::analyze_attack_bundle(*udef);
            write_attack_bundle_block(out_, *udef, bundle_support);
        } else {
            out_ << "attack_bundle:\n"
                    "  status=UNAVAILABLE_WITHOUT_UNIT_DEF\n";
        }
    }

    std::string fp = d2battle::compute_fingerprint(state);
    out_ << "state_fingerprint=" << fp << "\n";
    out_ << "battle_status="
         << (state.status == d2battle::BattleStatus::InProgress ? "InProgress" : "Finished")
         << "\n";
    out_ << "==================================================\n\n";
}

void BattleLogWriter::flush() {
    out_.flush();
}

} // namespace d2battle_sweep
