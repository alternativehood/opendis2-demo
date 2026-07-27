#pragma once

#include "stack_catalog.hpp"
#include "stack_pair_generator.hpp"

#include <d2engine/assets/game_data_registry.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace d2runtime {
struct AdventureWorldState;
} // namespace d2runtime

namespace d2battle_sweep {

struct SweepConfig {
    std::filesystem::path scenario_path;
    std::filesystem::path game_root;
    std::filesystem::path output_dir = "battle_logs";
    std::uint64_t         seed = 0x1A2B3C4D5E6F7890ULL;
    std::uint64_t         max_actions = 1000;
    std::uint32_t         max_rounds = 100;
};

struct BattleLogEntry {
    int         sequence = 0;
    std::string party1_stack_id;
    std::string party2_stack_id;
    std::string status;
    std::string winner;
    std::string rounds;
    std::string actions;
    std::string log_filename;
};

struct SweepSummary {
    std::string           scenario_path;
    std::uint64_t         global_seed = 0;
    std::size_t           eligible_stacks = 0;
    std::size_t           directed_pairs = 0;
    std::uint64_t         max_actions = 0;
    std::uint32_t         max_rounds = 0;
    std::filesystem::path run_dir;

    std::vector<BattleLogEntry> entries;

    std::size_t total = 0;
    std::size_t finished = 0;
    std::size_t aborted_no_valid_actions = 0;
    std::size_t aborted_action_limit = 0;
    std::size_t aborted_round_limit = 0;
    std::size_t aborted_rule_exception = 0;
    std::size_t aborted_bootstrap_failure = 0;
    std::size_t aborted_log_failure = 0;
    std::size_t party1_wins = 0;
    std::size_t party2_wins = 0;
};

[[nodiscard]] SweepSummary run_sweep(const SweepConfig& config);

[[nodiscard]] std::filesystem::path create_run_directory(const std::filesystem::path& output_dir,
                                                         std::uint64_t                seed);

[[nodiscard]] std::string make_battle_filename(int sequence, const std::string& p1_id,
                                               const std::string& p1_desc, const std::string& p2_id,
                                               const std::string& p2_desc);

[[nodiscard]] std::string format_battle_sequence(std::uint64_t sequence);

} // namespace d2battle_sweep
