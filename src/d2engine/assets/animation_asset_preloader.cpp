#include "animation_asset_preloader.hpp"

#include "d2res/rgba_buffer.hpp"

#include <d2log/log.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <thread>
#include <unordered_map>
#include <unordered_set>

namespace d2engine {

namespace {
auto kLog = d2log::get("d2.assets.preloader"); // NOLINT(cert-err58-cpp)

[[nodiscard]] unsigned default_worker_count() {
    unsigned hw = std::thread::hardware_concurrency();
    return std::max(1u, hw >= 4 ? hw / 2 : 1u);
}

} // namespace

AnimationAssetPreloader::AnimationAssetPreloader(AssetRuntime& assets, unsigned worker_count)
    : assets_(assets), pool_(worker_count > 0 ? worker_count : default_worker_count()) {}

AnimationAssetPreloader::~AnimationAssetPreloader() = default;

PreparedImageResult AnimationAssetPreloader::to_prepared_result(
    const ImageAssetKey& key, std::shared_ptr<const d2res::RgbaBuffer> pixels, double elapsed_ms) {
    auto image = std::make_shared<PreparedImage>(
        PreparedImage{.key = key, .pixels = std::move(pixels), .decode_ms = elapsed_ms});
    return PreparedImageResult{
        .key = key, .image = std::move(image), .success = true, .elapsed_ms = elapsed_ms};
}

PreparedImageResult AnimationAssetPreloader::postprocess_and_wrap(
    const ImageAssetKey& key, std::shared_ptr<const d2res::RgbaBuffer> pixels, double elapsed_ms) {
    if (pixels == nullptr || pixels->rgba.empty() || pixels->width == 0 || pixels->height == 0) {
        PreparedImageResult err;
        err.key = key;
        err.success = false;
        err.error = "empty image";
        err.elapsed_ms = elapsed_ms;
        return err;
    }

    if (key.kind == ImageAssetKind::RawPng) {
        if (has_postprocess(key.postprocess, ImagePostprocess::DetectMagentaBorder) ||
            has_postprocess(key.postprocess, ImagePostprocess::MagentaKey)) {
            auto copy = std::make_shared<d2res::RgbaBuffer>(*pixels);
            bool apply = false;
            if (has_postprocess(key.postprocess, ImagePostprocess::DetectMagentaBorder)) {
                apply = d2res::detect_magenta_key_border(*copy);
            }
            if (apply || has_postprocess(key.postprocess, ImagePostprocess::MagentaKey)) {
                d2res::apply_magenta_key_to_rgba(*copy);
            }
            return to_prepared_result(key, std::move(copy), elapsed_ms);
        }
    }

    return to_prepared_result(key, std::move(pixels), elapsed_ms);
}

std::vector<PreparedImageResult>
AnimationAssetPreloader::prepare_sprites(std::vector<ImageAssetKey> keys) {
    diag_ = Diagnostics{};
    const auto started = std::chrono::steady_clock::now();

    if (keys.empty()) {
        diag_.timing.total_ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started)
                .count();
        return {};
    }

    // ── Step 1: Deduplicate by full ImageAssetKey ──────────────────────
    diag_.requested_keys = keys.size();
    std::unordered_map<ImageAssetKey, std::size_t> seen;
    std::vector<ImageAssetKey>                     unique_keys;
    unique_keys.reserve(keys.size());
    for (auto& key : keys) {
        auto [it, inserted] = seen.emplace(key, unique_keys.size());
        if (inserted) {
            unique_keys.push_back(std::move(key));
        }
    }
    diag_.unique_logical_keys = unique_keys.size();
    diag_.duplicate_keys_removed = diag_.requested_keys - diag_.unique_logical_keys;

    // ── Step 2: Group by container and resolve dependencies ────────────
    const auto resolve_start = std::chrono::steady_clock::now();

    struct ContainerBatch {
        std::string                                                container;
        std::vector<ImageAssetKey>                                 keys;
        std::vector<const FfAssetStore::ResolvedSpriteDependency*> deps; // nullptr = unresolvable
        std::unordered_map<std::string, std::vector<std::size_t>>
                                 physical_to_logical; // phys name → logical indices
        std::vector<std::string> unique_physicals;
    };

    std::unordered_map<std::string, ContainerBatch> by_container;
    for (const auto& key : unique_keys) {
        auto& batch = by_container[key.container_path];
        batch.container = key.container_path;
        batch.keys.push_back(key);
    }

