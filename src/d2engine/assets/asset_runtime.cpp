#include "asset_runtime.hpp"

#include "d2res/rgba_buffer.hpp"

#include <d2log/log.hpp>

#include <algorithm>
#include <chrono>
#include <exception>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace d2engine {

namespace {

auto kLog = d2log::get("d2.assets.runtime"); // NOLINT(cert-err58-cpp)

[[nodiscard]] unsigned normalize_worker_count(unsigned requested) {
    return std::max(1u, requested);
}

[[nodiscard]] int priority_rank(AssetPriority priority) noexcept {
    return static_cast<int>(priority);
}

} // namespace

bool ImageRequestHandle::ready() const {
    return future_.valid() &&
           future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

void AssetBatchHandle::wait() const {
    for (const auto& handle : handles_) {
        handle.wait();
    }
}

AssetRuntime::AssetRuntime(std::filesystem::path game_root, unsigned worker_count)
    : store_(std::make_unique<FfAssetStore>(std::move(game_root))) {
    start_workers(normalize_worker_count(worker_count));
}

AssetRuntime::AssetRuntime(DecodeFn decode, unsigned worker_count) : decode_(std::move(decode)) {
    if (!decode_) {
        throw std::invalid_argument("AssetRuntime decode callback is empty");
    }
    start_workers(normalize_worker_count(worker_count));
}

AssetRuntime::AssetRuntime(std::unique_ptr<FfAssetStore> store, DecodeFn decode,
                           unsigned worker_count)
    : store_(std::move(store)), decode_(std::move(decode)) {
    if (!store_) {
        throw std::invalid_argument("AssetRuntime store is empty");
    }
    if (!decode_) {
        throw std::invalid_argument("AssetRuntime decode callback is empty");
    }
    start_workers(normalize_worker_count(worker_count));
}

AssetRuntime::~AssetRuntime() {
    const auto shutdown_start = std::chrono::steady_clock::now();
    {
        std::lock_guard lock(mtx_);
        stopping_ = true;
    }
    cv_.notify_all();
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    const auto worker_join_ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - shutdown_start)
            .count();

    const std::size_t states_count = states_.size();
    std::size_t       states_bytes = 0;
    for (const auto& [k, s] : states_) {
        if (s->ready_image && s->ready_image->pixels) {
            states_bytes += s->ready_image->pixels->rgba.size();
        }
    }

    {
        std::lock_guard lock(mtx_);
        states_.clear();
    }

    const auto clear_ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - shutdown_start)
            .count();

    kLog->debug("shutdown_timing stage=asset_runtime_worker_join duration_ms={:.1f}",
                worker_join_ms);
    kLog->debug("shutdown_timing stage=asset_runtime_prepared_clear count={} rgba_bytes={} "
                "rgba_mb={:.1f} total_duration_ms={:.1f}",
                states_count, states_bytes, static_cast<double>(states_bytes) / (1024.0 * 1024.0),
                clear_ms);
}

ImageRequestHandle AssetRuntime::request_image(const ImageAssetKey& key, AssetPriority priority,
                                               std::string_view consumer_tag) {
    std::lock_guard lock(mtx_);
    ++stats_.requests;

    auto it = states_.find(key);
    if (it != states_.end()) {
        auto& state = *it->second;
        if (state.status == RequestStatus::Ready || state.status == RequestStatus::Failed) {
            ++stats_.ready_hits;
            return ImageRequestHandle{key, state.future};
        }
        ++stats_.inflight_joined;
        if (priority_rank(priority) > priority_rank(state.priority) &&
            state.status == RequestStatus::Queued) {
            state.priority = priority;
            state.generation = ++next_generation_;
            queue_.push(QueuedJob{.key = key,
                                  .priority = priority,
                                  .sequence = next_sequence_++,
                                  .generation = state.generation});
            cv_.notify_one();
        }
        return ImageRequestHandle{key, state.future};
    }

    auto state = std::make_shared<RequestState>();
    state->priority = priority;
    state->promise = std::make_shared<std::promise<PreparedImageResult>>();
    state->future = state->promise->get_future().share();
    state->generation = ++next_generation_;
    states_.emplace(key, state);
    queue_.push(QueuedJob{.key = key,
                          .priority = priority,
                          .sequence = next_sequence_++,
                          .generation = state->generation});
    ++stats_.queued;
    kLog->debug("asset_batch consumer={} jobs=1 cache_ready=0 inflight_joined=0 queued=1",
                consumer_tag);
    cv_.notify_one();
    return ImageRequestHandle{key, state->future};
}

