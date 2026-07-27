#include "adventure_visual_resources.hpp"

#include "../assets/asset_runtime.hpp"
#include "../assets/ff_asset_store.hpp"
#include "../render/render_asset_runtime.hpp"
#include "../assets/image_asset_key.hpp"

#include <d2log/log.hpp>
#include <d2res/rgba_buffer.hpp>

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace d2engine {

namespace {

auto kLog = d2log::get("d2.vis"); // NOLINT

SelectVisual load_select(FfAssetStore& store, std::string_view name) {
    auto         meta = store.sprite_metadata("Imgs/IsoCmon.ff", name);
    SelectVisual sv;
    sv.key = {.container_path = "Imgs/IsoCmon.ff",
              .image_name = std::string(name),
              .kind = ImageAssetKind::ComposedSprite};
    sv.canvas_foot_x = static_cast<int>(meta.canvas_foot_x);
    sv.canvas_foot_y = static_cast<int>(meta.canvas_foot_y);
    sv.src_width = static_cast<int>(meta.canvas_width);
    sv.src_height = static_cast<int>(meta.canvas_height);
    return sv;
}

AdventureAnimatedVisual
build_route_preview_visual_contract(const AnimationSequence&                   sequence,
                                    std::string_view                           container_path,
                                    const AdventureRoutePreviewVisualContract& contract) {
    if (sequence.frames.size() != contract.expected_frame_count ||
        sequence.native_canvas_w != contract.expected_width ||
        sequence.native_canvas_h != contract.expected_height) {
        throw std::runtime_error(
            "route preview animation contract mismatch container=" + std::string(container_path) +
            " animation=" + std::string(contract.animation_name) +
            " actual_frames=" + std::to_string(sequence.frames.size()) +
            " expected_frames=" + std::to_string(contract.expected_frame_count) +
            " actual_dimensions=" + std::to_string(sequence.native_canvas_w) + "x" +
            std::to_string(sequence.native_canvas_h) +
            " expected_dimensions=" + std::to_string(contract.expected_width) + "x" +
            std::to_string(contract.expected_height));
    }
    AdventureAnimatedVisual visual;
    visual.container_path = std::string(container_path);
    visual.semantic_anchor_x = contract.semantic_anchor_x;
    visual.semantic_anchor_y = contract.semantic_anchor_y;
    visual.src_width = contract.expected_width;
    visual.src_height = contract.expected_height;
    visual.animation.animation_name = std::string(contract.animation_name);
    visual.animation.native_canvas_w = contract.expected_width;
    visual.animation.native_canvas_h = contract.expected_height;
    visual.animation.is_looping = AdventureRoutePreviewPlaybackPolicy::is_looping;
    visual.animation.timing_source = AdventureRoutePreviewPlaybackPolicy::timing_source;
    visual.animation.frames.reserve(sequence.frames.size());
    for (const auto& frame : sequence.frames) {
        if (frame.image_name.empty()) {
            throw std::runtime_error("route preview animation has empty frame record name "
                                     "container=" +
                                     std::string(container_path) +
                                     " animation=" + std::string(contract.animation_name));
        }
        visual.animation.frames.push_back({frame.image_name,
                                           AdventureRoutePreviewPlaybackPolicy::frame_duration_ms,
                                           contract.expected_width,
                                           contract.expected_height,
                                           {}});
    }
    return visual;
}

AdventureAnimatedVisual load_route_visual(AssetRuntime&                              assets,
                                          const AdventureRoutePreviewVisualContract& contract) {
    constexpr std::string_view container = "Imgs/IsoCmon.ff";
    return build_route_preview_visual_contract(
        assets.animation_sequence(container, contract.animation_name), container, contract);
}

// Decode logical sprite → RGBA, then alpha-crop and create SDL_Cursor.
// Hotspot is centre of logical canvas.  After crop, hotspot is adjusted.
static SDL_Cursor* create_cursor_from_sprite(FfAssetStore& store, std::string_view name) {
    auto      rgba = store.decode_sprite("Imgs/IsoCursr.ff", name);
    const int full_w = static_cast<int>(rgba.width);
    const int full_h = static_cast<int>(rgba.height);
    const int logical_hotspot_x = full_w / 2;
    const int logical_hotspot_y = full_h / 2;

    const uint8_t* src = rgba.rgba.data();
    const int      row_bytes = full_w * 4;

    int crop_min_x = full_w;
    int crop_min_y = full_h;
    int crop_max_x = 0;
    int crop_max_y = 0;

    for (int y = 0; y < full_h; ++y) {
        for (int x = 0; x < full_w; ++x) {
            const uint8_t a = src[static_cast<std::size_t>(y * row_bytes + x * 4 + 3)];
            if (a != 0) {
                crop_min_x = std::min(crop_min_x, x);
                crop_min_y = std::min(crop_min_y, y);
                crop_max_x = std::max(crop_max_x, x + 1);
                crop_max_y = std::max(crop_max_y, y + 1);
            }
        }
    }

    if (crop_max_x <= crop_min_x || crop_max_y <= crop_min_y) {
        kLog->error("cursor_alpha_bbox_empty cursor={}", name);
        return nullptr;
    }

    const int crop_w = crop_max_x - crop_min_x;
    const int crop_h = crop_max_y - crop_min_y;
    const int cropped_hot_x = logical_hotspot_x - crop_min_x;
    const int cropped_hot_y = logical_hotspot_y - crop_min_y;

    SDL_Surface* surface = SDL_CreateSurface(crop_w, crop_h, SDL_PIXELFORMAT_ARGB8888);
    if (!surface) {
        kLog->error("cursor_surface_failed cursor={} error={}", name, SDL_GetError());
        return nullptr;
    }

    SDL_LockSurface(surface);
    uint8_t*  dst = static_cast<uint8_t*>(surface->pixels);
    const int dst_pitch = surface->pitch;

    for (int y = 0; y < crop_h; ++y) {
        for (int x = 0; x < crop_w; ++x) {
            const int         sx = crop_min_x + x;
            const int         sy = crop_min_y + y;
            const std::size_t src_idx = static_cast<std::size_t>(sy * row_bytes + sx * 4);
            const uint8_t     r = src[src_idx + 0];
            const uint8_t     g = src[src_idx + 1];
            const uint8_t     b = src[src_idx + 2];
            const uint8_t     a = src[src_idx + 3];
            // ARGB8888: A in byte 0 (SDL endian-independent)
            const std::size_t dst_idx = static_cast<std::size_t>(y * dst_pitch + x * 4);
            dst[dst_idx + 0] = b;
            dst[dst_idx + 1] = g;
            dst[dst_idx + 2] = r;
            dst[dst_idx + 3] = a;
        }
    }
    SDL_UnlockSurface(surface);

    SDL_Cursor* cursor = SDL_CreateColorCursor(surface, cropped_hot_x, cropped_hot_y);
    SDL_DestroySurface(surface);

    if (!cursor) {
        kLog->error("cursor_create_failed cursor={} error={}", name, SDL_GetError());
        return nullptr;
    }

    kLog->info("cursor_created cursor={} logical={}x{} crop={}..{}..{}..{} hotspot={}->{}", name,
               full_w, full_h, crop_min_x, crop_min_y, crop_max_x, crop_max_y, logical_hotspot_x,
               cropped_hot_x);
    return cursor;
}

} // namespace

AdventureAnimatedVisual
build_adventure_route_preview_visual(const AnimationSequence&                   sequence,
                                     std::string_view                           container_path,
                                     const AdventureRoutePreviewVisualContract& contract) {
    return build_route_preview_visual_contract(sequence, container_path, contract);
}

AdventureVisualResourcesLoader::AdventureVisualResourcesLoader(AssetRuntime&       assets,
                                                               RenderAssetRuntime& render_assets)
    : assets_(assets), render_assets_(render_assets) {}

LoadedAdventureVisualResources AdventureVisualResourcesLoader::load() {
    auto&                    store = assets_.store();
    constexpr int            kExpected = 8;
    std::vector<std::string> missing;

    LoadedAdventureVisualResources result;

    std::string cursor_error;
    try {
        result.cursors.default_cursor = create_cursor_from_sprite(store, "DEFAULT");
    } catch (const std::exception& e) {
        cursor_error = e.what();
    }
    if (!result.cursors.default_cursor) {
        missing.push_back(std::string("IsoCursr.ff/DEFAULT") +
                          (cursor_error.empty() ? "" : ": " + cursor_error));
    }

    cursor_error.clear();
    try {
        result.cursors.select_unit = create_cursor_from_sprite(store, "SELECTUNIT");
    } catch (const std::exception& e) {
        cursor_error = e.what();
    }
    if (!result.cursors.select_unit) {
        missing.push_back(std::string("IsoCursr.ff/SELECTUNIT") +
                          (cursor_error.empty() ? "" : ": " + cursor_error));
    }

    try {
        result.world.select_selected = load_select(store, "SELECTED");
    } catch (const std::exception& e) {
        missing.push_back(std::string("IsoCmon.ff/SELECTED: ") + e.what());
    }
    try {
        result.world.select_no = load_select(store, "SELECT_NO");
    } catch (const std::exception& e) {
        missing.push_back(std::string("IsoCmon.ff/SELECT_NO: ") + e.what());
    }
    try {
        result.world.select_yes = load_select(store, "SELECT_YES");
    } catch (const std::exception& e) {
        missing.push_back(std::string("IsoCmon.ff/SELECT_YES: ") + e.what());
    }
    try {
        result.world.route_preview.normal =
            load_route_visual(assets_, {"MOVENORMAL", 11, 72, 72, 35, 40});
        result.world.route_preview.action_limit =
            load_route_visual(assets_, {"MOVEACTION", 11, 72, 72, 35, 40});
        result.world.route_preview.destination_highlight =
            load_route_visual(assets_, {"TILE_HIGHLIGHT", 31, 480, 480, 240, 240});
    } catch (const std::exception& e) {
        missing.push_back(std::string("IsoCmon.ff route preview: ") + e.what());
    }

    auto count = kExpected - static_cast<int>(missing.size());
    kLog->info("adventure_visuals resolved={}", count);

    if (!missing.empty()) {
        std::string list;
        for (const auto& m : missing) {
            if (!list.empty())
                list += ", ";
            list += m;
        }
        throw std::runtime_error("critical Adventure visuals missing: " + list);
    }

    // Preload SELECT world visual textures into GPU cache
    std::vector<ImageAssetKey> select_keys = {
        result.world.select_selected.key, result.world.select_no.key, result.world.select_yes.key};
    std::vector<ImageAssetKey> route_keys;
    for (const auto* visual :
         {&result.world.route_preview.normal, &result.world.route_preview.action_limit,
          &result.world.route_preview.destination_highlight}) {
        for (const auto& frame : visual->animation.frames) {
            route_keys.push_back(
                {visual->container_path, frame.record_name, ImageAssetKind::ComposedSprite});
        }
    }
    std::sort(route_keys.begin(), route_keys.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.container_path + lhs.image_name < rhs.container_path + rhs.image_name;
    });
    route_keys.erase(std::unique(route_keys.begin(), route_keys.end()), route_keys.end());
    auto select_batch = render_assets_.request_textures(select_keys, AssetPriority::Critical,
                                                        "AdventureSelectionVisuals");
    select_batch.wait();
    const auto select_upload = render_assets_.upload_ready(select_batch);
    kLog->info("adventure_select_textures uploaded={} failed={}", select_upload.uploaded,
               select_upload.failed);
    if (select_upload.failed > 0)
        throw std::runtime_error("critical Adventure selection texture upload failed");

    auto route_batch = render_assets_.request_textures(route_keys, AssetPriority::Critical,
                                                       "AdventureRoutePreviewVisuals");
    route_batch.wait();
    const auto route_upload = render_assets_.upload_ready(route_batch);
    kLog->info("adventure_route_preview_textures uploaded={} failed={}", route_upload.uploaded,
               route_upload.failed);
    if (route_upload.failed > 0)
        throw std::runtime_error("critical Adventure route-preview texture upload failed");

    // Validate every required SELECT texture is in cache
    auto& cache = render_assets_.textures();
    for (const auto& key : select_keys) {
        if (cache.find(key) == nullptr) {
            throw std::runtime_error("required Adventure selection texture not in cache: " +
                                     key.container_path + "/" + key.image_name);
        }
    }
    for (const auto& key : route_keys) {
        if (cache.find(key) == nullptr) {
            throw std::runtime_error("required Adventure route-preview texture not in cache: " +
                                     key.container_path + "/" + key.image_name);
        }
    }

    return result;
}

} // namespace d2engine
