#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace d2engine {

/// Bounded worker pool with deterministic idle semantics.
///
/// Invariant: `outstanding_` == queued + executing jobs at all times.
///   - enqueue() pushes the job before incrementing `outstanding_`.
///     If push throws, outstanding is unchanged; wait_idle() remains safe.
///   - Job completion decrements `outstanding_` (RAII, exception-safe).
///   - wait_idle() returns only when `outstanding_` == 0: every job
///     enqueued before that point has fully completed.
///
/// No exception may escape a worker thread: job() is wrapped in a
/// catch-all boundary; failures are logged and the pool remains usable.
/// Outstanding accounting is always correct even after a throwing job.
class BoundedWorkerPool {
public:
    explicit BoundedWorkerPool(unsigned worker_count);
    ~BoundedWorkerPool();

    BoundedWorkerPool(const BoundedWorkerPool&) = delete;
    BoundedWorkerPool& operator=(const BoundedWorkerPool&) = delete;
    BoundedWorkerPool(BoundedWorkerPool&&) = delete;
    BoundedWorkerPool& operator=(BoundedWorkerPool&&) = delete;

    /// Enqueue a job. Ownership of the callable is transferred.
    /// Jobs enqueued before a wait_idle() call are guaranteed to run
    /// to completion before that call returns.
    /// Must not be called after the destructor has begun draining.
    void enqueue(std::function<void()> job);

    /// Block until every job currently enqueued has fully completed.
    void wait_idle();

    /// Current number of jobs queued + executing.
    [[nodiscard]] std::size_t outstanding() const noexcept {
        return outstanding_.load(std::memory_order_acquire);
    }

    [[nodiscard]] std::size_t worker_count() const noexcept { return workers_.size(); }

private:
    void worker_loop() noexcept;

    std::vector<std::thread>          workers_;
    std::mutex                        mtx_;
    std::condition_variable           cv_;
    bool                              stopping_ = false;
    std::queue<std::function<void()>> jobs_;
    std::atomic<std::size_t>          outstanding_{0};
};

} // namespace d2engine
