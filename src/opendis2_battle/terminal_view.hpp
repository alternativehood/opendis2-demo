#pragma once

#include <d2battle_rules/battle_state.hpp>
#include <d2battle_rules/battle_action.hpp>
#include <d2engine/assets/game_data_registry.hpp>

#include <ostream>
#include <string>
#include <vector>

[[nodiscard]] std::string hp_bar(int current, int max, int width = 10);

void print_formation_board(std::ostream& out, const d2battle::BattleState& state,
                           const d2engine::GameDataRegistry& game_data);

void print_unit_legend(std::ostream& out, const d2battle::BattleState& state,
                       const d2engine::GameDataRegistry& game_data);

void print_turn_order(std::ostream& out, const d2battle::BattleState& state,
                      const d2engine::GameDataRegistry& game_data);

void print_actions_menu(std::ostream& out, const d2battle::BattleState& state,
                        const std::vector<d2battle::BattleActionOutcome>& outcomes,
                        const d2engine::GameDataRegistry&                 game_data);

void print_selected_action(std::ostream& out, const d2battle::BattleState& before,
                           const d2battle::BattleAction& action, const d2battle::BattleState& after,
                           const d2engine::GameDataRegistry& game_data);

void print_battle_finished(std::ostream& out, const d2battle::BattleState& state,
                           const d2engine::GameDataRegistry& game_data);
