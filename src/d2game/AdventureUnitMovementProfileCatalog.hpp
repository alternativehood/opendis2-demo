#pragma once

#include <d2adventure_rules/AdventureMovementProfile.hpp>

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace d2engine {
struct UnitDef;
}

namespace d2game {

struct AdventureUnitMovementProfileEntry {
    std::string                           unit_type_id;
    d2adventure::AdventureMovementProfile profile = d2adventure::AdventureMovementProfile::Walking;

    bool operator==(const AdventureUnitMovementProfileEntry&) const = default;
};

class AdventureUnitMovementProfileCatalog {
public:
    AdventureUnitMovementProfileCatalog() = default;

    explicit AdventureUnitMovementProfileCatalog(
        std::vector<AdventureUnitMovementProfileEntry> entries);

    [[nodiscard]] static AdventureUnitMovementProfileCatalog
    from_unit_defs(std::span<const d2engine::UnitDef> unit_defs);

    [[nodiscard]] bool        empty() const { return entries_.empty(); }
    [[nodiscard]] std::size_t size() const { return entries_.size(); }

    [[nodiscard]] std::optional<d2adventure::AdventureMovementProfile>
    find_profile(std::string_view unit_type_id) const;

private:
    std::vector<AdventureUnitMovementProfileEntry> entries_;
};

} // namespace d2game
