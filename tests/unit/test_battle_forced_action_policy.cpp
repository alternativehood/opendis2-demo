#include <gtest/gtest.h>

#include <opendis2_battle/forced_action_policy.hpp>

#include <d2battle_rules/battle_action.hpp>
#include <d2battle_rules/battle_outcomes.hpp>

#include <stdexcept>
#include <vector>

namespace {

using namespace d2battle;

TEST(ForcedActionPolicyTest, ForcedActionPolicyReturnsSingleSkip) {
    BattleState         target_state;
    BattleActionOutcome outcome{
        BattleAction{SkipActivationAction{"TU", SkipActivationReason::Petrified}}, target_state};

    std::vector<BattleActionOutcome> outcomes;
    outcomes.push_back(outcome);

    const auto* result = opendis2_battle::resolve_forced_outcome(outcomes);
    ASSERT_NE(result, nullptr);
    EXPECT_EQ(&outcomes[0], result);
    ASSERT_TRUE(std::holds_alternative<SkipActivationAction>(result->action));
    const auto& skp = std::get<SkipActivationAction>(result->action);
    EXPECT_EQ(skp.actor_id, "TU");
    EXPECT_EQ(skp.reason, SkipActivationReason::Petrified);
}

TEST(ForcedActionPolicyTest, ForcedActionPolicyRejectsSkipMixedWithOtherActions) {
    std::vector<BattleActionOutcome> outcomes;
    outcomes.push_back(
        {BattleAction{SkipActivationAction{"TU", SkipActivationReason::Petrified}}, BattleState{}});
    outcomes.push_back({BattleAction{AttackAction{"PU", UnitTarget{"TU"}}}, BattleState{}});

    try {
        static_cast<void>(opendis2_battle::resolve_forced_outcome(outcomes));
        FAIL() << "expected runtime_error";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find("expected exactly one forced outcome"),
                  std::string::npos);
    }
}

TEST(ForcedActionPolicyTest, NormalSingleAttackIsNotForced) {
    std::vector<BattleActionOutcome> outcomes;
    outcomes.push_back({BattleAction{AttackAction{"PU", UnitTarget{"TU"}}}, BattleState{}});

    const auto* result = opendis2_battle::resolve_forced_outcome(outcomes);
    EXPECT_EQ(result, nullptr);
}

TEST(ForcedActionPolicyTest, ForcedActionPolicyRejectsMultipleSkipOutcomes) {
    std::vector<BattleActionOutcome> outcomes;
    outcomes.push_back(
        {BattleAction{SkipActivationAction{"T1", SkipActivationReason::Petrified}}, BattleState{}});
    outcomes.push_back(
        {BattleAction{SkipActivationAction{"T2", SkipActivationReason::Petrified}}, BattleState{}});

    try {
        static_cast<void>(opendis2_battle::resolve_forced_outcome(outcomes));
        FAIL() << "expected runtime_error";
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find("expected exactly one forced outcome"),
                  std::string::npos);
    }
}

TEST(ForcedActionPolicyTest, EmptyOutcomesReturnsNullptr) {
    std::vector<BattleActionOutcome> outcomes;

    const auto* result = opendis2_battle::resolve_forced_outcome(outcomes);
    EXPECT_EQ(result, nullptr);
}

} // namespace
