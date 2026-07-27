#pragma once

#include "battle_ids.hpp"
#include "battle_render_snapshot.hpp"
#include "battle_unit.hpp"
#include "layered_animation_clip.hpp"
#include "../render/vec2.hpp"

#include <optional>
#include <vector>

namespace d2engine {

class BattleDebugSceneController;

class BattleScene {
public:
    void update(float delta_ms);

    void add_unit(BattleUnit unit);
    // Remove entire unit from scene (used by UnitRetreated lifecycle).
    // Erases the unit, all its tracks, and updates id_by_index_.
    void remove_unit(VisualEntityId id);
    void clear();

    [[nodiscard]] bool add_track(VisualEntityId id, VisualTrack track);
    [[nodiscard]] bool remove_track(VisualEntityId id, TrackSelector selector);
    [[nodiscard]] bool remove_track(VisualEntityId id, TrackKind kind);
    [[nodiscard]] bool remove_effect(EffectInstanceId id);
    [[nodiscard]] bool replace_track_clip(VisualEntityId id, TrackSelector selector,
                                          AnimationSequence seq, bool looping,
                                          BindingRole role = BindingRole::UnitBase);
    [[nodiscard]] bool replace_track_clip(VisualEntityId id, TrackKind kind, AnimationSequence seq,
                                          bool looping);
    [[nodiscard]] bool replace_track_layered_clip(VisualEntityId id, TrackSelector selector,
                                                  const LayeredAnimationClip& clip, bool looping,
                                                  BindingRole role = BindingRole::UnitBase);
    [[nodiscard]] bool stop_track(VisualEntityId id, TrackSelector selector);
    [[nodiscard]] bool stop_track(VisualEntityId id, TrackKind kind);
    [[nodiscard]] bool set_track_visibility(VisualEntityId id, TrackSelector selector,
                                            TrackVisibility visibility);
    [[nodiscard]] bool set_track_visibility(VisualEntityId id, TrackKind kind,
                                            TrackVisibility visibility);
    [[nodiscard]] bool set_track_alpha(VisualEntityId id, TrackSelector selector, float alpha);
    [[nodiscard]] bool set_track_alpha(VisualEntityId id, TrackKind kind, float alpha);
    [[nodiscard]] bool set_track_transform(VisualEntityId id, TrackSelector selector,
                                           VisualTransform transform);
    [[nodiscard]] bool set_track_transform(VisualEntityId id, TrackKind kind,
                                           VisualTransform transform);
    [[nodiscard]] bool set_life_state(VisualEntityId id, LifeVisualState state);
    [[nodiscard]] bool set_unit_hp(UnitInstanceId id, int current_hp);

    [[nodiscard]] VisualTrack*       find_track(VisualEntityId id, TrackSelector selector);
    [[nodiscard]] const VisualTrack* find_track(VisualEntityId id, TrackSelector selector) const;
    [[nodiscard]] VisualTrack*       find_track(VisualEntityId id, TrackKind kind);
    [[nodiscard]] const VisualTrack* find_track(VisualEntityId id, TrackKind kind) const;
    [[nodiscard]] VisualTrack*       find_effect(EffectInstanceId id);

    [[nodiscard]] const std::vector<BattleUnit>& units() const { return units_; }

    [[nodiscard]] BattleRenderSnapshot snapshot() const;

    // ID-based lookup (temporary — provides index resolution during migration)
    [[nodiscard]] std::optional<std::size_t> find_unit(VisualEntityId id) const;
    [[nodiscard]] std::optional<VisualEntityId>
                                    visual_entity_for(UnitInstanceId unit_instance_id) const;
    [[nodiscard]] BattleUnit*       try_unit_by_id(VisualEntityId id);
    [[nodiscard]] const BattleUnit* try_unit_by_id(VisualEntityId id) const;

private:
    friend class BattleDebugSceneController;

    [[nodiscard]] bool                track_id_exists(TrackId id) const;
    [[nodiscard]] TrackId             next_track_id();
    void                              assign_track_ids(BattleUnit& unit);
    void                              set_all_paused(bool paused);
    [[nodiscard]] bool                toggle_pause(VisualEntityId id);
    [[nodiscard]] bool                step_frame(VisualEntityId id, int delta);
    [[nodiscard]] bool                move_unit(VisualEntityId id, float dx, float dy);
    [[nodiscard]] std::optional<Vec2> unit_position_offset(VisualEntityId id) const;

    std::vector<BattleUnit>     units_;
    std::vector<VisualEntityId> id_by_index_;
    uint32_t                    next_id_ = 1; // 0 is reserved for null/invalid
    uint32_t                    next_unit_instance_id_ = 1;
    uint32_t                    next_track_id_ = 1;
};

} // namespace d2engine
