#include <gtest/gtest.h>

#include "d2engine/app/app_runtime_context.hpp"
#include "d2engine/app/application.hpp"
#include "d2engine/app/screen_manager.hpp"
#include "d2engine/assets/asset_runtime.hpp"
#include "d2engine/assets/ff_asset_store.hpp"
#include "d2engine/assets/game_data_registry.hpp"
#include "d2engine/assets/portrait_manifest_index.hpp"
#include "d2engine/render/renderer2d.hpp"
#include "d2engine/render/render_asset_runtime.hpp"
#include "d2engine/render/render_tree.hpp"

#include "opendis2/adventure_startup_screen.hpp"

#include <nlohmann/json.hpp>
#include <SDL3/SDL.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <stdexcept>
#include <thread>
#include <vector>

#include "tests/test_process.hpp"

namespace d2engine {
namespace ar = adventure_render;
namespace {

PreparedImageResult make_shared_runtime_result(const ImageAssetKey& key, std::uint8_t value = 9) {
    auto pixels = std::make_shared<d2res::RgbaBuffer>();
    pixels->width = 1;
    pixels->height = 1;
    pixels->rgba = {value, value, value, 255};
    auto image = std::make_shared<PreparedImage>(
        PreparedImage{.key = key, .pixels = std::move(pixels), .decode_ms = 0.1});
    return PreparedImageResult{.key = key, .image = std::move(image), .success = true};
}

ImageAssetKey shared_runtime_key() {
    return {.container_path = "Imgs/Interf.ff",
            .image_name = "BUTTON.PNG",
            .kind = ImageAssetKind::RawPng};
}

ImageAssetKey generated_runtime_key(std::string_view suffix) {
    return {.container_path = "Imgs/IncrementalPreload.ff",
            .image_name = "GENERATED_" + std::string(suffix) + ".PNG",
            .kind = ImageAssetKind::RawPng};
}

std::filesystem::path source_config_root() {
    return std::filesystem::path(OPENDIS2_SOURCE_DIR) / "configs";
}

class ControlledDecodeGate {
public:
    void decoder_started_and_wait() {
        std::unique_lock lock(mutex_);
        started_ = true;
        cv_.notify_all();
        cv_.wait(lock, [this] { return allowed_; });
    }

    [[nodiscard]] bool wait_until_started() {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, std::chrono::seconds(1), [this] { return started_; });
    }

    void allow_decode() noexcept {
        {
            std::lock_guard lock(mutex_);
            allowed_ = true;
        }
        cv_.notify_all();
    }

    ~ControlledDecodeGate() { allow_decode(); }

    ControlledDecodeGate() = default;
    ControlledDecodeGate(const ControlledDecodeGate&) = delete;
    ControlledDecodeGate& operator=(const ControlledDecodeGate&) = delete;

private:
    std::mutex              mutex_;
    std::condition_variable cv_;
    bool                    started_ = false;
    bool                    allowed_ = false;
};

class DecodeGateReleaseGuard {
public:
    explicit DecodeGateReleaseGuard(ControlledDecodeGate& gate) noexcept : gate_(&gate) {}
    ~DecodeGateReleaseGuard() {
        if (gate_ != nullptr) {
            gate_->allow_decode();
        }
    }

    DecodeGateReleaseGuard(const DecodeGateReleaseGuard&) = delete;
    DecodeGateReleaseGuard& operator=(const DecodeGateReleaseGuard&) = delete;
    DecodeGateReleaseGuard(DecodeGateReleaseGuard&&) = delete;
    DecodeGateReleaseGuard& operator=(DecodeGateReleaseGuard&&) = delete;

private:
    ControlledDecodeGate* gate_ = nullptr;
};

void drive_cancelled_cleanup_to_baseline(RenderAssetRuntime&                  runtime,
                                         const RenderAssetRuntimeMemoryStats& memory_baseline,
                                         const CancelledPreloadCleanupStats&  cleanup_baseline) {
    constexpr std::size_t kMaxCleanupPumps = 10'000;
    for (std::size_t iteration = 0; iteration < kMaxCleanupPumps; ++iteration) {
        static_cast<void>(runtime.pump_uploads({.max_textures = 0, .max_ms = 0.0}));
        const auto cleanup = runtime.cancelled_preload_cleanup_stats();
        const auto memory = runtime.memory_stats();
        if (cleanup.pending_handles == cleanup_baseline.pending_handles &&
            memory.prepared_resident_count == memory_baseline.prepared_resident_count &&
            memory.staging_owned_bytes == memory_baseline.staging_owned_bytes &&
            memory.reclaim_pending_bytes == memory_baseline.reclaim_pending_bytes) {
            runtime.wait_for_reclaim_idle();
            const auto final_cleanup = runtime.cancelled_preload_cleanup_stats();
            const auto final_memory = runtime.memory_stats();
            if (final_cleanup.pending_handles == cleanup_baseline.pending_handles &&
                final_memory.prepared_resident_count == memory_baseline.prepared_resident_count &&
                final_memory.staging_owned_bytes == memory_baseline.staging_owned_bytes &&
                final_memory.reclaim_pending_bytes == memory_baseline.reclaim_pending_bytes) {
                return;
            }
        }
        std::this_thread::yield();
    }

    const auto cleanup = runtime.cancelled_preload_cleanup_stats();
    const auto memory = runtime.memory_stats();
    ADD_FAILURE() << "cancelled cleanup did not reach baseline: pending=" << cleanup.pending_handles
                  << " baseline_pending=" << cleanup_baseline.pending_handles
                  << " ready=" << cleanup.ready_handles
                  << " prepared=" << memory.prepared_resident_count
                  << " baseline_prepared=" << memory_baseline.prepared_resident_count
                  << " staging=" << memory.staging_owned_bytes
                  << " baseline_staging=" << memory_baseline.staging_owned_bytes
                  << " reclaim=" << memory.reclaim_pending_bytes
                  << " baseline_reclaim=" << memory_baseline.reclaim_pending_bytes;
}

std::string read_source_file(const std::filesystem::path& path) {
    std::ifstream in(std::filesystem::path(OPENDIS2_SOURCE_DIR) / path);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

class SharedRuntimeSdlTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            GTEST_SKIP() << "SDL_Init failed: " << SDL_GetError();
        }
        window_ = SDL_CreateWindow("shared-runtime-test", 1, 1, SDL_WINDOW_HIDDEN);
        if (window_ == nullptr) {
            SDL_Quit();
            GTEST_SKIP() << "SDL_CreateWindow failed: " << SDL_GetError();
        }
        renderer_ = SDL_CreateRenderer(window_, nullptr);
        if (renderer_ == nullptr) {
            SDL_DestroyWindow(window_);
            SDL_Quit();
            GTEST_SKIP() << "SDL_CreateRenderer failed: " << SDL_GetError();
        }
    }

    void TearDown() override {
        if (renderer_ != nullptr) {
            SDL_DestroyRenderer(renderer_);
        }
        if (window_ != nullptr) {
            SDL_DestroyWindow(window_);
        }
        SDL_Quit();
    }

    SDL_Window*   window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
};

