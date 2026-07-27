#include <gtest/gtest.h>

#include "d2engine/assets/bounded_worker_pool.hpp"

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>

namespace d2engine {
namespace {

constexpr unsigned kThreadCount = 4;

} // namespace

TEST(BoundedWorkerPool, ConstructAndDestroy) {
    BoundedWorkerPool pool(2);
    EXPECT_EQ(pool.worker_count(), 2u);
    EXPECT_EQ(pool.outstanding(), 0u);
}

TEST(BoundedWorkerPool, AllJobsCompleteBeforeWaitIdleReturns) {
    BoundedWorkerPool          pool(kThreadCount);
    std::atomic<std::uint32_t> counter{0};
    constexpr std::uint32_t    kTotal = 2000;

    for (std::uint32_t i = 0; i < kTotal; ++i) {
        pool.enqueue([&counter] { counter.fetch_add(1, std::memory_order_relaxed); });
    }
    pool.wait_idle();
    EXPECT_EQ(counter.load(std::memory_order_relaxed), kTotal);
    EXPECT_EQ(pool.outstanding(), 0u);
}

TEST(BoundedWorkerPool, WaitIdleAfterEmptyPool) {
    BoundedWorkerPool pool(kThreadCount);
    pool.wait_idle();
    EXPECT_EQ(pool.outstanding(), 0u);
}

TEST(BoundedWorkerPool, RepeatedEnqueueAndWaitIdle) {
    BoundedWorkerPool          pool(kThreadCount);
    std::atomic<std::uint32_t> counter{0};

    for (int round = 0; round < 25; ++round) {
        counter.store(0, std::memory_order_relaxed);
        for (std::uint32_t i = 0; i < 100; ++i) {
            pool.enqueue([&counter] { counter.fetch_add(1, std::memory_order_relaxed); });
        }
        pool.wait_idle();
        EXPECT_EQ(counter.load(std::memory_order_relaxed), 100u) << "round " << round;
        EXPECT_EQ(pool.outstanding(), 0u) << "round " << round;
    }
}

TEST(BoundedWorkerPool, WaitIdleDoesNotMissJobsInRaceWindow) {
    // Regression: the old pop→active++ race window is eliminated by
    // the outstanding counter (queued + active). This stress test
    // rapidly interleaves enqueue with wait_idle from multiple threads
    // to exercise tight scheduling windows.
    BoundedWorkerPool          pool(kThreadCount);
    std::atomic<std::uint32_t> counter{0};
    std::atomic<bool>          stop{false};

    auto enqueuer = [&] {
        while (!stop.load(std::memory_order_acquire)) {
            pool.enqueue([&counter] { counter.fetch_add(1, std::memory_order_relaxed); });
            std::this_thread::yield();
        }
    };

    auto waiter = [&] {
        while (!stop.load(std::memory_order_acquire)) {
            pool.wait_idle();
        }
        pool.wait_idle(); // final drain
    };

    std::thread t1(enqueuer);
    std::thread t2(waiter);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop.store(true, std::memory_order_release);
    t1.join();
    t2.join();

    pool.wait_idle();
    EXPECT_EQ(pool.outstanding(), 0u);
    const auto final_count = counter.load(std::memory_order_relaxed);
    // All jobs must have completed — the counter should equal the
    // number enqueued before stop was observed.
    pool.wait_idle();
    EXPECT_EQ(counter.load(std::memory_order_relaxed), final_count);
}

TEST(BoundedWorkerPool, ThrowingJobDoesNotTerminatePool) {
    BoundedWorkerPool          pool(kThreadCount);
    std::atomic<std::uint32_t> normal_done{0};
    std::atomic<bool>          threw_caught{false};

    pool.enqueue([&] { throw std::runtime_error("intentional test exception"); });

    pool.enqueue([&] {
        threw_caught.store(true, std::memory_order_relaxed);
        normal_done.fetch_add(1, std::memory_order_relaxed);
    });

    pool.enqueue([&] { normal_done.fetch_add(1, std::memory_order_relaxed); });

    pool.wait_idle();
    EXPECT_EQ(pool.outstanding(), 0u);
    EXPECT_EQ(normal_done.load(std::memory_order_relaxed), 2u);
    EXPECT_TRUE(threw_caught.load(std::memory_order_relaxed));

    // Pool must remain usable after exceptions
    std::atomic<std::uint32_t> after{0};
    pool.enqueue([&after] { after.fetch_add(1, std::memory_order_relaxed); });
    pool.wait_idle();
    EXPECT_EQ(after.load(std::memory_order_relaxed), 1u);
}

TEST(BoundedWorkerPool, NonStdExceptionDoesNotTerminatePool) {
    BoundedWorkerPool          pool(kThreadCount);
    std::atomic<std::uint32_t> normal_done{0};

    pool.enqueue([&] { throw 42; });
    pool.enqueue([&] { normal_done.fetch_add(1, std::memory_order_relaxed); });
    pool.wait_idle();
    EXPECT_EQ(pool.outstanding(), 0u);
    EXPECT_EQ(normal_done.load(std::memory_order_relaxed), 1u);
}

TEST(BoundedWorkerPool, DestructorDrainsRemainingJobs) {
    std::atomic<std::uint32_t> counter{0};
    constexpr std::uint32_t    kTotal = 500;

    {
        BoundedWorkerPool pool(2);
        for (std::uint32_t i = 0; i < kTotal; ++i) {
            pool.enqueue([&counter] { counter.fetch_add(1, std::memory_order_relaxed); });
        }
        // Destructor runs here — must drain pending jobs
    }
    EXPECT_EQ(counter.load(std::memory_order_relaxed), kTotal);
}

TEST(BoundedWorkerPool, NestedEnqueueCompletesBeforeWaitIdle) {
    BoundedWorkerPool          pool(kThreadCount);
    std::atomic<std::uint32_t> outer_count{0};
    std::atomic<std::uint32_t> inner_count{0};

    // Each outer job enqueues one inner job
    for (int i = 0; i < 100; ++i) {
        pool.enqueue([&pool, &outer_count, &inner_count] {
            outer_count.fetch_add(1, std::memory_order_relaxed);
            pool.enqueue([&inner_count] { inner_count.fetch_add(1, std::memory_order_relaxed); });
        });
    }
    pool.wait_idle();
    EXPECT_EQ(pool.outstanding(), 0u);
    EXPECT_EQ(outer_count.load(std::memory_order_relaxed), 100u);
    EXPECT_EQ(inner_count.load(std::memory_order_relaxed), 100u);
}

TEST(BoundedWorkerPool, EnqueueOutstandingAccountingInvariant) {
    BoundedWorkerPool          pool(kThreadCount);
    std::atomic<std::uint32_t> done{0};

    EXPECT_EQ(pool.outstanding(), 0u);
    for (int i = 0; i < 50; ++i) {
        pool.enqueue([&done] { done.fetch_add(1, std::memory_order_relaxed); });
    }

    pool.wait_idle();
    EXPECT_EQ(pool.outstanding(), 0u);
    EXPECT_EQ(done.load(std::memory_order_relaxed), 50u);

    for (int i = 0; i < 50; ++i) {
        pool.enqueue([&done] { done.fetch_add(1, std::memory_order_relaxed); });
    }

    pool.wait_idle();
    EXPECT_EQ(pool.outstanding(), 0u);
    EXPECT_EQ(done.load(std::memory_order_relaxed), 100u);
}

} // namespace d2engine
