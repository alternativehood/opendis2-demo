#include "forced_action_policy.hpp"

#include <stdexcept>

namespace opendis2_battle {

const d2battle::BattleActionOutcome*
resolve_forced_outcome(const std::vector<d2battle::BattleActionOutcome>& outcomes) {
    const d2battle::BattleActionOutcome* forced = nullptr;

    for (const auto& o : outcomes) {
        if (std::holds_alternative<d2battle::SkipActivationAction>(o.action)) {
            if (forced) {
                throw std::runtime_error(
                    "resolve_forced_outcome: expected exactly one forced outcome, "
                    "got multiple");
            }
            forced = &o;
        }
    }

    if (!forced)
        return nullptr;

    if (outcomes.size() != 1) {
        throw std::runtime_error("resolve_forced_outcome: expected exactly one forced outcome, "
                                 "got mixed with non-skip");
    }

    return forced;
}

} // namespace opendis2_battle
