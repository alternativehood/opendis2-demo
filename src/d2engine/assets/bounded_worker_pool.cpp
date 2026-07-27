#include "bounded_worker_pool.hpp"

#include <d2log/log.hpp>

namespace d2engine {

namespace {
auto kPoolLog = d2log::get("d2.assets.pool"); // NOLINT(cert-err58-cpp)
} // namespace

BoundedWorkerPool::BoundedWorkerPool(unsigned worker_count) {
    workers_.reserve(worker_count);
    for (unsigned i = 0; i < worker_count; ++i) {
        workers_.emplace_back([this] { worker_loop(); });
    }
}

BoundedWorkerPool::~BoundedWorkerPool() {
    {
        std::lock_guard lock(mtx_);
        stopping_ = true;
    }
    cv_.notify_all();
    for (auto& w : workers_) {
        if (w.joinable())
            w.join();
    }
}

void BoundedWorkerPool::enqueue(std::function<void()> job) {
    {
        std::lock_guard lock(mtx_);
        jobs_.push(std::move(job));
        outstanding_.fetch_add(1, std::memory_order_acq_rel);
    }
    cv_.notify_one();
}

void BoundedWorkerPool::wait_idle() {
    std::unique_lock lock(mtx_);
    cv_.wait(lock, [&] { return outstanding_.load(std::memory_order_acquire) == 0; });
}

void BoundedWorkerPool::worker_loop() noexcept {
    for (;;) {
        std::function<void()> job;
        {
            std::unique_lock lock(mtx_);
            cv_.wait(lock, [&] { return stopping_ || !jobs_.empty(); });
            if (stopping_ && jobs_.empty())
                return;
            job = std::move(jobs_.front());
            jobs_.pop();
        }

        // RAII: outstanding accounting is guaranteed even if job() throws.
        struct CompletionGuard {
            BoundedWorkerPool* self;
            explicit CompletionGuard(BoundedWorkerPool* pool) : self(pool) {}
            ~CompletionGuard() {
                self->outstanding_.fetch_sub(1, std::memory_order_acq_rel);
                std::lock_guard lock(self->mtx_);
                self->cv_.notify_all();
            }
            CompletionGuard(const CompletionGuard&) = delete;
            CompletionGuard& operator=(const CompletionGuard&) = delete;
            CompletionGuard(CompletionGuard&&) = delete;
            CompletionGuard& operator=(CompletionGuard&&) = delete;
        } guard{this};

        try {
            job();
        } catch (const std::exception& e) {
            kPoolLog->error("worker job threw exception: {}", e.what());
        } catch (...) {
            kPoolLog->error("worker job threw unknown exception");
        }
    }
}

} // namespace d2engine
