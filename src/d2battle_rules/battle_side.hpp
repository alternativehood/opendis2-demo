#pragma once

#include "battle_types.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <string>

namespace d2battle {

struct BattleSideState {
    std::string  source_stack_id;
    std::string  owner;
    std::string  subrace;
    std::string  leader_id;
    std::uint8_t leader_alive = 0;
    int          morale = 0;
    int          battles_won = 0;

    std::array<std::optional<std::string>, 6> members;
    std::array<int, 6>                        positions = {-1, -1, -1, -1, -1, -1};
    std::array<int, 6>                        cell_members = {-1, -1, -1, -1, -1, -1};

    std::string banner;
    std::string tome;
    std::string battle1;
    std::string battle2;
    std::string artifact1;
    std::string artifact2;
    std::string boots;

    [[nodiscard]] std::size_t member_count() const {
        std::size_t n = 0;
        for (const auto& m : members)
            if (m.has_value())
                ++n;
        return n;
    }

    bool operator==(const BattleSideState&) const = default;
};

} // namespace d2battle
