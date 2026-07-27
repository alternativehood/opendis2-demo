#include "cpu_staging_reclaimer.hpp"

#include <d2log/log.hpp>
#include <d2res/rgba_buffer.hpp>

#include <algorithm>
#include <unordered_map>

namespace d2engine {

namespace {

auto kLog = d2log::get("d2.reclaimer"); // NOLINT(cert-err58-cpp)

std::size_t rgba_bytes(const PreparedImageResult& r) {
    if (r.image != nullptr && r.image->pixels != nullptr) {
        return r.image->pixels->rgba.size();
    }
    return 0;
}

} // namespace

CpuStagingReclaimer::CpuStagingReclaimer() {
    worker_ = std::thread([this] { worker_loop(); });
}

CpuStagingReclaimer::~CpuStagingReclaimer() {
    const auto shutdown_start = std::chrono::steady_clock::now();

    {
        std::unique_lock lock(batch_mtx_);
        for (auto& [id, b] : batches_) {
            if (b->activated.exchange(true, std::memory_order_acq_rel))
                continue;
            if (b->pending.empty())
                continue;
            std::size_t count = b->pending.size();
            std::size_t bytes = 0;
            for (auto& r : b->pending)
                bytes += rgba_bytes(r);

            auto batch_ref = b;
            lock.unlock();

            count = batch_ref->pending.size();
            bytes = 0;
            for (auto& r : batch_ref->pending)
                bytes += rgba_bytes(r);

            BulkWork bw;
            bw.batch_id = id;
            bw.items = std::move(batch_ref->pending);
            bw.bytes = bytes;
            batch_ref->pending.clear();

            bool published = false;
            try {
                std::lock_guard wlock(work_mtx_);
                work_queue_.push_back(std::move(bw));
                published = true;
                outstanding_.fetch_add(count, std::memory_order_relaxed);
                outstanding_bytes_.fetch_add(bytes, std::memory_order_relaxed);
                deferred_.fetch_sub(count, std::memory_order_relaxed);
                deferred_bytes_.fetch_sub(bytes, std::memory_order_relaxed);
                batch_ref->active_count.fetch_add(count, std::memory_order_relaxed);
                batch_ref->active_bytes.fetch_add(bytes, std::memory_order_relaxed);
            } catch (...) {
            }

            if (published) {
                lock.lock();
                continue;
            }

            lock.lock();
        }
    }

    const std::size_t pre_drain_staging_mb = total_owned_bytes() / (1024 * 1024);

    stopping_.store(true, std::memory_order_release);
    work_cv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }

    const std::size_t remaining_after_drain_mb = total_owned_bytes() / (1024 * 1024);
    const double      shutdown_ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - shutdown_start)
            .count();

    if (remaining_after_drain_mb > 0) {
        kLog->error("shutdown_reclaimer_ms={:.1f} pre_drain_staging_mb={} "
                    "remaining_after_drain_mb={} : LEAK_DETECTED",
                    shutdown_ms, pre_drain_staging_mb, remaining_after_drain_mb);
    } else {
        kLog->info("shutdown_reclaimer_ms={:.1f} pre_drain_staging_mb={} "
                   "remaining_after_drain_mb=0",
                   shutdown_ms, pre_drain_staging_mb);
    }
}

ReclaimBatch CpuStagingReclaimer::create_batch() {
    std::lock_guard lock(batch_mtx_);
    ReclaimBatch    h{next_handle_++};
    batches_[h.id] = std::make_shared<Batch>();
    return h;
}

void CpuStagingReclaimer::enqueue(ReclaimBatch batch, PreparedImageResult result) {
    const auto bytes = rgba_bytes(result);

    if (batch == kReclaimAuto) {
        std::lock_guard wlock(work_mtx_);
        BulkWork        bw;
        bw.batch_id = 0;
        bw.items.push_back(std::move(result));
        bw.bytes = bytes;
        work_queue_.push_back(std::move(bw));
        outstanding_.fetch_add(1, std::memory_order_relaxed);
        outstanding_bytes_.fetch_add(bytes, std::memory_order_relaxed);
        work_cv_.notify_one();
        return;
    }

    std::unique_lock lock(batch_mtx_);
    auto             it = batches_.find(batch.id);
    if (it == batches_.end())
        return;
    auto batch_ref = it->second;

    if (batch_ref->sealed.load(std::memory_order_acquire))
        return;

    batch_ref->pending.push_back(std::move(result));
    deferred_.fetch_add(1, std::memory_order_relaxed);
    deferred_bytes_.fetch_add(bytes, std::memory_order_relaxed);
    batch_ref->total_enqueued.fetch_add(1, std::memory_order_relaxed);
    batch_ref->total_bytes.fetch_add(bytes, std::memory_order_relaxed);
}

