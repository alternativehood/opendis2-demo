#include <d2engine/animation/animation_frame.hpp>
#include <d2engine/animation/animation_player.hpp>
#include <d2engine/animation/animation_sequence.hpp>
#include <d2engine/assets/ff_asset_store.hpp>
#include <d2game/GameEvent.hpp>

#include <gtest/gtest.h>

#include <cstddef>

TEST(AdventureHeaderSelfContainment, AnimationAndGameHeadersExposeTheirTypes) {
    EXPECT_GT(sizeof(d2engine::AnimationFrame), std::size_t{0});
    EXPECT_GT(sizeof(d2engine::AnimationPlayer), std::size_t{0});
    EXPECT_GT(sizeof(d2engine::AnimationSequence), std::size_t{0});
    EXPECT_GT(sizeof(d2engine::FfAssetStore), std::size_t{0});
    EXPECT_GT(sizeof(d2game::AdventureMovementRejected), std::size_t{0});
}
