#include <gtest/gtest.h>

#include "d2engine/battle_view/battle_visual_profile_registry.hpp"
#include "d2engine/battle_view/battle_unit.hpp"
#include "d2engine/battle_view/unit_lifecycle_visual_profile.hpp"
#include "d2engine/battle_view/unit_state_clip.hpp"

namespace d2engine {

// Compile-time: BattleUnit must be move-constructible
static_assert(std::is_move_constructible_v<BattleUnit>);
static_assert(!std::is_convertible_v<UnitVisualProfileId, UnitInstanceId>);
static_assert(!std::is_convertible_v<UnitLifecycleVisualProfileId, VisualEntityId>);

TEST(BattleUnit, DefaultDirectionIsA) {
    const BattleUnit unit;
    EXPECT_EQ(unit.coord.side, BattleSide::Attacker);
    EXPECT_EQ(unit.direction, 'A');
}

TEST(BattleUnit, AttackerDirectionA) {
    BattleUnit unit;
    unit.coord.side = BattleSide::Attacker;
    unit.direction = 'A';
    EXPECT_EQ(unit.direction, 'A');
}

TEST(BattleUnit, DefenderDirectionD) {
    BattleUnit unit;
    unit.coord.side = BattleSide::Defender;
    unit.direction = 'D';
    EXPECT_EQ(unit.direction, 'D');
}

TEST(BattleUnit, MoveConstruct) {
    BattleUnit a;
    a.coord.side = BattleSide::Defender;
    a.direction = 'D';

    const BattleUnit b = std::move(a);
    EXPECT_EQ(b.coord.side, BattleSide::Defender);
    EXPECT_EQ(b.direction, 'D');
}

TEST(BattleVisualProfileRegistry, MissingProfileReturnsNull) {
    UnitVisualProfileRegistry const          unit_profiles;
    UnitLifecycleVisualProfileRegistry const lifecycle_profiles;

    EXPECT_EQ(unit_profiles.roles(UnitVisualProfileId{}), nullptr);
    EXPECT_EQ(unit_profiles.roles(UnitVisualProfileId{99u}), nullptr);
    EXPECT_EQ(lifecycle_profiles.lifecycle(UnitLifecycleVisualProfileId{}), nullptr);
    EXPECT_EQ(lifecycle_profiles.lifecycle(UnitLifecycleVisualProfileId{99u}), nullptr);
}

TEST(BattleVisualProfileRegistry, AddedProfilesResolveByTypedId) {
    UnitVisualProfileRegistry unit_profiles;
    UnitAnimationRoleSet      roles;
    {
        AnimationSequence seq;
        seq.name = "idle";
        roles.idle = UnitStateClip::from_a1(std::move(seq));
    }
    const UnitVisualProfileId role_id = unit_profiles.add(std::move(roles));

    UnitLifecycleVisualProfileRegistry lifecycle_profiles;
    UnitLifecycleVisualProfile         lp;
    lp.death.corpse_sprite_base = "DEAD_TEST";
    const UnitLifecycleVisualProfileId lp_id = lifecycle_profiles.add(std::move(lp));

    ASSERT_NE(unit_profiles.roles(role_id), nullptr);
    EXPECT_FALSE(unit_profiles.roles(role_id)->idle.is_empty());
    ASSERT_TRUE(unit_profiles.roles(role_id)->idle.clip.a1.has_value());
    EXPECT_EQ(unit_profiles.roles(role_id)->idle.clip.a1->name, "idle");
    ASSERT_NE(lifecycle_profiles.lifecycle(lp_id), nullptr);
    EXPECT_EQ(lifecycle_profiles.lifecycle(lp_id)->death.corpse_sprite_base, "DEAD_TEST");
}

} // namespace d2engine
