#pragma once

#include <map>
#include <string>
#include <vector>

namespace d2engine {

struct EventsPortraitEntry {
    std::string unit_id;           // lowercased, e.g. "g000uu0001"
    std::string resource_unit_id;  // uppercased, e.g. "G000UU0001"
    std::string event_record_name; // raw record name in Imgs/Events.ff
    bool        has_event_portrait = false;
};

struct EventsPortraitManifest {
    int                              schema_version = 1;
    std::string                      container;
    std::vector<EventsPortraitEntry> entries;
    std::vector<std::string>         warnings;
};

// Build manifest from Imgs/Events.ff record names and Gunits DBF rows.
// Scans Events.ff for PNG records with G000UU#### prefix patterns.
// Matches them against Gunits UNIT_IDs via resource-unit-id normalization.
// Fast runtime index built from EventsPortraitManifest.
class EventsPortraitIndex {
public:
    explicit EventsPortraitIndex(const EventsPortraitManifest& manifest);

    [[nodiscard]] const EventsPortraitEntry* find_by_unit_type(std::string_view unit_type) const;

    [[nodiscard]] const std::vector<EventsPortraitEntry>& entries() const { return entries_; }
    [[nodiscard]] std::size_t                             size() const { return entries_.size(); }
    [[nodiscard]] const std::vector<std::string>&         warnings() const { return warnings_; }

private:
    std::vector<EventsPortraitEntry>                entries_;
    std::vector<std::string>                        warnings_;
    std::map<std::string, std::size_t, std::less<>> by_unit_type_;
};

} // namespace d2engine
