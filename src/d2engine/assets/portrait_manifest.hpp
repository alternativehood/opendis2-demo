#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace d2engine {

class FfAssetStore;

struct UnitPortraitEntry {
    std::string unit_id;
    std::string resource_unit_id;
    std::string face_record_name;
    std::string faceb_record_name;
    bool        has_face = false;
    bool        has_faceb = false;
};

struct PortraitManifest {
    int                            schema_version = 1;
    std::string                    container;
    std::vector<UnitPortraitEntry> units;
    std::vector<std::string>       warnings;
};

// Build manifest from scanned Faces.ff record names and Gunits DBF rows.
// gunits_rows should be the raw records from DbfReader (each map has a UNIT_ID key).
PortraitManifest build_portrait_manifest_from_names(
    const std::vector<std::string>&                        faces_record_names,
    const std::vector<std::map<std::string, std::string>>& gunits_rows);

// Build manifest using central FfAssetStore (reads Faces.ff inventory).
PortraitManifest
build_portrait_manifest(const FfAssetStore&                                    store,
                        const std::vector<std::map<std::string, std::string>>& gunits_rows);

// Normalize a unit ID: G000UN#### -> G000UU#### (uppercase).
// Used to cross-reference Gunits UNIT_IDs with Faces.ff portrait record prefixes.
std::string normalize_unit_id_to_resource(std::string_view unit_id);

} // namespace d2engine
