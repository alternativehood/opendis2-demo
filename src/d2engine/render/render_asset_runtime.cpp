#include "render_asset_runtime.hpp"

#include <d2log/log.hpp>

#include <chrono>
#include <stdexcept>
#include <utility>

namespace d2engine {

namespace {
auto kLog = d2log::get("d2.render.assets"); // NOLINT(cert-err58-cpp)
} // namespace

RenderAssetRuntime::RenderAssetRuntime(SDL_Renderer* renderer, AssetRuntime& assets)
    : assets_(assets), textures_(renderer, assets.store()) {}

ImageRequestHandle RenderAssetRuntime::request_texture(const ImageAssetKey& key,
                                                       AssetPriority        priority,
                                                       std::string_view     consumer_tag) {
    if (textures_.is_cached(key)) {
        return {};
    }
    textures_.mark_planned(key);
    auto handle = assets_.request_image(key, priority, consumer_tag);
    pending_.push_back(handle);
    return handle;
}

AssetBatchHandle RenderAssetRuntime::request_textures(const std::vector<ImageAssetKey>& keys,
                                                      AssetPriority                     priority,
                                                      std::string_view consumer_tag) {
    AssetBatchHandle batch;
    for (const auto& key : keys) {
        auto handle = request_texture(key, priority, consumer_tag);
        if (handle.valid()) {
            batch.add(std::move(handle));
        }
    }
    return batch;
}

RenderAssetUploadStats RenderAssetRuntime::pump_uploads(RenderAssetUploadBudget budget) {
    pump_cancelled_preload_cleanup();

    const auto             started = std::chrono::steady_clock::now();
    RenderAssetUploadStats stats;
    const std::size_t      initial = pending_.size();
    std::size_t            scanned = 0;

    while (!pending_.empty() && scanned < initial && stats.uploaded < budget.max_textures) {
        auto handle = std::move(pending_.front());
        pending_.pop_front();
        ++scanned;

        if (textures_.is_cached(handle.key())) {
            continue;
        }

        if (!handle.ready()) {
            pending_.push_back(std::move(handle));
            continue;
        }

        const auto result = handle.get();
        if (result.success && result.image != nullptr) {
            const auto uploaded = textures_.upload_prepared(*result.image);
            if (uploaded.success || uploaded.skipped_cache_hit) {
                ++stats.uploaded;
                assets_.release_prepared(result.key);
            } else {
                ++stats.failed;
            }
        } else {
            ++stats.failed;
        }

        const double elapsed_ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started)
                .count();
        if (elapsed_ms >= budget.max_ms) {
            break;
        }
    }

    stats.remaining = pending_.size();
    return stats;
}

void RenderAssetRuntime::pump_cancelled_preload_cleanup() {
    for (auto cleanup_it = cancelled_preload_cleanup_.begin();
         cleanup_it != cancelled_preload_cleanup_.end();) {
        auto&             cleanup = *cleanup_it;
        const std::size_t initial = cleanup.pending.size();
        std::size_t       scanned = 0;
        while (!cleanup.pending.empty() && scanned < initial) {
            auto handle = std::move(cleanup.pending.front());
            cleanup.pending.pop_front();
            ++scanned;

            if (!handle.ready()) {
                cleanup.pending.push_back(std::move(handle));
                continue;
            }

            auto result = handle.get();
            if (result.success && result.image != nullptr) {
                assets_.release_prepared_if_matches(result.key, result.image);
                ++released_cancelled_prepared_images_;
                reclaimer_.enqueue(cleanup.reclaim_batch, std::move(result));
            }
        }

        if (!cleanup.pending.empty()) {
            ++cleanup_it;
            continue;
        }

        reclaimer_.seal_batch(cleanup.reclaim_batch);
        reclaimer_.activate_batch(cleanup.reclaim_batch);
        cleanup_it = cancelled_preload_cleanup_.erase(cleanup_it);
    }
}

void RenderAssetRuntime::wait_for_reclaim_idle() {
    reclaimer_.wait_idle();
}

