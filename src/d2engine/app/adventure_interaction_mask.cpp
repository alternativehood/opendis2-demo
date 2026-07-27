#include "adventure_interaction_mask.hpp"

#include <d2adventure_render/adventure_render_types.hpp>
#include <d2res/rgba_buffer.hpp>
#include <d2log/log.hpp>

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace d2engine {

static auto kLog = d2log::get("d2.interact"); // NOLINT

std::shared_ptr<const adventure_render::InteractionMask>
build_interaction_mask(const d2res::RgbaBuffer& rgba) {
    auto mask = std::make_shared<adventure_render::InteractionMask>();
    mask->width = static_cast<int>(rgba.width);
    mask->height = static_cast<int>(rgba.height);
    mask->bits.resize(static_cast<std::size_t>(mask->stride() * mask->height), 0);

    for (int y = 0; y < mask->height; ++y) {
        for (int x = 0; x < mask->width; ++x) {
            const std::size_t pixel_index =
                (static_cast<std::size_t>(y) * static_cast<std::size_t>(mask->width) +
                 static_cast<std::size_t>(x)) *
                4;
            if (rgba.rgba[pixel_index + 3] != 0) {
                const std::size_t byte_index = static_cast<std::size_t>(y * mask->stride() + x / 8);
                mask->bits[byte_index] |= 0x80U >> (static_cast<unsigned>(x) % 8);
            }
        }
    }
    return mask;
}

std::shared_ptr<const adventure_render::InteractionMask>
build_union_interaction_mask(const std::vector<const d2res::RgbaBuffer*>& rgba_frames) {
    if (rgba_frames.empty())
        throw std::logic_error("build_union_interaction_mask: empty frame list");

    int max_w = 0;
    int max_h = 0;
    for (std::size_t i = 0; i < rgba_frames.size(); ++i) {
        const auto* rgba = rgba_frames[i];
        if (rgba == nullptr)
            continue;
        if (rgba->width == 0 || rgba->height == 0)
            continue;
        const std::size_t expected_size =
            static_cast<std::size_t>(rgba->width) * static_cast<std::size_t>(rgba->height) * 4;
        if (rgba->rgba.size() < expected_size)
            continue;
        if (static_cast<int>(rgba->width) > max_w)
            max_w = static_cast<int>(rgba->width);
        if (static_cast<int>(rgba->height) > max_h)
            max_h = static_cast<int>(rgba->height);
    }
    if (max_w == 0 || max_h == 0)
        throw std::logic_error("build_union_interaction_mask: zero union dimensions");

    auto mask = std::make_shared<adventure_render::InteractionMask>();
    mask->width = max_w;
    mask->height = max_h;
    mask->bits.resize(static_cast<std::size_t>(mask->stride() * mask->height), 0);

    for (const auto* rgba : rgba_frames) {
        if (rgba == nullptr)
            continue;
        const int fw = static_cast<int>(rgba->width);
        const int fh = static_cast<int>(rgba->height);
        for (int y = 0; y < fh && y < max_h; ++y) {
            for (int x = 0; x < fw && x < max_w; ++x) {
                const std::size_t pixel_index =
                    (static_cast<std::size_t>(y) * static_cast<std::size_t>(fw) +
                     static_cast<std::size_t>(x)) *
                    4;
                if (rgba->rgba[pixel_index + 3] != 0) {
                    const std::size_t byte_index =
                        static_cast<std::size_t>(y * mask->stride() + x / 8);
                    mask->bits[byte_index] |= 0x80U >> (static_cast<unsigned>(x) % 8);
                }
            }
        }
    }
    return mask;
}

namespace {
[[nodiscard]] std::string
actor_layer_frame_error(const adventure_render::AdventureActorVisualLayer& layer,
                        const adventure_render::AdventureActorVisualFrame& frame,
                        std::string_view                                   reason) {
    return "actor interaction mask animation=" + layer.logical_animation_name +
           " container=" + layer.container_path + " frame=" + frame.record_name +
           " reason=" + std::string(reason);
}
} // namespace

std::shared_ptr<const adventure_render::InteractionMask>
build_actor_layer_interaction_mask(const adventure_render::AdventureActorVisualLayer& layer,
                                   std::span<const PreparedImageResult> decoded_images) {
    if (layer.frames.empty())
        throw std::runtime_error(
            "actor interaction mask animation=" + layer.logical_animation_name +
            " container=" + layer.container_path + " reason=empty_frames");

    std::vector<const d2res::RgbaBuffer*> frame_buffers;
    frame_buffers.reserve(layer.frames.size());
    int expected_width = 0;
    int expected_height = 0;
    for (const auto& frame : layer.frames) {
        expected_width = std::max(expected_width, frame.canvas_width);
        expected_height = std::max(expected_height, frame.canvas_height);
        const ImageAssetKey        expected_key{layer.container_path, frame.record_name,
                                                ImageAssetKind::ComposedSprite};
        const PreparedImageResult* match = nullptr;
        std::size_t                match_count = 0;
        for (const auto& result : decoded_images) {
            if (result.key == expected_key) {
                match = &result;
                ++match_count;
            }
        }
        if (match_count != 1)
            throw std::runtime_error(actor_layer_frame_error(
                layer, frame,
                match_count == 0 ? "missing_decoded_result" : "duplicate_decoded_result"));
        if (!match->success)
            throw std::runtime_error(actor_layer_frame_error(layer, frame, "decode_failed"));
        if (!match->image)
            throw std::runtime_error(
                actor_layer_frame_error(layer, frame, "missing_prepared_image"));
        if (!match->image->pixels)
            throw std::runtime_error(actor_layer_frame_error(layer, frame, "missing_rgba_pixels"));
        const auto& rgba = *match->image->pixels;
        if (rgba.width == 0 || rgba.height == 0)
            throw std::runtime_error(actor_layer_frame_error(layer, frame, "invalid_dimensions"));
        const auto required_bytes = static_cast<std::size_t>(rgba.width) * rgba.height * 4;
        if (rgba.rgba.size() < required_bytes)
            throw std::runtime_error(actor_layer_frame_error(layer, frame, "undersized_rgba"));
        frame_buffers.push_back(&rgba);
    }
    if (expected_width == 0)
        expected_width = layer.native_canvas_w;
    if (expected_height == 0)
        expected_height = layer.native_canvas_h;
    const auto mask = build_union_interaction_mask(frame_buffers);
    if (mask->width != expected_width || mask->height != expected_height) {
        throw std::runtime_error(
            "actor interaction mask animation=" + layer.logical_animation_name +
            " container=" + layer.container_path + " reason=dimension_mismatch" +
            " actual=" + std::to_string(mask->width) + "x" + std::to_string(mask->height) +
            " expected=" + std::to_string(expected_width) + "x" + std::to_string(expected_height));
    }
    return mask;
}

ImageAssetKey
adventure_world_asset_key(const adventure_render::PreparedAdventureRenderPrimitive& primitive) {
    return make_world_composed_sprite_key(primitive.container_path, primitive.record_name);
}

std::vector<ImageAssetKey>
collect_stack_mask_asset_keys(const adventure_render::PreparedAdventureMap& map) {
    std::set<std::string>                                seen_ids;
    std::vector<ImageAssetKey>                           keys;
    std::unordered_set<adventure_render::StableRenderId> stack_ids;

    for (const auto& entry : map.pick_entries) {
        if (entry.kind == adventure_render::PickEntryKind::Stack) {
            stack_ids.insert(entry.stable_id);
        }
    }

    for (const auto& prim : map.world_graph.world) {
        if (prim.level != adventure_render::WorldRenderLevel::Actor ||
            !stack_ids.contains(prim.stable_id) || prim.container_path.empty()) {
            continue;
        }

        auto add_key = [&](const std::string& record) {
            if (record.empty()) {
                return;
            }
            const std::string asset_id = prim.container_path + "/" + record;
            if (!seen_ids.insert(asset_id).second) {
                return;
            }
            keys.push_back({.container_path = prim.container_path,
                            .image_name = record,
                            .kind = ImageAssetKind::ComposedSprite});
        };

        if (prim.animation.has_value()) {
            for (const auto& frame : prim.animation->frames) {
                add_key(frame.record_name);
            }
        } else {
            add_key(prim.record_name);
        }
    }

    return keys;
}

std::size_t
attach_stack_interaction_masks(adventure_render::PreparedAdventureMap& map,
                               std::span<const PreparedImageResult>    decoded_world_images) {
    std::unordered_map<ImageAssetKey, std::shared_ptr<const d2res::RgbaBuffer>> pixels_by_key;
    for (const auto& result : decoded_world_images) {
        if (result.success && result.image != nullptr && result.image->pixels != nullptr) {
            pixels_by_key.emplace(result.key, result.image->pixels);
        }
    }

    std::unordered_set<adventure_render::StableRenderId> stack_pick_ids;
    for (const auto& entry : map.pick_entries) {
        if (entry.kind == adventure_render::PickEntryKind::Stack) {
            stack_pick_ids.insert(entry.stable_id);
        }
    }

    std::size_t built = 0;
    for (auto& primitive : map.world_graph.world) {
        if (primitive.level != adventure_render::WorldRenderLevel::Actor ||
            !stack_pick_ids.contains(primitive.stable_id)) {
            continue;
        }

        if (primitive.animation.has_value() && primitive.animation->frames.size() > 1) {
            std::vector<const d2res::RgbaBuffer*> frame_buffers;
            frame_buffers.reserve(primitive.animation->frames.size());
            for (const auto& af : primitive.animation->frames) {
                const auto key =
                    make_world_composed_sprite_key(primitive.container_path, af.record_name);
                auto it = pixels_by_key.find(key);
                if (it == pixels_by_key.end() || it->second.get() == nullptr)
                    continue;
                const auto* buffer = it->second.get();
                if (buffer->width <= 0 || buffer->height <= 0)
                    continue;
                const std::size_t needed = static_cast<std::size_t>(buffer->width) *
                                           static_cast<std::size_t>(buffer->height) * 4;
                if (buffer->rgba.size() < needed)
                    continue;
                frame_buffers.push_back(buffer);
            }
            if (frame_buffers.empty()) {
                kLog->debug("skip_animated_mask_no_frames stable_id={} label={} animation={}",
                            primitive.stable_id, primitive.debug_label,
                            primitive.animation->animation_name);
                continue;
            }
            primitive.interaction_mask = build_union_interaction_mask(frame_buffers);
            if (primitive.interaction_mask != nullptr)
                ++built;
        } else {
            const auto pixels = pixels_by_key.find(adventure_world_asset_key(primitive));
            if (pixels == pixels_by_key.end()) {
                continue;
            }
            primitive.interaction_mask = build_interaction_mask(*pixels->second);
            if (primitive.interaction_mask != nullptr)
                ++built;
        }
    }
    return built;
}

} // namespace d2engine
