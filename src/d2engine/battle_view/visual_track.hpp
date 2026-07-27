#pragma once

#include "battle_ids.hpp"
#include "battle_visual_event.hpp"
#include "debug_renderable_item.hpp"
#include "layered_animation_player.hpp"
#include "track_render_layer.hpp"
#include "visual_visibility.hpp"
#include "anchor_policy.hpp"
#include "../animation/animation_player.hpp"
#include "../render/vec2.hpp"

#include <optional>
#include <string>
#include <type_traits>

namespace d2engine {

enum class TrackKind : std::uint8_t {
    Base,
    Shadow,
    ActorMarker,
    TargetMarker,
    DeathBody,
    DeathFx,
    ReviveFx,
    CastFx,
    HitFx,
    Effect,
    Aura,
    Hidden,
};

struct TrackSelector {
    enum class Mode : std::uint8_t {
        ById,
        ByKindSingleton,
    };

    Mode      mode = Mode::ByKindSingleton;
    TrackId   id;
    TrackKind kind = TrackKind::Base;

    constexpr TrackSelector() noexcept = default;
    constexpr TrackSelector(TrackKind track_kind) noexcept : kind(track_kind) {}

    [[nodiscard]] static constexpr TrackSelector by_id(TrackId track_id) noexcept {
        TrackSelector selector;
        selector.mode = Mode::ById;
        selector.id = track_id;
        return selector;
    }

    [[nodiscard]] static constexpr TrackSelector singleton(TrackKind track_kind) noexcept {
        return TrackSelector{track_kind};
    }
};

enum class TrackLifecycle : std::uint8_t {
    Active,
    PendingRemoval,
};

struct VisualTransform {
    Vec2  position_offset;
    float scale_x = 1.0f;
    float scale_y = 1.0f;
};

struct PlaybackTransform {
    bool  reverse_playback = false;
    bool  flip_x = false;
    bool  flip_y = false;
    float rotation_degrees = 0.0f;
};

struct VisualTrack {
    TrackId                         id;
    TrackKind                       kind = TrackKind::Base;
    AnimationPlayer                 player;
    TrackRenderLayer                layer = TrackRenderLayer::Base;
    AnchorPolicy                    anchor = AnchorPolicy::UnitFoot;
    TrackVisibility                 visibility = TrackVisibility::Visible;
    VisualTransform                 transform;
    int                             depth_bias = 0;
    PlaybackTransform               playback;
    std::optional<EffectInstanceId> effect_id;
    float                           alpha = 1.0f;
    TrackLifecycle                  lifecycle = TrackLifecycle::Active;
    BindingRole                     effect_role = BindingRole::Global;
    std::optional<BattleEffectVisualRole>
                visual_role;          // resolved visual category for spawned effects
    std::string lifecycle_profile_id; // corpse_sprite_base or death_fx_name
    // When clip is set, renders S1→A1→A2 layers using elapsed_ms phase normalisation.
    // elapsed_ms is advanced in BattleScene::update(); sample_frame() handles mismatched counts.
    LayeredAnimationPlayer layered_player;
};

static_assert(std::is_move_constructible_v<VisualTransform>);
static_assert(std::is_move_constructible_v<PlaybackTransform>);
static_assert(std::is_move_constructible_v<VisualTrack>);

} // namespace d2engine
