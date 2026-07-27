#include "battle_outcomes.hpp"
#include "battle_validate.hpp"
#include "detail/battle_apply_internal.hpp"
#include "detail/battle_valid_actions_internal.hpp"

#include <vector>

namespace d2battle {

std::vector<BattleActionOutcome>
valid_action_outcomes(const BattleState& state, const d2engine::GameDataRegistry& game_data) {
    validate_battle_state(state);
    auto actions = detail::valid_actions_on_valid_state(state, game_data);
    std::vector<BattleActionOutcome> outcomes;
    outcomes.reserve(actions.size());

    for (const auto& action : actions) {
        outcomes.push_back(
            {action, detail::apply_validated_action_on_valid_state(state, action, game_data)});
    }

    return outcomes;
}

} // namespace d2battle