CancelledPreloadCleanupStats RenderAssetRuntime::cancelled_preload_cleanup_stats() const {
    CancelledPreloadCleanupStats stats;
    stats.released_prepared_images = released_cancelled_prepared_images_;
    for (const auto& cleanup : cancelled_preload_cleanup_) {
        stats.pending_handles += cleanup.pending.size();
        for (const auto& handle : cleanup.pending) {
            if (handle.ready()) {
                ++stats.ready_handles;
            }
        }
    }
    return stats;
}

RenderAssetRuntimeMemoryStats RenderAssetRuntime::memory_stats() const {
    const auto prepared = assets_.prepared_resident_stats();
    return {.prepared_resident_count = prepared.count,
            .cleanup_pending_count = cancelled_preload_cleanup_stats().pending_handles,
            .staging_owned_bytes = reclaimer_.total_owned_bytes(),
            .reclaim_pending_bytes = reclaimer_.deferred_bytes()};
}

RenderAssetUploadStats RenderAssetRuntime::upload_ready(const AssetBatchHandle& batch) {
    RenderAssetUploadStats stats;
    for (const auto& handle : batch.handles()) {
        const auto result = handle.get();
        if (result.success && result.image != nullptr) {
            const auto uploaded = textures_.upload_prepared(*result.image);
            if (uploaded.success || uploaded.skipped_cache_hit) {
                ++stats.uploaded;
                assets_.release_prepared(result.key);
            } else {
                ++stats.failed;
            }
        } else {
            ++stats.failed;
        }
    }
    stats.remaining = pending_.size();
    return stats;
}

bool RenderAssetRuntime::upload_one_and_release(PreparedImageResult& result, ReclaimBatch batch) {
    if (!result.success || result.image == nullptr) {
        return false;
    }
    const auto uploaded = textures_.upload_prepared(*result.image);
    if (uploaded.success || uploaded.skipped_cache_hit) {
        assets_.release_prepared_if_matches(result.key, result.image);
        reclaimer_.enqueue(batch, std::move(result));
        return true;
    }
    assets_.release_prepared_if_matches(result.key, result.image);
    reclaimer_.enqueue(batch, std::move(result));
    return false;
}

// cppcheck-suppress unusedFunction
PreloadResult RenderAssetRuntime::preload_complete(
    const std::vector<ImageAssetKey>&                      keys,
    const std::function<void(const ImageAssetKey&, bool)>& progress_cb) {
    PreloadResult           result;
    AnimationAssetPreloader preloader{assets_, 4};

    result.reclaim_batch = reclaimer_.create_batch();
    auto                             batch = result.reclaim_batch;
    bool                             cleanup_armed = true;
    std::size_t                      processed_count = 0;
    std::vector<PreparedImageResult> bulk_results;
    std::exception_ptr               original_exception;

    auto cleanup_unprocessed = [&]() noexcept {
        for (std::size_t i = processed_count; i < bulk_results.size(); ++i) {
            auto& r = bulk_results[i];
            if (r.success && r.image != nullptr) {
                try {
                    assets_.release_prepared_if_matches(r.key, r.image);
                } catch (...) {
                }
                try {
                    reclaimer_.enqueue(batch, std::move(r));
                } catch (...) {
                }
            }
        }
    };

    auto cleanup = [&] {
        if (!cleanup_armed)
            return;
        cleanup_unprocessed();
        try {
            reclaimer_.seal_batch(batch);
            reclaimer_.activate_batch(batch);
            reclaimer_.drain_and_wait(batch);
        } catch (...) {
            kLog->error("preload_cleanup_failed: reclaim batch seal/activate/drain threw");
        }
    };

    try {
        bulk_results = preloader.prepare_sprites(std::vector<ImageAssetKey>(keys));

        const auto& diag = preloader.last_diagnostics();
        for (; processed_count < bulk_results.size(); ++processed_count) {
            auto&      r = bulk_results[processed_count];
            const auto key = r.key;
            const bool ok = upload_one_and_release(r, batch);
            if (ok) {
                ++result.upload_stats.uploaded;
            } else {
                ++result.upload_stats.failed;
            }
            if (progress_cb) {
                progress_cb(key, ok);
            }
        }

        reclaimer_.seal_batch(batch);
        cleanup_armed = false;

        kLog->info("preload_diagnostics requested={} unique={} composed={} raw={} "
                   "physical_pngs={} phys_ok={} phys_fail={} ok={} fail={} "
                   "deferred={} deferred_mb={:.1f} total_owned_mb={:.1f}",
                   diag.requested_keys, diag.unique_logical_keys, diag.resolved_composed,
                   diag.resolved_raw_png, diag.unique_physical_pngs, diag.physical_decode_successes,
                   diag.physical_decode_failures, result.upload_stats.uploaded,
                   result.upload_stats.failed, reclaimer_.deferred(),
                   static_cast<double>(reclaimer_.deferred_bytes()) / (1024.0 * 1024.0),
                   static_cast<double>(reclaimer_.total_owned_bytes()) / (1024.0 * 1024.0));
    } catch (...) {
        original_exception = std::current_exception();
        cleanup();
        if (original_exception) {
            std::rethrow_exception(original_exception);
        }
    }

    return result;
}

