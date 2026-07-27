#include "events_portrait_manifest.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace d2engine {

namespace {

std::string to_lower(std::string s) {
    std::ranges::transform(s, s.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string to_upper(std::string s) {
    std::ranges::transform(s, s.begin(),
                           [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

std::string normalize_to_resource_unit_id(std::string_view unit_id) {
    std::string result = to_upper(std::string(unit_id));
    if (result.size() >= 6 && result[4] == 'U' && result[5] == 'N')
        result[5] = 'U';
    return result;
}

} // namespace

// ── EventsPortraitIndex ────────────────────────────────────────────────────

EventsPortraitIndex::EventsPortraitIndex(const EventsPortraitManifest& manifest)
    : entries_(manifest.entries), warnings_(manifest.warnings) {
    for (std::size_t i = 0; i < entries_.size(); ++i) {
        by_unit_type_[entries_[i].unit_id] = i;
    }
}

const EventsPortraitEntry*
EventsPortraitIndex::find_by_unit_type(std::string_view unit_type) const {
    if (unit_type.size() >= 6 && unit_type[4] == 'U' && unit_type[5] == 'U') {
        const auto it = by_unit_type_.find(unit_type);
        if (it != by_unit_type_.end())
            return &entries_[it->second];
    }
    const std::string lower = to_lower(normalize_to_resource_unit_id(unit_type));
    const auto        it = by_unit_type_.find(lower);
    if (it != by_unit_type_.end())
        return &entries_[it->second];
    return nullptr;
}

} // namespace d2engine