std::filesystem::path make_temp_game_root() {
    static std::atomic<int> sequence{0};
    const auto              temp_root =
        std::filesystem::temp_directory_path() /
        ("opendis2_shared_runtime_test_" + std::to_string(test_support::process_id()) + "_" +
         std::to_string(sequence.fetch_add(1)));
    std::error_code ec;
    std::filesystem::remove_all(temp_root, ec);
    std::filesystem::create_directories(temp_root / "Imgs", ec);
    {
        std::ofstream dummy(temp_root / "Imgs" / "Dummy.ff", std::ios::binary);
        dummy << "not opened by this test";
    }
    return temp_root;
}

std::optional<std::filesystem::path> adventure_fixture_path() {
    const char* env = std::getenv("OPENDIS2_ADVENTURE_TEST_SG");
    if (env == nullptr || env[0] == '\0') {
        return std::nullopt;
    }
    const std::filesystem::path path(env);
    if (!std::filesystem::exists(path)) {
        return std::nullopt;
    }
    return path;
}

TEST_F(SharedRuntimeSdlTest, AdventureBattleAdventureUsesOneRuntimeForCpuCache) {
    std::atomic<int> decode_count{0};

    const auto   temp_root = make_temp_game_root();
    auto         store = std::make_unique<FfAssetStore>(temp_root);
    AssetRuntime assets(
        std::move(store),
        [&](const ImageAssetKey& key) {
            decode_count.fetch_add(1, std::memory_order_relaxed);
            return make_shared_runtime_result(key);
        },
        1);
    RenderAssetRuntime    render_assets(renderer_, assets);
    GameDataRegistry      game_data(std::filesystem::path("/nonexistent/shared-runtime-test"));
    PortraitManifest      empty_manifest;
    PortraitManifestIndex portraits(empty_manifest);

    AppRuntimeContext runtime{.assets = assets,
                              .render_assets = render_assets,
                              .game_data = game_data,
                              .portraits = portraits};
    EXPECT_EQ(&runtime.assets, &assets);
    EXPECT_EQ(&runtime.render_assets, &render_assets);
    EXPECT_EQ(&runtime.game_data, &game_data);
    EXPECT_EQ(&runtime.portraits, &portraits);

    const auto key = shared_runtime_key();

    auto       adventure = runtime.assets.request_image(key, AssetPriority::Prefetch, "Adventure");
    const auto adventure_result = adventure.get();
    ASSERT_TRUE(adventure_result.success);
    EXPECT_EQ(decode_count.load(std::memory_order_relaxed), 1);

    auto       battle = runtime.assets.request_image(key, AssetPriority::Critical, "Battle");
    const auto battle_result = battle.get();
    ASSERT_TRUE(battle_result.success);
    EXPECT_EQ(battle_result.image.get(), adventure_result.image.get());
    EXPECT_EQ(decode_count.load(std::memory_order_relaxed), 1);
    EXPECT_EQ(runtime.assets.stats().ready_hits, 1u);

    std::error_code ec;
    std::filesystem::remove_all(temp_root, ec);
}

TEST_F(SharedRuntimeSdlTest, AdventureBattleAdventureReusesTextureInSharedRenderRuntime) {
    std::atomic<int> decode_count{0};

    const auto   temp_root = make_temp_game_root();
    auto         store = std::make_unique<FfAssetStore>(temp_root);
    AssetRuntime assets(
        std::move(store),
        [&](const ImageAssetKey& key) {
            decode_count.fetch_add(1, std::memory_order_relaxed);
            return make_shared_runtime_result(key);
        },
        1);
    RenderAssetRuntime    render_assets(renderer_, assets);
    GameDataRegistry      game_data(std::filesystem::path("/nonexistent/shared-runtime-test"));
    PortraitManifest      empty_manifest;
    PortraitManifestIndex portraits(empty_manifest);
    AppRuntimeContext     runtime{.assets = assets,
                                  .render_assets = render_assets,
                                  .game_data = game_data,
                                  .portraits = portraits};

    const auto key = shared_runtime_key();
    auto handle = runtime.render_assets.request_texture(key, AssetPriority::Critical, "Adventure");
    AssetBatchHandle batch;
    batch.add(std::move(handle));
    batch.wait();
    const auto upload = runtime.render_assets.upload_ready(batch);
    EXPECT_EQ(upload.uploaded, 1u);
    EXPECT_EQ(upload.failed, 0u);

    SDL_Texture* first_texture = runtime.render_assets.textures().find(key);
    ASSERT_NE(first_texture, nullptr);
    EXPECT_EQ(decode_count.load(std::memory_order_relaxed), 1);

    auto battle_handle =
        runtime.render_assets.request_texture(key, AssetPriority::Critical, "Battle");
    EXPECT_FALSE(battle_handle.valid());
    EXPECT_EQ(runtime.render_assets.textures().find(key), first_texture);
    EXPECT_EQ(runtime.render_assets.textures().stats().prepared_upload_successes, 1u);
    EXPECT_EQ(runtime.render_assets.textures().stats().prepared_upload_skipped_cache_hits, 0u);
    EXPECT_EQ(decode_count.load(std::memory_order_relaxed), 1);

    std::error_code ec;
    std::filesystem::remove_all(temp_root, ec);
}