// NOLINTNEXTLINE(performance-unnecessary-value-param)
IncrementalRenderAssetPreload RenderAssetRuntime::begin_incremental_preload(
    std::vector<ImageAssetKey> keys, AssetPriority priority, std::string_view consumer_tag) {
    return IncrementalRenderAssetPreload{*this, std::move(keys), priority, consumer_tag};
}

// NOLINTBEGIN(performance-unnecessary-value-param)
IncrementalRenderAssetPreload::IncrementalRenderAssetPreload(RenderAssetRuntime&        runtime,
                                                             std::vector<ImageAssetKey> keys,
                                                             AssetPriority              priority,
                                                             std::string_view consumer_tag)
    : runtime_(&runtime), reclaim_batch_(runtime.reclaimer_.create_batch()), total_(keys.size()) {
    for (const auto& key : keys) {
        runtime_->textures_.mark_planned(key);
        if (!runtime_->textures_.is_cached(key)) {
            pending_.push_back(runtime_->assets_.request_image(key, priority, consumer_tag));
        }
    }
    stats_.uploaded = total_ - pending_.size();
    stats_.remaining = pending_.size();
    if (pending_.empty()) {
        complete_ = true;
        seal();
    }
}
// NOLINTEND(performance-unnecessary-value-param)

IncrementalRenderAssetPreload::~IncrementalRenderAssetPreload() {
    cancel();
}

// NOLINTNEXTLINE(performance-unnecessary-value-param)
IncrementalRenderAssetPreload::IncrementalRenderAssetPreload(
    IncrementalRenderAssetPreload&& other) noexcept
    : runtime_(std::exchange(other.runtime_, nullptr)), pending_(std::move(other.pending_)),
      stats_(other.stats_), reclaim_batch_(other.reclaim_batch_), total_(other.total_),
      complete_(other.complete_), sealed_(other.sealed_), finished_(other.finished_) {}

IncrementalRenderAssetPreload&
IncrementalRenderAssetPreload::operator=(IncrementalRenderAssetPreload&& other) noexcept {
    if (this == &other) {
        return *this;
    }
    cancel();
    runtime_ = std::exchange(other.runtime_, nullptr);
    pending_ = std::move(other.pending_);
    stats_ = other.stats_;
    reclaim_batch_ = other.reclaim_batch_;
    total_ = other.total_;
    complete_ = other.complete_;
    sealed_ = other.sealed_;
    finished_ = other.finished_;
    return *this;
}

RenderAssetUploadStats IncrementalRenderAssetPreload::progress() const noexcept {
    auto progress = stats_;
    progress.remaining = pending_.size();
    return progress;
}

void IncrementalRenderAssetPreload::advance(RenderAssetUploadBudget budget) {
    if (runtime_ == nullptr || complete_) {
        return;
    }

    const auto        started = std::chrono::steady_clock::now();
    const std::size_t initial = pending_.size();
    std::size_t       scanned = 0;
    std::size_t       processed = 0;
    while (!pending_.empty() && scanned < initial && processed < budget.max_textures) {
        auto handle = std::move(pending_.front());
        pending_.pop_front();
        ++scanned;

        if (runtime_->textures_.is_cached(handle.key())) {
            ++stats_.uploaded;
            continue;
        }
        if (!handle.ready()) {
            pending_.push_back(std::move(handle));
            continue;
        }

        auto result = handle.get();
        if (runtime_->upload_one_and_release(result, reclaim_batch_)) {
            ++stats_.uploaded;
        } else {
            ++stats_.failed;
        }
        ++processed;

        const double elapsed_ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started)
                .count();
        if (elapsed_ms >= budget.max_ms) {
            break;
        }
    }

    stats_.remaining = pending_.size();
    if (pending_.empty()) {
        complete_ = true;
        seal();
    }
}

