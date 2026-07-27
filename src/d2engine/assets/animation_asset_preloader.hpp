#pragma once

#include "asset_runtime.hpp"
#include "bounded_worker_pool.hpp"
#include "ff_asset_store.hpp"
#include "image_asset_key.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace d2engine {

/// Generic reusable component responsible for efficient animation/image
/// resource preparation. Game modes (Battle, Adventure, Menu) declare
/// WHICH logical animations/images they need; this component handles HOW
/// they are physically decoded, deduplicated, and composed.
///
/// Ownership boundaries (no Battle-specific types or logic here):
///   - This component owns bulk preparation orchestration.
///   - FF physical dependency resolution lives in FfAssetStore.
///   - Published results integrate with AssetRuntime's shared in-flight
///     state so single-request consumers share the same prepared images.
///   - GPU upload is handled separately by RenderAssetRuntime.
class AnimationAssetPreloader {
public:
    explicit AnimationAssetPreloader(AssetRuntime& assets, unsigned worker_count = 0);

    AnimationAssetPreloader(const AnimationAssetPreloader&) = delete;
    AnimationAssetPreloader& operator=(const AnimationAssetPreloader&) = delete;
    AnimationAssetPreloader(AnimationAssetPreloader&&) = delete;
    AnimationAssetPreloader& operator=(AnimationAssetPreloader&&) = delete;

    ~AnimationAssetPreloader();

    /// Request to expand an animation to its frame names.
    struct AnimationRequest {
        std::string container;
        std::string animation_name;
    };

    /// Structured per-stage timing breakdown.
    struct StageTiming {
        double resolve_ms = 0.0;         // logical → physical dependency resolution
        double dedup_physical_ms = 0.0;  // unique physical PNG identification
        double physical_decode_ms = 0.0; // lodepng inflate (aggregate wall)
        double logical_compose_ms = 0.0; // compose_image calls (aggregate wall)
        double postprocess_ms = 0.0;     // postprocess_and_wrap (aggregate wall)
        double publication_ms = 0.0;     // publish_prepared calls
        double total_ms = 0.0;
    };

    /// Image piece / transparency statistics.
    struct ImageStats {
        std::size_t total_pieces = 0;
        std::size_t single_piece_frames = 0;
        std::size_t multi_piece_frames = 0;
        std::size_t exact_cover_frames = 0; // single piece at (0,0) matching output

        std::size_t opacity_algo_0 = 0;
        std::size_t opacity_algo_300 = 0;
        std::size_t opacity_algo_400 = 0;
        std::size_t opacity_algo_other = 0;

        std::size_t needs_magenta_detect = 0; // frames requiring border heuristic
        std::size_t needs_magenta_apply = 0;  // frames needing actual magenta cleanup

        std::size_t physical_rgba_bytes = 0; // total decoded physical RGBA
        std::size_t logical_rgba_bytes = 0;  // total materialized logical RGBA
    };

    /// Detailed diagnostics from a preparation run.
    struct Diagnostics {
        std::size_t requested_keys = 0;
        std::size_t unique_logical_keys = 0;
        std::size_t duplicate_keys_removed = 0;
        std::size_t resolved_composed = 0;
        std::size_t resolved_raw_png = 0;
        std::size_t unresolvable = 0;
        std::size_t unique_physical_pngs = 0;
        std::size_t physical_decode_successes = 0;
        std::size_t physical_decode_failures = 0;
        std::size_t logical_successes = 0;
        std::size_t logical_failures = 0;
        StageTiming timing;
        ImageStats  image_stats;
    };

    /// Prepare individual sprite keys (no animation expansion).
    /// Deduplicates by full ImageAssetKey before any processing.
    /// Publishes results into AssetRuntime's shared state.
    /// Parallelizes physical PNG decode and logical frame composition
    /// using the internal bounded worker pool.
    [[nodiscard]] std::vector<PreparedImageResult> prepare_sprites(std::vector<ImageAssetKey> keys);

    /// Combined: expand animations + add explicit sprite keys.
    /// Deduplicates overlapping keys between the two sets.
    [[nodiscard]] std::vector<PreparedImageResult>
    prepare(const std::vector<AnimationRequest>& animation_requests,
            std::vector<ImageAssetKey>           extra_sprite_keys,
            ImageAssetKind                       kind = ImageAssetKind::ComposedSprite,
            ImagePostprocess postprocess = ImagePostprocess::DetectMagentaBorder);

    /// Access the underlying store.
    [[nodiscard]] const FfAssetStore& store() const { return assets_.store(); }
    [[nodiscard]] const Diagnostics&  last_diagnostics() const noexcept { return diag_; }

private:
    AssetRuntime& assets_;
    Diagnostics   diag_;

    BoundedWorkerPool pool_;

    [[nodiscard]] static PreparedImageResult
    to_prepared_result(const ImageAssetKey& key, std::shared_ptr<const d2res::RgbaBuffer> pixels,
                       double elapsed_ms);

    [[nodiscard]] static PreparedImageResult
    postprocess_and_wrap(const ImageAssetKey& key, std::shared_ptr<const d2res::RgbaBuffer> pixels,
                         double elapsed_ms);
};

} // namespace d2engine
