#pragma once

#include "battle_ids.hpp"
#include "unit_animation_role_set.hpp"
#include "unit_lifecycle_visual_profile.hpp"

#include <cstdint>
#include <deque>
#include <utility>

namespace d2engine {

class UnitVisualProfileRegistry {
public:
    [[nodiscard]] UnitVisualProfileId add(UnitAnimationRoleSet roles) {
        profiles_.push_back(std::move(roles));
        return UnitVisualProfileId{static_cast<std::uint32_t>(profiles_.size())};
    }

    // No-op: deque provides stable element addresses without pre-allocation.
    void reserve(std::size_t /*count*/) {}

    [[nodiscard]] const UnitAnimationRoleSet* roles(UnitVisualProfileId id) const {
        if (id.value == 0 || static_cast<std::size_t>(id.value) > profiles_.size()) {
            return nullptr;
        }
        return &profiles_[id.value - 1u];
    }

private:
    std::deque<UnitAnimationRoleSet> profiles_;
};

} // namespace d2engine
