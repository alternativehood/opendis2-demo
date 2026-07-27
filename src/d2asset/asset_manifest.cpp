// JSON validation policy:
// JSON Schema owns structural validation: required fields, primitive types,
// object/array shapes, unknown top-level keys.
// C++ typed parsers own semantic validation: asset existence, canonical paths,
// enum compatibility, profile references and cross-file invariants.
// Do not add large new hand-written structural validators when a schema can
// express the rule.

#include "asset_manifest.hpp"
#include "schemas.hpp"

#include "asset_error.hpp"
#include "asset_id.hpp"

#include <nlohmann/json-schema.hpp>

#include <fstream>
#include <unordered_set>

#include <nlohmann/json.hpp>

namespace d2asset {
namespace {

using Json = nlohmann::json;
namespace fs = std::filesystem;

[[noreturn]] void malformed(const std::string& message, std::string context = {}) {
    throw AssetError(AssetErrorCode::MalformedEntry, message, std::move(context));
}

const Json& require_field(const Json& object, const char* field, Json::value_t type,
                          std::string_view context) {
    if (!object.is_object())
        malformed("manifest entry must be an object", std::string(context));
    const auto it = object.find(field);
    if (it == object.end() || it->type() != type)
        malformed("required field has invalid type", std::string(context) + "." + field);
    return *it;
}

std::string require_non_empty_string(const Json& object, const char* field,
                                     std::string_view context) {
    const std::string value =
        require_field(object, field, Json::value_t::string, context).get<std::string>();
    if (value.empty())
        malformed("required string field is empty", std::string(context) + "." + field);
    return value;
}

std::vector<std::string> require_string_array(const Json& object, const char* field,
                                              std::string_view context) {
    const Json&              value = require_field(object, field, Json::value_t::array, context);
    std::vector<std::string> result;
    result.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (!value[i].is_string()) {
            malformed("array element must be a string",
                      std::string(context) + "." + field + "[" + std::to_string(i) + "]");
        }
        result.push_back(value[i].get<std::string>());
    }
    return result;
}

fs::path validate_asset_path(const fs::path& asset_root, const std::string& value,
                             const std::string& asset_id) {
    const fs::path relative(value);
    if (relative.empty() || relative.is_absolute() || relative.has_root_path()) {
        throw AssetError(AssetErrorCode::UnsafePath, "asset path must be relative", asset_id,
                         relative);
    }
    for (const auto& component : relative) {
        if (component == "..") {
            throw AssetError(AssetErrorCode::UnsafePath, "asset path escapes package root",
                             asset_id, relative);
        }
    }

    const fs::path normalized = relative.lexically_normal();
    if (normalized.empty() || normalized == ".") {
        throw AssetError(AssetErrorCode::UnsafePath, "asset path is empty after normalization",
                         asset_id, relative);
    }

    std::error_code ec;
    const fs::path  absolute = asset_root / normalized;
    if (!fs::is_regular_file(absolute, ec)) {
        throw AssetError(AssetErrorCode::MissingFile,
                         "asset path does not reference an existing regular file", asset_id,
                         normalized);
    }
    return normalized;
}

} // namespace

AssetType asset_type_from_string(const std::string& type_name) {
    if (type_name == "image")
        return AssetType::Image;
    if (type_name == "animation")
        return AssetType::Animation;
    if (type_name == "sound")
        return AssetType::Sound;
    if (type_name == "atlas")
        return AssetType::Atlas;
    if (type_name == "data_table")
        return AssetType::DataTable;
    return AssetType::Unknown;
}

const char* to_string(AssetType type) noexcept {
    switch (type) {
    case AssetType::Image:
        return "image";
    case AssetType::Animation:
        return "animation";
    case AssetType::Sound:
        return "sound";
    case AssetType::Atlas:
        return "atlas";
    case AssetType::DataTable:
        return "data_table";
    case AssetType::Unknown:
        return "unknown";
    }
    return "unknown";
}