TEST_F(SharedRuntimeSdlTest, IncrementalPreloadCancellationBeforeDecodeCompletionIsLeakFree) {
    ControlledDecodeGate gate;

    const auto   temp_root = make_temp_game_root();
    auto         store = std::make_unique<FfAssetStore>(temp_root);
    AssetRuntime assets(
        std::move(store),
        [&](const ImageAssetKey& key) {
            gate.decoder_started_and_wait();
            return make_shared_runtime_result(key);
        },
        1);
    RenderAssetRuntime     render_assets(renderer_, assets);
    DecodeGateReleaseGuard release_gate{gate};
    const auto             key = shared_runtime_key();
    const auto             baseline_memory = render_assets.memory_stats();
    const auto             baseline_cleanup = render_assets.cancelled_preload_cleanup_stats();

    {
        auto preload = render_assets.begin_incremental_preload(
            {key}, AssetPriority::Critical, "IncrementalPreloadCancellationTest");
        preload.advance({.max_textures = 1, .max_ms = 1.0});
        EXPECT_EQ(preload.progress().remaining, 1u);
    }
    EXPECT_EQ(render_assets.cancelled_preload_cleanup_stats().pending_handles,
              baseline_cleanup.pending_handles + 1U);
    ASSERT_TRUE(gate.wait_until_started());
    gate.allow_decode();
    drive_cancelled_cleanup_to_baseline(render_assets, baseline_memory, baseline_cleanup);
    const auto cleanup = render_assets.cancelled_preload_cleanup_stats();
    const auto memory = render_assets.memory_stats();
    ASSERT_EQ(cleanup.pending_handles, baseline_cleanup.pending_handles)
        << "ready_handles=" << cleanup.ready_handles
        << " prepared_resident=" << memory.prepared_resident_count
        << " staging_owned_bytes=" << memory.staging_owned_bytes
        << " reclaim_pending_bytes=" << memory.reclaim_pending_bytes;
    EXPECT_EQ(memory.prepared_resident_count, baseline_memory.prepared_resident_count);
    EXPECT_EQ(memory.staging_owned_bytes, baseline_memory.staging_owned_bytes);
    EXPECT_EQ(memory.reclaim_pending_bytes, baseline_memory.reclaim_pending_bytes);
    EXPECT_EQ(cleanup.released_prepared_images, baseline_cleanup.released_prepared_images + 1U);
    EXPECT_EQ(render_assets.textures().find(key), nullptr);

    std::error_code ec;
    std::filesystem::remove_all(temp_root, ec);
}

TEST_F(SharedRuntimeSdlTest, DecodeGateReleaseGuardUnblocksWorkerBeforeAssetRuntimeDestruction) {
    std::atomic<bool> decoded = false;
    const auto        temp_root = make_temp_game_root();
    {
        ControlledDecodeGate gate;
        AssetRuntime         assets(
            std::make_unique<FfAssetStore>(temp_root),
            [&](const ImageAssetKey& key) {
                gate.decoder_started_and_wait();
                decoded.store(true, std::memory_order_release);
                return make_shared_runtime_result(key);
            },
            1);
        RenderAssetRuntime     runtime(renderer_, assets);
        DecodeGateReleaseGuard release_gate{gate};
        auto handle = assets.request_image(generated_runtime_key("destruction_order"),
                                           AssetPriority::Critical, "GateGuardTest");
        ASSERT_TRUE(gate.wait_until_started());
        static_cast<void>(handle);
    }
    EXPECT_TRUE(decoded.load(std::memory_order_acquire));

    std::error_code ec;
    std::filesystem::remove_all(temp_root, ec);
}

TEST_F(SharedRuntimeSdlTest,
       IncrementalPreloadCancellationAfterPartialUploadReleasesOnlyPendingWork) {
    ControlledDecodeGate gate_b;
    const auto           key_a = generated_runtime_key("partial_a");
    const auto           key_b = generated_runtime_key("partial_b");
    const auto           temp_root = make_temp_game_root();
    AssetRuntime         assets(
        std::make_unique<FfAssetStore>(temp_root),
        [&](const ImageAssetKey& key) {
            if (key == key_b) {
                gate_b.decoder_started_and_wait();
            }
            return make_shared_runtime_result(key);
        },
        1);
    RenderAssetRuntime     runtime(renderer_, assets);
    DecodeGateReleaseGuard release_gate{gate_b};
    const auto             baseline_memory = runtime.memory_stats();
    const auto             baseline_cleanup = runtime.cancelled_preload_cleanup_stats();

    {
        auto preload = runtime.begin_incremental_preload({key_a, key_b}, AssetPriority::Critical,
                                                         "PartialCancellationTest");
        ASSERT_TRUE(gate_b.wait_until_started()); // A has already fulfilled its request.
        preload.advance({.max_textures = 1, .max_ms = 1000.0});
        EXPECT_EQ(preload.progress().uploaded, 1u);
        ASSERT_NE(runtime.textures().find(key_a), nullptr);
        EXPECT_EQ(runtime.textures().find(key_b), nullptr);
    }
    EXPECT_EQ(runtime.cancelled_preload_cleanup_stats().pending_handles,
              baseline_cleanup.pending_handles + 1U);
    gate_b.allow_decode();
    drive_cancelled_cleanup_to_baseline(runtime, baseline_memory, baseline_cleanup);
    EXPECT_NE(runtime.textures().find(key_a), nullptr);
    EXPECT_EQ(runtime.textures().find(key_b), nullptr);
    EXPECT_EQ(runtime.cancelled_preload_cleanup_stats().released_prepared_images,
              baseline_cleanup.released_prepared_images + 1U);

    std::error_code ec;
    std::filesystem::remove_all(temp_root, ec);
}