    // Resolve each key and build physical dependency map
    for (auto& [cname, batch] : by_container) {
        batch.deps.reserve(batch.keys.size());
        for (std::size_t i = 0; i < batch.keys.size(); ++i) {
            const auto& key = batch.keys[i];
            if (key.kind == ImageAssetKind::ComposedSprite) {
                auto dep = store().resolve_sprite_fast(cname, key.image_name);
                if (dep.has_value()) {
                    batch.physical_to_logical[dep->physical_record_name].push_back(i);
                    ++diag_.resolved_composed;
                } else {
                    ++diag_.unresolvable;
                }
            } else {
                ++diag_.resolved_raw_png;
            }
        }
        for (const auto& [phys_name, _] : batch.physical_to_logical) {
            batch.unique_physicals.push_back(phys_name);
        }
        diag_.unique_physical_pngs += batch.unique_physicals.size();
    }

    diag_.timing.resolve_ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - resolve_start)
            .count();

    // ── Step 3: Decode unique physical PNGs (parallel) ─────────────────
    const auto phys_decode_start = std::chrono::steady_clock::now();

    struct PhysicalResult {
        std::shared_ptr<const d2res::RgbaBuffer> pixels;
        bool                                     success = false;
        std::string                              error;
    };

    struct PhysDecodeState {
        std::mutex                                           mtx;
        std::unordered_map<PhysicalImageKey, PhysicalResult> results;
        std::atomic<std::size_t>                             success{0};
        std::atomic<std::size_t>                             fail{0};
    };
    auto phys = std::make_shared<PhysDecodeState>();

    for (const auto& [cname, batch] : by_container) {
        for (const auto& phys_name : batch.unique_physicals) {
            PhysicalImageKey phys_key{cname, phys_name};
            pool_.enqueue([this, cname, phys_name, phys_key, phys]() {
                auto           png = store().raw_png(cname, phys_name);
                PhysicalResult pr;
                if (png != nullptr) {
                    pr.pixels = std::move(png);
                    pr.success = true;
                    phys->success.fetch_add(1, std::memory_order_relaxed);
                } else {
                    pr.success = false;
                    pr.error = "physical PNG decode failed: " + phys_name;
                    phys->fail.fetch_add(1, std::memory_order_relaxed);
                }
                {
                    std::lock_guard lock(phys->mtx);
                    phys->results[phys_key] = std::move(pr);
                }
            });
        }
    }

    pool_.wait_idle();
    diag_.physical_decode_successes = phys->success.load(std::memory_order_relaxed);
    diag_.physical_decode_failures = phys->fail.load(std::memory_order_relaxed);

    diag_.timing.dedup_physical_ms = std::chrono::duration<double, std::milli>(
                                         std::chrono::steady_clock::now() - phys_decode_start)
                                         .count();
    diag_.timing.physical_decode_ms = std::chrono::duration<double, std::milli>(
                                          std::chrono::steady_clock::now() - phys_decode_start)
                                          .count();

    // ── Step 4: Compose logical frames (parallel) ─────────────────────
    const auto compose_start = std::chrono::steady_clock::now();

    const std::size_t                result_count = unique_keys.size();
    std::vector<PreparedImageResult> compose_results(result_count);
    // Identity-safe publication tracking: one slot per unique key.
    std::vector<std::shared_ptr<const PreparedImage>> published_images(result_count);
    std::atomic<std::size_t>                          compose_ok{0};
    std::atomic<std::size_t>                          compose_fail{0};

    // Build the key-index lookup for scheduling.
    std::unordered_map<ImageAssetKey, std::size_t> key_index;
    for (std::size_t i = 0; i < unique_keys.size(); ++i)
        key_index[unique_keys[i]] = i;

    // Rollback state: tracks indices whose publication was installed by this operation.
    struct OpRollback {
        std::vector<std::shared_ptr<const PreparedImage>>* pub_images;
        std::vector<ImageAssetKey>*                        keys;
        AssetRuntime*                                      assets;

        void rollback() noexcept {
            for (std::size_t i = 0; i < keys->size(); ++i) {
                if ((*pub_images)[i] != nullptr) {
                    (*assets).release_prepared_if_matches((*keys)[i], (*pub_images)[i]);
                }
            }
        }
    };
    OpRollback rollback{&published_images, &unique_keys, &assets_};

    try {
        for (auto& [cname, batch] : by_container) {
            for (const auto& key : batch.keys) {
                std::string   container = cname;
                ImageAssetKey logical_key = key;
                std::size_t   index = key_index.at(key);
                pool_.enqueue([this, logical_key, container, phys, started, index, &compose_results,
                               &published_images, &compose_ok, &compose_fail]() {
                    const double elapsed = std::chrono::duration<double, std::milli>(
                                               std::chrono::steady_clock::now() - started)
                                               .count();

                    PreparedImageResult result;
                    result.key = logical_key;

                    if (logical_key.kind == ImageAssetKind::RawPng) {
                        auto png = store().raw_png(container, logical_key.image_name);
                        if (png == nullptr) {
                            result.success = false;
                            result.error = "raw_png returned null";
                        } else {
                            result = postprocess_and_wrap(logical_key, std::move(png), elapsed);
                        }
                    } else {
                        auto dep = store().resolve_sprite_fast(container, logical_key.image_name);
                        if (!dep.has_value()) {
                            result.success = false;
                            result.error = "unresolvable sprite";
                        } else {
                            PhysicalImageKey phys_key{container, dep->physical_record_name};
                            std::shared_ptr<const d2res::RgbaBuffer> physical_pixels;
                            {
                                std::lock_guard lock(phys->mtx);
                                auto            pit = phys->results.find(phys_key);
                                if (pit != phys->results.end() && pit->second.success) {
                                    physical_pixels = pit->second.pixels;
                                }
                            }

                            if (physical_pixels == nullptr) {
                                result.success = false;
                                result.error =
                                    "physical PNG not found: " + dep->physical_record_name;
                            } else if (dep->is_raw_png) {
                                result = postprocess_and_wrap(logical_key,
                                                              std::move(physical_pixels), elapsed);
                            } else if (dep->frame != nullptr && dep->block != nullptr) {
                                try {
                                    auto composited =
                                        std::make_shared<d2res::RgbaBuffer>(d2res::compose_image(
                                            *dep->frame, *physical_pixels,
                                            dep->block->transparent_color_index,
                                            dep->block->palette, dep->block->opacity_algorithm));
                                    result = to_prepared_result(logical_key, std::move(composited),
                                                                elapsed);
                                    result.success = true;
                                } catch (const std::exception& e) {
                                    result.success = false;
                                    result.error = std::string("composition failed: ") + e.what();
                                }
                            } else {
                                result.success = false;
                                result.error = "missing frame/block metadata";
                            }
                        }
                    }

                    if (result.success) {
                        try {
                            assets_.publish_prepared(result);
                            published_images[index] = result.image;
                        } catch (...) {
                            result.success = false;
                            result.error = "publish_prepared failed";
                        }
                    }

                    compose_results[index] = std::move(result);

                    if (compose_results[index].success) {
                        compose_ok.fetch_add(1, std::memory_order_relaxed);
                    } else {
                        compose_fail.fetch_add(1, std::memory_order_relaxed);
                    }
                });
            }
        }

        pool_.wait_idle();

        diag_.logical_successes = compose_ok.load(std::memory_order_relaxed);
        diag_.logical_failures = compose_fail.load(std::memory_order_relaxed);

        const double compose_end_ms = std::chrono::duration<double, std::milli>(
                                          std::chrono::steady_clock::now() - compose_start)
                                          .count();
        diag_.timing.logical_compose_ms = compose_end_ms;

        // ── Step 5: Fill remaining diagnostics ─────────────────────────────

        ImageStats istats;
        for (const auto& [cname, batch] : by_container) {
            for (const auto& key : batch.keys) {
                if (key.kind != ImageAssetKind::ComposedSprite)
                    continue;
                auto dep = store().resolve_sprite_fast(cname, key.image_name);
                if (!dep.has_value())
                    continue;
                if (dep->frame == nullptr)
                    continue;

                const auto& frame = *dep->frame;
                istats.total_pieces += frame.pieces.size();
                if (frame.pieces.size() == 1) {
                    ++istats.single_piece_frames;
                    const auto& p = frame.pieces[0];
                    if (p.output_x == 0 && p.output_y == 0 && p.width == frame.output_width &&
                        p.height == frame.output_height) {
                        ++istats.exact_cover_frames;
                    }
                } else {
                    ++istats.multi_piece_frames;
                }

                if (dep->block != nullptr) {
                    const auto algo = dep->block->opacity_algorithm;
                    if (algo == 0) {
                        ++istats.opacity_algo_0;
                    } else if (algo == 300) {
                        ++istats.opacity_algo_300;
                    } else if (algo == 400) {
                        ++istats.opacity_algo_400;
                    } else {
                        ++istats.opacity_algo_other;
                    }
                }

                if (has_postprocess(key.postprocess, ImagePostprocess::DetectMagentaBorder))
                    ++istats.needs_magenta_detect;
                if (has_postprocess(key.postprocess, ImagePostprocess::MagentaKey))
                    ++istats.needs_magenta_apply;

                istats.logical_rgba_bytes += static_cast<std::size_t>(
                    static_cast<int64_t>(frame.output_width) * frame.output_height * 4);
            }
        }

        {
            std::lock_guard lock(phys->mtx);
            for (const auto& [pk, pr] : phys->results) {
                if (pr.success && pr.pixels != nullptr) {
                    istats.physical_rgba_bytes +=
                        static_cast<std::size_t>(pr.pixels->width) * pr.pixels->height * 4;
                }
            }
        }

        diag_.image_stats = istats;
        diag_.timing.total_ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started)
                .count();

        kLog->info("preloader_summary requested={} unique={} deduped={} composed={} raw={} "
                   "unresolvable={} physical_pngs={} phys_ok={} phys_fail={} "
                   "logical_ok={} logical_fail={} "
                   "resolve_ms={:.1f} dedup_phys_ms={:.1f} phys_decode_ms={:.1f} "
                   "compose_ms={:.1f} postprocess_ms={:.1f} pub_ms={:.1f} total_ms={:.1f} "
                   "pieces={} single={} multi={} exact_cover={} "
                   "opacity_algo_0={} algo_300={} algo_400={} algo_other={} "
                   "magenta_detect={} magenta_apply={} "
                   "phys_rgba_mb={:.1f} logical_rgba_mb={:.1f}",
                   diag_.requested_keys, diag_.unique_logical_keys, diag_.duplicate_keys_removed,
                   diag_.resolved_composed, diag_.resolved_raw_png, diag_.unresolvable,
                   diag_.unique_physical_pngs, diag_.physical_decode_successes,
                   diag_.physical_decode_failures, diag_.logical_successes, diag_.logical_failures,
                   diag_.timing.resolve_ms, diag_.timing.dedup_physical_ms,
                   diag_.timing.physical_decode_ms, diag_.timing.logical_compose_ms,
                   diag_.timing.postprocess_ms, diag_.timing.publication_ms, diag_.timing.total_ms,
                   istats.total_pieces, istats.single_piece_frames, istats.multi_piece_frames,
                   istats.exact_cover_frames, istats.opacity_algo_0, istats.opacity_algo_300,
                   istats.opacity_algo_400, istats.opacity_algo_other, istats.needs_magenta_detect,
                   istats.needs_magenta_apply,
                   static_cast<double>(istats.physical_rgba_bytes) / (1024.0 * 1024.0),
                   static_cast<double>(istats.logical_rgba_bytes) / (1024.0 * 1024.0));
    } catch (...) {
        pool_.wait_idle();
        rollback.rollback();
        throw;
    }

    return compose_results;
}

std::vector<PreparedImageResult>
AnimationAssetPreloader::prepare(const std::vector<AnimationRequest>& animation_requests,
                                 std::vector<ImageAssetKey> extra_sprite_keys, ImageAssetKind kind,
                                 ImagePostprocess postprocess) {
    std::vector<ImageAssetKey> all_keys = std::move(extra_sprite_keys);

    for (const auto& req : animation_requests) {
        try {
            const auto seq = assets_.animation_sequence(req.container, req.animation_name);
            for (const auto& frame : seq.frames) {
                all_keys.push_back(ImageAssetKey{.container_path = req.container,
                                                 .image_name = frame.image_name,
                                                 .kind = kind,
                                                 .postprocess = postprocess});
            }
        } catch (const std::exception& e) {
            kLog->warn("animation_expand_failed container={} anim={} error={}", req.container,
                       req.animation_name, e.what());
        }
    }

    return prepare_sprites(std::move(all_keys));
}

} // namespace d2engine
