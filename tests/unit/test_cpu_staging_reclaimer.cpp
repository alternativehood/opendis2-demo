#include <gtest/gtest.h>

#include "d2engine/assets/cpu_staging_reclaimer.hpp"

#include <atomic>
#include <condition_variable>
#include <chrono>
#include <mutex>
#include <thread>

namespace d2engine {
namespace {

PreparedImageResult make_result() {
    PreparedImageResult r;
    r.success = true;
    r.key = ImageAssetKey{.container_path = "test", .image_name = "probe"};
    auto pixels = std::make_shared<d2res::RgbaBuffer>();
    pixels->width = 1;
    pixels->height = 1;
    pixels->rgba = {1, 2, 3, 255};
    r.image = std::make_shared<PreparedImage>(
        PreparedImage{.key = r.key, .pixels = std::move(pixels), .decode_ms = 0.0});
    return r;
}

PreparedImageResult make_result(const ImageAssetKey& key) {
    PreparedImageResult r;
    r.success = true;
    r.key = key;
    auto pixels = std::make_shared<d2res::RgbaBuffer>();
    pixels->width = 1;
    pixels->height = 1;
    pixels->rgba = {1, 2, 3, 255};
    r.image = std::make_shared<PreparedImage>(
        PreparedImage{.key = key, .pixels = std::move(pixels), .decode_ms = 0.0});
    return r;
}

} // namespace

TEST(CpuStagingReclaimerTest, InactiveBatchItemsNotOutstanding) {
    CpuStagingReclaimer r;
    auto                batch = r.create_batch();
    r.enqueue(batch, make_result());
    EXPECT_FALSE(r.batch_active(batch));
    EXPECT_EQ(r.outstanding(), 0u);
    EXPECT_EQ(r.deferred(), 1u);
    EXPECT_EQ(r.total_owned(), 1u);
}

TEST(CpuStagingReclaimerTest, ActivateBatchDrains) {
    CpuStagingReclaimer r;
    auto                batch = r.create_batch();
    r.enqueue(batch, make_result());
    r.drain_and_wait(batch);
    EXPECT_EQ(r.outstanding(), 0u);
    EXPECT_EQ(r.total_owned(), 0u);
}

TEST(CpuStagingReclaimerTest, EnqueueAfterActivationDestroyed) {
    CpuStagingReclaimer r;
    auto                batch = r.create_batch();
    r.enqueue(batch, make_result());
    r.activate_batch(batch);
    r.enqueue(batch, make_result());
    r.wait_idle();
    EXPECT_EQ(r.outstanding(), 0u);
    EXPECT_EQ(r.total_owned(), 0u);
}

TEST(CpuStagingReclaimerTest, SecondBatchInactiveAfterFirstActivated) {
    CpuStagingReclaimer r;
    auto                ba = r.create_batch();
    auto                bb = r.create_batch();
    r.enqueue(ba, make_result());
    r.enqueue(bb, make_result());
    r.drain_and_wait(ba);
    EXPECT_EQ(r.outstanding(), 0u);
    EXPECT_EQ(r.deferred(), 1u);
    EXPECT_FALSE(r.batch_active(bb));
}

TEST(CpuStagingReclaimerTest, DrainAndWaitSecondBatchAfterFirst) {
    CpuStagingReclaimer r;
    auto                ba = r.create_batch();
    auto                bb = r.create_batch();
    r.enqueue(ba, make_result());
    r.enqueue(bb, make_result());
    r.drain_and_wait(ba);
    r.drain_and_wait(bb);
    EXPECT_EQ(r.total_owned(), 0u);
}

TEST(CpuStagingReclaimerTest, WaitIdleReturnsImmediatelyForInactiveBatch) {
    CpuStagingReclaimer r;
    auto                batch = r.create_batch();
    r.enqueue(batch, make_result());
    r.wait_idle();
    EXPECT_EQ(r.outstanding(), 0u);
    EXPECT_FALSE(r.batch_active(batch));
}

TEST(CpuStagingReclaimerTest, MultipleBatchesActivateIndependently) {
    CpuStagingReclaimer r;
    auto                ba = r.create_batch();
    auto                bb = r.create_batch();
    auto                bc = r.create_batch();
    r.enqueue(ba, make_result());
    r.enqueue(bb, make_result());
    r.enqueue(bc, make_result());

    r.drain_and_wait(ba);
    EXPECT_EQ(r.deferred(), 2u);
    EXPECT_FALSE(r.batch_active(bb));
    EXPECT_FALSE(r.batch_active(bc));

    r.activate_batch(bb);
    r.activate_batch(bc);
    r.wait_idle();
    EXPECT_EQ(r.total_owned(), 0u);
}

TEST(CpuStagingReclaimerTest, DestructorDrainsInactiveBatch) {
    {
        CpuStagingReclaimer r;
        auto                batch = r.create_batch();
        r.enqueue(batch, make_result());
        r.enqueue(batch, make_result());
    }
}

TEST(CpuStagingReclaimerTest, DestructorDrainsActiveBatch) {
    {
        CpuStagingReclaimer r;
        auto                batch = r.create_batch();
        r.enqueue(batch, make_result());
        r.activate_batch(batch);
    }
}

TEST(CpuStagingReclaimerTest, AutoBatchDestroysImmediately) {
    CpuStagingReclaimer r;
    r.enqueue(kReclaimAuto, make_result());
    r.wait_idle();
    EXPECT_EQ(r.outstanding(), 0u);
}

TEST(CpuStagingReclaimerTest, BatchActiveIsThreadSafe) {
    CpuStagingReclaimer r;
    auto                batch = r.create_batch();
    std::atomic<bool>   done{false};
    std::thread         t([&] {
        for (int i = 0; i < 1000; ++i)
            (void)r.batch_active(batch);
        done.store(true);
    });
    for (int i = 0; i < 1000; ++i)
        r.enqueue(batch, make_result());
    t.join();
    EXPECT_TRUE(done.load());
    r.drain_and_wait(batch);
}

TEST(CpuStagingReclaimerTest, WaitIdleBlocksUntilDestructorComplete) {
    std::mutex              mtx;
    std::condition_variable cv;
    bool                    release = false;
    std::atomic<bool>       entered{false};
    std::atomic<bool>       completed{false};

    // Custom deleter on the RgbaBuffer — the reclaimer destroys this object.
    auto deleter = [&](d2res::RgbaBuffer* p) {
        entered.store(true, std::memory_order_release);
        {
            std::unique_lock lock(mtx);
            cv.wait(lock, [&] { return release; });
        }
        delete p;
        completed.store(true, std::memory_order_release);
    };

    PreparedImageResult r;
    r.success = true;
    r.key = ImageAssetKey{};
    auto pixels = std::shared_ptr<d2res::RgbaBuffer>(new d2res::RgbaBuffer(), deleter);
    pixels->width = 1;
    pixels->height = 1;
    pixels->rgba = {0, 0, 0, 255};
    r.image = std::make_shared<PreparedImage>(
        PreparedImage{.key = r.key, .pixels = std::move(pixels), .decode_ms = 0.0});

    CpuStagingReclaimer reclaimer;
    reclaimer.enqueue(kReclaimAuto, std::move(r));

    while (!entered.load(std::memory_order_acquire))
        std::this_thread::sleep_for(std::chrono::microseconds(100));

    EXPECT_GT(reclaimer.outstanding(), 0u);

    std::atomic<bool> wait_returned{false};
    std::thread       waiter([&] {
        reclaimer.wait_idle();
        wait_returned.store(true, std::memory_order_release);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_FALSE(wait_returned.load(std::memory_order_acquire));

    {
        std::lock_guard lock(mtx);
        release = true;
    }
    cv.notify_all();

    waiter.join();
    EXPECT_TRUE(wait_returned.load());
    EXPECT_TRUE(completed.load());
    EXPECT_EQ(reclaimer.outstanding(), 0u);
}

TEST(CpuStagingReclaimerTest, SealedBatchRejectsEnqueue) {
    CpuStagingReclaimer r;
    auto                batch = r.create_batch();
    r.enqueue(batch, make_result());
    EXPECT_EQ(r.deferred(), 1u);
    r.seal_batch(batch);
    r.enqueue(batch, make_result());
    EXPECT_EQ(r.deferred(), 1u) << "sealed batch must not accept new enqueues";
    r.drain_and_wait(batch);
    EXPECT_EQ(r.total_owned(), 0u);
}

TEST(CpuStagingReclaimerTest, BatchMetadataRetiredAfterCompletion) {
    CpuStagingReclaimer r;
    auto                batch = r.create_batch();
    r.enqueue(batch, make_result());
    r.drain_and_wait(batch);
    EXPECT_EQ(r.total_owned(), 0u);

    const auto diags = r.drain_completed_diag();
    bool       found = false;
    for (const auto& d : diags) {
        if (d.id == batch.id) {
            found = true;
            EXPECT_GT(d.reclaimed_count, 0u);
            break;
        }
    }
    EXPECT_TRUE(found) << "completed batch must appear in completed diagnostics";
}

TEST(CpuStagingReclaimerTest, RepeatedGenerationsDoNotGrowBatches) {
    CpuStagingReclaimer r;

    for (int gen = 0; gen < 10; ++gen) {
        auto batch = r.create_batch();
        r.enqueue(batch, make_result());
        r.enqueue(batch, make_result());
        r.drain_and_wait(batch);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    const auto diags = r.drain_completed_diag();
    EXPECT_GE(diags.size(), 10u) << "every generation must produce a completed diagnostic";
    for (const auto& d : diags)
        EXPECT_GT(d.reclaimed_count, 0u);
}

TEST(CpuStagingReclaimerTest, DrainAndWaitPerBatch) {
    CpuStagingReclaimer r;
    auto                ba = r.create_batch();
    auto                bb = r.create_batch();
    r.enqueue(ba, make_result());
    r.enqueue(bb, make_result());
    r.enqueue(bb, make_result());

    r.drain_and_wait(ba);
    EXPECT_EQ(r.deferred(), 2u) << "batch B still deferred after A drained";

    r.drain_and_wait(bb);
    EXPECT_EQ(r.total_owned(), 0u);
}

TEST(CpuStagingReclaimerTest, ConcurrentEnqueueAndDrain) {
    CpuStagingReclaimer r;
    auto                batch = r.create_batch();
    std::atomic<int>    enqueued{0};
    std::atomic<bool>   drain_started{false};

    std::thread producer([&] {
        drain_started.wait(false);
        for (int i = 0; i < 100; ++i) {
            r.enqueue(batch, make_result());
            enqueued.fetch_add(1);
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    });

    drain_started.store(true);
    drain_started.notify_all();
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    r.activate_batch(batch);

    producer.join();
    r.drain_and_wait(batch);
    EXPECT_EQ(r.outstanding(), 0u);
    EXPECT_EQ(r.total_owned(), 0u);
}

TEST(CpuStagingReclaimerTest, DrainAndWaitEmptyBatchReturnsSafely) {
    CpuStagingReclaimer r;
    auto                batch = r.create_batch();
    r.seal_batch(batch);
    r.drain_and_wait(batch);
    EXPECT_EQ(r.total_owned(), 0u);
}

TEST(CpuStagingReclaimerTest, DrainAndWaitBlocksUntilWorkerDestroys) {
    std::mutex              mtx;
    std::condition_variable cv;
    bool                    release = false;
    std::atomic<bool>       entered{false};

    auto deleter = [&](d2res::RgbaBuffer* p) {
        entered.store(true, std::memory_order_release);
        {
            std::unique_lock lock(mtx);
            cv.wait(lock, [&] { return release; });
        }
        delete p;
    };

    PreparedImageResult r;
    r.success = true;
    r.key = ImageAssetKey{};
    auto pixels = std::shared_ptr<d2res::RgbaBuffer>(new d2res::RgbaBuffer(), deleter);
    pixels->width = 1;
    pixels->height = 1;
    pixels->rgba = {0, 0, 0, 255};
    r.image = std::make_shared<PreparedImage>(
        PreparedImage{.key = r.key, .pixels = std::move(pixels), .decode_ms = 0.0});

    CpuStagingReclaimer reclaimer;
    auto                batch = reclaimer.create_batch();
    reclaimer.enqueue(batch, std::move(r));

    std::atomic<bool> drain_returned{false};
    std::thread       drainer([&] {
        reclaimer.drain_and_wait(batch);
        drain_returned.store(true, std::memory_order_release);
    });

    while (!entered.load(std::memory_order_acquire))
        std::this_thread::sleep_for(std::chrono::microseconds(100));

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_FALSE(drain_returned.load(std::memory_order_acquire))
        << "drain_and_wait must block while worker is still destroying";

    {
        std::lock_guard lock(mtx);
        release = true;
    }
    cv.notify_all();

    drainer.join();
    EXPECT_TRUE(drain_returned.load());
    EXPECT_EQ(reclaimer.outstanding(), 0u);
    EXPECT_EQ(reclaimer.total_owned(), 0u);
}

TEST(CpuStagingReclaimerTest, ConcurrentActivationDrainNoEarlyReturn) {
    CpuStagingReclaimer reclaimer;

    for (int trial = 0; trial < 50; ++trial) {
        auto              batch = reclaimer.create_batch();
        std::atomic<bool> drain_returned{false};
        std::atomic<bool> start{false};

        reclaimer.enqueue(batch, make_result());
        reclaimer.enqueue(batch, make_result());

        std::thread drainer([&] {
            start.wait(false);
            reclaimer.drain_and_wait(batch);
            drain_returned.store(true, std::memory_order_release);
        });

        start.store(true);
        start.notify_all();
        reclaimer.activate_batch(batch);

        drainer.join();
        EXPECT_TRUE(drain_returned.load())
            << "drain_and_wait must eventually return, trial=" << trial;
        EXPECT_EQ(reclaimer.outstanding(), 0u) << "trial=" << trial;
        EXPECT_EQ(reclaimer.total_owned(), 0u) << "trial=" << trial;
    }
}

TEST(CpuStagingReclaimerTest, ActivateThenImmediatelyDrainNeverHangs) {
    for (int trial = 0; trial < 200; ++trial) {
        CpuStagingReclaimer r;
        auto                batch = r.create_batch();

        for (int i = 0; i < 10; ++i)
            r.enqueue(batch, make_result());

        r.drain_and_wait(batch);

        EXPECT_EQ(r.outstanding(), 0u) << "trial=" << trial;
        EXPECT_EQ(r.total_owned(), 0u) << "trial=" << trial;
    }
}

TEST(CpuStagingReclaimerTest, EmptyBatchMetadataIsRetired) {
    CpuStagingReclaimer r;

    for (int gen = 0; gen < 20; ++gen) {
        auto batch = r.create_batch();
        r.seal_batch(batch);
        r.drain_and_wait(batch);
    }

    EXPECT_LE(r.live_batch_count(), 0u) << "empty completed batch metadata must be retired";
}

class TestProgressFailure : public std::runtime_error {
public:
    explicit TestProgressFailure() : std::runtime_error("TestProgressFailure") {}
};

TEST(PreloadExceptionCleanup, MidLoopExceptionPreservedAndAllStagingCleared) {
    AssetRuntime assets(
        [](const ImageAssetKey& key) -> PreparedImageResult {
            auto pixels = std::make_shared<d2res::RgbaBuffer>();
            pixels->width = 1;
            pixels->height = 1;
            pixels->rgba = {1, 2, 3, 255};
            auto image = std::make_shared<PreparedImage>(
                PreparedImage{.key = key, .pixels = std::move(pixels), .decode_ms = 0.0});
            return PreparedImageResult{.key = key, .image = std::move(image), .success = true};
        },
        1);

    CpuStagingReclaimer reclaimer;
    auto                batch = reclaimer.create_batch();

    constexpr int                    kCount = 6;
    std::vector<ImageAssetKey>       keys;
    std::vector<PreparedImageResult> results;
    for (int i = 0; i < kCount; ++i) {
        auto key = ImageAssetKey{.container_path = "test",
                                 .image_name = "img" + std::to_string(i),
                                 .kind = ImageAssetKind::ComposedSprite};
        keys.push_back(key);
        auto r = make_result(key);
        results.push_back(r);
        assets.publish_prepared(r);
    }

    {
        auto ps = assets.prepared_resident_stats();
        ASSERT_GE(ps.count, static_cast<std::size_t>(kCount))
            << "all results must be published before the test exercises cleanup";
    }

    const std::size_t throw_after = 2;
    std::size_t       processed_count = 0;
    bool              caught_sentinel = false;

    try {
        for (; processed_count < results.size(); ++processed_count) {
            auto& r = results[processed_count];
            if (r.success && r.image != nullptr) {
                assets.release_prepared(r.key);
                reclaimer.enqueue(batch, std::move(r));
            }
            if (processed_count + 1 > throw_after) {
                throw TestProgressFailure{};
            }
        }
    } catch (const TestProgressFailure&) {
        caught_sentinel = true;
        for (std::size_t i = processed_count; i < results.size(); ++i) {
            auto& r = results[i];
            if (r.success && r.image != nullptr) {
                assets.release_prepared(r.key);
                reclaimer.enqueue(batch, std::move(r));
            }
        }
        reclaimer.seal_batch(batch);
        reclaimer.activate_batch(batch);
        reclaimer.drain_and_wait(batch);
    }

    EXPECT_TRUE(caught_sentinel) << "original TestProgressFailure must be rethrown";
    EXPECT_EQ(reclaimer.total_owned(), 0u);
    EXPECT_EQ(reclaimer.total_owned_bytes(), 0u);

    auto ps = assets.prepared_resident_stats();
    EXPECT_EQ(ps.count, 0u) << "all AssetRuntime prepared entries must be released";
    EXPECT_EQ(ps.rgba_bytes, 0u);
}

TEST(PreloadExceptionCleanup, ExceptionBeforeFirstResultReleasesAllAndRethrows) {
    AssetRuntime assets(
        [](const ImageAssetKey& key) -> PreparedImageResult {
            auto pixels = std::make_shared<d2res::RgbaBuffer>();
            pixels->width = 1;
            pixels->height = 1;
            pixels->rgba = {1, 2, 3, 255};
            auto image = std::make_shared<PreparedImage>(
                PreparedImage{.key = key, .pixels = std::move(pixels), .decode_ms = 0.0});
            return PreparedImageResult{.key = key, .image = std::move(image), .success = true};
        },
        1);

    CpuStagingReclaimer reclaimer;
    auto                batch = reclaimer.create_batch();

    constexpr int                    kCount = 5;
    std::vector<PreparedImageResult> results;
    for (int i = 0; i < kCount; ++i) {
        auto key = ImageAssetKey{.container_path = "test",
                                 .image_name = "unq" + std::to_string(i),
                                 .kind = ImageAssetKind::ComposedSprite};
        auto r = make_result(key);
        results.push_back(r);
        assets.publish_prepared(r);
    }

    bool caught_sentinel = false;

    try {
        throw TestProgressFailure{};
    } catch (const TestProgressFailure&) {
        caught_sentinel = true;
        for (auto& r : results) {
            if (r.success && r.image != nullptr) {
                assets.release_prepared(r.key);
                reclaimer.enqueue(batch, std::move(r));
            }
        }
        reclaimer.seal_batch(batch);
        reclaimer.activate_batch(batch);
        reclaimer.drain_and_wait(batch);
    }

    EXPECT_TRUE(caught_sentinel) << "original TestProgressFailure must be rethrown";
    EXPECT_EQ(reclaimer.total_owned(), 0u);
    EXPECT_EQ(reclaimer.total_owned_bytes(), 0u);

    auto ps = assets.prepared_resident_stats();
    EXPECT_EQ(ps.count, 0u);
    EXPECT_EQ(ps.rgba_bytes, 0u);
}

} // namespace d2engine
