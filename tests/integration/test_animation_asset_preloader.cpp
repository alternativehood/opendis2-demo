#include <gtest/gtest.h>

#include "d2engine/assets/animation_asset_preloader.hpp"
#include "d2engine/assets/asset_runtime.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace d2engine {
namespace {

class AnimationAssetPreloaderGameDataTest : public ::testing::Test {
protected:
    std::filesystem::path game_root_;

    void SetUp() override {
        const char* env = std::getenv("DISCIPLES2_GAME_ROOT"); // NOLINT
        if (env == nullptr || env[0] == '\0') {
            GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set";
        }
        game_root_ = std::filesystem::path(env);
        if (!std::filesystem::is_directory(game_root_)) {
            GTEST_SKIP() << "DISCIPLES2_GAME_ROOT is not a directory";
        }
    }
};

TEST_F(AnimationAssetPreloaderGameDataTest, PrepareSpritesFromBattleContainer) {
    AssetRuntime            runtime(game_root_, 2);
    AnimationAssetPreloader preloader(runtime);
    runtime.store().prewarm("Imgs/Battle.ff");

    std::vector<ImageAssetKey> keys;
    keys.push_back(ImageAssetKey{.container_path = "Imgs/Battle.ff",
                                 .image_name = "HU_BATTLE_0_BG",
                                 .kind = ImageAssetKind::ComposedSprite,
                                 .postprocess = ImagePostprocess::DetectMagentaBorder});

    auto results = preloader.prepare_sprites(keys);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_TRUE(results[0].success) << "error: " << results[0].error;
    ASSERT_NE(results[0].image, nullptr);
    EXPECT_GT(results[0].image->pixels->width, 0u);
    EXPECT_GT(results[0].image->pixels->height, 0u);
}

TEST_F(AnimationAssetPreloaderGameDataTest, BulkDecodeDeduplicatesBasePngs) {
    AssetRuntime            runtime(game_root_, 2);
    AnimationAssetPreloader preloader(runtime);
    auto&                   store = runtime.store();
    store.prewarm("Imgs/Battle.ff");

    auto sprites = store.sprites_in("Imgs/Battle.ff");
    ASSERT_FALSE(sprites.empty());

    const std::size_t limit = std::min(sprites.size(), std::size_t{20});
    sprites.resize(limit);

    std::vector<ImageAssetKey> keys;
    keys.reserve(sprites.size());
    for (const auto& name : sprites) {
        keys.push_back(ImageAssetKey{.container_path = "Imgs/Battle.ff",
                                     .image_name = name,
                                     .kind = ImageAssetKind::ComposedSprite,
                                     .postprocess = ImagePostprocess::DetectMagentaBorder});
    }

    auto results = preloader.prepare_sprites(keys);
    ASSERT_EQ(results.size(), keys.size());
    std::size_t successes = 0;
    for (const auto& r : results) {
        if (r.success) {
            ++successes;
            EXPECT_GT(r.image->pixels->width, 0u);
            EXPECT_GT(r.image->pixels->height, 0u);
        }
    }
    EXPECT_GE(successes, sprites.size() * 8 / 10);
}

TEST_F(AnimationAssetPreloaderGameDataTest, PrepareDeduplicatesByKey) {
    AssetRuntime            runtime(game_root_, 2);
    AnimationAssetPreloader preloader(runtime);
    runtime.store().prewarm("Imgs/Battle.ff");

    std::vector<AnimationAssetPreloader::AnimationRequest> anims;
    anims.push_back({.container = "Imgs/Battle.ff", .animation_name = "DUMMY_NONEXISTENT"});

    std::vector<ImageAssetKey> extra;
    extra.push_back(ImageAssetKey{.container_path = "Imgs/Battle.ff",
                                  .image_name = "HU_BATTLE_0_BG",
                                  .kind = ImageAssetKind::ComposedSprite,
                                  .postprocess = ImagePostprocess::DetectMagentaBorder});

    auto results = preloader.prepare(anims, extra);
    ASSERT_GE(results.size(), 1u);
    bool found = false;
    for (const auto& r : results) {
        if (r.key.image_name == "HU_BATTLE_0_BG" && r.success) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

} // namespace
} // namespace d2engine
