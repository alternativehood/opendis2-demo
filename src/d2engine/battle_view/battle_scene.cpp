#include "battle_scene.hpp"

#include "../animation/animation_sequence.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace d2engine {
namespace {

[[nodiscard]] RenderPlacement placement_for_track(const VisualTrack& track) {
    RenderPlacement placement = render_placement_for(track.layer, track.anchor);
    placement.depth_bias = track.depth_bias;
    return placement;
}

} // namespace

bool BattleScene::track_id_exists(TrackId id) const {
    if (id.value == 0) {
        return false;
    }
    for (const auto& unit : units_) {
        for (const auto& track : unit.tracks) {
            if (track.id == id) {
                return true;
            }
        }
    }
    return false;
}

TrackId BattleScene::next_track_id() {
    return TrackId{next_track_id_++};
}

void BattleScene::assign_track_ids(BattleUnit& unit) {
    for (auto& track : unit.tracks) {
        if (track.id.value == 0) {
            track.id = next_track_id();
        } else {
            if (track_id_exists(track.id)) {
                throw std::runtime_error("add_unit: duplicate TrackId");
            }
            next_track_id_ = std::max(next_track_id_, track.id.value + 1);
        }
    }
}

void BattleScene::add_unit(BattleUnit unit) {
    if (unit.unit_instance_id.value == 0) {
        unit.unit_instance_id = UnitInstanceId{next_unit_instance_id_++};
    } else {
        if (visual_entity_for(unit.unit_instance_id).has_value()) {
            throw std::runtime_error("add_unit: duplicate UnitInstanceId");
        }
        next_unit_instance_id_ = std::max(next_unit_instance_id_, unit.unit_instance_id.value + 1);
    }
    assign_track_ids(unit);
    unit.id = VisualEntityId{next_id_++};
    id_by_index_.push_back(unit.id);
    units_.push_back(std::move(unit));
}

void BattleScene::clear() {
    units_.clear();
    id_by_index_.clear();
    next_id_ = 1;
    next_unit_instance_id_ = 1;
    next_track_id_ = 1;
}

void BattleScene::remove_unit(VisualEntityId id) {
    const auto idx = find_unit(id);
    if (!idx.has_value()) {
        return;
    }
    const auto entity_it = std::ranges::find(id_by_index_, id);
    if (entity_it != id_by_index_.end()) {
        id_by_index_.erase(entity_it);
    }
    units_.erase(units_.begin() + static_cast<std::ptrdiff_t>(*idx));
}

