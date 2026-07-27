#pragma once

#include "animation_clip_ref.hpp"
#include "battle_ids.hpp"
#include "battle_visual_event.hpp"
#include "layered_animation_clip.hpp"
#include "life_visual_state.hpp"
#include "visual_track.hpp"
#include "../animation/animation_sequence.hpp"

#include <cstddef>
#include <variant>
#include <vector>

namespace d2engine {

// ClipPayload: carries either a concrete sequence, a handle, or a pointer to a layered clip.
// Transitional: AnimationSequence will be phased out once all callers migrate to
// LayeredAnimationClip. The layered-clip pointer must outlive all command nodes that hold it.
using ClipPayload =
    std::variant<AnimationSequence, AnimationClipHandle, const LayeredAnimationClip*>;

struct PlayClip {
    VisualEntityId entity_id;
    TrackSelector  track = TrackSelector::singleton(TrackKind::Base);
    ClipPayload    clip;
    bool           looping = false;
    // Propagated to track.effect_role so renderer can key binding without sequence_name inference
    BindingRole unit_anim_role = BindingRole::UnitBase;
};

struct StopTrack {
    VisualEntityId entity_id;
    TrackSelector  track = TrackSelector::singleton(TrackKind::Base);
};

struct SetTrackVisible {
    VisualEntityId entity_id;
    TrackSelector  track = TrackSelector::singleton(TrackKind::Base);
    bool           visible = true;
};

struct SetTrackVisibility {
    VisualEntityId  entity_id;
    TrackSelector   track = TrackSelector::singleton(TrackKind::Base);
    TrackVisibility visibility = TrackVisibility::Visible;
};

struct SetAlpha {
    VisualEntityId entity_id;
    TrackSelector  track = TrackSelector::singleton(TrackKind::Base);
    float          alpha = 1.0f;
};

struct TweenAlpha {
    VisualEntityId entity_id;
    TrackSelector  track = TrackSelector::singleton(TrackKind::Base);
    float          from = 1.0f;
    float          to = 1.0f;
    float          duration_ms = 0.0f;
    float          elapsed_ms = 0.0f;
};

struct SetTransform {
    VisualEntityId  entity_id;
    TrackSelector   track = TrackSelector::singleton(TrackKind::Base);
    VisualTransform transform;
};

struct SpawnEffect {
    VisualEntityId entity_id;
    VisualTrack    track;
};

struct DespawnEffect {
    EffectInstanceId effect_id;
};

struct WaitTime {
    float duration_ms = 0.0f;
    float elapsed_ms = 0.0f;
};

struct WaitTrackComplete {
    VisualEntityId entity_id;
    TrackSelector  track = TrackSelector::singleton(TrackKind::Base);
};

struct SetLifeVisualState {
    VisualEntityId  entity_id;
    LifeVisualState state = LifeVisualState::Alive;
};

struct EmitBattleEvent {
    BattleVisualEvent event;
};

using VisualCommand =
    std::variant<PlayClip, StopTrack, SetTrackVisible, SetTrackVisibility, SetAlpha, TweenAlpha,
                 SetTransform, SpawnEffect, DespawnEffect, WaitTime, WaitTrackComplete,
                 SetLifeVisualState, EmitBattleEvent>;

struct CommandNode;

struct Sequence {
    std::vector<CommandNode> children;
    std::size_t              current = 0;
};

struct Parallel {
    std::vector<CommandNode> children;
};

struct CommandNode {
    std::variant<VisualCommand, Sequence, Parallel> value;
    bool                                            completed = false;

    static CommandNode command(VisualCommand command) {
        return CommandNode{.value = std::move(command)};
    }
    static CommandNode sequence(std::vector<CommandNode> children) {
        return CommandNode{.value = Sequence{.children = std::move(children)}};
    }
    static CommandNode parallel(std::vector<CommandNode> children) {
        return CommandNode{.value = Parallel{.children = std::move(children)}};
    }
};

} // namespace d2engine
