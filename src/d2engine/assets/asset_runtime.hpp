#pragma once

#include "ff_asset_store.hpp"
#include "image_asset_key.hpp"

#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace d2engine {

enum class AssetPriority : std::uint8_t {
    Background = 0,
    Prefetch = 1,
    Critical = 2,
    Immediate = 3,
};

struct PreparedImage {
    ImageAssetKey                            key;
    std::shared_ptr<const d2res::RgbaBuffer> pixels;
    double                                   decode_ms = 0.0;
};

struct PreparedImageResult {
    ImageAssetKey                        key;
    std::shared_ptr<const PreparedImage> image;
    bool                                 success = false;
    std::string                          error;
    double                               elapsed_ms = 0.0;
};

class ImageRequestHandle {
public:
    ImageRequestHandle() = default;
    ImageRequestHandle(ImageAssetKey key, std::shared_future<PreparedImageResult> future)
        : key_(std::move(key)), future_(std::move(future)) {}

    [[nodiscard]] const ImageAssetKey& key() const noexcept { return key_; }
    [[nodiscard]] bool                 valid() const noexcept { return future_.valid(); }
    [[nodiscard]] bool                 ready() const;
    [[nodiscard]] PreparedImageResult  get() const { return future_.get(); }
    void                               wait() const { future_.wait(); }

private:
    ImageAssetKey                           key_;
    std::shared_future<PreparedImageResult> future_;
};

class AssetBatchHandle {
public:
    void add(ImageRequestHandle handle) { handles_.push_back(std::move(handle)); }
    [[nodiscard]] const std::vector<ImageRequestHandle>& handles() const noexcept {
        return handles_;
    }
    void wait() const;

private:
    std::vector<ImageRequestHandle> handles_;
};

struct AssetRuntimeStats {
    std::uint64_t requests = 0;
    std::uint64_t ready_hits = 0;
    std::uint64_t inflight_joined = 0;
    std::uint64_t queued = 0;
    std::uint64_t decoded = 0;
    std::uint64_t failed = 0;
};

struct PreparedResidentStats {
    std::size_t count = 0;
    std::size_t rgba_bytes = 0;
};

class AssetRuntime {
public:
    using DecodeFn = std::function<PreparedImageResult(const ImageAssetKey&)>;

    explicit AssetRuntime(std::filesystem::path game_root,
                          unsigned              worker_count = std::thread::hardware_concurrency());
    AssetRuntime(DecodeFn decode, unsigned worker_count);
    AssetRuntime(std::unique_ptr<FfAssetStore> store, DecodeFn decode, unsigned worker_count);
    ~AssetRuntime();

    AssetRuntime(const AssetRuntime&) = delete;
    AssetRuntime& operator=(const AssetRuntime&) = delete;
    AssetRuntime(AssetRuntime&&) = delete;
    AssetRuntime& operator=(AssetRuntime&&) = delete;

    [[nodiscard]] ImageRequestHandle request_image(const ImageAssetKey& key, AssetPriority priority,
                                                   std::string_view consumer_tag = {});
    [[nodiscard]] AssetBatchHandle   request_batch(const std::vector<ImageAssetKey>& keys,
                                                   AssetPriority                     priority,
                                                   std::string_view consumer_tag = {});

    [[nodiscard]] AnimationSequence animation_sequence(std::string_view container,
                                                       std::string_view animation) const;

    [[nodiscard]] const IImageStore&    image_store() const;
    [[nodiscard]] FfAssetStore&         store();
    [[nodiscard]] const FfAssetStore&   store() const;
    [[nodiscard]] AssetRuntimeStats     stats() const;
    [[nodiscard]] PreparedResidentStats prepared_resident_stats() const;
    void                                release_prepared(const ImageAssetKey& key);

    void release_prepared_if_matches(const ImageAssetKey&                        key,
                                     const std::shared_ptr<const PreparedImage>& expected_image);

    // Publish a pre-decoded image into shared state so that future
    // request_image() / already-in-flight requests find it ready.
    // Used by AnimationAssetPreloader for bulk preparation integration.
    void publish_prepared(const PreparedImageResult& result);

private:
    enum class RequestStatus : std::uint8_t {
        Queued,
        Decoding,
        Ready,
        Failed,
    };

    struct RequestState {
        RequestStatus                                      status = RequestStatus::Queued;
        AssetPriority                                      priority = AssetPriority::Background;
        std::shared_ptr<std::promise<PreparedImageResult>> promise;
        std::shared_future<PreparedImageResult>            future;
        std::shared_ptr<const PreparedImage>               ready_image;
        std::uint64_t                                      generation = 0;
    };

    struct QueuedJob {
        ImageAssetKey key;
        AssetPriority priority = AssetPriority::Background;
        std::uint64_t sequence = 0;
        std::uint64_t generation = 0;
    };

    struct JobLess {
        [[nodiscard]] bool operator()(const QueuedJob& lhs, const QueuedJob& rhs) const noexcept;
    };

    void                              start_workers(unsigned count);
    void                              worker_loop();
    [[nodiscard]] PreparedImageResult decode_image(const ImageAssetKey& key) const;

    std::unique_ptr<FfAssetStore> store_;
    DecodeFn                      decode_;

    mutable std::mutex                                               mtx_;
    std::condition_variable                                          cv_;
    bool                                                             stopping_ = false;
    std::uint64_t                                                    next_sequence_ = 0;
    std::uint64_t                                                    next_generation_ = 0;
    std::priority_queue<QueuedJob, std::vector<QueuedJob>, JobLess>  queue_;
    std::unordered_map<ImageAssetKey, std::shared_ptr<RequestState>> states_;
    std::vector<std::thread>                                         workers_;
    AssetRuntimeStats                                                stats_;
};

} // namespace d2engine