TEST_F(SharedRuntimeSdlTest,
       RepeatedIncrementalPreloadCancellationReturnsTransientMemoryToBaseline) {
    const auto            temp_root = make_temp_game_root();
    ControlledDecodeGate* active_gate = nullptr;
    AssetRuntime          assets(
        std::make_unique<FfAssetStore>(temp_root),
        [&](const ImageAssetKey& key) {
            active_gate->decoder_started_and_wait();
            return make_shared_runtime_result(key);
        },
        1);
    RenderAssetRuntime runtime(renderer_, assets);
    const auto         baseline_memory = runtime.memory_stats();
    const auto         baseline_cleanup = runtime.cancelled_preload_cleanup_stats();

    for (int cycle = 0; cycle < 10; ++cycle) {
        ControlledDecodeGate   gate;
        DecodeGateReleaseGuard release_gate{gate};
        active_gate = &gate;
        const auto key = generated_runtime_key("cycle_" + std::to_string(cycle));
        {
            auto preload = runtime.begin_incremental_preload({key}, AssetPriority::Critical,
                                                             "RepeatedCancellationTest");
            ASSERT_TRUE(gate.wait_until_started());
        }
        gate.allow_decode();
        drive_cancelled_cleanup_to_baseline(runtime, baseline_memory, baseline_cleanup);
        EXPECT_EQ(runtime.memory_stats().prepared_resident_count,
                  baseline_memory.prepared_resident_count);
    }
    EXPECT_EQ(runtime.cancelled_preload_cleanup_stats().released_prepared_images,
              baseline_cleanup.released_prepared_images + 10U);

    std::error_code ec;
    std::filesystem::remove_all(temp_root, ec);
}

TEST_F(SharedRuntimeSdlTest, IncrementalPreloadMoveConstructionTransfersPendingOwnershipOnce) {
    ControlledDecodeGate gate;
    const auto           key = generated_runtime_key("move_construct");
    const auto           temp_root = make_temp_game_root();
    AssetRuntime         assets(
        std::make_unique<FfAssetStore>(temp_root),
        [&](const ImageAssetKey& request) {
            gate.decoder_started_and_wait();
            return make_shared_runtime_result(request);
        },
        1);
    RenderAssetRuntime     runtime(renderer_, assets);
    DecodeGateReleaseGuard release_gate{gate};
    const auto             baseline_memory = runtime.memory_stats();
    const auto             baseline_cleanup = runtime.cancelled_preload_cleanup_stats();
    {
        auto source = runtime.begin_incremental_preload({key}, AssetPriority::Critical, "MoveTest");
        ASSERT_TRUE(gate.wait_until_started());
        auto moved = std::move(source);
        EXPECT_EQ(runtime.cancelled_preload_cleanup_stats().pending_handles,
                  baseline_cleanup.pending_handles);
    }
    EXPECT_EQ(runtime.cancelled_preload_cleanup_stats().pending_handles,
              baseline_cleanup.pending_handles + 1U);
    gate.allow_decode();
    drive_cancelled_cleanup_to_baseline(runtime, baseline_memory, baseline_cleanup);
    EXPECT_EQ(runtime.cancelled_preload_cleanup_stats().released_prepared_images,
              baseline_cleanup.released_prepared_images + 1U);

    std::error_code ec;
    std::filesystem::remove_all(temp_root, ec);
}

TEST_F(SharedRuntimeSdlTest, IncrementalPreloadMoveAssignmentTransfersPendingOwnershipOnce) {
    ControlledDecodeGate gate;
    const auto           key = generated_runtime_key("move_assign");
    const auto           temp_root = make_temp_game_root();
    AssetRuntime         assets(
        std::make_unique<FfAssetStore>(temp_root),
        [&](const ImageAssetKey& request) {
            gate.decoder_started_and_wait();
            return make_shared_runtime_result(request);
        },
        1);
    RenderAssetRuntime     runtime(renderer_, assets);
    DecodeGateReleaseGuard release_gate{gate};
    const auto             baseline_memory = runtime.memory_stats();
    const auto             baseline_cleanup = runtime.cancelled_preload_cleanup_stats();
    {
        auto source = runtime.begin_incremental_preload({key}, AssetPriority::Critical, "MoveTest");
        auto target = runtime.begin_incremental_preload({}, AssetPriority::Critical, "MoveTest");
        ASSERT_TRUE(gate.wait_until_started());
        target = std::move(source);
        EXPECT_EQ(runtime.cancelled_preload_cleanup_stats().pending_handles,
                  baseline_cleanup.pending_handles);
    }
    EXPECT_EQ(runtime.cancelled_preload_cleanup_stats().pending_handles,
              baseline_cleanup.pending_handles + 1U);
    gate.allow_decode();
    drive_cancelled_cleanup_to_baseline(runtime, baseline_memory, baseline_cleanup);
    EXPECT_EQ(runtime.cancelled_preload_cleanup_stats().released_prepared_images,
              baseline_cleanup.released_prepared_images + 1U);

    std::error_code ec;
    std::filesystem::remove_all(temp_root, ec);
}

