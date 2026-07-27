#include "battle_simulator.hpp"
#include "battle_log_writer.hpp"
#include "random_action_policy.hpp"

#include <d2battle_rules/battle_fingerprint.hpp>
#include <d2battle_rules/battle_outcomes.hpp>
#include <d2log/log.hpp>
#include <opendis2_battle/forced_action_policy.hpp>
#include <opendis2_battle/terminal_view.hpp>

#include <exception>
#include <string>

namespace d2battle_sweep {

namespace {

auto kLog = d2log::get("d2battle.sweep");

[[nodiscard]] std::string actor_label(const d2battle::BattleState&      state,
                                      const d2engine::GameDataRegistry& game_data) {
    const auto* actor = state.current_actor();
    if (!actor)
        return {};
    const auto* udef = game_data.find_unit(actor->type_id);
    std::string name = udef ? udef->name : actor->type_id;
    if (name.empty())
        name = actor->type_id;
    std::string side = (actor->side == d2battle::BattleSide::Party1) ? "P1#" : "P2#";
    return side + std::to_string(actor->member_index) + " " + actor->name + " (" + name + ")";
}

void fill_abort_fields(BattleRunResult& result, const d2battle::BattleState& state,
                       const d2engine::GameDataRegistry& game_data) {
    const auto* actor = state.current_actor();
    if (actor) {
        result.current_actor_id = actor->id;
        result.current_actor_label = actor_label(state, game_data);
    } else {
        result.current_actor_id = "<missing-current-actor>";
    }
    result.final_fingerprint = d2battle::compute_fingerprint(state);
}

void write_abort_section(BattleRunResult& result, const d2battle::BattleState& state,
                         const d2engine::GameDataRegistry& game_data, BattleLogWriter& logger,
                         BattleRunStatus status, std::string_view reason, std::uint64_t actions,
                         const std::string& diagnostic) {
    fill_abort_fields(result, state, game_data);
    result.status = status;
    result.final_round = state.round_state.round_number;
    result.actions_applied = actions;
    result.diagnostic = diagnostic;
    logger.write_aborted(result.status, reason, result.diagnostic, result.final_round,
                         result.actions_applied, result.current_actor_id,
                         result.current_actor_label, result.final_fingerprint);
    logger.flush();
}

} // namespace

BattleRunResult run_one_battle(const d2battle::BattleState&      initial_state,
                               const d2engine::GameDataRegistry& game_data,
                               RandomActionPolicy& policy, BattleLogWriter& logger,
                               const SimulatorConfig& config) {
    BattleRunResult result;
    result.party1_stack_id = initial_state.party1.source_stack_id;
    result.party2_stack_id = initial_state.party2.source_stack_id;

    try {
        d2battle::BattleState state = initial_state;
        std::uint64_t         actions_applied = 0;

        while (state.status == d2battle::BattleStatus::InProgress) {
            if (actions_applied >= config.max_actions) {
                logger.write_fatal_invariant("ACTION_LIMIT_REACHED", result.party1_stack_id,
                                             result.party2_stack_id, state, game_data);
                write_abort_section(result, state, game_data, logger,
                                    BattleRunStatus::AbortedActionLimit, "ACTION_LIMIT_REACHED",
                                    actions_applied, "action limit exceeded");
                return result;
            }

            if (state.round_state.round_number > config.max_rounds) {
                logger.write_fatal_invariant("ROUND_LIMIT_REACHED", result.party1_stack_id,
                                             result.party2_stack_id, state, game_data);
                write_abort_section(result, state, game_data, logger,
                                    BattleRunStatus::AbortedRoundLimit, "ROUND_LIMIT_REACHED",
                                    actions_applied, "round limit exceeded");
                return result;
            }

            auto outcomes = d2battle::valid_action_outcomes(state, game_data);

            if (outcomes.empty()) {
                logger.write_fatal_invariant("NO_VALID_ACTIONS_FOR_LIVE_CURRENT_ACTOR",
                                             result.party1_stack_id, result.party2_stack_id, state,
                                             game_data);
                write_abort_section(result, state, game_data, logger,
                                    BattleRunStatus::AbortedNoValidActions,
                                    "NO_VALID_ACTIONS_FOR_LIVE_CURRENT_ACTOR", actions_applied,
                                    "no valid actions for live current actor");
                return result;
            }

            logger.write_turn_header(state.round_state.round_number,
                                     state.round_state.current_turn_index, state, game_data);
            logger.write_outcomes(outcomes, state, game_data);

            const auto* forced = opendis2_battle::resolve_forced_outcome(outcomes);
            if (forced) {
                logger.write_forced_selection();
                logger.write_selected_action(state, forced->action, forced->outcome, game_data);
                logger.write_successor_deltas(state, forced->outcome, game_data);
                state = forced->outcome;
            } else {
                std::size_t idx = policy.choose_index(outcomes);
                logger.write_random_selection(outcomes.size(), idx);
                const auto& chosen = outcomes[idx];
                logger.write_selected_action(state, chosen.action, chosen.outcome, game_data);
                logger.write_successor_deltas(state, chosen.outcome, game_data);
                state = chosen.outcome;
            }

            actions_applied++;
        }

        std::string fp = d2battle::compute_fingerprint(state);
        logger.write_finished(state.winner, state.round_state.round_number, actions_applied, fp);
        logger.flush();

        result.status = BattleRunStatus::Finished;
        result.winner = state.winner;
        result.final_round = state.round_state.round_number;
        result.actions_applied = actions_applied;
        result.final_fingerprint = fp;

    } catch (const std::exception& e) {
        D2_LOG_ERROR(kLog, "rule exception in battle {} vs {}: {}", result.party1_stack_id,
                     result.party2_stack_id, e.what());

        result.status = BattleRunStatus::AbortedRuleException;
        result.diagnostic = std::string("exception: ") + e.what();
        result.current_actor_id = "<exception>";
        result.current_actor_label = "";
        result.final_fingerprint = "exception_during_battle";

        logger.write_fatal_invariant(std::string("RULE_EXCEPTION: ") + e.what(),
                                     result.party1_stack_id, result.party2_stack_id, {}, game_data);
        logger.write_aborted(result.status, "RULE_EXCEPTION", result.diagnostic, 0, 0,
                             result.current_actor_id, result.current_actor_label,
                             result.final_fingerprint);
        logger.flush();

    } catch (...) {
        D2_LOG_ERROR(kLog, "unknown exception in battle {} vs {}", result.party1_stack_id,
                     result.party2_stack_id);

        result.status = BattleRunStatus::AbortedRuleException;
        result.diagnostic = "unknown exception";
        result.current_actor_id = "<exception>";
        result.current_actor_label = "";
        result.final_fingerprint = "";

        logger.write_fatal_invariant("UNKNOWN_EXCEPTION", result.party1_stack_id,
                                     result.party2_stack_id, {}, game_data);
        logger.write_aborted(result.status, "UNKNOWN_EXCEPTION", result.diagnostic, 0, 0,
                             result.current_actor_id, result.current_actor_label,
                             result.final_fingerprint);
        logger.flush();
    }

    return result;
}

} // namespace d2battle_sweep
