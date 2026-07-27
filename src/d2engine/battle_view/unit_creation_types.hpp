#pragma once

#include "battle_slot.hpp"
#include "unit_animation_role_set.hpp"
#include "unit_lifecycle_visual_profile.hpp"

#include <optional>
#include <string>
#include <vector>

namespace d2engine {

struct UnitCreationRequest {
    std::string              alias;
    std::string              unit_type;
    std::string              slot;
    std::optional<int>       hp;
    std::optional<int>       max_hp;
    std::vector<std::string> status;
};

struct UnitCreationData {
    BattleSlotCoord            coord;
    std::string                slot_name;
    std::string                unit_type;
    std::string                animation_unit_type;
    std::string                display_name;
    int                        current_hp = 0;
    int                        max_hp = 0;
    char                       direction = 'A';
    UnitAnimationRoleSet       roles;
    UnitLifecycleVisualProfile lifecycle;
    bool                       is_large = false;
    std::string                debug_alias;
    std::vector<std::string>   diagnostics;
    bool                       success = false;
};

} // namespace d2engine