TEST_F(SharedRuntimeSdlTest, IncrementalPreloadFinishAndCancelEnforceSingleTerminalState) {
    const auto           temp_root = make_temp_game_root();
    ControlledDecodeGate early_gate;
    ControlledDecodeGate early_sentinel_gate;
    ControlledDecodeGate cancellation_gate;
    ControlledDecodeGate finish_sentinel_gate;
    const auto           early_key = generated_runtime_key("state_early");
    const auto           early_sentinel = generated_runtime_key("state_early_sentinel");
    const auto           cancellation_key = generated_runtime_key("state_cancel");
    const auto           finish_key = generated_runtime_key("state_finish");
    const auto           finish_sentinel = generated_runtime_key("state_finish_sentinel");
    AssetRuntime         assets(
        std::make_unique<FfAssetStore>(temp_root),
        [&](const ImageAssetKey& key) {
            if (key == early_key) {
                early_gate.decoder_started_and_wait();
            } else if (key == early_sentinel) {
                early_sentinel_gate.decoder_started_and_wait();
            } else if (key == cancellation_key) {
                cancellation_gate.decoder_started_and_wait();
            } else if (key == finish_sentinel) {
                finish_sentinel_gate.decoder_started_and_wait();
            }
            return make_shared_runtime_result(key);
        },
        1);
    RenderAssetRuntime     runtime(renderer_, assets);
    DecodeGateReleaseGuard release_early{early_gate};
    DecodeGateReleaseGuard release_early_sentinel{early_sentinel_gate};
    DecodeGateReleaseGuard release_cancellation{cancellation_gate};
    DecodeGateReleaseGuard release_finish_sentinel{finish_sentinel_gate};

    const auto early_cleanup = runtime.cancelled_preload_cleanup_stats();
    {
        auto preload =
            runtime.begin_incremental_preload({early_key}, AssetPriority::Critical, "StateTest");
        auto sentinel_handle =
            assets.request_image(early_sentinel, AssetPriority::Prefetch, "StateTest");
        ASSERT_TRUE(early_gate.wait_until_started());
        EXPECT_THROW(static_cast<void>(preload.finish()), std::logic_error);
        EXPECT_FALSE(preload.complete());
        EXPECT_EQ(runtime.cancelled_preload_cleanup_stats().pending_handles,
                  early_cleanup.pending_handles);
        early_gate.allow_decode();
        ASSERT_TRUE(early_sentinel_gate.wait_until_started());
        preload.advance({.max_textures = 1, .max_ms = 1000.0});
        EXPECT_TRUE(preload.complete());
        static_cast<void>(preload.finish());
        EXPECT_THROW(static_cast<void>(preload.finish()), std::logic_error);
        early_sentinel_gate.allow_decode();
        static_cast<void>(sentinel_handle);
    }
    EXPECT_EQ(runtime.cancelled_preload_cleanup_stats().pending_handles,
              early_cleanup.pending_handles);
    EXPECT_EQ(runtime.cancelled_preload_cleanup_stats().released_prepared_images,
              early_cleanup.released_prepared_images);

    const auto cancellation_baseline_memory = runtime.memory_stats();
    const auto cancellation_baseline_cleanup = runtime.cancelled_preload_cleanup_stats();
    {
        auto preload = runtime.begin_incremental_preload({cancellation_key},
                                                         AssetPriority::Critical, "StateTest");
        ASSERT_TRUE(cancellation_gate.wait_until_started());
        assets.release_prepared(early_sentinel);
    }
    EXPECT_EQ(runtime.cancelled_preload_cleanup_stats().pending_handles,
              cancellation_baseline_cleanup.pending_handles + 1U);
    cancellation_gate.allow_decode();
    drive_cancelled_cleanup_to_baseline(runtime, cancellation_baseline_memory,
                                        cancellation_baseline_cleanup);
    EXPECT_EQ(runtime.cancelled_preload_cleanup_stats().released_prepared_images,
              cancellation_baseline_cleanup.released_prepared_images + 1U);
    EXPECT_EQ(runtime.textures().find(cancellation_key), nullptr);

    const auto finish_memory = runtime.memory_stats();
    const auto finish_cleanup = runtime.cancelled_preload_cleanup_stats();
    {
        auto preload =
            runtime.begin_incremental_preload({finish_key}, AssetPriority::Critical, "StateTest");
        auto sentinel_handle =
            assets.request_image(finish_sentinel, AssetPriority::Prefetch, "StateTest");
        ASSERT_TRUE(finish_sentinel_gate.wait_until_started());
        preload.advance({.max_textures = 1, .max_ms = 1000.0});
        ASSERT_TRUE(preload.complete());
        ASSERT_NE(runtime.textures().find(finish_key), nullptr);
        static_cast<void>(preload.finish());
        EXPECT_THROW(static_cast<void>(preload.finish()), std::logic_error);
        finish_sentinel_gate.allow_decode();
        static_cast<void>(sentinel_handle);
    }
    EXPECT_EQ(runtime.cancelled_preload_cleanup_stats().pending_handles,
              finish_cleanup.pending_handles);
    EXPECT_EQ(runtime.cancelled_preload_cleanup_stats().released_prepared_images,
              finish_cleanup.released_prepared_images);
    EXPECT_EQ(runtime.memory_stats().prepared_resident_count,
              finish_memory.prepared_resident_count);

    std::error_code ec;
    std::filesystem::remove_all(temp_root, ec);
}

TEST_F(SharedRuntimeSdlTest, IncrementalPreloadRespectsTextureCountBudget) {
    const auto           temp_root = make_temp_game_root();
    ControlledDecodeGate sentinel_gate;
    const auto           sentinel = generated_runtime_key("budget_sentinel");
    AssetRuntime         assets(
        std::make_unique<FfAssetStore>(temp_root),
        [&](const ImageAssetKey& key) {
            if (key == sentinel) {
                sentinel_gate.decoder_started_and_wait();
            }
            return make_shared_runtime_result(key);
        },
        1);
    RenderAssetRuntime         runtime(renderer_, assets);
    DecodeGateReleaseGuard     release_gate{sentinel_gate};
    std::vector<ImageAssetKey> keys;
    for (int index = 0; index < 5; ++index) {
        keys.push_back(generated_runtime_key("budget_" + std::to_string(index)));
    }
    auto preload = runtime.begin_incremental_preload(keys, AssetPriority::Critical, "BudgetTest");
    auto sentinel_handle = assets.request_image(sentinel, AssetPriority::Prefetch, "BudgetTest");
    ASSERT_TRUE(
        sentinel_gate.wait_until_started()); // All five preload futures are fulfilled first.
    const RenderAssetUploadBudget budget{.max_textures = 2, .max_ms = 1000.0};
    preload.advance(budget);
    EXPECT_EQ(preload.progress().uploaded, 2u);
    preload.advance(budget);
    EXPECT_EQ(preload.progress().uploaded, 4u);
    preload.advance(budget);
    EXPECT_EQ(preload.progress().uploaded, 5u);
    EXPECT_TRUE(preload.complete());
    for (const auto& key : keys) {
        EXPECT_NE(runtime.textures().find(key), nullptr);
    }
    sentinel_gate.allow_decode();
    static_cast<void>(sentinel_handle);

    std::error_code ec;
    std::filesystem::remove_all(temp_root, ec);
}

