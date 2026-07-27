#pragma once

#include "battle_ids.hpp"
#include "battle_slot.hpp"
#include "life_visual_state.hpp"
#include "visual_track.hpp"

#include <optional>
#include <type_traits>
#include <vector>

namespace d2engine {

struct VisualEntity {
    VisualEntityId               id;
    UnitInstanceId               source_unit;
    UnitVisualProfileId          visual_profile_id;
    UnitLifecycleVisualProfileId lifecycle_profile_id;
    BattleSlotCoord              slot;
    LifeVisualState              life_state = LifeVisualState::Alive;
    std::vector<VisualTrack>     tracks;
    bool                         flip = false;

    [[nodiscard]] std::optional<std::size_t> find_track(TrackKind kind) const;
    void                                     add_track(VisualTrack track);
    void                                     update(float delta_ms);
};

static_assert(std::is_move_constructible_v<VisualEntity>);

} // namespace d2engine
