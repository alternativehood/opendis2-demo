#include <gtest/gtest.h>

#include "d2engine/assets/asset_runtime.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace d2engine {
namespace {

PreparedImageResult make_result(const ImageAssetKey& key, std::uint8_t value = 7) {
    auto pixels = std::make_shared<d2res::RgbaBuffer>();
    pixels->width = 1;
    pixels->height = 1;
    pixels->rgba = {value, value, value, 255};
    auto image = std::make_shared<PreparedImage>(
        PreparedImage{.key = key, .pixels = std::move(pixels), .decode_ms = 0.1});
    return PreparedImageResult{.key = key, .image = std::move(image), .success = true};
}

ImageAssetKey sprite_key(std::string image) {
    return {.container_path = "Imgs/Batunits.ff",
            .image_name = std::move(image),
            .kind = ImageAssetKind::ComposedSprite,
            .postprocess = ImagePostprocess::DetectMagentaBorder};
}

ImageAssetKey banner_key(int banner) {
    std::string image_name;
    if (banner < 14) {
        const int value = banner * 100;
        image_name = "STACK_BANNER_";
        if (value < 1000) {
            image_name.push_back('0');
        }
        if (value < 100) {
            image_name.push_back('0');
        }
        if (value < 10) {
            image_name.push_back('0');
        }
        image_name += std::to_string(value);
    } else {
        image_name = "TI";
    }
    return {.container_path = "Imgs/IsoCmon.ff",
            .image_name = std::move(image_name),
            .kind = ImageAssetKind::ComposedSprite};
}

TEST(AssetRuntime, InFlightCrossModeRequestsDeduplicate) {
    std::mutex              mtx;
    std::condition_variable cv;
    bool                    decode_started = false;
    bool                    release_decode = false;
    std::atomic<int>        decode_count{0};

    AssetRuntime runtime(
        [&](const ImageAssetKey& key) {
            decode_count.fetch_add(1, std::memory_order_relaxed);
            {
                std::lock_guard lock(mtx);
                decode_started = true;
            }
            cv.notify_all();
            {
                std::unique_lock lock(mtx);
                cv.wait(lock, [&] { return release_decode; });
            }
            return make_result(key);
        },
        1);

    const auto key = sprite_key("G000000001.PNG");
    auto       adventure = runtime.request_image(key, AssetPriority::Prefetch, "Adventure");
    {
        std::unique_lock lock(mtx);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(1), [&] { return decode_started; }));
    }
    auto battle = runtime.request_image(key, AssetPriority::Critical, "Battle");
    {
        std::lock_guard lock(mtx);
        release_decode = true;
    }
    cv.notify_all();

    const auto adventure_result = adventure.get();
    const auto battle_result = battle.get();
    EXPECT_TRUE(adventure_result.success);
    EXPECT_TRUE(battle_result.success);
    EXPECT_EQ(adventure_result.image.get(), battle_result.image.get());
    EXPECT_EQ(decode_count.load(std::memory_order_relaxed), 1);
    EXPECT_EQ(runtime.stats().inflight_joined, 1u);
}