std::optional<std::size_t> BattleScene::find_unit(VisualEntityId id) const {
    for (std::size_t i = 0; i < units_.size(); ++i) {
        if (units_[i].id == id) {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<VisualEntityId>
BattleScene::visual_entity_for(UnitInstanceId unit_instance_id) const {
    if (unit_instance_id.value == 0) {
        return std::nullopt;
    }
    for (const auto& unit : units_) {
        if (unit.unit_instance_id == unit_instance_id) {
            return unit.id;
        }
    }
    return std::nullopt;
}

BattleUnit* BattleScene::try_unit_by_id(VisualEntityId id) {
    const auto idx = find_unit(id);
    if (!idx.has_value()) {
        return nullptr;
    }
    return &units_[*idx];
}

const BattleUnit* BattleScene::try_unit_by_id(VisualEntityId id) const {
    const auto idx = find_unit(id);
    if (!idx.has_value()) {
        return nullptr;
    }
    return &units_[*idx];
}

void BattleScene::set_all_paused(bool paused) {
    for (auto& unit : units_) {
        unit.paused = paused;
    }
}

bool BattleScene::add_track(VisualEntityId id, VisualTrack track) {
    const auto idx = find_unit(id);
    if (!idx.has_value()) {
        return false;
    }
    if (track.effect_id.has_value() &&
        (track.effect_id->value == 0 || find_effect(*track.effect_id) != nullptr)) {
        return false;
    }
    if (track.id.value == 0) {
        track.id = next_track_id();
    } else {
        if (track_id_exists(track.id)) {
            return false;
        }
        next_track_id_ = std::max(next_track_id_, track.id.value + 1);
    }
    track.player.set_reverse_playback(track.playback.reverse_playback);
    units_[*idx].tracks.push_back(std::move(track));
    return true;
}

bool BattleScene::remove_track(VisualEntityId id, TrackSelector selector) {
    VisualTrack* track = find_track(id, selector);
    if (track == nullptr) {
        return false;
    }
    track->lifecycle = TrackLifecycle::PendingRemoval;
    return true;
}

bool BattleScene::remove_track(VisualEntityId id, TrackKind kind) {
    return remove_track(id, TrackSelector::singleton(kind));
}

bool BattleScene::remove_effect(EffectInstanceId id) {
    VisualTrack* track = find_effect(id);
    if (track == nullptr) {
        return false;
    }
    track->lifecycle = TrackLifecycle::PendingRemoval;
    return true;
}

bool BattleScene::replace_track_clip(VisualEntityId id, TrackSelector selector,
                                     AnimationSequence seq, bool looping, BindingRole role) {
    VisualTrack* track = find_track(id, selector);
    if (track == nullptr) {
        return false;
    }
    track->layered_player = {};
    seq.is_looping = looping;
    track->player.load(std::move(seq));
    track->player.set_reverse_playback(track->playback.reverse_playback);
    track->player.play();
    // Propagate role so renderer can determine binding without sequence_name inference
    if (role != BindingRole::UnitBase || track->kind == TrackKind::Base) {
        track->effect_role = role;
    }
    return true;
}

bool BattleScene::replace_track_clip(VisualEntityId id, TrackKind kind, AnimationSequence seq,
                                     bool looping) {
    return replace_track_clip(id, TrackSelector::singleton(kind), std::move(seq), looping);
}

bool BattleScene::replace_track_layered_clip(VisualEntityId id, TrackSelector selector,
                                             const LayeredAnimationClip& clip, bool looping,
                                             BindingRole role) {
    VisualTrack* track = find_track(id, selector);
    if (track == nullptr) {
        return false;
    }
    const bool effective_looping = looping || clip.loop_policy;
    track->layered_player = {.clip = &clip,
                             .elapsed_ms = 0,
                             .looping = effective_looping,
                             .reverse_playback = track->playback.reverse_playback};

    const AnimationSequence* drv = driver_sequence(clip);
    if (drv != nullptr) {
        AnimationSequence seq = *drv;
        seq.is_looping = effective_looping;
        track->player.load(std::move(seq));
        track->player.set_reverse_playback(track->playback.reverse_playback);
        track->player.play();
    }
    if (role != BindingRole::UnitBase || track->kind == TrackKind::Base) {
        track->effect_role = role;
    }
    return true;
}

bool BattleScene::stop_track(VisualEntityId id, TrackSelector selector) {
    VisualTrack* track = find_track(id, selector);
    if (track == nullptr) {
        return false;
    }
    track->player.stop();
    return true;
}

bool BattleScene::stop_track(VisualEntityId id, TrackKind kind) {
    return stop_track(id, TrackSelector::singleton(kind));
}

bool BattleScene::set_track_visibility(VisualEntityId id, TrackSelector selector,
                                       TrackVisibility visibility) {
    VisualTrack* track = find_track(id, selector);
    if (track == nullptr) {
        return false;
    }
    track->visibility = visibility;
    return true;
}

bool BattleScene::set_track_visibility(VisualEntityId id, TrackKind kind,
                                       TrackVisibility visibility) {
    return set_track_visibility(id, TrackSelector::singleton(kind), visibility);
}

bool BattleScene::set_track_alpha(VisualEntityId id, TrackSelector selector, float alpha) {
    VisualTrack* track = find_track(id, selector);
    if (track == nullptr) {
        return false;
    }
    track->alpha = std::clamp(alpha, 0.0f, 1.0f);
    return true;
}

bool BattleScene::set_track_alpha(VisualEntityId id, TrackKind kind, float alpha) {
    return set_track_alpha(id, TrackSelector::singleton(kind), alpha);
}

bool BattleScene::set_track_transform(VisualEntityId id, TrackSelector selector,
                                      VisualTransform transform) {
    VisualTrack* track = find_track(id, selector);
    if (track == nullptr) {
        return false;
    }
    track->transform = transform;
    return true;
}

bool BattleScene::set_track_transform(VisualEntityId id, TrackKind kind,
                                      VisualTransform transform) {
    return set_track_transform(id, TrackSelector::singleton(kind), transform);
}

bool BattleScene::set_life_state(VisualEntityId id, LifeVisualState state) {
    BattleUnit* unit = try_unit_by_id(id);
    if (unit == nullptr) {
        return false;
    }
    unit->life_state = state;
    return true;
}

bool BattleScene::set_unit_hp(UnitInstanceId id, int current_hp) {
    const auto  entity_id = visual_entity_for(id);
    BattleUnit* unit = entity_id.has_value() ? try_unit_by_id(*entity_id) : nullptr;
    if (unit == nullptr || unit->max_hp <= 0) {
        return false;
    }
    unit->current_hp = std::clamp(current_hp, 0, unit->max_hp);
    return true;
}

bool BattleScene::toggle_pause(VisualEntityId id) {
    BattleUnit* unit = try_unit_by_id(id);
    if (unit == nullptr) {
        return false;
    }
    unit->paused = !unit->paused;
    return true;
}

bool BattleScene::step_frame(VisualEntityId id, int delta) {
    BattleUnit* unit = try_unit_by_id(id);
    if (unit == nullptr || !unit->paused) {
        return false;
    }
    const auto base_track = unit->find_track(TrackKind::Base);
    if (!base_track.has_value()) {
        return false;
    }
    VisualTrack& track = unit->tracks[*base_track];
    track.player.step(delta);

    // Also advance layered_player by one driver-frame worth of time per step unit.
    if (track.layered_player.clip != nullptr) {
        const AnimationSequence* drv = driver_sequence(*track.layered_player.clip);
        if (drv != nullptr && !drv->frames.empty()) {
            // Use frame index from legacy player as the target frame for the driver.
            const std::size_t target_frame = track.player.current_frame_index() < drv->frames.size()
                                                 ? track.player.current_frame_index()
                                                 : drv->frames.size() - 1u;
            // Compute cumulative elapsed_ms to land exactly on target_frame.
            uint32_t cum = 0;
            for (std::size_t i = 0; i < target_frame; ++i) {
                cum += static_cast<uint32_t>(
                    drv->frames[i].duration_ms > 0 ? drv->frames[i].duration_ms : 40u);
            }
            track.layered_player.elapsed_ms = cum;
        }
    }
    return true;
}

bool BattleScene::move_unit(VisualEntityId id, float dx, float dy) {
    BattleUnit* unit = try_unit_by_id(id);
    if (unit == nullptr) {
        return false;
    }
    unit->position_offset.x += dx;
    unit->position_offset.y += dy;
    return true;
}

std::optional<Vec2> BattleScene::unit_position_offset(VisualEntityId id) const {
    const auto idx = find_unit(id);
    if (!idx.has_value()) {
        return std::nullopt;
    }
    return units_[*idx].position_offset;
}

VisualTrack* BattleScene::find_track(VisualEntityId id, TrackKind kind) {
    return find_track(id, TrackSelector::singleton(kind));
}

const VisualTrack* BattleScene::find_track(VisualEntityId id, TrackKind kind) const {
    return find_track(id, TrackSelector::singleton(kind));
}

VisualTrack* BattleScene::find_track(VisualEntityId id, TrackSelector selector) {
    BattleUnit* unit = try_unit_by_id(id);
    if (unit == nullptr) {
        return nullptr;
    }
    for (auto& track : unit->tracks) {
        if (track.lifecycle != TrackLifecycle::Active) {
            continue;
        }
        if (selector.mode == TrackSelector::Mode::ById && track.id == selector.id) {
            return &track;
        }
        if (selector.mode == TrackSelector::Mode::ByKindSingleton && track.kind == selector.kind) {
            return &track;
        }
    }
    return nullptr;
}

const VisualTrack* BattleScene::find_track(VisualEntityId id, TrackSelector selector) const {
    const auto idx = find_unit(id);
    if (!idx.has_value()) {
        return nullptr;
    }
    for (const auto& track : units_[*idx].tracks) {
        if (track.lifecycle != TrackLifecycle::Active) {
            continue;
        }
        if (selector.mode == TrackSelector::Mode::ById && track.id == selector.id) {
            return &track;
        }
        if (selector.mode == TrackSelector::Mode::ByKindSingleton && track.kind == selector.kind) {
            return &track;
        }
    }
    return nullptr;
}

VisualTrack* BattleScene::find_effect(EffectInstanceId id) {
    if (id.value == 0) {
        return nullptr;
    }
    for (auto& unit : units_) {
        for (auto& track : unit.tracks) {
            if (track.lifecycle == TrackLifecycle::Active && track.effect_id == id) {
                return &track;
            }
        }
    }
    return nullptr;
}

void BattleScene::update(float delta_ms) {
    for (auto& unit : units_) {
        const float effective_delta = unit.paused ? 0.0f : delta_ms;
        // Advance visual tracks (Phase 3)
        for (auto& track : unit.tracks) {
            if (track.lifecycle == TrackLifecycle::PendingRemoval) {
                continue;
            }
            if (track.visibility == TrackVisibility::Visible ||
                track.visibility == TrackVisibility::HiddenButPlaying) {
                track.player.update(effective_delta);
                if (track.layered_player.clip != nullptr) {
                    track.layered_player.elapsed_ms += static_cast<uint32_t>(effective_delta);
                }
            }
        }
        // Remove pending tracks
        std::erase_if(unit.tracks, [](const VisualTrack& t) {
            return t.lifecycle == TrackLifecycle::PendingRemoval;
        });
    }
}

BattleRenderSnapshot BattleScene::snapshot() const {
    BattleRenderSnapshot result;
    result.entities.reserve(units_.size());
    for (const auto& unit : units_) {
        SnapshotEntity entity{.id = unit.id,
                              .unit_instance_id = unit.unit_instance_id,
                              .coord = unit.coord,
                              .life_state = unit.life_state,
                              .flip = unit.flip,
                              .is_large = unit.is_large,
                              .alpha = unit.alpha,
                              .position_offset = unit.position_offset,
                              .unit_type = unit.unit_type,
                              .animation_unit_type = unit.animation_unit_type,
                              .display_name = unit.display_name,
                              .current_hp = unit.current_hp,
                              .max_hp = unit.max_hp};
        entity.tracks.reserve(unit.tracks.size());
        for (const auto& track : unit.tracks) {
            if (track.lifecycle == TrackLifecycle::PendingRemoval) {
                continue;
            }

            if (track.layered_player.clip != nullptr && !track.layered_player.clip->is_empty()) {
                // Layered clip: emit one SnapshotTrack per present layer in S1→A1→A2 order.
                // sample_frame() uses elapsed_ms phase normalisation, handling mismatched counts.
                const AnimationSequence* drv = driver_sequence(*track.layered_player.clip);
                const std::string        bundle_name = (drv != nullptr) ? drv->name : std::string{};
                for (const auto slot : LayeredAnimationClip::kDrawOrder) {
                    const auto* seq = layer_sequence(*track.layered_player.clip, slot);
                    if (seq == nullptr || seq->frames.empty())
                        continue;

                    const std::size_t frame_idx = sample_frame(track.layered_player, slot);
                    const auto&       frame = seq->frames[frame_idx];
                    entity.tracks.push_back({.id = track.id,
                                             .kind = track.kind,
                                             .layer = track.layer,
                                             .anchor = track.anchor,
                                             .placement = placement_for_track(track),
                                             .visibility = track.visibility,
                                             .transform = track.transform,
                                             .playback = track.playback,
                                             .effect_id = track.effect_id,
                                             .alpha = track.alpha,
                                             .current_frame_index = frame_idx,
                                             .current_frame_name = frame.image_name,
                                             .container_path = seq->container_path,
                                             .sequence_name = seq->name,
                                             .bundle_sequence_name = bundle_name,
                                             .layer_slot = slot,
                                             .effect_role = track.effect_role,
                                             .visual_role = track.visual_role,
                                             .lifecycle_profile_id = track.lifecycle_profile_id,
                                             .sequence = *seq,
                                             .is_looping = track.layered_player.looping,
                                             .canvas_foot_x = seq->canvas_foot_x,
                                             .canvas_foot_y = seq->canvas_foot_y,
                                             .canvas_top_y = seq->canvas_top_y,
                                             .native_canvas_w = seq->native_canvas_w,
                                             .native_canvas_h = seq->native_canvas_h});
                }
            } else if (!track.player.sequence().frames.empty()) {
                // Single sequence (legacy path)
                const auto& sequence = track.player.sequence();
                const auto& frame = track.player.current_frame();
                entity.tracks.push_back({.id = track.id,
                                         .kind = track.kind,
                                         .layer = track.layer,
                                         .anchor = track.anchor,
                                         .placement = placement_for_track(track),
                                         .visibility = track.visibility,
                                         .transform = track.transform,
                                         .playback = track.playback,
                                         .effect_id = track.effect_id,
                                         .alpha = track.alpha,
                                         .current_frame_index = track.player.current_frame_index(),
                                         .current_frame_name = frame.image_name,
                                         .container_path = sequence.container_path,
                                         .sequence_name = sequence.name,
                                         .effect_role = track.effect_role,
                                         .visual_role = track.visual_role,
                                         .lifecycle_profile_id = track.lifecycle_profile_id,
                                         .sequence = sequence,
                                         .is_looping = sequence.is_looping,
                                         .canvas_foot_x = sequence.canvas_foot_x,
                                         .canvas_foot_y = sequence.canvas_foot_y,
                                         .canvas_top_y = sequence.canvas_top_y,
                                         .native_canvas_w = sequence.native_canvas_w,
                                         .native_canvas_h = sequence.native_canvas_h});
            }
        }
        result.entities.push_back(std::move(entity));
    }
    return result;
}

} // namespace d2engine
