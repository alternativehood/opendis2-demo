#include "landmark_asset_catalog_builder.hpp"

#include "detail/landmark_asset_resolution.hpp"
#include "ff_asset_store.hpp"

#include <d2adventure_render/terrain/landmark_asset_catalog.hpp>
#include <d2log/log.hpp>

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace d2engine {

namespace {

auto kLog = d2log::get("d2.lmk"); // NOLINT

} // namespace

adventure_render::LandmarkAssetCatalog build_landmark_asset_catalog(const FfAssetStore& store) {
    adventure_render::LandmarkAssetCatalog catalog;

    const auto containers = store.containers();

    std::unordered_map<std::string, std::vector<detail::LandmarkAssetCandidate>> candidates;

    for (const auto& container : containers) {
        // sprites_in throws on corrupt metadata — fail fast, do not silently skip.
        const std::vector<std::string> sprites = store.sprites_in(container);

        // animations_in_if_present: silently returns empty for missing -ANIMS.OPT
        // (normal absence); throws on corrupt data.
        const std::vector<std::string> anims = store.animations_in_if_present(container);

        for (const auto& raw_name : sprites) {
            const auto canonical = detail::canonical_landmark_type_id(raw_name);
            if (!detail::is_landmark_global_id(canonical))
                continue;
            candidates[canonical].push_back({.container_path = container,
                                             .logical_name = raw_name,
                                             .kind = detail::LandmarkAssetKind::StaticSprite});
        }
        for (const auto& raw_name : anims) {
            const auto canonical = detail::canonical_landmark_type_id(raw_name);
            if (!detail::is_landmark_global_id(canonical))
                continue;
            candidates[canonical].push_back({.container_path = container,
                                             .logical_name = raw_name,
                                             .kind = detail::LandmarkAssetKind::Animation});
        }
    }

    for (auto& [key, entries] : candidates) {
        // Deduplicate: (container, logical_name, kind) tuples.
        std::unordered_set<std::string>             seen;
        std::vector<detail::LandmarkAssetCandidate> unique;
        unique.reserve(entries.size());
        for (auto& e : entries) {
            const std::string cid = e.container_path + "/" + e.logical_name + "/" +
                                    (e.kind == detail::LandmarkAssetKind::Animation ? "A" : "S");
            if (seen.insert(cid).second)
                unique.push_back(std::move(e));
        }

        const auto resolved = detail::resolve_landmark_candidates(unique);
        if (!resolved.has_value()) {
            std::size_t anim_n = 0;
            std::size_t sprite_n = 0;
            for (const auto& e : unique) {
                if (e.kind == detail::LandmarkAssetKind::Animation) {
                    ++anim_n;
                } else {
                    ++sprite_n;
                }
            }
            if (anim_n > 1 || sprite_n > 1) {
                kLog->warn("landmark_asset_ambiguous type={} anim_candidates={} "
                           "sprite_candidates={}",
                           key, anim_n, sprite_n);
            }
            continue;
        }

        if (resolved->kind == detail::LandmarkAssetKind::StaticSprite) {
            adventure_render::StaticLandmarkVisual visual;
            visual.container_path = resolved->container_path;
            visual.logical_sprite = resolved->logical_name;

            // sprite_metadata throws on corrupt metadata — fail fast.
            const auto meta =
                store.sprite_metadata(resolved->container_path, resolved->logical_name);
            visual.width = meta.canvas_width;
            visual.height = meta.canvas_height;
            visual.canvas_foot_x = meta.canvas_foot_x;
            visual.canvas_foot_y = meta.canvas_foot_y;

            catalog.visuals.emplace(key, std::move(visual));
        } else {
            adventure_render::AnimatedLandmarkVisual visual;
            visual.container_path = resolved->container_path;
            visual.logical_animation = resolved->logical_name;

            // animation_metadata throws on corrupt metadata — fail fast.
            const auto meta =
                store.animation_metadata(resolved->container_path, resolved->logical_name);

            if (meta.frames.empty()) {
                kLog->warn("landmark_anim_zero_frames container={} name={}",
                           resolved->container_path, resolved->logical_name);
                continue;
            }

            visual.canvas_foot_x = meta.canvas_foot_x;
            visual.canvas_foot_y = meta.canvas_foot_y;

            visual.animation_data.animation_name = resolved->logical_name;
            visual.animation_data.native_canvas_w = meta.native_canvas_w;
            visual.animation_data.native_canvas_h = meta.native_canvas_h;
            visual.animation_data.is_looping = true;
            visual.animation_data.frames.reserve(meta.frames.size());
            for (const auto& frame : meta.frames) {
                // Per-frame geometry from the authoritative sprite metadata.
                adventure_render::AdventureAnimationFrame af;
                af.record_name = frame.image_name;
                af.duration_ms = static_cast<int>(frame.duration_ms);
                const auto fm = store.sprite_metadata(resolved->container_path, frame.image_name);
                af.canvas_width = fm.canvas_width;
                af.canvas_height = fm.canvas_height;
                visual.animation_data.frames.push_back(std::move(af));
            }

            catalog.visuals.emplace(key, std::move(visual));
        }
    }

    return catalog;
}

} // namespace d2engine
