#include "portrait_manifest.hpp"

#include "ff_asset_store.hpp"

#include <algorithm>
#include <cctype>
#include <set>
#include <stdexcept>

namespace d2engine {

namespace {

bool is_digit(char c) noexcept {
    return c >= '0' && c <= '9';
}

// Try to parse a record name as G000UU####{FACE|FACEB}.PNG.
// Returns the resource_unit_id prefix (e.g. "G000UU0001") or empty string.
std::string try_parse_portrait_record(const std::string& name) {
    constexpr std::string_view prefix = "G000UU";
    constexpr std::size_t      prefix_len = 6;
    constexpr std::size_t      digit_count = 4;

    // Minimum valid: prefix + digits + "FACE" + ".PNG" = 18
    if (name.size() < prefix_len + digit_count + 8)
        return {};

    if (name.substr(0, prefix_len) != prefix)
        return {};

    for (std::size_t i = 0; i < digit_count; ++i) {
        if (!is_digit(name[prefix_len + i]))
            return {};
    }

    std::string resource_id = name.substr(0, prefix_len + digit_count);

    if (name.size() == prefix_len + digit_count + 9 &&
        name.substr(prefix_len + digit_count) == "FACEB.PNG")
        return resource_id;
    if (name.size() == prefix_len + digit_count + 8 &&
        name.substr(prefix_len + digit_count) == "FACE.PNG")
        return resource_id;

    return {};
}

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

} // namespace

std::string normalize_unit_id_to_resource(std::string_view unit_id) {
    std::string result = to_upper(std::string(unit_id));
    if (result.size() >= 6 && result[4] == 'U' && result[5] == 'N')
        result[5] = 'U';
    return result;
}

PortraitManifest
build_portrait_manifest(const FfAssetStore&                                    store,
                        const std::vector<std::map<std::string, std::string>>& gunits_rows) {
    try {
        const auto names = store.record_names("Imgs/Faces.ff");
        return build_portrait_manifest_from_names(names, gunits_rows);
    } catch (const std::exception& e) {
        PortraitManifest m;
        m.container = "Imgs/Faces.ff";
        m.warnings.push_back(std::string("Failed to open Faces.ff: ") + e.what());
        return m;
    }
}

PortraitManifest build_portrait_manifest_from_names(
    const std::vector<std::string>&                        faces_record_names,
    const std::vector<std::map<std::string, std::string>>& gunits_rows) {
    PortraitManifest manifest;
    manifest.container = "Imgs/Faces.ff";

    // Step 1: Scan Faces.ff records, collect FACE and FACEB by resource_unit_id
    std::map<std::string, std::string> face_by_resource;
    std::map<std::string, std::string> faceb_by_resource;

    for (const auto& name : faces_record_names) {
        const std::string resource_id = try_parse_portrait_record(name);
        if (resource_id.empty())
            continue;

        if (name.ends_with("FACEB.PNG")) {
            faceb_by_resource[resource_id] = name;
        } else if (name.ends_with("FACE.PNG")) {
            face_by_resource[resource_id] = name;
        }
    }

    // Step 2: Process Gunits rows
    std::set<std::string> seen_resources;

    for (const auto& row : gunits_rows) {
        const auto it = row.find("UNIT_ID");
        if (it == row.end() || it->second.empty()) {
            manifest.warnings.emplace_back("Gunits row missing UNIT_ID");
            continue;
        }

        const std::string raw_unit_id = it->second;
        const std::string resource_id = normalize_unit_id_to_resource(raw_unit_id);
        const std::string unit_id = to_lower(resource_id);

        UnitPortraitEntry entry;
        entry.unit_id = unit_id;
        entry.resource_unit_id = resource_id;

        const auto fit = face_by_resource.find(resource_id);
        if (fit != face_by_resource.end()) {
            entry.face_record_name = fit->second;
            entry.has_face = true;
            seen_resources.insert(resource_id);
        } else {
            manifest.warnings.push_back("Missing FACE portrait for " + resource_id);
        }

        const auto fbit = faceb_by_resource.find(resource_id);
        if (fbit != faceb_by_resource.end()) {
            entry.faceb_record_name = fbit->second;
            entry.has_faceb = true;
            seen_resources.insert(resource_id);
        } else {
            manifest.warnings.push_back("Missing FACEB portrait for " + resource_id);
        }

        manifest.units.push_back(std::move(entry));
    }

    // Step 3: Warn about Faces.ff records not linked to any Gunits row
    for (const auto& pair : face_by_resource) {
        if (!seen_resources.contains(pair.first)) {
            manifest.warnings.push_back("Faces.ff has unlinked FACE record for " + pair.first);
        }
    }
    for (const auto& pair : faceb_by_resource) {
        if (!seen_resources.contains(pair.first)) {
            manifest.warnings.push_back("Faces.ff has unlinked FACEB record for " + pair.first);
        }
    }

    // Step 4: Sort by unit_id deterministically
    std::ranges::sort(manifest.units, [](const UnitPortraitEntry& a, const UnitPortraitEntry& b) {
        return a.unit_id < b.unit_id;
    });

    // Step 5: Deduplicate and sort warnings
    std::ranges::sort(manifest.warnings);
    const auto [first, last] = std::ranges::unique(manifest.warnings);
    manifest.warnings.erase(first, last);

    return manifest;
}

} // namespace d2engine