TEST_F(SharedRuntimeSdlTest, DeferredScreenTransitionsKeepOneRuntimeContext) {
    struct SeenRuntime {
        const AssetRuntime*          assets = nullptr;
        const RenderAssetRuntime*    render_assets = nullptr;
        const GameDataRegistry*      game_data = nullptr;
        const PortraitManifestIndex* portraits = nullptr;
    };

    struct TransitionState {
        std::vector<SeenRuntime> seen;
        int                      destroyed = 0;
    };

    class ContextScreen final : public Screen {
    public:
        ContextScreen(std::string id, AppRuntimeContext& runtime, ScreenManager& manager,
                      TransitionState& state, std::function<std::unique_ptr<Screen>()> next)
            : Screen(
                  [] {
                      nlohmann::json j;
                      j["/root"] = nlohmann::json::object();
                      TreeLayout tl;
                      tl.load(j);
                      return tl;
                  }(),
                  "test://screen-layout"),
              id_(std::move(id)), runtime_(runtime), manager_(manager), state_(state),
              next_(std::move(next)) {}
        ContextScreen(const ContextScreen&) = delete;
        ContextScreen& operator=(const ContextScreen&) = delete;
        ContextScreen(ContextScreen&&) = delete;
        ContextScreen& operator=(ContextScreen&&) = delete;
        ~ContextScreen() override { ++state_.destroyed; }

        std::string_view name() const override { return id_; }

        void on_enter() override {
            state_.seen.push_back(SeenRuntime{.assets = &runtime_.assets,
                                              .render_assets = &runtime_.render_assets,
                                              .game_data = &runtime_.game_data,
                                              .portraits = &runtime_.portraits});
        }

        bool handle_input(const InputEvent&) override {
            if (next_) {
                manager_.request_switch_to(next_());
                return true;
            }
            return false;
        }

        void update(const d2::app::ScreenUpdateContext&) override {}
        void render(Renderer2D&) override {}

    private:
        std::string                              id_;
        AppRuntimeContext&                       runtime_;
        ScreenManager&                           manager_;
        TransitionState&                         state_;
        std::function<std::unique_ptr<Screen>()> next_;
    };

    const auto   temp_root = make_temp_game_root();
    auto         store = std::make_unique<FfAssetStore>(temp_root);
    AssetRuntime assets(
        std::move(store), [](const ImageAssetKey& key) { return make_shared_runtime_result(key); },
        1);
    RenderAssetRuntime    render_assets(renderer_, assets);
    GameDataRegistry      game_data(std::filesystem::path("/nonexistent/shared-runtime-test"));
    PortraitManifest      empty_manifest;
    PortraitManifestIndex portraits(empty_manifest);
    AppRuntimeContext     runtime{.assets = assets,
                                  .render_assets = render_assets,
                                  .game_data = game_data,
                                  .portraits = portraits};

    TransitionState state;
    ScreenManager   manager;
    auto            make_a2 = [&]() -> std::unique_ptr<Screen> {
        return std::make_unique<ContextScreen>("Adventure2", runtime, manager, state, nullptr);
    };
    auto make_b = [&]() -> std::unique_ptr<Screen> {
        return std::make_unique<ContextScreen>("Battle", runtime, manager, state, make_a2);
    };
    manager.switch_to(
        std::make_unique<ContextScreen>("Adventure", runtime, manager, state, make_b));

    InputEvent test_event = KeyPressed{Key::Space};
    ASSERT_EQ(state.seen.size(), 1u);
    EXPECT_TRUE(manager.handle_input(test_event));
    EXPECT_TRUE(manager.has_pending_transition());
    EXPECT_EQ(state.destroyed, 0);
    manager.apply_pending_transition();

    ASSERT_EQ(state.seen.size(), 2u);
    EXPECT_EQ(state.destroyed, 1);
    EXPECT_TRUE(manager.handle_input(test_event));
    EXPECT_TRUE(manager.has_pending_transition());
    EXPECT_EQ(state.destroyed, 1);
    manager.apply_pending_transition();

    ASSERT_EQ(state.seen.size(), 3u);
    EXPECT_EQ(state.destroyed, 2);
    for (const auto& seen : state.seen) {
        EXPECT_EQ(seen.assets, &assets);
        EXPECT_EQ(seen.render_assets, &render_assets);
        EXPECT_EQ(seen.game_data, &game_data);
        EXPECT_EQ(seen.portraits, &portraits);
    }

    std::error_code ec;
    std::filesystem::remove_all(temp_root, ec);
}

TEST(SharedAppRuntime, InFlightAdventureBattleConsumersDeduplicateOnOneRuntime) {
    std::mutex              mtx;
    std::condition_variable cv;
    bool                    decode_started = false;
    bool                    release_decode = false;
    std::atomic<int>        decode_count{0};

    AssetRuntime assets(
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
            return make_shared_runtime_result(key);
        },
        1);

    const auto key = shared_runtime_key();
    auto       adventure = assets.request_image(key, AssetPriority::Prefetch, "AdventureScreen");
    {
        std::unique_lock lock(mtx);
        ASSERT_TRUE(cv.wait_for(lock, std::chrono::seconds(1), [&] { return decode_started; }));
    }
    auto battle = assets.request_image(key, AssetPriority::Critical, "BattleScreen");
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
    EXPECT_EQ(assets.stats().inflight_joined, 1u);
}