ReclaimBatch IncrementalRenderAssetPreload::finish() {
    if (!complete_) {
        throw std::logic_error("IncrementalRenderAssetPreload finished before completion");
    }
    if (failed()) {
        throw std::runtime_error("IncrementalRenderAssetPreload completed with failed assets");
    }
    if (finished_) {
        throw std::logic_error("IncrementalRenderAssetPreload finished more than once");
    }
    finished_ = true;
    return reclaim_batch_;
}

void IncrementalRenderAssetPreload::seal() {
    if (sealed_) {
        return;
    }
    runtime_->reclaimer_.seal_batch(reclaim_batch_);
    sealed_ = true;
}

void IncrementalRenderAssetPreload::cancel() noexcept {
    if (runtime_ == nullptr || finished_) {
        return;
    }
    try {
        if (pending_.empty()) {
            seal();
            runtime_->reclaimer_.activate_batch(reclaim_batch_);
            finished_ = true;
            runtime_ = nullptr;
            return;
        }
        runtime_->cancelled_preload_cleanup_.push_back(
            {.pending = std::move(pending_), .reclaim_batch = reclaim_batch_});
        finished_ = true;
    } catch (...) {
        kLog->error("incremental_preload_cleanup_failed");
    }
    runtime_ = nullptr;
}

void RenderAssetRuntime::arm_reclaim_after_next_present(ReclaimBatch batch) {
    for (const auto& b : armed_batches_) {
        if (b == batch)
            return;
    }
    armed_batches_.push_back(batch);
}

// cppcheck-suppress unusedFunction
void RenderAssetRuntime::on_frame_presented() {
    if (armed_batches_.empty())
        return;
    for (auto& batch : armed_batches_)
        reclaimer_.activate_batch(batch);
    last_activate_at_ = std::chrono::steady_clock::now();
    snapshot_1s_done_ = false;
    snapshot_5s_done_ = false;
    armed_batches_.clear();
}

// cppcheck-suppress unusedFunction
void RenderAssetRuntime::pump_reclaim_diagnostics() {
    if (last_activate_at_.time_since_epoch().count() == 0)
        return;
    const auto now = std::chrono::steady_clock::now();
    const auto elapsed_s = std::chrono::duration<double>(now - last_activate_at_).count();

    if (!snapshot_1s_done_ && elapsed_s >= 1.0) {
        snapshot_1s_done_ = true;
        kLog->info("reclaim_snapshot_1s deferred_count={} deferred_mb={} "
                   "outstanding_count={} outstanding_mb={} total_owned_count={} "
                   "total_owned_mb={}",
                   reclaimer_.deferred(), reclaimer_.deferred_bytes() / (1024 * 1024),
                   reclaimer_.outstanding(), reclaimer_.outstanding_bytes() / (1024 * 1024),
                   reclaimer_.total_owned(), reclaimer_.total_owned_bytes() / (1024 * 1024));
    }
    if (!snapshot_5s_done_ && elapsed_s >= 5.0) {
        snapshot_5s_done_ = true;
        kLog->info("reclaim_snapshot_5s deferred_count={} deferred_mb={} "
                   "outstanding_count={} outstanding_mb={} total_owned_count={} "
                   "total_owned_mb={}",
                   reclaimer_.deferred(), reclaimer_.deferred_bytes() / (1024 * 1024),
                   reclaimer_.outstanding(), reclaimer_.outstanding_bytes() / (1024 * 1024),
                   reclaimer_.total_owned(), reclaimer_.total_owned_bytes() / (1024 * 1024));
    }
}

} // namespace d2engine
