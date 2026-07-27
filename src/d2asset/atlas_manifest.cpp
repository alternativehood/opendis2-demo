#include "atlas_manifest.hpp"
#include "schemas.hpp"

#include "asset_error.hpp"
#include "asset_id.hpp"

#include <nlohmann/json-schema.hpp>

#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_set>

#include <nlohmann/json.hpp>

namespace d2asset {
namespace {

using Json = nlohmann::json;
namespace fs = std::filesystem;

[[noreturn]] void malformed_atlas(const std::string& message, const std::string& context,
                                  const fs::path& path) {
    throw AssetError(AssetErrorCode::MalformedAtlas, message, context, path);
}

const Json& require_field(const Json& object, const char* field, Json::value_t type,
                          const std::string& context, const fs::path& path) {
    if (!object.is_object())
        malformed_atlas("atlas value must be an object", context, path);
    const auto it = object.find(field);
    if (it == object.end() || it->type() != type)
        malformed_atlas("required atlas field has invalid type", context + "." + field, path);
    return *it;
}

std::string require_non_empty_string(const Json& object, const char* field,
                                     const std::string& context, const fs::path& path) {
    const std::string value =
        require_field(object, field, Json::value_t::string, context, path).get<std::string>();
    if (value.empty())
        malformed_atlas("required atlas string is empty", context + "." + field, path);
    return value;
}

std::uint32_t require_non_negative_integer(const Json& object, const char* field,
                                           const std::string& context, const fs::path& path) {
    const auto it = object.find(field);
    if (it == object.end() || (!it->is_number_integer() && !it->is_number_unsigned())) {
        malformed_atlas("required atlas field must be an integer", context + "." + field, path);
    }
    if (it->is_number_unsigned()) {
        const std::uint64_t value = it->get<std::uint64_t>();
        if (value > std::numeric_limits<std::uint32_t>::max()) {
            malformed_atlas("atlas integer is outside the supported range", context + "." + field,
                            path);
        }
        return static_cast<std::uint32_t>(value);
    }
    const std::int64_t value = it->get<std::int64_t>();
    if (value < 0) {
        malformed_atlas("atlas integer is outside the supported range", context + "." + field,
                        path);
    }
    return static_cast<std::uint32_t>(value);
}

std::uint32_t require_positive_integer(const Json& object, const char* field,
                                       const std::string& context, const fs::path& path) {
    const std::uint32_t value = require_non_negative_integer(object, field, context, path);
    if (value == 0)
        malformed_atlas("atlas integer must be positive", context + "." + field, path);
    return value;
}

fs::path sheet_path(const fs::path& sidecar_path, std::uint32_t index) {
    std::ostringstream filename;
    filename << "atlas_" << std::setfill('0') << std::setw(3) << index << ".png";
    return sidecar_path.parent_path() / filename.str();
}

} // namespace

AtlasManifest AtlasManifest::load(const fs::path& asset_root, const fs::path& sidecar_path,
                                  const std::string& atlas_asset_id) {
    const fs::path absolute_sidecar = asset_root / sidecar_path;
    std::ifstream  input(absolute_sidecar);
    if (!input) {
        throw AssetError(AssetErrorCode::MalformedAtlas, "atlas sidecar cannot be opened",
                         atlas_asset_id, sidecar_path);
    }

    Json root;
    try {
        input >> root;
    } catch (const Json::exception& error) {
        throw AssetError(AssetErrorCode::MalformedAtlas,
                         std::string("invalid atlas JSON: ") + error.what(), atlas_asset_id,
                         sidecar_path);
    }
    if (!root.is_object())
        malformed_atlas("atlas root must be an object", atlas_asset_id, sidecar_path);

    // Schema validation: structural checks before semantic parsing
    try {
        nlohmann::json_schema::json_validator validator;
        nlohmann::json schema_json = nlohmann::json::parse(d2asset::schemas::atlas_manifest());
        validator.set_root_schema(schema_json);
        validator.validate(root);
    } catch (const std::exception& e) {
        malformed_atlas(std::string("atlas schema validation failed: ") + e.what(), atlas_asset_id,
                        sidecar_path);
    }

    AtlasManifest manifest;
    manifest.max_sheet_size_ =
        require_positive_integer(root, "max_sheet_size", atlas_asset_id, sidecar_path);
    const std::uint32_t sheet_count =
        require_non_negative_integer(root, "sheet_count", atlas_asset_id, sidecar_path);
    manifest.total_sprites_ =
        require_non_negative_integer(root, "total_sprites", atlas_asset_id, sidecar_path);
    manifest.skipped_sprites_ =
        require_non_negative_integer(root, "skipped_sprites", atlas_asset_id, sidecar_path);
    const Json& entries =
        require_field(root, "entries", Json::value_t::array, atlas_asset_id, sidecar_path);

    if (manifest.total_sprites_ != entries.size()) {
        malformed_atlas("total_sprites does not match entries count",
                        atlas_asset_id + ".total_sprites", sidecar_path);
    }

    manifest.sheets_.reserve(sheet_count);
    for (std::uint32_t index = 0; index < sheet_count; ++index) {
        const fs::path  relative = sheet_path(sidecar_path, index);
        std::error_code ec;
        if (!fs::is_regular_file(asset_root / relative, ec)) {
            throw AssetError(AssetErrorCode::MissingAtlasSheet,
                             "atlas sheet does not reference an existing regular file",
                             atlas_asset_id, relative);
        }
        manifest.sheets_.push_back(
            {.atlas_asset_id = atlas_asset_id,
             .index = index,
             .path = relative,
             .size = {.width = manifest.max_sheet_size_, .height = manifest.max_sheet_size_}});
    }

    std::unordered_set<std::string> names;
    manifest.regions_.reserve(entries.size());
    for (std::size_t i = 0; i < entries.size(); ++i) {
        const std::string context = atlas_asset_id + ".entries[" + std::to_string(i) + "]";
        const Json&       entry = entries[i];
        AtlasSpriteRegion region;
        region.logical_name = require_non_empty_string(entry, "name", context, sidecar_path);
        region.sheet_index = require_non_negative_integer(entry, "sheet", context, sidecar_path);
        region.rectangle.x = require_non_negative_integer(entry, "x", context, sidecar_path);
        region.rectangle.y = require_non_negative_integer(entry, "y", context, sidecar_path);
        region.rectangle.width = require_positive_integer(entry, "w", context, sidecar_path);
        region.rectangle.height = require_positive_integer(entry, "h", context, sidecar_path);

        if (region.sheet_index >= sheet_count) {
            malformed_atlas("atlas sheet index is outside sheet_count", context + ".sheet",
                            sidecar_path);
        }

        const std::uint64_t right =
            static_cast<std::uint64_t>(region.rectangle.x) + region.rectangle.width;
        const std::uint64_t bottom =
            static_cast<std::uint64_t>(region.rectangle.y) + region.rectangle.height;
        if (right > manifest.max_sheet_size_ || bottom > manifest.max_sheet_size_) {
            throw AssetError(AssetErrorCode::InvalidAtlasRectangle,
                             "atlas rectangle exceeds declared sheet bounds", context,
                             sidecar_path);
        }

        if (!names.insert(normalize_ascii(region.logical_name)).second) {
            throw AssetError(AssetErrorCode::DuplicateAtlasEntry,
                             "duplicate case-insensitive atlas entry name", region.logical_name,
                             sidecar_path);
        }
        manifest.regions_.push_back(std::move(region));
    }
    return manifest;
}

} // namespace d2asset