AssetManifest AssetManifest::load(const fs::path& asset_root) {
    const fs::path manifest_path = asset_root / "game_manifest.json";
    std::ifstream  input(manifest_path);
    if (!input) {
        throw AssetError(AssetErrorCode::MissingManifest, "game_manifest.json not found", {},
                         manifest_path);
    }

    Json root;
    try {
        input >> root;
    } catch (const Json::exception& error) {
        throw AssetError(AssetErrorCode::InvalidJson,
                         std::string("invalid game_manifest.json: ") + error.what(), {},
                         manifest_path);
    }
    if (!root.is_object())
        malformed("manifest root must be an object", "root");

    // Schema validation: structural checks before semantic parsing
    {
        nlohmann::json_schema::json_validator validator;
        nlohmann::json schema_json = nlohmann::json::parse(schemas::runtime_asset_manifest());
        try {
            validator.set_root_schema(schema_json);
            validator.validate(root);
        } catch (const std::exception& e) {
            throw AssetError(AssetErrorCode::InvalidJson,
                             std::string("game_manifest.json schema validation failed: ") +
                                 e.what(),
                             {}, manifest_path);
        }
    }

    if (!root.contains("asset_schema_version") ||
        (!root["asset_schema_version"].is_number_integer() &&
         !root["asset_schema_version"].is_number_unsigned())) {
        malformed("required field has invalid type", "root.asset_schema_version");
    }
    const Json& version = root["asset_schema_version"];
    const Json& containers_json = require_field(root, "containers", Json::value_t::array, "root");
    const Json& assets_json = require_field(root, "assets", Json::value_t::array, "root");
    const std::vector<std::string> warnings = require_string_array(root, "warnings", "root");

    const int schema_version = version.get<int>();
    if (schema_version != 1) {
        throw AssetError(AssetErrorCode::UnsupportedSchema,
                         "unsupported asset_schema_version: " + std::to_string(schema_version),
                         std::to_string(schema_version), manifest_path);
    }

    AssetManifest                   manifest;
    std::unordered_set<std::string> container_ids;
    manifest.schema_version_ = schema_version;
    manifest.warnings_ = warnings;
    manifest.containers_.reserve(containers_json.size());
    for (std::size_t i = 0; i < containers_json.size(); ++i) {
        const std::string context = "containers[" + std::to_string(i) + "]";
        ContainerRecord   container;
        container.container_id =
            require_non_empty_string(containers_json[i], "container_id", context);
        container.path = require_non_empty_string(containers_json[i], "path", context);
        container.content_kinds =
            require_string_array(containers_json[i], "content_kinds", context);
        if (!is_canonical_id(container.container_id))
            malformed("container_id is not canonical", context + ".container_id");
        if (!container_ids.insert(container.container_id).second) {
            throw AssetError(AssetErrorCode::DuplicateId, "duplicate container_id",
                             container.container_id);
        }
        manifest.containers_.push_back(std::move(container));
    }

    std::unordered_set<std::string> asset_ids;
    manifest.assets_.reserve(assets_json.size());
    for (std::size_t i = 0; i < assets_json.size(); ++i) {
        const std::string context = "assets[" + std::to_string(i) + "]";
        AssetRecord       asset;
        asset.asset_id = require_non_empty_string(assets_json[i], "asset_id", context);
        asset.logical_name = require_non_empty_string(assets_json[i], "logical_name", context);
        asset.type_name = require_non_empty_string(assets_json[i], "type", context);
        asset.container_id = require_non_empty_string(assets_json[i], "container_id", context);
        const std::string path = require_non_empty_string(assets_json[i], "path", context);

        if (!is_canonical_id(asset.asset_id))
            malformed("asset_id is not canonical", context + ".asset_id");
        if (!asset_ids.insert(asset.asset_id).second)
            throw AssetError(AssetErrorCode::DuplicateId, "duplicate asset_id", asset.asset_id);
        if (!container_ids.contains(asset.container_id))
            malformed("asset references unknown container_id", context + ".container_id");

        asset.type = asset_type_from_string(asset.type_name);
        asset.path = validate_asset_path(asset_root, path, asset.asset_id);
        manifest.assets_.push_back(std::move(asset));
    }
    return manifest;
}

} // namespace d2asset