TEST(SharedAppRuntime, ApplicationOwnsSharedRuntimeAndScreensOnlyConsumeContext) {
    const std::string app_header = read_source_file("src/d2engine/app/application.hpp");
    EXPECT_NE(app_header.find("std::unique_ptr<AssetRuntime>"), std::string::npos);
    EXPECT_NE(app_header.find("std::unique_ptr<RenderAssetRuntime>"), std::string::npos);
    EXPECT_NE(app_header.find("std::unique_ptr<GameDataRegistry>"), std::string::npos);
    EXPECT_NE(app_header.find("std::unique_ptr<PortraitManifestIndex>"), std::string::npos);
    EXPECT_NE(app_header.find("std::unique_ptr<AppRuntimeContext>"), std::string::npos);
    EXPECT_NE(app_header.find("shared_runtime_initialized() const noexcept"), std::string::npos);
    EXPECT_NE(app_header.find("ensure_shared_runtime_initialized()"), std::string::npos);
    EXPECT_NE(app_header.find(
                  "Application runtime context requested before shared runtime initialization"),
              std::string::npos);
    const auto context_member = app_header.find("std::unique_ptr<AppRuntimeContext>     context_");
    const auto screen_manager_member =
        app_header.find("ScreenManager                      screen_manager_");
    ASSERT_NE(context_member, std::string::npos);
    ASSERT_NE(screen_manager_member, std::string::npos);
    EXPECT_LT(context_member, screen_manager_member)
        << "ScreenManager must be declared after AppRuntimeContext so active screens are destroyed "
           "before their referenced context";

    const std::string context_header = read_source_file("src/d2engine/app/app_runtime_context.hpp");
    EXPECT_NE(context_header.find("AssetRuntime&"), std::string::npos);
    EXPECT_NE(context_header.find("RenderAssetRuntime&"), std::string::npos);
    EXPECT_NE(context_header.find("GameDataRegistry&"), std::string::npos);
    EXPECT_NE(context_header.find("const PortraitManifestIndex&"), std::string::npos);
    EXPECT_EQ(context_header.find("ScreenManager"), std::string::npos);
    EXPECT_EQ(context_header.find("std::any"), std::string::npos);
    EXPECT_EQ(context_header.find("type_index"), std::string::npos);

    const std::string adventure_header = read_source_file("src/d2engine/app/adventure_screen.hpp");
    EXPECT_NE(
        adventure_header.find(
            "AdventureScreen(AppRuntimeContext& runtime, std::unique_ptr<d2game::GameSession>"),
        std::string::npos);
    EXPECT_EQ(adventure_header.find("Application&"), std::string::npos);
    EXPECT_EQ(adventure_header.find("IncrementalRenderAssetPreload"), std::string::npos);
    EXPECT_EQ(adventure_header.find("deferred_asset_preload_"), std::string::npos);
    EXPECT_EQ(adventure_header.find("std::unique_ptr<AssetRuntime>"), std::string::npos);
    EXPECT_EQ(adventure_header.find("std::unique_ptr<RenderAssetRuntime>"), std::string::npos);
    EXPECT_EQ(adventure_header.find("std::unique_ptr<GameDataRegistry>"), std::string::npos);
    EXPECT_EQ(adventure_header.find("std::unique_ptr<PortraitManifestIndex>"), std::string::npos);
    EXPECT_NE(adventure_header.find("std::unique_ptr<d2game::GameSession>             session_;"),
              std::string::npos);
    EXPECT_EQ(adventure_header.find("d2game::GameSession&                             session_;"),
              std::string::npos);

    const std::string battle_header = read_source_file("src/d2engine/app/battle_screen.hpp");
    EXPECT_NE(battle_header.find("BattleScreen(const AppConfig& config, const AppRuntimeContext&"),
              std::string::npos);
    EXPECT_EQ(battle_header.find("Application&"), std::string::npos);
    EXPECT_EQ(battle_header.find("std::optional<PortraitManifestIndex>"), std::string::npos);
    EXPECT_EQ(battle_header.find("std::unique_ptr<AssetRuntime>"), std::string::npos);
    EXPECT_EQ(battle_header.find("std::unique_ptr<RenderAssetRuntime>"), std::string::npos);
    EXPECT_EQ(battle_header.find("std::unique_ptr<GameDataRegistry>"), std::string::npos);
    EXPECT_EQ(battle_header.find("std::unique_ptr<PortraitManifestIndex>"), std::string::npos);
}

TEST_F(SharedRuntimeSdlTest, ApplicationRuntimeContextThrowsBeforeSharedRuntimeInit) {
    const auto temp_root = make_temp_game_root();
    AppConfig  config;
    config.game_root = temp_root.string();
    config.config_root_override = source_config_root();
    Application app(config);

    EXPECT_FALSE(app.shared_runtime_initialized());
    EXPECT_THROW(static_cast<void>(app.runtime_context()), std::logic_error);

    std::error_code ec;
    std::filesystem::remove_all(temp_root, ec);
}

TEST_F(SharedRuntimeSdlTest, AdventureFirstFramePrecedesSharedRuntimeInit) {
    const auto fixture = adventure_fixture_path();
    if (!fixture.has_value()) {
        GTEST_SKIP() << "OPENDIS2_ADVENTURE_TEST_SG not set";
    }
    const char* game_root_env = std::getenv("DISCIPLES2_GAME_ROOT");
    if (game_root_env == nullptr || game_root_env[0] == '\0') {
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set";
    }

    AppConfig config;
    config.game_root = game_root_env;
    config.scenario_path = fixture->string();
    config.config_root_override = source_config_root();

    Application                      app(config);
    opendis2::AdventureStartupScreen screen(app, config, [&app]() { app.request_quit(); }, nullptr);

    EXPECT_FALSE(app.shared_runtime_initialized());
    screen.on_enter();
    Renderer2D renderer(renderer_);
    screen.render(renderer);
    EXPECT_FALSE(app.shared_runtime_initialized());

    const d2::app::ScreenUpdateContext update_context{.real_delta_ms = 0.0f,
                                                      .animation_delta_ms = 0.0f};
    screen.update(update_context);
    EXPECT_FALSE(app.shared_runtime_initialized());

    screen.update(update_context);
    EXPECT_FALSE(app.shared_runtime_initialized());

    screen.update(update_context);
    EXPECT_TRUE(app.shared_runtime_initialized());
}

