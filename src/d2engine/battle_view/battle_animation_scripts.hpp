#pragma once

#include "battle_effect_role_set.hpp"
#include "battle_scene.hpp"
#include "battle_visual_profile_registry.hpp"
#include "battle_visual_event.hpp"
#include "unit_animation_role_set.hpp"
#include "unit_lifecycle_visual_profile.hpp"
#include "visual_command.hpp"

#include <map>
#include <optional>
#include <string>

namespace d2engine {

enum class EffectLifetime : std::uint8_t {
    TrackCompletion,
    Duration,
};

struct EffectScriptParameters {
    TrackKind         track = TrackKind::Effect;
    TrackRenderLayer  layer = TrackRenderLayer::Effect;
    AnchorPolicy      anchor = AnchorPolicy::UnitFoot;
    TrackVisibility   visibility = TrackVisibility::Visible;
    PlaybackTransform playback;
    EffectLifetime    lifetime = EffectLifetime::TrackCompletion;
    float             duration_ms = 0.0f;
    float             start_delay_ms = 0.0f;
    int               depth_bias = 0;
    bool              looping = false;
    bool              remove_on_complete = true;
};

struct TimedTrackSpec {
    VisualEntityId              entity_id;
    EffectInstanceId            effect_id;
    AnimationSequence           sequence;
    const LayeredAnimationClip* layered_clip = nullptr; // when set, overrides sequence
    TrackKind                   track = TrackKind::Effect;
    TrackRenderLayer            layer = TrackRenderLayer::Effect;
    AnchorPolicy                anchor = AnchorPolicy::UnitFoot;
    TrackVisibility             visibility = TrackVisibility::Visible;
    PlaybackTransform           playback;
    EffectLifetime              lifetime = EffectLifetime::TrackCompletion;
    float                       duration_ms = 0.0f;
    float                       start_delay_ms = 0.0f;
    int                         depth_bias = 0;
    int frame_delay = 0; // launch-time offset in frames: positive = later, negative = earlier;
                         // first visible frame is always frame 0
    bool        looping = false;
    bool        remove_on_complete = true;
    BindingRole effect_role = BindingRole::Global;
    std::optional<BattleEffectVisualRole>
                visual_role;          // resolved category for effect default lookup
    std::string lifecycle_profile_id; // set on lifecycle tracks for typed binding
};

struct BattleScriptParameters {
    float                  death_fade_ms = 500.0f;
    float                  revive_delay_ms = 500.0f;
    float                  revive_fade_ms = 1500.0f;
    EffectScriptParameters marker{.track = TrackKind::ActorMarker,
                                  .layer = TrackRenderLayer::Base,
                                  .anchor = AnchorPolicy::UnitFoot,
                                  .playback = {},
                                  .depth_bias = -1};
    EffectScriptParameters death{.track = TrackKind::DeathFx,
                                 .layer = TrackRenderLayer::Overlay,
                                 .anchor = AnchorPolicy::UnitFoot,
                                 .playback = {}};
    EffectScriptParameters revive{.track = TrackKind::ReviveFx,
                                  .layer = TrackRenderLayer::Overlay,
                                  .anchor = AnchorPolicy::UnitFoot,
                                  .playback = {},
                                  .lifetime = EffectLifetime::Duration,
                                  .duration_ms = 500.0f};
    EffectScriptParameters heff{.track = TrackKind::Effect,
                                .layer = TrackRenderLayer::Effect,
                                .anchor = AnchorPolicy::OppositeTeamCentroid,
                                .playback = {}};
    EffectScriptParameters tuch{.track = TrackKind::Effect,
                                .layer = TrackRenderLayer::Effect,
                                .anchor = AnchorPolicy::OppositeLaneMidpoint,
                                .playback = {}};
    EffectScriptParameters drain{.track = TrackKind::Effect,
                                 .layer = TrackRenderLayer::Effect,
                                 .anchor = AnchorPolicy::UnitFoot,
                                 .playback = {}};
    EffectScriptParameters heal{.track = TrackKind::Effect,
                                .layer = TrackRenderLayer::Effect,
                                .anchor = AnchorPolicy::UnitFoot,
                                .playback = {}};
    // Per-sequence launch-time frame offsets (sequence_name → signed frame count); positive =
    // later, negative = earlier
    std::map<std::string, int> sequence_frame_delays;
};

struct BattleScriptAssets {
    const UnitVisualProfileRegistry*          unit_profiles = nullptr;
    const UnitLifecycleVisualProfileRegistry* lifecycle_profiles = nullptr;
    const BattleEffectRoleSet*                effects = nullptr;
    const AnimationSequence*                  marker = nullptr; // legacy — same as marker_large
    const AnimationSequence*                  marker_small = nullptr; // MRKCURSMALLA
    const AnimationSequence*                  marker_large = nullptr; // MRKCURLARGEA
    const AnimationSequence*                  drain = nullptr;
    const AnimationSequence*                  heal = nullptr;

    [[nodiscard]] const AnimationSequence* marker_for_size(bool is_large) const noexcept {
        const AnimationSequence* specific = is_large ? marker_large : marker_small;
        if (specific != nullptr && !specific->frames.empty()) {
            return specific;
        }
        return marker;
    }
};

struct BattleScriptContext {
    const BattleScene&     scene;
    BattleScriptAssets     assets;
    BattleScriptParameters parameters;
};

class BattleAnimationScripts {
public:
    [[nodiscard]] static std::optional<CommandNode> build(const BattleVisualEvent&   event,
                                                          const BattleScriptContext& context);
    [[nodiscard]] static CommandNode                build_timed_track(TimedTrackSpec spec);
};

} // namespace d2engine
