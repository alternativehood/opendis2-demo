#include <gtest/gtest.h>

#include "d2engine/battle_view/animation_role.hpp"

TEST(AnimationRole, BuildIdleADirection) {
    EXPECT_EQ(d2engine::AnimationNames::build("UU0001", "IDLEA", 1, 'A'), "G000UU0001IDLEA1A00");
}

TEST(AnimationRole, BuildIdleDDirection) {
    EXPECT_EQ(d2engine::AnimationNames::build("UU0091", "IDLEA", 1, 'D'), "G000UU0091IDLEA1D00");
}

TEST(AnimationRole, BuildWithVariant2) {
    EXPECT_EQ(d2engine::AnimationNames::build("UU6102", "IDLEA", 2, 'A'), "G000UU6102IDLEA2A00");
}

TEST(AnimationRole, RoleConstantIdle) {
    EXPECT_EQ(d2engine::AnimationNames::build("UU0001", std::string{d2engine::AnimationRoles::IDLE},
                                              1, 'A'),
              "G000UU0001IDLEA1A00");
}

TEST(AnimationRole, RoleConstantHit) {
    EXPECT_EQ(d2engine::AnimationNames::build("UU0001", std::string{d2engine::AnimationRoles::HIT},
                                              1, 'A'),
              "G000UU0001HHITA1A00");
}

TEST(AnimationRole, RoleConstantDeath) {
    EXPECT_EQ(d2engine::AnimationNames::build("UU0001",
                                              std::string{d2engine::AnimationRoles::DEATH}, 1, 'A'),
              "G000UU0001STILA1A00");
}