TEST_F(SharedRuntimeSdlTest, AdventureStartupMasksAdvanceWithoutBlocking) {
    ControlledDecodeGate gate;
    const auto           temp_root = make_temp_game_root();
    const auto           blocked_key =
        d2engine::make_world_composed_sprite_key("Imgs/MaskBlock.ff", "MASK_BLOCKED.PNG");
    AssetRuntime assets(
        std::make_unique<FfAssetStore>(temp_root),
        [&](const ImageAssetKey& key) {
            gate.decoder_started_and_wait();
            return make_shared_runtime_result(key);
        },
        1);

    auto mask_batch =
        assets.request_batch({blocked_key}, AssetPriority::Prefetch, "AdventureWorldMasksTest");
    ASSERT_TRUE(gate.wait_until_started());

    ar::PreparedAdventureMap map;
    map.geometry = ar::AdventureMapGeometry::from_source(10, 10);
    map.pick_entries.push_back({.stable_id = ar::stable_render_id("Stack:S1"),
                                .kind = ar::PickEntryKind::Stack,
                                .object_id = "S1"});

    ar::PreparedAdventureRenderPrimitive prim;
    prim.stable_id = ar::stable_render_id("Stack:S1");
    prim.level = ar::WorldRenderLevel::Actor;
    prim.container_path = blocked_key.container_path;
    prim.record_name = blocked_key.image_name;
    prim.draw_origin = {0, 0};
    prim.src_width = 1;
    prim.src_height = 1;
    map.world_graph.world.push_back(std::move(prim));

    AdventureLoadingScreen loading_screen(nullptr);
    loading_screen.set_progress(0.90f);

    std::optional<d2engine::AssetBatchHandle> mask_batch_opt{std::move(mask_batch)};
    const auto incomplete = opendis2::detail::advance_adventure_startup_interaction_masks(
        loading_screen, map, mask_batch_opt, 1u, 0.90f, 1.0f);
    EXPECT_FALSE(incomplete.has_value());
    EXPECT_TRUE(mask_batch_opt.has_value());
    EXPECT_GE(loading_screen.progress(), 0.90f);

    Renderer2D renderer(renderer_);
    loading_screen.render(renderer, 1416, 852);

    gate.allow_decode();

    std::optional<std::size_t> built;
    for (int i = 0; i < 1000; ++i) {
        built = opendis2::detail::advance_adventure_startup_interaction_masks(
            loading_screen, map, mask_batch_opt, 1u, 0.90f, 1.0f);
        if (built.has_value()) {
            break;
        }
        std::this_thread::yield();
    }

    ASSERT_TRUE(built.has_value());
    EXPECT_EQ(*built, 1u);
    EXPECT_FALSE(mask_batch_opt.has_value());
    EXPECT_FLOAT_EQ(loading_screen.progress(), 1.0f);

    std::error_code ec;
    std::filesystem::remove_all(temp_root, ec);
}

TEST_F(SharedRuntimeSdlTest, StandaloneBattleInitializesSharedRuntimeOnce) {
    const char* game_root_env = std::getenv("DISCIPLES2_GAME_ROOT");
    if (game_root_env == nullptr || game_root_env[0] == '\0') {
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set";
    }
    const std::filesystem::path game_root_path(game_root_env);
    if (!std::filesystem::is_directory(game_root_path)) {
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT is not a directory: " << game_root_path;
    }

    AppConfig config;
    config.game_root = game_root_path.string();
    config.config_root_override = source_config_root();

    Application app(config);
    EXPECT_FALSE(app.shared_runtime_initialized());
    app.start_battle_screen();
    EXPECT_TRUE(app.shared_runtime_initialized());

    auto* game_data = &app.runtime_context().game_data;
    auto* portraits = &app.runtime_context().portraits;
    auto* context = &app.runtime_context();

    app.start_battle_screen();
    EXPECT_EQ(&app.runtime_context().game_data, game_data);
    EXPECT_EQ(&app.runtime_context().portraits, portraits);
    EXPECT_EQ(&app.runtime_context(), context);
}

TEST_F(SharedRuntimeSdlTest, AdventureToBattleReusesSharedRuntimeOnce) {
    const auto fixture = adventure_fixture_path();
    if (!fixture.has_value()) {
        GTEST_SKIP() << "OPENDIS2_ADVENTURE_TEST_SG not set";
    }
    const char* game_root_env = std::getenv("DISCIPLES2_GAME_ROOT");
    if (game_root_env == nullptr || game_root_env[0] == '\0') {
        GTEST_SKIP() << "DISCIPLES2_GAME_ROOT not set";
    }

    AppConfig config;
    config.game_root = game_root_env;
    config.scenario_path = fixture->string();
    config.config_root_override = source_config_root();

    Application                      app(config);
    opendis2::AdventureStartupScreen screen(app, config, [&app]() { app.request_quit(); }, nullptr);

    screen.on_enter();
    Renderer2D renderer(renderer_);
    screen.render(renderer);
    const d2::app::ScreenUpdateContext update_context{.real_delta_ms = 0.0f,
                                                      .animation_delta_ms = 0.0f};
    screen.update(update_context);
    screen.update(update_context);
    ASSERT_TRUE(app.shared_runtime_initialized());

    auto* game_data = &app.runtime_context().game_data;
    auto* portraits = &app.runtime_context().portraits;

    app.start_battle_screen();
    EXPECT_EQ(&app.runtime_context().game_data, game_data);
    EXPECT_EQ(&app.runtime_context().portraits, portraits);
}

} // namespace
} // namespace d2engine