TEST(AssetRuntime, SameFfImagesDecodeConcurrently) {
    std::atomic<int> active{0};
    std::atomic<int> max_active{0};

    AssetRuntime runtime(
        [&](const ImageAssetKey& key) {
            const int now = active.fetch_add(1, std::memory_order_relaxed) + 1;
            int       observed = max_active.load(std::memory_order_relaxed);
            while (now > observed &&
                   !max_active.compare_exchange_weak(observed, now, std::memory_order_relaxed)) {
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            active.fetch_sub(1, std::memory_order_relaxed);
            return make_result(key);
        },
        4);

    std::vector<ImageAssetKey> keys;
    keys.reserve(100);
    for (int i = 0; i < 100; ++i) {
        keys.push_back(sprite_key("image_" + std::to_string(i)));
    }
    auto batch = runtime.request_batch(keys, AssetPriority::Critical, "Battle");
    batch.wait();

    EXPECT_GT(max_active.load(std::memory_order_relaxed), 1);
    EXPECT_EQ(runtime.stats().decoded, 100u);
}

TEST(AssetRuntime, TypedKeysKeepRawAndComposedImagesSeparate) {
    std::atomic<int> decode_count{0};
    AssetRuntime     runtime(
        [&](const ImageAssetKey& key) {
            decode_count.fetch_add(1, std::memory_order_relaxed);
            return make_result(key);
        },
        2);

    ImageAssetKey raw{.container_path = "Imgs/Interf.ff",
                      .image_name = "BUTTON.PNG",
                      .kind = ImageAssetKind::RawPng};
    ImageAssetKey composed = raw;
    composed.kind = ImageAssetKind::ComposedSprite;

    auto raw_handle = runtime.request_image(raw, AssetPriority::Critical, "Adventure");
    auto composed_handle = runtime.request_image(composed, AssetPriority::Critical, "Battle");

    EXPECT_TRUE(raw_handle.get().success);
    EXPECT_TRUE(composed_handle.get().success);
    EXPECT_EQ(decode_count.load(std::memory_order_relaxed), 2);
}

TEST(AssetRuntime, AnimationFrameRequestsShareImageRuntimeState) {
    std::mutex                           mtx;
    std::unordered_map<std::string, int> decodes;
    AssetRuntime                         runtime(
        [&](const ImageAssetKey& key) {
            std::lock_guard lock(mtx);
            ++decodes[key.image_name];
            return make_result(key);
        },
        2);

    std::vector<ImageAssetKey> frames = {sprite_key("A.PNG"), sprite_key("B.PNG"),
                                         sprite_key("C.PNG")};
    auto animation_batch = runtime.request_batch(frames, AssetPriority::Prefetch, "BattleAnim");
    animation_batch.wait();

    auto direct = runtime.request_image(sprite_key("B.PNG"), AssetPriority::Critical, "Menu");
    EXPECT_TRUE(direct.get().success);

    std::lock_guard lock(mtx);
    EXPECT_EQ(decodes["A.PNG"], 1);
    EXPECT_EQ(decodes["B.PNG"], 1);
    EXPECT_EQ(decodes["C.PNG"], 1);
}

TEST(AssetRuntime, DuplicateContainedBannerRequestsDeduplicateByLogicalImageName) {
    std::atomic<int> decode_count{0};
    AssetRuntime     runtime(
        [&](const ImageAssetKey& key) {
            decode_count.fetch_add(1, std::memory_order_relaxed);
            return make_result(key);
        },
        2);

    auto batch =
        runtime.request_batch({banner_key(4), banner_key(4), banner_key(14), banner_key(14)},
                              AssetPriority::Critical, "ContainedBanner");
    batch.wait();

    EXPECT_EQ(decode_count.load(std::memory_order_relaxed), 2);
    EXPECT_EQ(runtime.stats().decoded, 2u);
}

TEST(AssetRuntime, ReleasePreparedIfMatchesOnlyErasesMatchingImage) {
    auto         key = sprite_key("identity.PNG");
    AssetRuntime runtime([&](const ImageAssetKey& k) { return make_result(k); }, 1);

    auto handle = runtime.request_image(key, AssetPriority::Critical, "test");
    handle.wait();
    auto result = handle.get();
    ASSERT_TRUE(result.success);
    ASSERT_NE(result.image, nullptr);

    auto stats = runtime.prepared_resident_stats();
    ASSERT_EQ(stats.count, 1u);

    // Non-matching image: should NOT erase
    auto other_pixels = std::make_shared<d2res::RgbaBuffer>();
    other_pixels->width = 1;
    other_pixels->height = 1;
    other_pixels->rgba = {0, 0, 0, 255};
    auto other_img = std::make_shared<PreparedImage>(
        PreparedImage{.key = key, .pixels = std::move(other_pixels), .decode_ms = 0.0});
    auto other_image = std::make_shared<const PreparedImage>(*other_img);
    runtime.release_prepared_if_matches(key, other_image);

    stats = runtime.prepared_resident_stats();
    EXPECT_EQ(stats.count, 1u) << "non-matching image must not erase";

    // Matching image: SHOULD erase
    runtime.release_prepared_if_matches(key, result.image);
    stats = runtime.prepared_resident_stats();
    EXPECT_EQ(stats.count, 0u) << "matching image must erase";
}

TEST(AssetRuntime, ReleaseIfMatchesPreservesUnrelatedReadyImage) {
    auto         key_a = sprite_key("ident_a.PNG");
    auto         key_b = sprite_key("ident_b.PNG");
    AssetRuntime runtime([&](const ImageAssetKey& k) { return make_result(k); }, 1);

    auto h_a = runtime.request_image(key_a, AssetPriority::Critical, "test");
    h_a.wait();
    auto res_a = h_a.get();
    ASSERT_TRUE(res_a.success);

    auto h_b = runtime.request_image(key_b, AssetPriority::Critical, "test");
    h_b.wait();
    auto res_b = h_b.get();
    ASSERT_TRUE(res_b.success);

    auto stats = runtime.prepared_resident_stats();
    ASSERT_EQ(stats.count, 2u);

    // Attempt release of key_b with key_a's image — must not erase key_b
    runtime.release_prepared_if_matches(key_b, res_a.image);
    stats = runtime.prepared_resident_stats();
    EXPECT_EQ(stats.count, 2u) << "cross-key image identity must not match";

    // Release key_a with its own image — must erase only key_a
    runtime.release_prepared_if_matches(key_a, res_a.image);
    stats = runtime.prepared_resident_stats();
    EXPECT_EQ(stats.count, 1u);
}

} // namespace
} // namespace d2engine
