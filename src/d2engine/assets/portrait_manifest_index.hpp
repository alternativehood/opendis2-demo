#pragma once

#include "portrait_manifest.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace d2engine {

// Fast runtime index built from PortraitManifest.
// Provides lookups by full unit ID or short unit type.
class PortraitManifestIndex {
public:
    explicit PortraitManifestIndex(const PortraitManifest& manifest);

    // Lookup by short unit type (e.g. "UU0001", "uu0001").
    // Returns nullptr if not found.
    [[nodiscard]] const UnitPortraitEntry* find_by_unit_type(std::string_view unit_type) const;

    // All entries in manifest order (sorted by unit_id).
    [[nodiscard]] const std::vector<UnitPortraitEntry>& entries() const { return entries_; }

    // Number of entries.
    [[nodiscard]] std::size_t size() const { return entries_.size(); }

    // Manifest warnings from build.
    [[nodiscard]] const std::vector<std::string>& warnings() const { return warnings_; }

private:
    std::vector<UnitPortraitEntry>               entries_;
    std::vector<std::string>                     warnings_;
    std::unordered_map<std::string, std::size_t> by_unit_type_; // key: uppercase "UU0001"

    static std::string extract_unit_type_key(std::string_view resource_unit_id);
};

} // namespace d2engine
