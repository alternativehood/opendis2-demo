#pragma once

#include "AdventureGroundType.hpp"

#include <algorithm>
#include <initializer_list>
#include <span>
#include <vector>

namespace d2runtime {

class MovementCapabilities {
public:
    MovementCapabilities() = default;

    [[nodiscard]] bool can_natively_traverse(AdventureGroundType type) const {
        return std::ranges::find(capabilities_, type) != capabilities_.end();
    }

    void add(AdventureGroundType type) {
        if (!can_natively_traverse(type))
            capabilities_.push_back(type);
    }

    [[nodiscard]] static MovementCapabilities
    from_native_ability_ids(std::span<const int> ability_ids) {
        MovementCapabilities caps;
        for (int id : ability_ids) {
            auto ground = adventure_ground_type_from_id(id);
            if (ground != AdventureGroundType::Unknown)
                caps.add(ground);
        }
        return caps;
    }

    // Convenience overload
    [[nodiscard]] static MovementCapabilities
    from_native_ability_ids(std::initializer_list<int> ids) {
        return from_native_ability_ids(std::span<const int>{ids.begin(), ids.size()});
    }

    [[nodiscard]] const std::vector<AdventureGroundType>& capabilities() const {
        return capabilities_;
    }

private:
    std::vector<AdventureGroundType> capabilities_;
};

} // namespace d2runtime
