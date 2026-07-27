#pragma once

#include "battle_ids.hpp"
#include "unit_state_clip.hpp"
#include "../animation/animation_sequence.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace d2engine {

struct DeathVisualAssets {
    std::string       death_fx_name; // e.g. "DEATH_HERETIC_S13"
    AnimationSequence death_fx;
    std::string       corpse_sprite_base; // e.g. "DEAD_HUMAN_SMALL.PNG"
    // Pre-loaded corpse sequence with canvas_foot_x/y anchor metadata from Battle.ff.
    // Empty when game files unavailable — build_kill() falls back to synthetic bottom-center.
    AnimationSequence corpse;
    // Animated corpse (STILA with >1 frame); usually empty — use corpse when absent.
    UnitStateClip stil_clip;
};

struct ReviveVisualAssets {
    std::string       small_fx_name; // "REVIVEANIMS"
    AnimationSequence small_fx;
    std::string       large_fx_name; // "REVIVEANIML"
    AnimationSequence large_fx;
};

struct UnitLifecycleVisualProfile {
    DeathVisualAssets  death;
    ReviveVisualAssets revive;
};

class UnitLifecycleVisualProfileRegistry {
public:
    [[nodiscard]] UnitLifecycleVisualProfileId add(std::string                name,
                                                   UnitLifecycleVisualProfile profile) {
        names_.push_back(std::move(name));
        profiles_.push_back(std::move(profile));
        return UnitLifecycleVisualProfileId{static_cast<std::uint32_t>(profiles_.size())};
    }

    // Backward-compat overload — name is empty (lifecycle_profile_id will be empty on tracks)
    [[nodiscard]] UnitLifecycleVisualProfileId add(UnitLifecycleVisualProfile profile) {
        return add({}, std::move(profile));
    }

    [[nodiscard]] const UnitLifecycleVisualProfile*
    lifecycle(UnitLifecycleVisualProfileId id) const {
        if (id.value == 0 || static_cast<std::size_t>(id.value) > profiles_.size()) {
            return nullptr;
        }
        return &profiles_[id.value - 1u];
    }

    // Returns the profile name for display/binding purposes; nullptr if id is invalid or unnamed
    [[nodiscard]] const std::string* name(UnitLifecycleVisualProfileId id) const {
        if (id.value == 0 || static_cast<std::size_t>(id.value) > names_.size()) {
            return nullptr;
        }
        const auto& n = names_[id.value - 1u];
        return n.empty() ? nullptr : &n;
    }

private:
    std::vector<std::string>                names_;
    std::vector<UnitLifecycleVisualProfile> profiles_;
};

} // namespace d2engine
