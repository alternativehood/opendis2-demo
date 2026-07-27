#include "attack_target_enumeration.hpp"

#include <algorithm>
#include <set>
#include <stdexcept>

namespace d2battle {
namespace detail {

namespace {

bool matches_vitality(const BattleUnitState& unit, AttackTargetVitality vitality) {
    if (vitality == AttackTargetVitality::Alive)
        return unit.alive;
    return !unit.alive;
}

} // namespace

std::vector<std::string> enumerate_unit_targets(const BattleState&         state,
                                                const BattleUnitState&     actor,
                                                const SupportedAttackRule& rule) {

    BattleSide target_side = (rule.target_policy.relation == AttackTargetRelation::Enemy)
                                 ? opposite_side(actor.side)
                                 : actor.side;

    const auto&              side_state = state.side(target_side);
    std::vector<std::string> result;
    std::set<int>            seen_members;

    for (std::size_t slot = 0; slot < 6; ++slot) {
        if (!side_state.members[slot].has_value())
            continue;

        const auto& uid = *side_state.members[slot];
        const auto* unit = state.find_unit(uid);
        if (!unit) {
            throw std::runtime_error("enumerate_unit_targets: member unit not found: side=" +
                                     std::to_string(static_cast<int>(target_side)) +
                                     " slot=" + std::to_string(slot) + " unit=" + uid);
        }

        if (!matches_vitality(*unit, rule.target_policy.vitality))
            continue;

        if (seen_members.contains(unit->member_index))
            continue;

        seen_members.insert(unit->member_index);
        result.push_back(uid);
    }

    return result;
}

} // namespace detail
} // namespace d2battle
