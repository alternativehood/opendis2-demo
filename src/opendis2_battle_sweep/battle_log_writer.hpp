#pragma once

#include <d2battle_rules/attack_support.hpp>
#include <d2battle_rules/battle_action.hpp>
#include <d2battle_rules/battle_state.hpp>
#include <d2engine/assets/game_data_registry.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace d2battle_sweep {

enum class BattleRunStatus;

void write_attack_bundle_block(std::ostream& out, const d2engine::UnitDef& udef,
                               const d2battle::AttackBundleSupport& support);

class BattleLogWriter {
public:
    explicit BattleLogWriter(const std::filesystem::path& filepath);

    BattleLogWriter(const BattleLogWriter&) = delete;
    BattleLogWriter& operator=(const BattleLogWriter&) = delete;
    BattleLogWriter(BattleLogWriter&&) = delete;
    BattleLogWriter& operator=(BattleLogWriter&&) = delete;

    ~BattleLogWriter();

    void write_header(const std::string& scenario_path, std::uint64_t global_seed,
                      std::uint64_t battle_seed, std::string_view sequence,
                      const std::string& party1_stack_id, const std::string& party2_stack_id);

    void write_party_detail(const d2battle::BattleState& state, d2battle::BattleSide side,
                            const d2engine::GameDataRegistry& game_data);

    void write_initial_fingerprint(const std::string& fingerprint);

    void write_turn_header(std::uint32_t round, std::size_t turn_index,
                           const d2battle::BattleState&      state,
                           const d2engine::GameDataRegistry& game_data);

    void write_outcomes(const std::vector<d2battle::BattleActionOutcome>& outcomes,
                        const d2battle::BattleState&                      state,
                        const d2engine::GameDataRegistry&                 game_data);

    void write_random_selection(std::size_t candidate_count, std::size_t selected_index);

    void write_forced_selection();

    void write_selected_action(const d2battle::BattleState&      before,
                               const d2battle::BattleAction&     action,
                               const d2battle::BattleState&      after,
                               const d2engine::GameDataRegistry& game_data);

    void write_successor_deltas(const d2battle::BattleState&      before,
                                const d2battle::BattleState&      after,
                                const d2engine::GameDataRegistry& game_data);

    void write_finished(std::optional<d2battle::BattleSide> winner, std::uint32_t rounds,
                        std::uint64_t actions, const std::string& fingerprint);

    void write_aborted(BattleRunStatus status, std::string_view reason, std::string_view diagnostic,
                       std::uint32_t round, std::uint64_t actions,
                       std::string_view current_actor_id, std::string_view current_actor_label,
                       std::string_view fingerprint);

    void write_fatal_invariant(const std::string& reason, const std::string& party1_stack,
                               const std::string& party2_stack, const d2battle::BattleState& state,
                               const d2engine::GameDataRegistry& game_data);

    void flush();

private:
    std::ofstream file_;
    std::ostream& out_;
};

} // namespace d2battle_sweep