void CpuStagingReclaimer::seal_batch(ReclaimBatch batch) {
    if (batch == kReclaimAuto)
        return;
    std::lock_guard lock(batch_mtx_);
    auto            it = batches_.find(batch.id);
    if (it == batches_.end())
        return;
    it->second->sealed.store(true, std::memory_order_release);
}

void CpuStagingReclaimer::activate_batch(ReclaimBatch batch) {
    if (batch == kReclaimAuto)
        return;

    std::deque<PreparedImageResult> detached;
    uint64_t                        detached_bytes = 0;
    uint64_t                        id = batch.id;
    std::shared_ptr<Batch>          batch_ref;

    {
        std::lock_guard lock(batch_mtx_);
        auto            it = batches_.find(id);
        if (it == batches_.end())
            return;
        batch_ref = it->second;
        batch_ref->sealed.store(true, std::memory_order_release);
        if (batch_ref->activated.exchange(true, std::memory_order_acq_rel))
            return;
        detached = std::move(batch_ref->pending);

        if (detached.empty()) {
            batch_ref->activated_at = std::chrono::steady_clock::now();
            batch_ref->notify_completion();
            batches_.erase(it);
            return;
        }

        for (auto& r : detached)
            detached_bytes += rgba_bytes(r);
    }

    Batch*            raw_batch = batch_ref.get();
    const std::size_t count = detached.size();

    {
        BulkWork bw;
        bw.batch_id = id;
        bw.items = std::move(detached);
        bw.bytes = detached_bytes;

        std::lock_guard wlock(work_mtx_);
        work_queue_.push_back(std::move(bw));

        outstanding_.fetch_add(count, std::memory_order_relaxed);
        outstanding_bytes_.fetch_add(detached_bytes, std::memory_order_relaxed);
        deferred_.fetch_sub(count, std::memory_order_relaxed);
        deferred_bytes_.fetch_sub(detached_bytes, std::memory_order_relaxed);
        raw_batch->active_count.fetch_add(count, std::memory_order_relaxed);
        raw_batch->active_bytes.fetch_add(detached_bytes, std::memory_order_relaxed);
        raw_batch->activated_at = std::chrono::steady_clock::now();

        work_cv_.notify_all();
    }

    const std::size_t owned_mb = total_owned_bytes() / (1024 * 1024);
    kLog->info("reclaim_activated batch_id={} activated_count={} activated_mb={} total_owned_mb={}",
               id, count, detached_bytes / (1024 * 1024), owned_mb);
}

void CpuStagingReclaimer::drain_and_wait(ReclaimBatch batch) {
    activate_batch(batch);

    std::shared_ptr<Batch> batch_ref;
    {
        std::lock_guard lock(batch_mtx_);
        auto            it = batches_.find(batch.id);
        if (it == batches_.end())
            return;
        batch_ref = it->second;
    }

    std::unique_lock lock(batch_ref->completion_mtx);
    batch_ref->completion_cv.wait(lock, [&] { return batch_ref->completed_flag; });
}

void CpuStagingReclaimer::wait_idle() {
    std::unique_lock lock(work_mtx_);
    work_cv_.wait(lock, [&] { return outstanding_.load(std::memory_order_acquire) == 0; });
}

std::size_t CpuStagingReclaimer::outstanding() const noexcept {
    return outstanding_.load(std::memory_order_acquire);
}
std::size_t CpuStagingReclaimer::outstanding_bytes() const noexcept {
    return outstanding_bytes_.load(std::memory_order_acquire);
}
std::size_t CpuStagingReclaimer::deferred() const noexcept {
    return deferred_.load(std::memory_order_acquire);
}
std::size_t CpuStagingReclaimer::deferred_bytes() const noexcept {
    return deferred_bytes_.load(std::memory_order_acquire);
}
std::size_t CpuStagingReclaimer::total_owned() const noexcept {
    return deferred_.load(std::memory_order_acquire) + outstanding_.load(std::memory_order_acquire);
}
std::size_t CpuStagingReclaimer::total_owned_bytes() const noexcept {
    return deferred_bytes_.load(std::memory_order_acquire) +
           outstanding_bytes_.load(std::memory_order_acquire);
}

