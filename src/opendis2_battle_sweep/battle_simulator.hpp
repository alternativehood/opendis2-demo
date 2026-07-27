#pragma once

#include <d2battle_rules/battle_state.hpp>
#include <d2engine/assets/game_data_registry.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace d2battle_sweep {

enum class BattleRunStatus {
    Finished,
    AbortedNoValidActions,
    AbortedActionLimit,
    AbortedRoundLimit,
    AbortedRuleException,
    AbortedBootstrapFailure,
    AbortedLogFailure,
};

struct BattleRunResult {
    BattleRunStatus status;

    std::optional<d2battle::BattleSide> winner;

    std::uint32_t final_round = 0;
    std::uint64_t actions_applied = 0;

    std::string party1_stack_id;
    std::string party2_stack_id;

    std::string current_actor_id;
    std::string current_actor_label;
    std::string final_fingerprint;

    std::string diagnostic;
};

class RandomActionPolicy;
class BattleLogWriter;

struct SimulatorConfig {
    std::uint64_t max_actions = 1000;
    std::uint32_t max_rounds = 100;
    std::uint64_t battle_seed = 0;
};

[[nodiscard]] BattleRunResult run_one_battle(const d2battle::BattleState&      initial_state,
                                             const d2engine::GameDataRegistry& game_data,
                                             RandomActionPolicy& policy, BattleLogWriter& logger,
                                             const SimulatorConfig& config);

} // namespace d2battle_sweep
