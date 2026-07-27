#include "raw_ff_animation_catalog.hpp"

#include "raw_animation_role_resolver_factory.hpp"
#include "../assets/asset_runtime.hpp"

#include "d2res/opt_maps.hpp"

#include <d2log/log.hpp>

#include <algorithm>
#include <cctype>

namespace d2engine {

namespace {
auto kLog = d2log::get("d2.animation"); // NOLINT(cert-err58-cpp)

std::string upper(std::string_view value) {
    std::string result(value);
    for (char& c : result) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return result;
}

const d2res::ImageFrame* find_image_frame(const d2res::OptMaps& maps, std::string_view name) {
    const std::string key = upper(name);
    const auto        block_it = maps.image_map.frame_name_to_block.find(key);
    if (block_it == maps.image_map.frame_name_to_block.end()) {
        return nullptr;
    }
    const d2res::ImageBlock& block = maps.image_map.blocks[block_it->second];
    const auto               frame_it = std::ranges::find_if(
        block.frames, [&](const d2res::ImageFrame& frame) { return upper(frame.name) == key; });
    return frame_it == block.frames.end() ? nullptr : &*frame_it;
}

void apply_frame_anchor(AnimationSequence& sequence, const d2res::ImageFrame& frame) {
    bool    first_piece = true;
    int32_t min_x = 0;
    int32_t max_x = 0;
    int32_t min_y = 0;
    int32_t max_y = 0;
    for (const auto& piece : frame.pieces) {
        const int32_t piece_max_x = piece.output_x + piece.width;
        const int32_t piece_max_y = piece.output_y + piece.height;
        if (first_piece) {
            min_x = piece.output_x;
            max_x = piece_max_x;
            min_y = piece.output_y;
            max_y = piece_max_y;
            first_piece = false;
        } else {
            min_x = std::min(min_x, piece.output_x);
            max_x = std::max(max_x, piece_max_x);
            min_y = std::min(min_y, piece.output_y);
            max_y = std::max(max_y, piece_max_y);
        }
    }
    sequence.native_canvas_w = frame.output_width;
    sequence.native_canvas_h = frame.output_height;
    if (!first_piece) {
        sequence.canvas_foot_x = (min_x + max_x) / 2;
        sequence.canvas_foot_y = max_y;
        sequence.canvas_top_y = min_y;
    }
}
} // namespace

RawFfAnimationCatalog::RawFfAnimationCatalog(AssetRuntime& assets, std::string unit_container,
                                             std::string effect_container)
    : unit_container_(std::move(unit_container)), effect_container_(std::move(effect_container)),
      assets_(&assets),
      resolver_(std::make_unique<AnimationRoleResolver>(
          make_animation_role_resolver_from_raw_ff(assets.store(), unit_container_))) {}

AnimationSequence RawFfAnimationCatalog::unit_clip(std::string_view unit_type,
                                                   std::string_view role, char direction) const {
    const std::string name =
        resolver_->resolve(std::string{unit_type}, std::string{role}, direction);
    if (name.empty()) {
        return {};
    }
    try {
        AnimationSequence seq = assets_->animation_sequence(unit_container_, name);
        seq.is_looping = false; // caller controls looping
        return seq;
    } catch (const std::exception& e) {
        kLog->warn("unit_clip_decode_failed clip={} error={}", name, e.what());
        return {};
    }
}

AnimationSequence RawFfAnimationCatalog::battle_effect(std::string_view role) const {
    try {
        AnimationSequence seq = assets_->animation_sequence(effect_container_, std::string{role});
        seq.is_looping = false; // caller controls looping
        return seq;
    } catch (const std::exception& e) {
        kLog->warn("battle_effect_decode_failed role={} error={}", std::string{role}, e.what());
        return {};
    }
}

AnimationSequence RawFfAnimationCatalog::image_sequence(std::string_view base_name) const {
    AnimationSequence sequence;
    sequence.name = std::string{base_name};
    sequence.container_path = effect_container_;
    sequence.is_looping = false;

    const d2res::OptMaps* maps = assets_->store().container_maps(effect_container_);
    if (maps == nullptr) {
        kLog->warn("image_sequence_no_opt_maps base={} container={}", sequence.name,
                   effect_container_);
        return {};
    }

    for (std::size_t i = 0; i < 2; ++i) {
        const std::string        frame_name = sequence.name + (i == 0 ? "00" : "01");
        const d2res::ImageFrame* frame = find_image_frame(*maps, frame_name);
        if (frame == nullptr) {
            kLog->warn("image_sequence_frame_missing base={} frame={} container={}", sequence.name,
                       frame_name, effect_container_);
            return {};
        }
        if (sequence.frames.empty()) {
            apply_frame_anchor(sequence, *frame);
        }
        sequence.frames.push_back({.image_name = frame_name,
                                   .index = static_cast<std::uint32_t>(i),
                                   .duration_ms = static_cast<std::uint16_t>(100)});
    }

    return sequence;
}

UnitStateClip RawFfAnimationCatalog::unit_state_bundle(std::string_view unit_type,
                                                       std::string_view action,
                                                       char             direction) const {
    // action is e.g. "IDLE" → role_suffix is e.g. "IDLEA"
    const std::string role_suffix = std::string{action} + "A";

    const LayerTriple layers =
        resolver_->resolve_layers(std::string{unit_type}, role_suffix, direction);

    UnitStateClip result;
    result.action = std::string{action};
    result.direction = direction;
    result.clip.loop_policy = false; // set by caller for looping
    result.clip.family_name = std::string{action};
    result.clip.direction = direction;

    if (!layers.a1.empty()) {
        try {
            AnimationSequence seq = assets_->animation_sequence(unit_container_, layers.a1);
            seq.is_looping = false;
            result.clip.a1 = std::move(seq);
            result.direction = extract_direction_from_name(layers.a1);
        } catch (...) {
        }
    }
    if (!layers.s1.empty()) {
        try {
            AnimationSequence seq = assets_->animation_sequence(unit_container_, layers.s1);
            seq.is_looping = false;
            result.clip.s1 = std::move(seq);
        } catch (...) {
        }
    }
    if (!layers.a2.empty()) {
        try {
            AnimationSequence seq = assets_->animation_sequence(unit_container_, layers.a2);
            seq.is_looping = false;
            result.clip.a2 = std::move(seq);
        } catch (...) {
        }
    }
    return result;
}

BattleEffectClip RawFfAnimationCatalog::battle_effect_bundle(std::string_view unit_type,
                                                             std::string_view effect_family,
                                                             char             direction) const {
    const std::string role_suffix = std::string{effect_family} + "A";

    const LayerTriple layers =
        resolver_->resolve_layers(std::string{unit_type}, role_suffix, direction);

    BattleEffectClip result;
    result.family = std::string{effect_family};
    result.requested_direction = direction;
    result.clip.family_name = std::string{effect_family};
    result.clip.direction = direction;

    if (!layers.a1.empty()) {
        try {
            AnimationSequence seq = assets_->animation_sequence(unit_container_, layers.a1);
            seq.is_looping = false;
            result.clip.a1 = std::move(seq);
            result.direction_or_variant = extract_direction_from_name(layers.a1);
        } catch (...) {
        }
    }
    if (!layers.s1.empty()) {
        try {
            AnimationSequence seq = assets_->animation_sequence(unit_container_, layers.s1);
            seq.is_looping = false;
            result.clip.s1 = std::move(seq);
        } catch (...) {
        }
    }
    if (!layers.a2.empty()) {
        try {
            AnimationSequence seq = assets_->animation_sequence(unit_container_, layers.a2);
            seq.is_looping = false;
            result.clip.a2 = std::move(seq);
        } catch (...) {
        }
    }

    if (result.is_suspicious()) {
        kLog->warn("suspicious_heff_clip unit_type={} dir={} reason=S1_present_A1_absent",
                   std::string{unit_type}, direction);
    }

    return result;
}

} // namespace d2engine
