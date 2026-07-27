#pragma once

#include "anchor_policy.hpp"
#include "battle_ids.hpp"
#include "battle_slot.hpp"
#include "layered_animation_clip.hpp"
#include "life_visual_state.hpp"
#include "render_placement.hpp"
#include "track_render_layer.hpp"
#include "visual_track.hpp"
#include "visual_visibility.hpp"
#include "../animation/animation_sequence.hpp"
#include "../render/vec2.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

namespace d2engine {

struct SnapshotTrack {
    TrackId                         id;
    TrackKind                       kind = TrackKind::Base;
    TrackRenderLayer                layer = TrackRenderLayer::Base;
    AnchorPolicy                    anchor = AnchorPolicy::UnitFoot;
    RenderPlacement                 placement;
    TrackVisibility                 visibility = TrackVisibility::Visible;
    VisualTransform                 transform;
    PlaybackTransform               playback;
    std::optional<EffectInstanceId> effect_id;
    float                           alpha = 1.0f;
    std::size_t                     current_frame_index = 0;
    std::string                     current_frame_name;
    std::string                     container_path;
    std::string                     sequence_name;
    // For layered clips: driver sequence name shared by all S1/A1/A2 layers of this bundle.
    // Empty for single-sequence tracks. Used as the binding owner_id for effect profiles.
    std::string              bundle_sequence_name;
    std::optional<LayerSlot> layer_slot; // which slot this track represents in its bundle
    BindingRole              effect_role = BindingRole::Global;
    std::optional<BattleEffectVisualRole> visual_role; // resolved category for spawned effects
    std::string                           lifecycle_profile_id;
    AnimationSequence                     sequence; // full sequence for warmup planning
    bool                                  is_looping = false;
    int                                   canvas_foot_x = 0;
    int                                   canvas_foot_y = 0;
    int                                   canvas_top_y = 0;
    int                                   native_canvas_w = 0;
    int                                   native_canvas_h = 0;
};

struct SnapshotEntity {
    VisualEntityId  id;
    UnitInstanceId  unit_instance_id;
    BattleSlotCoord coord;
    LifeVisualState life_state = LifeVisualState::Alive;
    bool            flip = false;
    bool            is_large = false;
    float           alpha = 1.0f;
    Vec2            position_offset;
    std::string     unit_type;
    std::string     animation_unit_type; // may differ from unit_type when base unit substituted
    std::string     display_name;
    int             current_hp = 0;
    int             max_hp = 0;
    std::vector<SnapshotTrack> tracks;
};

struct BattleRenderSnapshot {
    std::vector<SnapshotEntity> entities;
};

static_assert(std::is_move_constructible_v<SnapshotTrack>);
static_assert(std::is_move_constructible_v<SnapshotEntity>);
static_assert(std::is_move_constructible_v<BattleRenderSnapshot>);

} // namespace d2engine
