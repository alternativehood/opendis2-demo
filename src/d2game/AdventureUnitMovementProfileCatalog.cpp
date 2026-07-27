#include "AdventureUnitMovementProfileCatalog.hpp"

#include <d2engine/assets/unit_def.hpp>

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <utility>

namespace d2game {

namespace {

std::string normalize_ascii(std::string_view value) {
    std::string normalized;
    normalized.reserve(value.size());
    for (const char character : value) {
        const auto unsigned_character = static_cast<unsigned char>(character);
        normalized.push_back(static_cast<char>(std::tolower(unsigned_character)));
    }
    return normalized;
}

} // namespace

AdventureUnitMovementProfileCatalog::AdventureUnitMovementProfileCatalog(
    std::vector<AdventureUnitMovementProfileEntry> entries)
    : entries_(std::move(entries)) {
    for (auto& entry : entries_) {
        entry.unit_type_id = normalize_ascii(entry.unit_type_id);
        if (entry.unit_type_id.empty())
            throw std::invalid_argument(
                "adventure movement profile catalog contains empty unit ID");
    }
    std::sort(entries_.begin(), entries_.end(),
              [](const auto& lhs, const auto& rhs) { return lhs.unit_type_id < rhs.unit_type_id; });
    for (std::size_t index = 1; index < entries_.size(); ++index) {
        if (entries_[index - 1].unit_type_id == entries_[index].unit_type_id)
            throw std::invalid_argument(
                "adventure movement profile catalog contains duplicate unit ID");
    }
}

AdventureUnitMovementProfileCatalog
AdventureUnitMovementProfileCatalog::from_unit_defs(std::span<const d2engine::UnitDef> unit_defs) {
    std::vector<AdventureUnitMovementProfileEntry> entries;
    entries.reserve(unit_defs.size());
    for (const auto& unit_def : unit_defs)
        entries.push_back(
            {unit_def.unit_id, d2adventure::resolve_adventure_movement_profile(unit_def)});
    return AdventureUnitMovementProfileCatalog(std::move(entries));
}

std::optional<d2adventure::AdventureMovementProfile>
AdventureUnitMovementProfileCatalog::find_profile(const std::string_view unit_type_id) const {
    const auto normalized = normalize_ascii(unit_type_id);
    const auto iterator = std::lower_bound(
        entries_.begin(), entries_.end(), normalized,
        [](const auto& entry, const std::string& value) { return entry.unit_type_id < value; });
    if (iterator == entries_.end() || iterator->unit_type_id != normalized)
        return std::nullopt;
    return iterator->profile;
}

} // namespace d2game
