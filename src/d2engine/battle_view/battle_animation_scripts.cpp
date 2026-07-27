// Animation scripting policy:
// Current battle animation roles/timings are reverse-engineered and must remain
// deterministic C++/data-driven until the visual model stabilizes.
// Do not introduce Lua or another scripting runtime for battle animation as a
// shortcut. If scriptability is needed later, first design a declarative data
// format and tests for existing S1/A1/A2/impact/FX semantics.

#include "battle_animation_scripts.hpp"

#include <d2log/log.hpp>

#include <cctype>
#include <limits>
#include <type_traits>
#include <utility>

namespace d2engine {
namespace {

auto kLog = d2log::get("d2.render"); // NOLINT(cert-err58-cpp)

enum class EffectSlot : std::uint32_t {
    ActorMarker = 1,
    TargetMarker = 2,
    Death = 3,
    Body = 4,
    Revive = 5,
    Cast = 6,
    Drain = 7,
    Heal = 8,
    BattleEffectSourceUnit = 9,           // SourceCastFx (TUCH): anchored to source unit
    BattleEffectTeamOverlay = 10,         // TeamOverlayFx: team-level overlay
    BattleEffectTargetDamage = 11,        // TargetDamageFx: anchored to target unit
    BattleEffectSourceAttackOverlay = 12, // SourceAttackOverlayFx: override source overlay
    BattleEffectTeamAttack = 13,          // TeamAttackOverlayFx: single shared AoE attack overlay
};

struct ResolvedUnit {
    VisualEntityId    entity_id;
    const BattleUnit* unit = nullptr;
};

CommandNode command(VisualCommand value) {
    return CommandNode::command(std::move(value));
}

std::optional<ResolvedUnit> resolve(const BattleScene& scene, UnitInstanceId unit_id) {
    const auto entity_id = scene.visual_entity_for(unit_id);
    if (!entity_id.has_value()) {
        return std::nullopt;
    }
    const BattleUnit* unit = scene.try_unit_by_id(*entity_id);
    if (unit == nullptr) {
        return std::nullopt;
    }
    return ResolvedUnit{.entity_id = *entity_id, .unit = unit};
}

const UnitAnimationRoleSet* roles_for(const BattleScriptContext& context,
                                      const ResolvedUnit&        unit) {
    return context.assets.unit_profiles != nullptr
               ? context.assets.unit_profiles->roles(unit.unit->visual_profile_id)
               : nullptr;
}

const UnitLifecycleVisualProfile* lifecycle_for(const BattleScriptContext& context,
                                                const ResolvedUnit&        unit) {
    return context.assets.lifecycle_profiles != nullptr
               ? context.assets.lifecycle_profiles->lifecycle(unit.unit->lifecycle_profile_id)
               : nullptr;
}

std::optional<EffectInstanceId> effect_id(UnitInstanceId unit_id, EffectSlot slot) {
    constexpr std::uint32_t stride = 16;
    const auto              offset = static_cast<std::uint32_t>(slot);
    if (unit_id.value == 0 ||
        unit_id.value > (std::numeric_limits<std::uint32_t>::max() - offset) / stride) {
        return std::nullopt;
    }
    return EffectInstanceId{(unit_id.value * stride) + offset};
}

AnimationSequence corpse_sequence(const std::string& base) {
    AnimationSequence sequence;
    if (base.empty()) {
        return sequence;
    }
    sequence.name = base; // unique per unit type so per-item tuning works correctly
    sequence.container_path = "Imgs/Battle.ff";
    sequence.frames.push_back(
        {.image_name = base + "00", .index = 0, .duration_ms = static_cast<std::uint16_t>(100)});
    sequence.frames.push_back(
        {.image_name = base + "01", .index = 1, .duration_ms = static_cast<std::uint16_t>(100)});
    return sequence;
}

float sequence_duration_ms(const AnimationSequence& sequence) {
    float total = 0.0F;
    for (const auto& frame : sequence.frames) {
        total += static_cast<float>(frame.duration_ms);
    }
    return total;
}

// Returns duration of A1 layer; 0 if absent.
float clip_a1_duration_ms(const LayeredAnimationClip& clip) {
    if (!clip.a1.has_value() || clip.a1->frames.empty()) {
        return 0.0F;
    }
    return sequence_duration_ms(*clip.a1);
}

// Returns the name of the driver layer (A1 if present, else longest layer); empty if clip empty.
const std::string& clip_driver_name(const LayeredAnimationClip& clip) {
    static const std::string kEmpty;
    const AnimationSequence* drv = driver_sequence(clip);
    return (drv != nullptr) ? drv->name : kEmpty;
}

VisualTrack effect_track(TimedTrackSpec spec) {
    VisualTrack track;
    track.id = TrackId{spec.effect_id.value};
    track.kind = spec.track;
    track.layer = spec.layer;
    track.anchor = spec.anchor;
    track.visibility = spec.visibility;
    track.playback = spec.playback;
    track.depth_bias = spec.depth_bias;
    track.effect_id = spec.effect_id;
    track.effect_role = spec.effect_role;
    track.visual_role = spec.visual_role;
    track.lifecycle_profile_id = std::move(spec.lifecycle_profile_id);

    if (spec.layered_clip != nullptr && !spec.layered_clip->is_empty()) {
        const bool effective_looping = spec.looping || spec.layered_clip->loop_policy;
        track.layered_player = {.clip = spec.layered_clip,
                                .elapsed_ms = 0,
                                .looping = effective_looping,
                                .reverse_playback = spec.playback.reverse_playback};
        const AnimationSequence* drv = driver_sequence(*spec.layered_clip);
        if (drv != nullptr) {
            AnimationSequence seq = *drv;
            seq.is_looping = effective_looping;
            track.player.load(std::move(seq));
            track.player.set_reverse_playback(track.playback.reverse_playback);
        }
    } else {
        AnimationSequence sequence = std::move(spec.sequence);
        sequence.is_looping = spec.looping;
        track.player.load(std::move(sequence));
        track.player.set_reverse_playback(track.playback.reverse_playback);
    }
    return track;
}

TimedTrackSpec timed_track_spec(VisualEntityId entity_id, EffectInstanceId id,
                                AnimationSequence             sequence,
                                const EffectScriptParameters& parameters) {
    return TimedTrackSpec{.entity_id = entity_id,
                          .effect_id = id,
                          .sequence = std::move(sequence),
                          .track = parameters.track,
                          .layer = parameters.layer,
                          .anchor = parameters.anchor,
                          .visibility = parameters.visibility,
                          .playback = parameters.playback,
                          .lifetime = parameters.lifetime,
                          .duration_ms = parameters.duration_ms,
                          .start_delay_ms = parameters.start_delay_ms,
                          .depth_bias = parameters.depth_bias,
                          .looping = parameters.looping,
                          .remove_on_complete = parameters.remove_on_complete};
}

// Build a TimedTrackSpec carrying a layered clip pointer.
TimedTrackSpec timed_track_spec_layered(VisualEntityId entity_id, EffectInstanceId id,
                                        const LayeredAnimationClip&   clip,
                                        const EffectScriptParameters& parameters) {
    TimedTrackSpec ts = timed_track_spec(entity_id, id, AnimationSequence{}, parameters);
    ts.layered_clip = &clip;
    return ts;
}

CommandNode effect_command(VisualEntityId entity_id, EffectInstanceId id,
                           const AnimationSequence&      sequence,
                           const EffectScriptParameters& parameters) {
    return BattleAnimationScripts::build_timed_track(
        timed_track_spec(entity_id, id, sequence, parameters));
}

VisualTrack effect_track(const EffectScriptParameters& parameters, EffectInstanceId id,
                         AnimationSequence sequence) {
    return effect_track(timed_track_spec(VisualEntityId{}, id, std::move(sequence), parameters));
}

std::vector<CommandNode> timed_track_commands(TimedTrackSpec spec) {
    const VisualEntityId   entity_id = spec.entity_id;
    const EffectInstanceId effect_id = spec.effect_id;
    const EffectLifetime   lifetime = spec.lifetime;
    const float            duration_ms = spec.duration_ms;
    const bool             remove_on_complete = spec.remove_on_complete;

    // frame_delay is a signed launch-time offset in frames (positive = later, negative = earlier)
    const int frame_delay = spec.frame_delay;

    const float avg_frame_ms = [&]() -> float {
        if (spec.layered_clip != nullptr) {
            const AnimationSequence* drv = driver_sequence(*spec.layered_clip);
            if (drv != nullptr && !drv->frames.empty()) {
                return sequence_duration_ms(*drv) / static_cast<float>(drv->frames.size());
            }
        }
        if (!spec.sequence.frames.empty()) {
            return sequence_duration_ms(spec.sequence) /
                   static_cast<float>(spec.sequence.frames.size());
        }
        return 40.0F; // 25 fps fallback
    }();

    const float offset_ms = static_cast<float>(frame_delay) * avg_frame_ms;
    const float total_delay_ms = std::max(spec.start_delay_ms + offset_ms, 0.0F);

    std::vector<CommandNode> result;
    if (total_delay_ms > 0.0F) {
        result.push_back(command(WaitTime{.duration_ms = total_delay_ms}));
    }
    result.push_back(
        command(SpawnEffect{.entity_id = entity_id, .track = effect_track(std::move(spec))}));
    if (lifetime == EffectLifetime::Duration) {
        result.push_back(command(WaitTime{.duration_ms = duration_ms}));
    } else {
        result.push_back(command(WaitTrackComplete{
            .entity_id = entity_id, .track = TrackSelector::by_id(TrackId{effect_id.value})}));
    }
    if (remove_on_complete) {
        result.push_back(command(DespawnEffect{.effect_id = effect_id}));
    }
    return result;
}

std::optional<CommandNode> build_selection(UnitInstanceId previous, UnitInstanceId selected_id,
                                           EffectSlot slot, TrackKind marker_kind,
                                           const BattleScriptContext& context) {
    const auto selected = resolve(context.scene, selected_id);
    const auto selected_effect = effect_id(selected_id, slot);
    if (!selected.has_value() || !selected_effect.has_value()) {
        return std::nullopt;
    }

    // Pick marker animation based on unit size
    const bool               is_large = selected->unit != nullptr && selected->unit->is_large;
    const AnimationSequence* marker_seq = context.assets.marker_for_size(is_large);
    if (marker_seq == nullptr || marker_seq->frames.empty()) {
        return std::nullopt;
    }

    std::vector<CommandNode> commands;
    if (const auto previous_effect = effect_id(previous, slot); previous_effect.has_value()) {
        commands.push_back(command(DespawnEffect{.effect_id = *previous_effect}));
    }
    auto parameters = context.parameters.marker;
    parameters.track = marker_kind;
    VisualTrack marker = effect_track(parameters, *selected_effect, *marker_seq);
    marker.effect_role = (marker_kind == TrackKind::ActorMarker) ? BindingRole::SelectionMarker
                                                                 : BindingRole::TargetMarker;
    if (marker_kind == TrackKind::TargetMarker) {
        marker.alpha = 0.75F;
        marker.transform.scale_x = 1.15F;
        marker.transform.scale_y = 1.15F;
    }
    commands.push_back(
        command(SpawnEffect{.entity_id = selected->entity_id, .track = std::move(marker)}));
    return CommandNode::sequence(std::move(commands));
}

std::optional<CommandNode> build_hit(const UnitHitReceived&     event,
                                     const BattleScriptContext& context);
std::optional<CommandNode> build_kill(const UnitKilled& event, const BattleScriptContext& context);

std::optional<CommandNode> build_attack(const AttackStarted&       event,
                                        const BattleScriptContext& context) {
    const auto attacker = resolve(context.scene, event.source);
    if (!attacker.has_value()) {
        return std::nullopt;
    }
    const auto* attacker_roles = roles_for(context, *attacker);
    if (attacker_roles == nullptr || attacker_roles->attack.is_empty() ||
        attacker_roles->idle.is_empty()) {
        return std::nullopt;
    }
    const float impact_ms = clip_a1_duration_ms(attacker_roles->attack.clip) * 0.5F;

    return CommandNode::sequence(
        {command(PlayClip{.entity_id = attacker->entity_id,
                          .track = TrackKind::Base,
                          .clip = &attacker_roles->attack.clip,
                          .unit_anim_role = BindingRole::UnitAttack}),
         command(WaitTime{.duration_ms = impact_ms}),
         command(EmitBattleEvent{.event =
                                     AttackImpactCue{
                                         .attack_id = event.attack_id,
                                         .source = event.source,
                                         .targets = event.targets,
                                     }}),
         command(WaitTrackComplete{.entity_id = attacker->entity_id, .track = TrackKind::Base}),
         command(PlayClip{.entity_id = attacker->entity_id,
                          .track = TrackKind::Base,
                          .clip = &attacker_roles->idle.clip,
                          .looping = true,
                          .unit_anim_role = BindingRole::UnitIdle})});
}

std::optional<CommandNode> build_target_damaged(const TargetDamaged&       event,
                                                const BattleScriptContext& context) {
    static_cast<void>(event);
    static_cast<void>(context);
    return std::nullopt;
}

std::optional<CommandNode> build_target_killed(const TargetKilled&        event,
                                               const BattleScriptContext& context) {
    return build_kill(UnitKilled{.target = event.target}, context);
}

std::optional<CommandNode> build_hit(const UnitHitReceived&     event,
                                     const BattleScriptContext& context) {
    const auto target = resolve(context.scene, event.target);
    if (!target.has_value()) {
        kLog->debug("build_hit unit_not_found unit={} required_failure=true", event.target.value);
        return std::nullopt;
    }
    const auto* roles = roles_for(context, *target);
    if (roles == nullptr) {
        kLog->debug("build_hit no_visual_profile unit={} profile_id={} required_failure=true",
                    event.target.value, target->unit->visual_profile_id.value);
        return std::nullopt;
    }
    if (roles->hit.is_empty()) {
        // Non-critical: missing hit clip — skip visual, keep sequence alive
        kLog->debug("build_hit no_hit_clip unit={} profile_id={} skipping_visual_hit=true",
                    event.target.value, target->unit->visual_profile_id.value);
        if (!roles->idle.is_empty()) {
            return command(PlayClip{.entity_id = target->entity_id,
                                    .track = TrackKind::Base,
                                    .clip = &roles->idle.clip,
                                    .looping = true,
                                    .unit_anim_role = BindingRole::UnitIdle});
        }
        return command(WaitTime{.duration_ms = 0.0f});
    }
    if (roles->idle.is_empty()) {
        kLog->debug("build_hit no_idle_clip unit={} playing_hit_only=true", event.target.value);
        return command(PlayClip{.entity_id = target->entity_id,
                                .track = TrackKind::Base,
                                .clip = &roles->hit.clip,
                                .unit_anim_role = BindingRole::UnitHit});
    }
    return CommandNode::sequence(
        {command(PlayClip{.entity_id = target->entity_id,
                          .track = TrackKind::Base,
                          .clip = &roles->hit.clip,
                          .unit_anim_role = BindingRole::UnitHit}),
         command(WaitTrackComplete{.entity_id = target->entity_id, .track = TrackKind::Base}),
         command(PlayClip{.entity_id = target->entity_id,
                          .track = TrackKind::Base,
                          .clip = &roles->idle.clip,
                          .looping = true,
                          .unit_anim_role = BindingRole::UnitIdle})});
}

std::optional<CommandNode> build_kill(const UnitKilled& event, const BattleScriptContext& context) {
    const auto target = resolve(context.scene, event.target);
    if (!target.has_value()) {
        kLog->debug("build_kill unit_not_found unit={}", event.target.value);
        return std::nullopt;
    }

    const auto* lifecycle_profile = lifecycle_for(context, *target);
    if (lifecycle_profile == nullptr) {
        kLog->debug("build_kill no_lifecycle_profile unit={} lc_id={}", event.target.value,
                    target->unit->lifecycle_profile_id.value);
        return std::nullopt;
    }
    const std::string* profile_name =
        context.assets.lifecycle_profiles != nullptr
            ? context.assets.lifecycle_profiles->name(target->unit->lifecycle_profile_id)
            : nullptr;
    const std::string lc_profile_id = profile_name != nullptr ? *profile_name : "";

    // STIL path: play animated death-pose, then hide Base and spawn corpse DeathBody.
    if (!lifecycle_profile->death.stil_clip.is_empty()) {
        const bool multi_frame = lifecycle_profile->death.stil_clip.clip.a1.has_value()
                                     ? lifecycle_profile->death.stil_clip.clip.a1->frames.size() > 1
                                     : false;
        std::vector<CommandNode> stil_seq{
            command(PlayClip{.entity_id = target->entity_id,
                             .track = TrackKind::Base,
                             .clip = &lifecycle_profile->death.stil_clip.clip,
                             .looping = lifecycle_profile->death.stil_clip.clip.loop_policy,
                             .unit_anim_role = BindingRole::UnitBase})};
        if (multi_frame) {
            stil_seq.push_back(
                command(WaitTrackComplete{.entity_id = target->entity_id,
                                          .track = TrackSelector::singleton(TrackKind::Base)}));
        } else {
            stil_seq.push_back(command(WaitTime{.duration_ms = context.parameters.death_fade_ms}));
        }
        stil_seq.push_back(
            command(SetTrackVisibility{.entity_id = target->entity_id,
                                       .track = TrackSelector::singleton(TrackKind::Base),
                                       .visibility = TrackVisibility::Disabled}));

        const std::string& corpse_base = lifecycle_profile->death.corpse_sprite_base;
        if (!corpse_base.empty()) {
            AnimationSequence corpse = lifecycle_profile->death.corpse.frames.empty()
                                           ? corpse_sequence(corpse_base)
                                           : lifecycle_profile->death.corpse;
            const auto        body_id = effect_id(event.target, EffectSlot::Body);
            if (!corpse.frames.empty() && body_id.has_value()) {
                auto body_params = context.parameters.death;
                body_params.track = TrackKind::DeathBody;
                body_params.layer = TrackRenderLayer::Base;
                auto body_track = effect_track(body_params, *body_id, std::move(corpse));
                body_track.lifecycle_profile_id = lc_profile_id;
                stil_seq.push_back(command(
                    SpawnEffect{.entity_id = target->entity_id, .track = std::move(body_track)}));
            }
        }
        stil_seq.push_back(command(
            SetLifeVisualState{.entity_id = target->entity_id, .state = LifeVisualState::Dead}));

        if (lifecycle_profile->death.death_fx.frames.empty()) {
            return CommandNode::sequence(std::move(stil_seq));
        }
        const auto death_fx_id = effect_id(event.target, EffectSlot::Death);
        if (!death_fx_id.has_value()) {
            return CommandNode::sequence(std::move(stil_seq));
        }
        TimedTrackSpec fx_ts =
            timed_track_spec(target->entity_id, *death_fx_id, lifecycle_profile->death.death_fx,
                             context.parameters.death);
        fx_ts.lifecycle_profile_id = lc_profile_id;
        return CommandNode::parallel({BattleAnimationScripts::build_timed_track(std::move(fx_ts)),
                                      CommandNode::sequence(std::move(stil_seq))});
    }

    // Fallback: fade-out, disable Base, spawn static DeathBody from Battle.ff corpse sprite.
    std::vector<CommandNode> death{
        command(SetLifeVisualState{.entity_id = target->entity_id,
                                   .state = LifeVisualState::FadingOut}),
        command(TweenAlpha{.entity_id = target->entity_id,
                           .track = TrackKind::Base,
                           .from = 1.0f,
                           .to = 0.0f,
                           .duration_ms = context.parameters.death_fade_ms}),
        command(SetLifeVisualState{.entity_id = target->entity_id, .state = LifeVisualState::Dead}),
        command(SetAlpha{.entity_id = target->entity_id, .track = TrackKind::Base, .alpha = 1.0f}),
        command(SetTrackVisibility{.entity_id = target->entity_id,
                                   .track = TrackKind::Base,
                                   .visibility = TrackVisibility::Disabled})};

    const std::string& corpse_base = lifecycle_profile->death.corpse_sprite_base;
    if (!corpse_base.empty()) {
        AnimationSequence corpse = lifecycle_profile->death.corpse.frames.empty()
                                       ? corpse_sequence(corpse_base)
                                       : lifecycle_profile->death.corpse;
        const auto        id = effect_id(event.target, EffectSlot::Body);
        if (!corpse.frames.empty() && id.has_value()) {
            auto body_parameters = context.parameters.death;
            body_parameters.track = TrackKind::DeathBody;
            body_parameters.layer = TrackRenderLayer::Base;
            auto body_track = effect_track(body_parameters, *id, std::move(corpse));
            body_track.lifecycle_profile_id = lc_profile_id;
            death.push_back(command(
                SpawnEffect{.entity_id = target->entity_id, .track = std::move(body_track)}));
        }
    }
    if (lifecycle_profile->death.death_fx.frames.empty()) {
        return CommandNode::sequence(std::move(death));
    }
    const auto death_fx_id = effect_id(event.target, EffectSlot::Death);
    if (!death_fx_id.has_value()) {
        return CommandNode::sequence(std::move(death));
    }
    TimedTrackSpec death_fx_ts =
        timed_track_spec(target->entity_id, *death_fx_id, lifecycle_profile->death.death_fx,
                         context.parameters.death);
    death_fx_ts.lifecycle_profile_id = lc_profile_id;
    return CommandNode::parallel({BattleAnimationScripts::build_timed_track(std::move(death_fx_ts)),
                                  CommandNode::sequence(std::move(death))});
}

std::optional<CommandNode> build_revive_started(const UnitReviveStarted&   event,
                                                const BattleScriptContext& context) {
    const auto target = resolve(context.scene, event.target);
    if (!target.has_value()) {
        return std::nullopt;
    }
    const auto* lp = lifecycle_for(context, *target);
    const bool  large = target->unit->is_large;
    const auto& seq =
        (lp != nullptr) ? (large ? lp->revive.large_fx : lp->revive.small_fx) : AnimationSequence{};
    if (lp == nullptr || seq.frames.empty()) {
        const char* fx_name = (lp != nullptr) ? (large ? lp->revive.large_fx_name.c_str()
                                                       : lp->revive.small_fx_name.c_str())
                                              : "";
        kLog->debug("build_revive_started no_revive_fx unit={} fx_name={}", event.target.value,
                    fx_name);
        return std::nullopt;
    }
    const auto id = effect_id(event.target, EffectSlot::Revive);
    if (!id.has_value()) {
        return std::nullopt;
    }
    const std::string* rp_name =
        (context.assets.lifecycle_profiles != nullptr)
            ? context.assets.lifecycle_profiles->name(target->unit->lifecycle_profile_id)
            : nullptr;
    VisualTrack revive_track = effect_track(context.parameters.revive, *id, seq);
    revive_track.lifecycle_profile_id = (rp_name != nullptr) ? *rp_name : "";
    revive_track.effect_role = large ? BindingRole::ReviveLarge : BindingRole::ReviveSmall;
    return CommandNode::sequence(
        {command(SetLifeVisualState{.entity_id = target->entity_id,
                                    .state = LifeVisualState::Reviving}),
         command(SpawnEffect{.entity_id = target->entity_id, .track = std::move(revive_track)})});
}

std::optional<CommandNode> build_revived(const UnitRevived&         event,
                                         const BattleScriptContext& context) {
    const auto target = resolve(context.scene, event.target);
    if (!target.has_value()) {
        return std::nullopt;
    }
    const auto* roles = roles_for(context, *target);
    if (roles == nullptr || roles->idle.is_empty()) {
        return std::nullopt;
    }
    const auto body_id = effect_id(event.target, EffectSlot::Body);
    const auto revive_id = effect_id(event.target, EffectSlot::Revive);
    if (!body_id.has_value() || !revive_id.has_value()) {
        return std::nullopt;
    }
    return CommandNode::sequence(
        {command(WaitTime{.duration_ms = context.parameters.revive_delay_ms}),
         command(DespawnEffect{.effect_id = *revive_id}),
         command(DespawnEffect{.effect_id = *body_id}),
         command(SetTrackVisibility{.entity_id = target->entity_id,
                                    .track = TrackKind::Base,
                                    .visibility = TrackVisibility::Visible}),
         command(SetAlpha{.entity_id = target->entity_id, .track = TrackKind::Base, .alpha = 0.0f}),
         command(PlayClip{.entity_id = target->entity_id,
                          .track = TrackKind::Base,
                          .clip = &roles->idle.clip,
                          .looping = true,
                          .unit_anim_role = BindingRole::UnitIdle}),
         command(TweenAlpha{.entity_id = target->entity_id,
                            .track = TrackKind::Base,
                            .from = 0.0f,
                            .to = 1.0f,
                            .duration_ms = context.parameters.revive_fade_ms}),
         command(
             SetLifeVisualState{.entity_id = target->entity_id, .state = LifeVisualState::Alive})});
}

std::optional<CommandNode> build_cast(const CastEffectStarted&   event,
                                      const BattleScriptContext& context) {
    const auto caster = resolve(context.scene, event.caster);
    if (!caster.has_value()) {
        return std::nullopt;
    }
    const auto* roles = roles_for(context, *caster);
    if (roles == nullptr) {
        return std::nullopt;
    }
    const bool              heff = event.role == BattleEffectRole::Heff;
    const BattleEffectClip& effect_clip = heff ? roles->heff : roles->tuch;
    const auto&             parameters = heff ? context.parameters.heff : context.parameters.tuch;
    const auto              id = effect_id(event.caster, EffectSlot::Cast);
    if (effect_clip.is_empty() || !id.has_value()) {
        return std::nullopt;
    }
    TimedTrackSpec ts =
        timed_track_spec_layered(caster->entity_id, *id, effect_clip.clip, parameters);
    ts.effect_role = heff ? BindingRole::TargetTeam : BindingRole::Source;
    const std::string& seq_name = clip_driver_name(effect_clip.clip);
    const auto         delay_it = context.parameters.sequence_frame_delays.find(seq_name);
    if (delay_it != context.parameters.sequence_frame_delays.end()) {
        ts.frame_delay = delay_it->second;
    }
    return BattleAnimationScripts::build_timed_track(std::move(ts));
}

std::optional<CommandNode> build_battle_effect(const BattleEffectStarted& event,
                                               const BattleScriptContext& context) {
    const auto source = resolve(context.scene, event.source);
    if (!source.has_value()) {
        return std::nullopt;
    }
    const auto* roles = roles_for(context, *source);
    if (roles == nullptr) {
        return std::nullopt;
    }

    // Select clip: override-aware for TargetDamageFx; otherwise use role family.
    auto select_clip = [&](const BattleEffectClip& fallback) -> const BattleEffectClip* {
        if (event.visual_role == BattleEffectVisualRole::TargetDamageFx &&
            roles->attack_fx_override && !roles->attack_fx_override->target_damage_fx.is_empty()) {
            return &roles->attack_fx_override->target_damage_fx;
        }
        return fallback.is_empty() ? nullptr : &fallback;
    };
    const bool              heff = event.role == BattleEffectRole::Heff;
    const BattleEffectClip* effect_clip_ptr = select_clip(heff ? roles->heff : roles->tuch);

    auto frame_delay_for = [&](const BattleEffectClip& clip) -> int {
        const std::string& seq_name = clip_driver_name(clip.clip);
        const auto         it = context.parameters.sequence_frame_delays.find(seq_name);
        return it != context.parameters.sequence_frame_delays.end() ? it->second : 0;
    };

    switch (event.visual_role) {
    case BattleEffectVisualRole::SourceCastFx: {
        if (effect_clip_ptr == nullptr)
            return std::nullopt;
        const auto id = effect_id(event.source, EffectSlot::BattleEffectSourceUnit);
        if (!id.has_value())
            return std::nullopt;
        auto params = context.parameters.tuch;
        params.anchor = AnchorPolicy::UnitFoot;
        TimedTrackSpec ts =
            timed_track_spec_layered(source->entity_id, *id, effect_clip_ptr->clip, params);
        ts.effect_role = BindingRole::Source;
        ts.visual_role = BattleEffectVisualRole::SourceCastFx;
        ts.frame_delay = frame_delay_for(*effect_clip_ptr);
        return BattleAnimationScripts::build_timed_track(std::move(ts));
    }
    case BattleEffectVisualRole::TargetDamageFx: {
        if (effect_clip_ptr == nullptr)
            return std::nullopt;
        const auto target = resolve(context.scene, event.target);
        if (!target.has_value())
            return std::nullopt;
        const auto id = effect_id(event.target, EffectSlot::BattleEffectTargetDamage);
        if (!id.has_value())
            return std::nullopt;
        auto params = context.parameters.heff;
        params.anchor = AnchorPolicy::UnitFoot;
        TimedTrackSpec ts =
            timed_track_spec_layered(target->entity_id, *id, effect_clip_ptr->clip, params);
        ts.effect_role = BindingRole::Target;
        ts.visual_role = BattleEffectVisualRole::TargetDamageFx;
        ts.frame_delay = frame_delay_for(*effect_clip_ptr);
        return BattleAnimationScripts::build_timed_track(std::move(ts));
    }
    case BattleEffectVisualRole::TeamOverlayFx: {
        if (effect_clip_ptr == nullptr)
            return std::nullopt;
        if (event.targets.units.empty())
            return std::nullopt;
        const auto id = effect_id(event.source, EffectSlot::BattleEffectTeamOverlay);
        if (!id.has_value())
            return std::nullopt;
        auto params = heff ? context.parameters.heff : context.parameters.tuch;
        params.anchor = AnchorPolicy::OppositeTeamCentroid;
        params.layer = TrackRenderLayer::Overlay;
        TimedTrackSpec ts =
            timed_track_spec_layered(source->entity_id, *id, effect_clip_ptr->clip, params);
        ts.effect_role = BindingRole::TargetTeam;
        ts.visual_role = BattleEffectVisualRole::TeamOverlayFx;
        ts.frame_delay = frame_delay_for(*effect_clip_ptr);
        return BattleAnimationScripts::build_timed_track(std::move(ts));
    }
    case BattleEffectVisualRole::SourceAttackOverlayFx: {
        if (!roles->attack_fx_override)
            return std::nullopt;
        const auto& clip = roles->attack_fx_override->source_attack_overlay;
        if (clip.is_empty())
            return std::nullopt;
        const auto id = effect_id(event.source, EffectSlot::BattleEffectSourceAttackOverlay);
        if (!id.has_value())
            return std::nullopt;
        auto params = context.parameters.tuch;
        params.anchor = AnchorPolicy::UnitFoot;
        const char         render_side = source->unit->direction;
        const char         resolved_dir = clip.direction_or_variant;
        const char         requested = clip.requested_direction;
        const bool         did_fallback = (resolved_dir != requested);
        const std::string& seq_name = clip_driver_name(clip.clip);
        kLog->debug("effect_spawn unit={} role=SourceAttackOverlayFx family={} "
                    "source_side={} requested={} actual={} selected={} fallback={} "
                    "placement_side={}",
                    event.source.value, clip.family, render_side, requested, resolved_dir, seq_name,
                    did_fallback ? "yes" : "no",
                    static_cast<char>(std::tolower(static_cast<unsigned char>(render_side))));
        TimedTrackSpec ts = timed_track_spec_layered(source->entity_id, *id, clip.clip, params);
        ts.effect_role = BindingRole::Source;
        ts.visual_role = BattleEffectVisualRole::SourceAttackOverlayFx;
        ts.frame_delay = frame_delay_for(clip);
        return BattleAnimationScripts::build_timed_track(std::move(ts));
    }
    case BattleEffectVisualRole::TeamAttackOverlayFx: {
        if (!roles->attack_fx_override)
            return std::nullopt;
        const auto& clip = roles->attack_fx_override->team_attack_overlay;
        if (clip.is_empty())
            return std::nullopt;
        const auto id = effect_id(event.source, EffectSlot::BattleEffectTeamAttack);
        if (!id.has_value())
            return std::nullopt;
        auto params = context.parameters.heff;
        params.anchor = AnchorPolicy::OppositeTeamCentroid;
        const char render_side = source->unit->direction;
        const char target_team_side = (render_side == 'A') ? 'd' : 'a';
        kLog->debug("effect_spawn unit={} role=TeamAttackOverlayFx family={} "
                    "source_side={} target_team_side={} preferred_direction={} selected={} "
                    "anchor=OppositeTeamCentroid",
                    event.source.value, clip.family, render_side, target_team_side,
                    clip.direction_or_variant, clip_driver_name(clip.clip));
        TimedTrackSpec ts = timed_track_spec_layered(source->entity_id, *id, clip.clip, params);
        ts.effect_role = BindingRole::TargetTeam;
        ts.visual_role = BattleEffectVisualRole::TeamAttackOverlayFx;
        ts.frame_delay = frame_delay_for(clip);
        return BattleAnimationScripts::build_timed_track(std::move(ts));
    }
    case BattleEffectVisualRole::FieldOverlayFx:
        break;
    }
    return std::nullopt;
}

std::optional<CommandNode> build_life_drained(const LifeDrained&         event,
                                              const BattleScriptContext& context) {
    const auto source = resolve(context.scene, event.source);
    if (!source.has_value() || context.assets.drain == nullptr ||
        context.assets.drain->frames.empty()) {
        return std::nullopt;
    }
    const auto id = effect_id(event.source, EffectSlot::Drain);
    if (!id.has_value()) {
        return std::nullopt;
    }
    return effect_command(source->entity_id, *id, *context.assets.drain, context.parameters.drain);
}

std::optional<CommandNode> build_source_healed(const SourceHealed&        event,
                                               const BattleScriptContext& context) {
    const auto source = resolve(context.scene, event.source);
    if (!source.has_value() || context.assets.heal == nullptr ||
        context.assets.heal->frames.empty()) {
        return std::nullopt;
    }
    const auto id = effect_id(event.source, EffectSlot::Heal);
    if (!id.has_value()) {
        return std::nullopt;
    }
    return effect_command(source->entity_id, *id, *context.assets.heal, context.parameters.heal);
}

} // namespace

std::optional<CommandNode> BattleAnimationScripts::build(const BattleVisualEvent&   event,
                                                         const BattleScriptContext& context) {
    return std::visit(
        [&](const auto& value) -> std::optional<CommandNode> {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, ActorSelected>) {
                return build_selection(value.previous, value.selected, EffectSlot::ActorMarker,
                                       TrackKind::ActorMarker, context);
            } else if constexpr (std::is_same_v<T, TargetSelected>) {
                return build_selection(value.previous, value.selected, EffectSlot::TargetMarker,
                                       TrackKind::TargetMarker, context);
            } else if constexpr (std::is_same_v<T, AttackStarted>) {
                return build_attack(value, context);
            } else if constexpr (std::is_same_v<T, AttackImpactCue> ||
                                 std::is_same_v<T, TargetMissed> ||
                                 std::is_same_v<T, TargetResisted>) {
                return std::nullopt;
            } else if constexpr (std::is_same_v<T, TargetDamaged>) {
                return build_target_damaged(value, context);
            } else if constexpr (std::is_same_v<T, TargetKilled>) {
                return build_target_killed(value, context);
            } else if constexpr (std::is_same_v<T, LifeDrained>) {
                return build_life_drained(value, context);
            } else if constexpr (std::is_same_v<T, SourceHealed>) {
                return build_source_healed(value, context);
            } else if constexpr (std::is_same_v<T, UnitHitReceived>) {
                return build_hit(value, context);
            } else if constexpr (std::is_same_v<T, UnitKilled>) {
                return build_kill(value, context);
            } else if constexpr (std::is_same_v<T, UnitReviveStarted>) {
                return build_revive_started(value, context);
            } else if constexpr (std::is_same_v<T, UnitRevived>) {
                return build_revived(value, context);
            } else if constexpr (std::is_same_v<T, CastEffectStarted>) {
                return build_cast(value, context);
            } else if constexpr (std::is_same_v<T, BattleEffectStarted>) {
                return build_battle_effect(value, context);
            } else {
                static_assert(unsupported_battle_visual_event<T>);
            }
        },
        event);
}

CommandNode BattleAnimationScripts::build_timed_track(TimedTrackSpec spec) {
    return CommandNode::sequence(timed_track_commands(std::move(spec)));
}

} // namespace d2engine
