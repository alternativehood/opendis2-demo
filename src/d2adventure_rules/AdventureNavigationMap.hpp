#pragma once

#include "AdventureMovementPolicy.hpp"

#include <d2runtime/MapCellCoord.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace d2adventure {

struct AdventureNavigationCell {
    d2runtime::AdventureGroundType ground = d2runtime::AdventureGroundType::Unknown;
    bool                           has_road = false;
    std::optional<std::string>     blocking_object_id;
    std::optional<std::string>     occupying_stack_id;
};

class AdventureNavigationMap {
public:
    AdventureNavigationMap() = default;

    [[nodiscard]] int         width() const { return width_; }
    [[nodiscard]] int         height() const { return height_; }
    [[nodiscard]] bool        empty() const { return cells_.empty(); }
    [[nodiscard]] std::size_t size() const { return cells_.size(); }

    [[nodiscard]] bool                           contains(d2runtime::MapCellCoord cell) const;
    [[nodiscard]] const AdventureNavigationCell* cell_at(d2runtime::MapCellCoord cell) const;
    [[nodiscard]] std::optional<AdventureTraversalCell>
    traversal_cell_at(d2runtime::MapCellCoord cell, std::string_view ignored_stack_id = {}) const;

private:
    friend class AdventureNavigationMapBuilder;

    AdventureNavigationMap(int width, int height, std::vector<AdventureNavigationCell> cells);

    int                                  width_ = 0;
    int                                  height_ = 0;
    std::vector<AdventureNavigationCell> cells_;
};

} // namespace d2adventure
