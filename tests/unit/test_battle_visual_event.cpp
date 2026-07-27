#include <gtest/gtest.h>

#include "d2engine/battle_view/battle_visual_event.hpp"

#include <array>
#include <optional>
#include <type_traits>
#include <vector>

namespace d2engine {

static_assert(std::is_same_v<decltype(ActorSelected::selected), UnitInstanceId>);
static_assert(std::is_same_v<decltype(TargetSelected::selected), UnitInstanceId>);
static_assert(std::is_same_v<decltype(AttackStarted::attack_id), AttackInstanceId>);
static_assert(std::is_same_v<decltype(AttackStarted::source), UnitInstanceId>);
static_assert(std::is_same_v<decltype(TargetDamaged::attack_id), AttackInstanceId>);
static_assert(std::is_same_v<decltype(TargetDamaged::target), UnitInstanceId>);
static_assert(std::is_same_v<decltype(TargetDamaged::current_hp), std::optional<int>>);
static_assert(std::is_same_v<decltype(TargetKilled::target), UnitInstanceId>);
static_assert(std::is_same_v<decltype(LifeDrained::source), UnitInstanceId>);
static_assert(std::is_same_v<decltype(LifeDrained::target_current_hp), std::optional<int>>);
static_assert(std::is_same_v<decltype(SourceHealed::source), UnitInstanceId>);
static_assert(std::is_same_v<decltype(SourceHealed::current_hp), std::optional<int>>);
static_assert(std::is_same_v<decltype(UnitHitReceived::target), UnitInstanceId>);
static_assert(std::is_same_v<decltype(UnitKilled::target), UnitInstanceId>);
static_assert(std::is_same_v<decltype(UnitReviveStarted::target), UnitInstanceId>);
static_assert(std::is_same_v<decltype(UnitRevived::target), UnitInstanceId>);
static_assert(std::is_same_v<decltype(CastEffectStarted::caster), UnitInstanceId>);
static_assert(std::is_same_v<decltype(BattleEffectStarted::source), UnitInstanceId>);
TEST(BattleVisualEvent, ReportsStableNamesForAllAlternatives) {
    const std::array<BattleVisualEvent, 16> events = {
        ActorSelected{.previous = UnitInstanceId{1u}, .selected = UnitInstanceId{2u}},
        TargetSelected{.previous = UnitInstanceId{1u}, .selected = UnitInstanceId{2u}},
        AttackStarted{.attack_id = AttackInstanceId{1u},
                      .source = UnitInstanceId{1u},
                      .targets = TargetSet::single(UnitInstanceId{2u})},
        AttackImpactCue{.attack_id = AttackInstanceId{1u},
                        .source = UnitInstanceId{1u},
                        .targets = TargetSet::single(UnitInstanceId{2u})},
        TargetDamaged{.attack_id = AttackInstanceId{1u},
                      .source = UnitInstanceId{1u},
                      .target = UnitInstanceId{2u}},
        TargetMissed{.attack_id = AttackInstanceId{1u}, .target = UnitInstanceId{2u}},
        TargetResisted{.attack_id = AttackInstanceId{1u}, .target = UnitInstanceId{2u}},
        TargetKilled{.attack_id = AttackInstanceId{1u}, .target = UnitInstanceId{2u}},
        LifeDrained{.attack_id = AttackInstanceId{1u},
                    .source = UnitInstanceId{1u},
                    .target = UnitInstanceId{2u}},
        SourceHealed{.attack_id = AttackInstanceId{1u}, .source = UnitInstanceId{1u}},
        UnitHitReceived{.target = UnitInstanceId{2u}},
        UnitKilled{.target = UnitInstanceId{2u}},
        UnitReviveStarted{.target = UnitInstanceId{2u}},
        UnitRevived{.target = UnitInstanceId{2u}},
        CastEffectStarted{.caster = UnitInstanceId{1u}, .role = BattleEffectRole::Heff},
        BattleEffectStarted{.source = UnitInstanceId{1u},
                            .role = BattleEffectRole::Heff,
                            .visual_role = BattleEffectVisualRole::TargetDamageFx},
    };
    const std::array<std::string_view, 16> names = {
        "ActorSelected",     "TargetSelected", "AttackStarted",     "AttackImpactCue",
        "TargetDamaged",     "TargetMissed",   "TargetResisted",    "TargetKilled",
        "LifeDrained",       "SourceHealed",   "UnitHitReceived",   "UnitKilled",
        "UnitReviveStarted", "UnitRevived",    "CastEffectStarted", "BattleEffectStarted",
    };

    for (std::size_t i = 0; i < events.size(); ++i) {
        EXPECT_EQ(battle_visual_event_name(events[i]), names[i]);
    }
}

TEST(BattleVisualEvent, TargetSetUnitListPreservesScriptOrder) {
    const auto targets =
        TargetSet::unit_list({UnitInstanceId{7u}, UnitInstanceId{4u}, UnitInstanceId{9u}});
    EXPECT_EQ(targets.kind, TargetSetKind::UnitList);
    EXPECT_EQ(targets.units, (std::vector<UnitInstanceId>{UnitInstanceId{7u}, UnitInstanceId{4u},
                                                          UnitInstanceId{9u}}));
    EXPECT_FALSE(targets.single_target().has_value());
}

TEST(BattleVisualEvent, TargetSetSingleExposesUnit) {
    const auto targets = TargetSet::single(UnitInstanceId{7u});
    ASSERT_TRUE(targets.single_target().has_value());
    EXPECT_EQ(*targets.single_target(), UnitInstanceId{7u});
}

TEST(BattleVisualEvent, BattleEffectStartedStoresVisualRoleAndScope) {
    const BattleEffectStarted effect{
        .source = UnitInstanceId{1u},
        .role = BattleEffectRole::Tuch,
        .visual_role = BattleEffectVisualRole::TeamOverlayFx,
        .targets = TargetSet::unit_list({UnitInstanceId{2u}, UnitInstanceId{3u}}),
    };

    EXPECT_EQ(effect.source, UnitInstanceId{1u});
    EXPECT_EQ(effect.role, BattleEffectRole::Tuch);
    EXPECT_EQ(effect.visual_role, BattleEffectVisualRole::TeamOverlayFx);
    EXPECT_EQ(effect.targets.kind, TargetSetKind::UnitList);
}

} // namespace d2engine
