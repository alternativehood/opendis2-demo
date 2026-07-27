#include "battle_apply.hpp"
#include "battle_fingerprint.hpp"
#include "battle_validate.hpp"
#include "detail/battle_action_validate_internal.hpp"
#include "detail/battle_apply_internal.hpp"

#include <d2log/log.hpp>

#include <sstream>
#include <stdexcept>
#include <string>

namespace d2battle {

namespace {
auto kLog = d2log::get("d2battle.apply");

[[nodiscard]] std::string target_to_string(const AttackTarget& target) {
    if (std::holds_alternative<AllEnemyUnitsTarget>(target))
        return "ALL_ENEMY_UNITS";
    if (std::holds_alternative<AllAlliedUnitsTarget>(target))
        return "ALL_ALLIED_UNITS";
    if (auto* ut = std::get_if<UnitTarget>(&target))
        return ut->unit_id;
    return "?";
}

} // namespace

BattleState apply(const BattleState& state, const BattleAction& action,
                  const d2engine::GameDataRegistry& game_data) {
    validate_battle_state(state);

    if (state.status != BattleStatus::InProgress)
        throw std::runtime_error("apply: battle is already finished");

    auto validation = detail::validate_action_on_valid_state(action, state, game_data);
    if (validation != ActionValidationError::None) {
        std::ostringstream oss;
        const auto*        atk = std::get_if<AttackAction>(&action);
        oss << "apply: invalid action reason=" << to_string(validation);
        if (atk) {
            oss << " actor=" << atk->actor_id << " target=" << target_to_string(atk->target);
        }
        const auto* actor = state.current_actor();
        oss << " current_actor=" << (actor ? actor->id : "none");
        oss << " round=" << state.round_state.round_number;
        D2_LOG_ERROR(kLog, "{}", oss.str());
        throw std::runtime_error(oss.str());
    }

    D2_LOG_DEBUG(kLog, "=== STATE TRANSITION COMPUTED ===");
    if (const auto* skp = std::get_if<SkipActivationAction>(&action)) {
        D2_LOG_DEBUG(kLog, "round={} actor={} action=SKIP reason={}",
                     state.round_state.round_number, skp->actor_id, static_cast<int>(skp->reason));
    } else if (const auto* atk_action = std::get_if<AttackAction>(&action)) {
        D2_LOG_DEBUG(kLog, "round={} actor={} action=ATTACK target={}",
                     state.round_state.round_number, atk_action->actor_id,
                     target_to_string(atk_action->target));
    }

    std::string fp_before = compute_fingerprint(state);

    BattleState next = detail::apply_validated_action_on_valid_state(state, action, game_data);

    std::string fp_after = compute_fingerprint(next);
    D2_LOG_DEBUG(kLog, "state_before={} state_after={}", fp_before, fp_after);

    return next;
}

} // namespace d2battle
