#pragma once

#include "battle_ids.hpp"
#include "battle_slot.hpp"
#include "life_visual_state.hpp"
#include "visual_track.hpp"
#include "../animation/animation_player.hpp"
#include "../render/vec2.hpp"

#include <type_traits>
#include <vector>

namespace d2engine {

struct BattleUnit {
    BattleSlotCoord     coord;
    char                direction = 'A';
    float               alpha = 1.0f;
    bool                is_large = false; // true if unit occupies both front and back slots
    bool                flip = false;
    bool                paused = false;
    Vec2                position_offset = {.x = 0.0f, .y = 0.0f};
    UnitInstanceId      unit_instance_id;
    VisualEntityId      id;
    std::string         unit_type;
    std::string         animation_unit_type; // may differ from unit_type when base unit substituted
    std::string         display_name;
    int                 current_hp = 0;
    int                 max_hp = 0;
    UnitVisualProfileId visual_profile_id;
    UnitLifecycleVisualProfileId lifecycle_profile_id;
    LifeVisualState              life_state = LifeVisualState::Alive;
    std::vector<VisualTrack>     tracks;

    [[nodiscard]] std::optional<std::size_t> find_track(TrackKind kind) const {
        for (std::size_t i = 0; i < tracks.size(); ++i) {
            if (tracks[i].kind == kind) {
                return i;
            }
        }
        return std::nullopt;
    }
};

static_assert(std::is_move_constructible_v<BattleUnit>);

} // namespace d2engine