AssetBatchHandle AssetRuntime::request_batch(const std::vector<ImageAssetKey>& keys,
                                             AssetPriority                     priority,
                                             std::string_view                  consumer_tag) {
    AssetBatchHandle batch;
    std::uint64_t    ready_hits = 0;
    std::uint64_t    inflight_joined = 0;
    std::uint64_t    queued = 0;
    for (const auto& key : keys) {
        const auto before = stats();
        batch.add(request_image(key, priority, consumer_tag));
        const auto after = stats();
        ready_hits += after.ready_hits - before.ready_hits;
        inflight_joined += after.inflight_joined - before.inflight_joined;
        queued += after.queued - before.queued;
    }
    kLog->info("asset_batch consumer={} jobs={} cache_ready={} inflight_joined={} queued={}",
               consumer_tag, keys.size(), ready_hits, inflight_joined, queued);
    return batch;
}

AnimationSequence AssetRuntime::animation_sequence(std::string_view container,
                                                   std::string_view animation) const {
    if (!store_) {
        throw std::runtime_error("AssetRuntime has no FfAssetStore");
    }
    return store_->animation_metadata(container, animation);
}

// cppcheck-suppress unusedFunction
const IImageStore& AssetRuntime::image_store() const {
    if (!store_) {
        throw std::runtime_error("AssetRuntime has no FfAssetStore");
    }
    return *store_;
}

FfAssetStore& AssetRuntime::store() {
    if (!store_) {
        throw std::runtime_error("AssetRuntime has no FfAssetStore");
    }
    return *store_;
}

const FfAssetStore& AssetRuntime::store() const {
    if (!store_) {
        throw std::runtime_error("AssetRuntime has no FfAssetStore");
    }
    return *store_;
}

AssetRuntimeStats AssetRuntime::stats() const {
    std::lock_guard lock(mtx_);
    return stats_;
}

PreparedResidentStats AssetRuntime::prepared_resident_stats() const {
    std::lock_guard       lock(mtx_);
    PreparedResidentStats s;
    for (const auto& [k, state] : states_) {
        if (state->status == RequestStatus::Ready && state->ready_image &&
            state->ready_image->pixels) {
            ++s.count;
            s.rgba_bytes += state->ready_image->pixels->rgba.size();
        }
    }
    return s;
}

void AssetRuntime::release_prepared(const ImageAssetKey& key) {
    std::lock_guard lock(mtx_);
    auto            it = states_.find(key);
    if (it != states_.end() && it->second->status == RequestStatus::Ready) {
        states_.erase(it);
    }
}

void AssetRuntime::release_prepared_if_matches(
    const ImageAssetKey& key, const std::shared_ptr<const PreparedImage>& expected_image) {
    std::lock_guard lock(mtx_);
    auto            it = states_.find(key);
    if (it == states_.end())
        return;
    auto& state = *it->second;
    if (state.status != RequestStatus::Ready)
        return;
    if (state.ready_image != expected_image)
        return;
    states_.erase(it);
}

void AssetRuntime::publish_prepared(const PreparedImageResult& result) {
    std::lock_guard lock(mtx_);
    auto            it = states_.find(result.key);
    if (it != states_.end()) {
        // Already in-flight; do not overwrite — the worker will deliver.
        // However, if the existing state is still queued, we can upgrade it
        // to ready so that waiters get the pre-decoded result.
        auto& state = *it->second;
        if (state.status == RequestStatus::Queued || state.status == RequestStatus::Decoding) {
            // Another thread is already decoding; our publish is redundant.
            // The worker will produce the same result.
            return;
        }
        if (state.status == RequestStatus::Ready && state.ready_image != nullptr) {
            // Already ready — no-op.
            return;
        }
    }

    auto state = std::make_shared<RequestState>();
    state->status = RequestStatus::Ready;
    state->ready_image = result.image;
    state->promise = std::make_shared<std::promise<PreparedImageResult>>();
    state->future = state->promise->get_future().share();
    state->promise->set_value(result);
    states_.emplace(result.key, std::move(state));
}

