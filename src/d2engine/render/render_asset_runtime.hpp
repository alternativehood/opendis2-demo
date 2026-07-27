#pragma once

#include "../assets/animation_asset_preloader.hpp"
#include "../assets/asset_runtime.hpp"
#include "../assets/cpu_staging_reclaimer.hpp"
#include "game_texture_cache.hpp"

#include <SDL3/SDL.h>

#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace d2engine {

struct RenderAssetUploadBudget {
    std::size_t max_textures = 4;
    double      max_ms = 2.0;
};

struct RenderAssetUploadStats {
    std::size_t uploaded = 0;
    std::size_t failed = 0;
    std::size_t remaining = 0;
};

struct CancelledPreloadCleanupStats {
    std::size_t pending_handles = 0;
    std::size_t ready_handles = 0;
    std::size_t released_prepared_images = 0;
};

struct RenderAssetRuntimeMemoryStats {
    std::size_t prepared_resident_count = 0;
    std::size_t cleanup_pending_count = 0;
    std::size_t staging_owned_bytes = 0;
    std::size_t reclaim_pending_bytes = 0;
};

struct BulkUploadStats {
    std::size_t uploaded = 0;
    std::size_t skipped = 0;
    std::size_t failed = 0;
};

struct PreloadResult {
    BulkUploadStats upload_stats;
    ReclaimBatch    reclaim_batch;
};

class IncrementalRenderAssetPreload;

class RenderAssetRuntime {
public:
    RenderAssetRuntime(SDL_Renderer* renderer, AssetRuntime& assets);

    [[nodiscard]] GameTextureCache&       textures() noexcept { return textures_; }
    [[nodiscard]] const GameTextureCache& textures() const noexcept { return textures_; }

    [[nodiscard]] ImageRequestHandle request_texture(const ImageAssetKey& key,
                                                     AssetPriority        priority,
                                                     std::string_view     consumer_tag = {});
    [[nodiscard]] AssetBatchHandle   request_textures(const std::vector<ImageAssetKey>& keys,
                                                      AssetPriority                     priority,
                                                      std::string_view consumer_tag = {});

    [[nodiscard]] RenderAssetUploadStats pump_uploads(RenderAssetUploadBudget budget = {});
    [[nodiscard]] RenderAssetUploadStats upload_ready(const AssetBatchHandle& batch);

    [[nodiscard]] bool upload_one_and_release(PreparedImageResult& result,
                                              ReclaimBatch         batch = kReclaimAuto);

    /// Complete bulk preload: fast parallel compose, GPU upload, deferred CPU reclaim.
    /// See preload_complete() for the full contract.
    [[nodiscard]] PreloadResult
    preload_complete(const std::vector<ImageAssetKey>&                      keys,
                     const std::function<void(const ImageAssetKey&, bool)>& progress_cb = {});

    /// Starts an upload session which only consumes asset handles that are already ready.
    /// GPU upload remains on the caller's thread and is bounded by advance().
    [[nodiscard]] IncrementalRenderAssetPreload
    begin_incremental_preload(std::vector<ImageAssetKey> keys, AssetPriority priority,
                              std::string_view consumer_tag);

    /// Arm the given batch for activation on the next call to on_frame_presented().
    /// Multiple batches may be armed before one present — all are activated.
    /// Duplicate batches are ignored.
    void arm_reclaim_after_next_present(ReclaimBatch batch);

    /// Activate all armed reclaim batches. Call from the application render
    /// loop after SDL_RenderPresent.
    void on_frame_presented();

    /// Access read-only reclaimer diagnostics.
    // cppcheck-suppress unusedFunction ; public diagnostics API
    [[nodiscard]] const CpuStagingReclaimer&    reclaimer() const noexcept { return reclaimer_; }
    void                                        wait_for_reclaim_idle();
    [[nodiscard]] CancelledPreloadCleanupStats  cancelled_preload_cleanup_stats() const;
    [[nodiscard]] RenderAssetRuntimeMemoryStats memory_stats() const;

    void pump_reclaim_diagnostics();

private:
    struct CancelledPreloadCleanup {
        std::deque<ImageRequestHandle> pending;
        ReclaimBatch                   reclaim_batch = kReclaimAuto;
    };

    void pump_cancelled_preload_cleanup();

    AssetRuntime&                         assets_;
    GameTextureCache                      textures_;
    std::deque<ImageRequestHandle>        pending_;
    std::deque<CancelledPreloadCleanup>   cancelled_preload_cleanup_;
    std::size_t                           released_cancelled_prepared_images_ = 0;
    CpuStagingReclaimer                   reclaimer_;
    std::vector<ReclaimBatch>             armed_batches_;
    std::chrono::steady_clock::time_point last_snapshot_at_{};
    std::chrono::steady_clock::time_point last_activate_at_{};
    bool                                  snapshot_1s_done_{false};
    bool                                  snapshot_5s_done_{false};

    friend class IncrementalRenderAssetPreload;
};

class IncrementalRenderAssetPreload {
public:
    IncrementalRenderAssetPreload(IncrementalRenderAssetPreload&& other) noexcept;
    IncrementalRenderAssetPreload& operator=(IncrementalRenderAssetPreload&& other) noexcept;
    ~IncrementalRenderAssetPreload();

    IncrementalRenderAssetPreload(const IncrementalRenderAssetPreload&) = delete;
    IncrementalRenderAssetPreload& operator=(const IncrementalRenderAssetPreload&) = delete;

    [[nodiscard]] bool                   complete() const noexcept { return complete_; }
    [[nodiscard]] bool                   failed() const noexcept { return stats_.failed != 0; }
    [[nodiscard]] RenderAssetUploadStats progress() const noexcept;
    void                                 advance(RenderAssetUploadBudget budget);
    [[nodiscard]] ReclaimBatch           finish();

private:
    friend class RenderAssetRuntime;

    // NOLINTNEXTLINE(performance-unnecessary-value-param)
    IncrementalRenderAssetPreload(RenderAssetRuntime& runtime, std::vector<ImageAssetKey> keys,
                                  AssetPriority priority, std::string_view consumer_tag);
    void cancel() noexcept;
    void seal();

    RenderAssetRuntime*            runtime_ = nullptr;
    std::deque<ImageRequestHandle> pending_;
    RenderAssetUploadStats         stats_;
    ReclaimBatch                   reclaim_batch_ = kReclaimAuto;
    std::size_t                    total_ = 0;
    bool                           complete_ = false;
    bool                           sealed_ = false;
    bool                           finished_ = false;
};

} // namespace d2engine
