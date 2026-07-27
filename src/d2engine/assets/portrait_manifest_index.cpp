#include "portrait_manifest_index.hpp"

#include <algorithm>
#include <cctype>

namespace d2engine {

std::string PortraitManifestIndex::extract_unit_type_key(std::string_view resource_unit_id) {
    // Normalize full global IDs ("G000UU0001") → short unit type ("UU0001")
    std::string_view trimmed = resource_unit_id;
    if (trimmed.size() >= 4) {
        const std::string_view prefix = trimmed.substr(0, 4);
        if ((prefix[0] == 'G' || prefix[0] == 'g') && (prefix[1] == '0') && (prefix[2] == '0') &&
            (prefix[3] == '0')) {
            trimmed = trimmed.substr(4);
        }
    }
    if (trimmed.empty())
        return {};
    std::string result(trimmed);
    for (char& c : result) {
        if (c >= 'a' && c <= 'z')
            c = static_cast<char>(c - 'a' + 'A');
    }
    return result;
}

PortraitManifestIndex::PortraitManifestIndex(const PortraitManifest& manifest)
    : entries_(manifest.units), warnings_(manifest.warnings) {

    by_unit_type_.reserve(entries_.size());

    for (std::size_t i = 0; i < entries_.size(); ++i) {
        const auto& e = entries_[i];

        // Index by unit type (uppercase, e.g. "UU0001")
        const std::string type_key = extract_unit_type_key(e.resource_unit_id);
        if (!type_key.empty()) {
            by_unit_type_.emplace(type_key, i);
        }
    }
}

const UnitPortraitEntry*
PortraitManifestIndex::find_by_unit_type(std::string_view unit_type) const {
    const std::string key = extract_unit_type_key(unit_type);
    if (key.empty())
        return nullptr;
    const auto it = by_unit_type_.find(key);
    return (it != by_unit_type_.end()) ? &entries_[it->second] : nullptr;
}

} // namespace d2engine