bool AssetRuntime::JobLess::operator()(const QueuedJob& lhs, const QueuedJob& rhs) const noexcept {
    const int lhs_rank = priority_rank(lhs.priority);
    const int rhs_rank = priority_rank(rhs.priority);
    if (lhs_rank != rhs_rank) {
        return lhs_rank < rhs_rank;
    }
    return lhs.sequence > rhs.sequence;
}

void AssetRuntime::start_workers(unsigned count) {
    workers_.reserve(count);
    for (unsigned i = 0; i < count; ++i) {
        workers_.emplace_back([this] { worker_loop(); });
    }
    kLog->info("asset_runtime_started workers={}", count);
}

void AssetRuntime::worker_loop() {
    for (;;) {
        QueuedJob                     job;
        std::shared_ptr<RequestState> state;
        {
            std::unique_lock lock(mtx_);
            cv_.wait(lock, [&] { return stopping_ || !queue_.empty(); });
            if (stopping_ && queue_.empty()) {
                return;
            }
            job = queue_.top();
            queue_.pop();
            auto it = states_.find(job.key);
            if (it == states_.end()) {
                continue;
            }
            state = it->second;
            if (state->status != RequestStatus::Queued || state->generation != job.generation ||
                state->priority != job.priority) {
                continue;
            }
            state->status = RequestStatus::Decoding;
        }

        auto result = decode_image(job.key);
        {
            std::lock_guard lock(mtx_);
            auto            it = states_.find(job.key);
            if (it != states_.end() && it->second == state) {
                state->ready_image = result.image;
                state->status = result.success ? RequestStatus::Ready : RequestStatus::Failed;
            }
            if (result.success) {
                ++stats_.decoded;
            } else {
                ++stats_.failed;
            }
        }
        state->promise->set_value(std::move(result));
    }
}

PreparedImageResult AssetRuntime::decode_image(const ImageAssetKey& key) const {
    if (decode_) {
        return decode_(key);
    }

    const auto          started = std::chrono::steady_clock::now();
    PreparedImageResult result{.key = key};
    try {
        std::shared_ptr<const d2res::RgbaBuffer> pixels;
        if (key.kind == ImageAssetKind::RawPng) {
            pixels = store().raw_png(key.container_path, key.image_name);
            if (pixels != nullptr &&
                has_postprocess(key.postprocess, ImagePostprocess::MagentaKey)) {
                auto copy = std::make_shared<d2res::RgbaBuffer>(*pixels);
                d2res::apply_magenta_key_to_rgba(*copy);
                pixels = std::move(copy);
            }
        } else {
            auto decoded = std::make_shared<d2res::RgbaBuffer>(
                store().decode_sprite(key.container_path, key.image_name));
            if (has_postprocess(key.postprocess, ImagePostprocess::DetectMagentaBorder) &&
                d2res::detect_magenta_key_border(*decoded)) {
                d2res::apply_magenta_key_to_rgba(*decoded);
            }
            if (has_postprocess(key.postprocess, ImagePostprocess::MagentaKey)) {
                d2res::apply_magenta_key_to_rgba(*decoded);
            }
            pixels = std::move(decoded);
        }

        if (pixels == nullptr || pixels->rgba.empty() || pixels->width == 0 ||
            pixels->height == 0) {
            result.error = "decoded empty image: " + to_string(key);
        } else {
            const double elapsed_ms = std::chrono::duration<double, std::milli>(
                                          std::chrono::steady_clock::now() - started)
                                          .count();
            result.image = std::make_shared<PreparedImage>(
                PreparedImage{.key = key, .pixels = std::move(pixels), .decode_ms = elapsed_ms});
            result.success = true;
            result.elapsed_ms = elapsed_ms;
        }
    } catch (const std::exception& e) {
        std::ostringstream msg;
        msg << "asset decode failed key=" << to_string(key) << " error=" << e.what();
        result.error = msg.str();
    }
    if (result.elapsed_ms == 0.0) {
        result.elapsed_ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started)
                .count();
    }
    return result;
}

} // namespace d2engine
