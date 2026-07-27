#include <gtest/gtest.h>

#include "d2engine/assets/animation_asset_preloader.hpp"
#include "d2engine/assets/asset_runtime.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace d2engine {
namespace {

// ── Empty / edge-case tests (no game data required) ─────────────────────────

TEST(AnimationAssetPreloader, EmptyPrepareSpritesReturnsEmpty) {
    // AssetRuntime with a decode callback (no FfAssetStore).
    // prepare_sprites will fail since there's no store, but with empty
    // keys it should return empty immediately.
    AssetRuntime runtime(
        [](const ImageAssetKey&) -> PreparedImageResult { return PreparedImageResult{}; }, 1);
    AnimationAssetPreloader preloader(runtime);
    auto                    results = preloader.prepare_sprites({});
    EXPECT_TRUE(results.empty());
}

TEST(AnimationAssetPreloader, EmptyPrepareReturnsEmpty) {
    AssetRuntime runtime(
        [](const ImageAssetKey&) -> PreparedImageResult { return PreparedImageResult{}; }, 1);
    AnimationAssetPreloader preloader(runtime);
    auto                    results = preloader.prepare({}, {});
    EXPECT_TRUE(results.empty());
}

} // namespace
} // namespace d2engine