bool CpuStagingReclaimer::batch_active(ReclaimBatch batch) const {
    std::lock_guard lock(batch_mtx_);
    auto            it = batches_.find(batch.id);
    return it != batches_.end() && it->second->activated.load(std::memory_order_acquire);
}

std::size_t CpuStagingReclaimer::live_batch_count() const {
    std::lock_guard lock(batch_mtx_);
    return batches_.size();
}

std::vector<ReclaimCompleteDiag> CpuStagingReclaimer::drain_completed_diag() {
    std::lock_guard                  lock(completed_diag_mtx_);
    std::vector<ReclaimCompleteDiag> result;
    result.swap(completed_diag_);
    return result;
}

void CpuStagingReclaimer::worker_loop() noexcept {
    std::vector<
        std::tuple<uint64_t, std::size_t, std::size_t, std::chrono::steady_clock::time_point>>
        completed_batches;

    for (;;) {
        std::deque<BulkWork> items;
        {
            std::unique_lock lock(work_mtx_);
            work_cv_.wait(lock, [&] {
                return stopping_.load(std::memory_order_acquire) || !work_queue_.empty();
            });
            if (stopping_.load(std::memory_order_acquire) && work_queue_.empty())
                break;
            items.swap(work_queue_);
        }
        if (items.empty())
            continue;

        std::unordered_map<uint64_t, std::size_t> batch_bytes;
        std::unordered_map<uint64_t, std::size_t> batch_counts;
        std::size_t                               total_count = 0;
        for (auto& bw : items) {
            for (auto& r : bw.items) {
                if (r.image != nullptr && r.image->pixels != nullptr) {
                    auto sz = r.image->pixels->rgba.size();
                    batch_bytes[bw.batch_id] += sz;
                }
                ++batch_counts[bw.batch_id];
                ++total_count;
            }
        }
        items.clear();

        std::size_t total_bytes = 0;
        for (auto& [id, b] : batch_bytes)
            total_bytes += b;

        outstanding_.fetch_sub(total_count, std::memory_order_relaxed);
        outstanding_bytes_.fetch_sub(total_bytes, std::memory_order_relaxed);

        completed_batches.clear();
        {
            const auto      now = std::chrono::steady_clock::now();
            std::lock_guard blk(batch_mtx_);
            for (auto& [id, count] : batch_counts) {
                if (id == 0)
                    continue;
                auto it = batches_.find(id);
                if (it == batches_.end())
                    continue;
                auto& b = *it->second;
                b.active_count.fetch_sub(count, std::memory_order_relaxed);
                b.active_bytes.fetch_sub(batch_bytes[id], std::memory_order_relaxed);

                if (b.sealed.load(std::memory_order_acquire) &&
                    b.activated.load(std::memory_order_acquire) &&
                    b.active_count.load(std::memory_order_acquire) == 0) {
                    std::size_t reclaimed_count = b.total_enqueued.load(std::memory_order_acquire);
                    std::size_t reclaimed_bytes_val = b.total_bytes.load(std::memory_order_acquire);
                    auto        at = b.activated_at;
                    double dur_ms = std::chrono::duration<double, std::milli>(now - at).count();

                    kLog->info("reclaim_batch_complete batch_id={} reclaimed_count={} "
                               "reclaimed_mb={} duration_ms={:.1f}",
                               id, reclaimed_count, reclaimed_bytes_val / (1024 * 1024), dur_ms);

                    {
                        std::lock_guard diag_lock(completed_diag_mtx_);
                        completed_diag_.push_back(
                            ReclaimCompleteDiag{.id = id,
                                                .reclaimed_count = reclaimed_count,
                                                .reclaimed_mb = reclaimed_bytes_val / (1024 * 1024),
                                                .duration_ms = dur_ms});
                    }

                    it->second->notify_completion();
                    batches_.erase(it);
                }
            }
        }

        work_cv_.notify_all();
    }

    completed_batches.clear();
    {
        std::lock_guard blk(batch_mtx_);
        for (auto it = batches_.begin(); it != batches_.end();) {
            auto& b = *it->second;
            if (b.active_count.load(std::memory_order_acquire) == 0 &&
                b.sealed.load(std::memory_order_acquire) &&
                b.activated.load(std::memory_order_acquire)) {
                it->second->notify_completion();
                it = batches_.erase(it);
            } else {
                ++it;
            }
        }
    }
}

} // namespace d2engine
