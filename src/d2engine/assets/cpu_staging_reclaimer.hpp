#pragma once

#include "asset_runtime.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace d2engine {

struct ReclaimBatch {
    uint64_t    id = 0;
    friend bool operator==(ReclaimBatch, ReclaimBatch) noexcept = default;
};

inline constexpr ReclaimBatch kReclaimAuto = {0};

struct ReclaimBatchDiag {
    uint64_t    id = 0;
    std::size_t deferred_count = 0;
    std::size_t deferred_mb = 0;
    std::size_t total_owned_mb = 0;
};

struct ReclaimCompleteDiag {
    uint64_t    id = 0;
    std::size_t reclaimed_count = 0;
    std::size_t reclaimed_mb = 0;
    double      duration_ms = 0.0;
};

class CpuStagingReclaimer {
public:
    CpuStagingReclaimer();
    ~CpuStagingReclaimer();

    CpuStagingReclaimer(const CpuStagingReclaimer&) = delete;
    CpuStagingReclaimer& operator=(const CpuStagingReclaimer&) = delete;
    CpuStagingReclaimer(CpuStagingReclaimer&&) = delete;
    CpuStagingReclaimer& operator=(CpuStagingReclaimer&&) = delete;

    [[nodiscard]] ReclaimBatch create_batch();
    void                       enqueue(ReclaimBatch batch, PreparedImageResult result);
    void                       seal_batch(ReclaimBatch batch);
    void                       activate_batch(ReclaimBatch batch);

    void drain_and_wait(ReclaimBatch batch);
    void wait_idle();

    [[nodiscard]] std::size_t outstanding() const noexcept;
    [[nodiscard]] std::size_t outstanding_bytes() const noexcept;
    [[nodiscard]] std::size_t deferred() const noexcept;
    [[nodiscard]] std::size_t deferred_bytes() const noexcept;
    [[nodiscard]] std::size_t total_owned() const noexcept;
    [[nodiscard]] std::size_t total_owned_bytes() const noexcept;
    [[nodiscard]] bool        batch_active(ReclaimBatch batch) const;
    [[nodiscard]] std::size_t live_batch_count() const;

    std::vector<ReclaimCompleteDiag> drain_completed_diag();

private:
    void worker_loop() noexcept;

    struct Batch {
        std::deque<PreparedImageResult> pending;
        std::atomic<bool>               activated{false};
        std::atomic<bool>               sealed{false};
        std::atomic<std::size_t>        active_count{0};
        std::atomic<std::size_t>        active_bytes{0};
        std::atomic<std::size_t>        total_enqueued{0};
        std::atomic<std::size_t>        total_bytes{0};

        mutable std::mutex      completion_mtx;
        std::condition_variable completion_cv;
        bool                    completed_flag{false};

        std::chrono::steady_clock::time_point activated_at;

        void notify_completion() {
            {
                std::lock_guard lock(completion_mtx);
                completed_flag = true;
            }
            completion_cv.notify_all();
        }
    };

    struct BulkWork {
        uint64_t                        batch_id;
        std::deque<PreparedImageResult> items;
        std::size_t                     bytes = 0;
    };

    struct WorkItem {
        PreparedImageResult result;
        uint64_t            batch_id;
    };

    mutable std::mutex                                   batch_mtx_;
    std::unordered_map<uint64_t, std::shared_ptr<Batch>> batches_;
    uint64_t                                             next_handle_{1};
    std::atomic<bool>                                    stopping_{false};

    std::mutex               work_mtx_;
    std::condition_variable  work_cv_;
    std::deque<BulkWork>     work_queue_;
    std::atomic<std::size_t> outstanding_{0};
    std::atomic<std::size_t> outstanding_bytes_{0};
    std::atomic<std::size_t> deferred_{0};
    std::atomic<std::size_t> deferred_bytes_{0};

    mutable std::mutex               completed_diag_mtx_;
    std::vector<ReclaimCompleteDiag> completed_diag_;

    std::thread worker_;
};

} // namespace d2engine
