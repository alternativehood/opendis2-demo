#include "command_timeline.hpp"

#include "animation_clip_store.hpp"
#include "battle_scene.hpp"
#include "layered_animation_clip.hpp"
#include "../animation/animation_player.hpp"

#include <d2log/log.hpp>

#include <algorithm>
#include <limits>
#include <string>
#include <type_traits>

namespace d2engine {
namespace {

auto kLog = d2log::get("d2.render"); // NOLINT(cert-err58-cpp)

struct StepResult {
    bool  completed = false;
    float unused_ms = 0.0f;
};

const char* to_string(TrackKind k) noexcept {
    switch (k) {
    case TrackKind::Base:
        return "Base";
    case TrackKind::Shadow:
        return "Shadow";
    case TrackKind::ActorMarker:
        return "ActorMarker";
    case TrackKind::TargetMarker:
        return "TargetMarker";
    case TrackKind::DeathBody:
        return "DeathBody";
    case TrackKind::DeathFx:
        return "DeathFx";
    case TrackKind::ReviveFx:
        return "ReviveFx";
    case TrackKind::CastFx:
        return "CastFx";
    case TrackKind::HitFx:
        return "HitFx";
    case TrackKind::Effect:
        return "Effect";
    case TrackKind::Aura:
        return "Aura";
    case TrackKind::Hidden:
        return "Hidden";
    }
    return "?";
}

const char* to_string(AnchorPolicy a) noexcept {
    switch (a) {
    case AnchorPolicy::UnitFoot:
        return "UnitFoot";
    case AnchorPolicy::UnitCanvasFoot:
        return "UnitCanvasFoot";
    case AnchorPolicy::TeamCentroid:
        return "TeamCentroid";
    case AnchorPolicy::OppositeTeamCentroid:
        return "OppositeTeamCentroid";
    case AnchorPolicy::LaneMidpoint:
        return "LaneMidpoint";
    case AnchorPolicy::OppositeLaneMidpoint:
        return "OppositeLaneMidpoint";
    case AnchorPolicy::BattlefieldReferenceRect:
        return "BattlefieldReferenceRect";
    case AnchorPolicy::ScreenRect:
        return "ScreenRect";
    case AnchorPolicy::WorldPoint:
        return "WorldPoint";
    }
    return "?";
}

const char* to_string(TrackRenderLayer l) noexcept {
    switch (l) {
    case TrackRenderLayer::Background:
        return "Background";
    case TrackRenderLayer::Ground:
        return "Ground";
    case TrackRenderLayer::Shadow:
        return "Shadow";
    case TrackRenderLayer::Base:
        return "Base";
    case TrackRenderLayer::Effect:
        return "Effect";
    case TrackRenderLayer::Overlay:
        return "Overlay";
    case TrackRenderLayer::Marker:
        return "Marker";
    case TrackRenderLayer::Frame:
        return "Frame";
    case TrackRenderLayer::Debug:
        return "Debug";
    }
    return "?";
}

StepResult update_node(CommandNode& node, BattleScene& scene, float delta_ms,
                       std::vector<BattleVisualEvent>& emitted_events,
                       const AnimationClipStore*       clip_store);

StepResult update_command(VisualCommand& command, BattleScene& scene, float delta_ms,
                          std::vector<BattleVisualEvent>& emitted_events,
                          const AnimationClipStore*       clip_store) {
    return std::visit(
        [&](auto& value) -> StepResult {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, PlayClip>) {
                const AnimationSequence* sequence = std::get_if<AnimationSequence>(&value.clip);
                if (sequence == nullptr) {
                    if (const auto* handle = std::get_if<AnimationClipHandle>(&value.clip)) {
                        sequence = clip_store == nullptr ? nullptr : clip_store->get(*handle);
                    }
                }
                {
                    if (sequence != nullptr) {
                        kLog->debug("anim_play_clip entity={} role={} clip={} looping={}",
                                    value.entity_id.value, to_string(value.unit_anim_role),
                                    sequence->name, value.looping);
                    } else if (const auto* lp =
                                   std::get_if<const LayeredAnimationClip*>(&value.clip)) {
                        if (*lp != nullptr) {
                            const auto& clip = **lp;
                            const char* drv_name =
                                driver_sequence(clip) ? driver_sequence(clip)->name.c_str() : "";
                            std::string layers;
                            if (clip.s1) {
                                layers += " S1=" + clip.s1->name + "(" +
                                          std::to_string(clip.s1->frames.size()) + ")";
                            }
                            if (clip.a1) {
                                layers += " A1=" + clip.a1->name + "(" +
                                          std::to_string(clip.a1->frames.size()) + ")";
                            }
                            if (clip.a2) {
                                layers += " A2=" + clip.a2->name + "(" +
                                          std::to_string(clip.a2->frames.size()) + ")";
                            }
                            kLog->debug(
                                "anim_play_clip entity={} role={} driver={} looping={} layers={}",
                                value.entity_id.value, to_string(value.unit_anim_role), drv_name,
                                value.looping, layers);
                        }
                    } else if (const auto* handle = std::get_if<AnimationClipHandle>(&value.clip)) {
                        kLog->debug("anim_play_clip entity={} role={} clip=handle#{} looping={}",
                                    value.entity_id.value, to_string(value.unit_anim_role),
                                    handle->value, value.looping);
                    }
                }
                if (sequence != nullptr) {
                    static_cast<void>(scene.replace_track_clip(value.entity_id, value.track,
                                                               *sequence, value.looping,
                                                               value.unit_anim_role));
                } else if (const auto* layered_ptr =
                               std::get_if<const LayeredAnimationClip*>(&value.clip)) {
                    if (*layered_ptr != nullptr) {
                        static_cast<void>(scene.replace_track_layered_clip(
                            value.entity_id, value.track, **layered_ptr, value.looping,
                            value.unit_anim_role));
                    }
                }
            } else if constexpr (std::is_same_v<T, StopTrack>) {
                static_cast<void>(scene.stop_track(value.entity_id, value.track));
            } else if constexpr (std::is_same_v<T, SetTrackVisible>) {
                static_cast<void>(scene.set_track_visibility(
                    value.entity_id, value.track,
                    value.visible ? TrackVisibility::Visible : TrackVisibility::Disabled));
            } else if constexpr (std::is_same_v<T, SetTrackVisibility>) {
                static_cast<void>(
                    scene.set_track_visibility(value.entity_id, value.track, value.visibility));
            } else if constexpr (std::is_same_v<T, SetAlpha>) {
                static_cast<void>(scene.set_track_alpha(value.entity_id, value.track, value.alpha));
            } else if constexpr (std::is_same_v<T, TweenAlpha>) {
                if (value.duration_ms <= 0.0f) {
                    static_cast<void>(
                        scene.set_track_alpha(value.entity_id, value.track, value.to));
                } else {
                    const float remaining = std::max(value.duration_ms - value.elapsed_ms, 0.0f);
                    const float consumed = std::min(delta_ms, remaining);
                    value.elapsed_ms += consumed;
                    const float progress =
                        std::clamp(value.elapsed_ms / value.duration_ms, 0.0f, 1.0f);
                    static_cast<void>(
                        scene.set_track_alpha(value.entity_id, value.track,
                                              value.from + ((value.to - value.from) * progress)));
                    if (value.elapsed_ms < value.duration_ms) {
                        return {};
                    }
                    return {.completed = true, .unused_ms = delta_ms - consumed};
                }
            } else if constexpr (std::is_same_v<T, SetTransform>) {
                static_cast<void>(
                    scene.set_track_transform(value.entity_id, value.track, value.transform));
            } else if constexpr (std::is_same_v<T, SpawnEffect>) {
                {
                    const auto& t = value.track;
                    const char* seq_name = t.player.sequence().name.c_str();
                    const auto& pb = t.playback;
                    std::string flags;
                    if (pb.flip_x)
                        flags += " flip_x";
                    if (pb.flip_y)
                        flags += " flip_y";
                    if (pb.reverse_playback)
                        flags += " reverse";
                    if (pb.rotation_degrees != 0.0f)
                        flags += " rot=" + std::to_string(static_cast<double>(pb.rotation_degrees));
                    if (!t.lifecycle_profile_id.empty())
                        flags += " lc=" + t.lifecycle_profile_id;
                    kLog->debug(
                        "anim_spawn_effect entity={} kind={} layer={} anchor={} role={} clip={}{}",
                        value.entity_id.value, to_string(t.kind), to_string(t.layer),
                        to_string(t.anchor), to_string(t.effect_role), seq_name, flags);
                }
                value.track.player.play();
                if (!scene.add_track(value.entity_id, std::move(value.track))) {
                    kLog->error("spawn_effect_failed entity={} reason=not_found_or_track_exists",
                                value.entity_id.value);
                }
            } else if constexpr (std::is_same_v<T, DespawnEffect>) {
                static_cast<void>(scene.remove_effect(value.effect_id));
            } else if constexpr (std::is_same_v<T, WaitTime>) {
                if (value.duration_ms > 0.0f) {
                    const float remaining = std::max(value.duration_ms - value.elapsed_ms, 0.0f);
                    const float consumed = std::min(delta_ms, remaining);
                    value.elapsed_ms += consumed;
                    if (value.elapsed_ms < value.duration_ms) {
                        return {};
                    }
                    return {.completed = true, .unused_ms = delta_ms - consumed};
                }
            } else if constexpr (std::is_same_v<T, WaitTrackComplete>) {
                const VisualTrack* track = scene.find_track(value.entity_id, value.track);
                if (track == nullptr) {
                    kLog->warn(
                        "wait_track_complete track_not_found entity={} completing_anyway=true",
                        value.entity_id.value);
                } else if (track->player.state() != AnimationPlayerState::Completed) {
                    return {};
                }
            } else if constexpr (std::is_same_v<T, SetLifeVisualState>) {
                static_cast<void>(scene.set_life_state(value.entity_id, value.state));
            } else if constexpr (std::is_same_v<T, EmitBattleEvent>) {
                emitted_events.push_back(value.event);
            }
            return {.completed = true, .unused_ms = delta_ms};
        },
        command);
}

StepResult update_sequence(Sequence& sequence, BattleScene& scene, float delta_ms,
                           std::vector<BattleVisualEvent>& emitted_events,
                           const AnimationClipStore*       clip_store) {
    float remaining = delta_ms;
    while (sequence.current < sequence.children.size()) {
        StepResult const result = update_node(sequence.children[sequence.current], scene, remaining,
                                              emitted_events, clip_store);
        if (!result.completed) {
            return {};
        }
        ++sequence.current;
        remaining = result.unused_ms;
    }
    return {.completed = true, .unused_ms = remaining};
}

StepResult update_parallel(Parallel& parallel, BattleScene& scene, float delta_ms,
                           std::vector<BattleVisualEvent>& emitted_events,
                           const AnimationClipStore*       clip_store) {
    if (parallel.children.empty()) {
        return {.completed = true, .unused_ms = delta_ms};
    }
    bool  all_complete = true;
    float unused = std::numeric_limits<float>::max();
    for (auto& child : parallel.children) {
        if (child.completed) {
            continue;
        }
        const StepResult result = update_node(child, scene, delta_ms, emitted_events, clip_store);
        all_complete = all_complete && result.completed;
        if (result.completed) {
            unused = std::min(unused, result.unused_ms);
        }
    }
    if (!all_complete) {
        return {};
    }
    return {.completed = true,
            .unused_ms = unused == std::numeric_limits<float>::max() ? delta_ms : unused};
}

StepResult update_node(CommandNode& node, BattleScene& scene, float delta_ms,
                       std::vector<BattleVisualEvent>& emitted_events,
                       const AnimationClipStore*       clip_store) {
    if (node.completed) {
        return {.completed = true, .unused_ms = delta_ms};
    }
    StepResult result = std::visit(
        [&](auto& value) -> StepResult {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, VisualCommand>) {
                return update_command(value, scene, delta_ms, emitted_events, clip_store);
            } else if constexpr (std::is_same_v<T, Sequence>) {
                return update_sequence(value, scene, delta_ms, emitted_events, clip_store);
            } else {
                return update_parallel(value, scene, delta_ms, emitted_events, clip_store);
            }
        },
        node.value);
    node.completed = result.completed;
    return result;
}

} // namespace

void CommandTimeline::enqueue(CommandNode command) {
    commands_.push_back(std::move(command));
}

void CommandTimeline::clear() noexcept {
    commands_.clear();
    emitted_events_.clear();
}

void CommandTimeline::update(BattleScene& scene, float delta_ms) {
    for (auto& command : commands_) {
        update_node(command, scene, std::max(delta_ms, 0.0f), emitted_events_, clip_store_);
    }
    std::erase_if(commands_, [](const CommandNode& command) { return command.completed; });
}

std::vector<BattleVisualEvent> CommandTimeline::drain_emitted_events() {
    std::vector<BattleVisualEvent> result;
    result.swap(emitted_events_);
    return result;
}

} // namespace d2engine
