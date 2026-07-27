#include "debug_battle_outcome_resolver.hpp"

#include <variant>

namespace d2engine {

std::vector<BattleVisualEvent>
DebugBattleOutcomeResolver::resolve(const BattleVisualEvent&    emitted,
                                    const BattleSelectionModel& selection) {
    const auto* cue = std::get_if<AttackImpactCue>(&emitted);
    if (cue == nullptr) {
        return {};
    }
    auto target = cue->targets.single_target();
    if (!target.has_value()) {
        target = selection.target;
    }
    if (!target.has_value()) {
        return {};
    }
    return {TargetDamaged{.attack_id = cue->attack_id, .source = cue->source, .target = *target}};
}

} // namespace d2engine
