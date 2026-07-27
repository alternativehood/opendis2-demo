#include "asset_link_manifest.hpp"

#include "asset_error.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <set>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

#include <nlohmann/json.hpp>
#include <utility>

namespace d2asset {
namespace {

namespace fs = std::filesystem;
using Json = nlohmann::ordered_json;

[[noreturn]] void malformed(const std::string& message, const std::string& context,
                            const fs::path& path) {
    throw AssetError(AssetErrorCode::MalformedAssetLink, message, context, path);
}

const Json& require_field(const Json& object, std::string_view field, Json::value_t type,
                          const std::string& context, const fs::path& path) {
    if (!object.is_object())
        malformed("asset-link value must be an object", context, path);
    const auto it = object.find(field);
    if (it == object.end() || it->type() != type) {
        malformed("required asset-link field has invalid type", context + "." + std::string(field),
                  path);
    }
    return *it;
}

std::string require_string(const Json& object, std::string_view field, const std::string& context,
                           const fs::path& path) {
    const std::string value =
        require_field(object, field, Json::value_t::string, context, path).get<std::string>();
    if (value.empty())
        malformed("required asset-link string is empty", context + "." + std::string(field), path);
    return value;
}

template <typename Enum>
Enum parse_enum(const std::string& value, const std::vector<std::pair<std::string_view, Enum>>& map,
                const std::string& context, const fs::path& path) {
    const auto it = std::ranges::find(map, value, &std::pair<std::string_view, Enum>::first);
    if (it == map.end())
        malformed("unknown asset-link enum value", context, path);
    return it->second;
}

AssetLinkEndpointKind parse_endpoint_kind(const std::string& value, const std::string& context,
                                          const fs::path& path) {
    return parse_enum(
        value,
        std::vector<std::pair<std::string_view, AssetLinkEndpointKind>>{
            {"asset", AssetLinkEndpointKind::Asset}, {"data_row", AssetLinkEndpointKind::DataRow}},
        context, path);
}

AssetLinkKind parse_link_kind(const std::string& value, const std::string& context,
                              const fs::path& path) {
    return parse_enum(value,
                      std::vector<std::pair<std::string_view, AssetLinkKind>>{
                          {"unknown", AssetLinkKind::Unknown},
                          {"idle_animation", AssetLinkKind::IdleAnimation},
                          {"move_animation", AssetLinkKind::MoveAnimation},
                          {"attack_animation", AssetLinkKind::AttackAnimation},
                          {"hit_animation", AssetLinkKind::HitAnimation},
                          {"death_animation", AssetLinkKind::DeathAnimation},
                          {"cast_animation", AssetLinkKind::CastAnimation},
                          {"defend_animation", AssetLinkKind::DefendAnimation},
                          {"portrait_image", AssetLinkKind::PortraitImage},
                          {"icon_image", AssetLinkKind::IconImage},
                          {"effect_animation", AssetLinkKind::EffectAnimation},
                          {"building_image", AssetLinkKind::BuildingImage},
                          {"dialog_image", AssetLinkKind::DialogImage},
                          {"sound_candidate", AssetLinkKind::SoundCandidate}},
                      context, path);
}

AssetLinkResolution parse_resolution(const std::string& value, const std::string& context,
                                     const fs::path& path) {
    return parse_enum(value,
                      std::vector<std::pair<std::string_view, AssetLinkResolution>>{
                          {"confirmed", AssetLinkResolution::Confirmed},
                          {"heuristic", AssetLinkResolution::Heuristic}},
                      context, path);
}

UnresolvedAssetReason parse_unresolved_reason(const std::string& value, const std::string& context,
                                              const fs::path& path) {
    return parse_enum(value,
                      std::vector<std::pair<std::string_view, UnresolvedAssetReason>>{
                          {"no_candidate", UnresolvedAssetReason::NoCandidate},
                          {"ambiguous", UnresolvedAssetReason::Ambiguous},
                          {"wrong_type", UnresolvedAssetReason::WrongType},
                          {"unsupported_mapping", UnresolvedAssetReason::UnsupportedMapping}},
                      context, path);
}

// NOLINTNEXTLINE(misc-no-recursion)
DataValue parse_value(const Json& json, const std::string& context, const fs::path& path) {
    if (json.is_null())
        return {.value = std::monostate{}};
    if (json.is_boolean())
        return {.value = json.get<bool>()};
    if (json.is_number_unsigned()) {
        const std::uint64_t value = json.get<std::uint64_t>();
        if (value > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
            malformed("asset-link extension integer is out of range", context, path);
        return {.value = static_cast<std::int64_t>(value)};
    }
    if (json.is_number_integer())
        return {.value = json.get<std::int64_t>()};
    if (json.is_number_float()) {
        const double value = json.get<double>();
        if (!std::isfinite(value))
            malformed("asset-link extension number must be finite", context, path);
        return {.value = value};
    }
    if (json.is_string())
        return {.value = json.get<std::string>()};
    if (json.is_array()) {
        DataValue::Array result;
        result.reserve(json.size());
        for (std::size_t index = 0; index < json.size(); ++index) {
            result.push_back(
                parse_value(json[index], context + "[" + std::to_string(index) + "]", path));
        }
        return {.value = std::move(result)};
    }
    if (json.is_object()) {
        DataValue::Object result;
        result.reserve(json.size());
        for (auto it = json.begin(); it != json.end(); ++it)
            result.emplace_back(it.key(), parse_value(it.value(), context + "." + it.key(), path));
        return {.value = std::move(result)};
    }
    malformed("unsupported asset-link extension value", context, path);
}

AssetLinkEndpoint parse_endpoint(const Json& json, const std::string& context, const fs::path& path,
                                 const std::unordered_map<std::string, const AssetRecord*>& assets,
                                 const std::unordered_map<std::string, const DataTable*>& tables) {
    AssetLinkEndpoint endpoint;
    endpoint.kind =
        parse_endpoint_kind(require_string(json, "kind", context, path), context + ".kind", path);
    if (endpoint.kind == AssetLinkEndpointKind::Asset) {
        endpoint.asset_id = require_string(json, "asset_id", context, path);
        if (!assets.contains(endpoint.asset_id))
            malformed("asset-link source asset does not exist", context + ".asset_id", path);
    } else {
        endpoint.table_asset_id = require_string(json, "table_asset_id", context, path);
        endpoint.row_key = require_string(json, "row_key", context, path);
        const auto table = tables.find(endpoint.table_asset_id);
        if (table == tables.end())
            malformed("asset-link source table does not exist", context + ".table_asset_id", path);
        if (!table->second->find_row(endpoint.row_key).value.has_value())
            malformed("asset-link source row does not exist", context + ".row_key", path);
    }
    return endpoint;
}

std::vector<AssetLinkEvidence> parse_evidence(const Json& json, const std::string& context,
                                              const fs::path& path) {
    if (!json.is_array())
        malformed("asset-link evidence must be an array", context, path);
    std::vector<AssetLinkEvidence> result;
    result.reserve(json.size());
    for (std::size_t index = 0; index < json.size(); ++index) {
        const std::string item_context = context + "[" + std::to_string(index) + "]";
        result.push_back(
            {.field = require_string(json[index], "field", item_context, path),
             .source_value = require_string(json[index], "source_value", item_context, path),
             .target_value = require_string(json[index], "target_value", item_context, path)});
    }
    if (result.empty())
        malformed("asset-link evidence must not be empty", context, path);
    return result;
}

AssetType expected_type(AssetLinkKind kind) {
    switch (kind) {
    case AssetLinkKind::IdleAnimation:
    case AssetLinkKind::MoveAnimation:
    case AssetLinkKind::AttackAnimation:
    case AssetLinkKind::HitAnimation:
    case AssetLinkKind::DeathAnimation:
    case AssetLinkKind::CastAnimation:
    case AssetLinkKind::DefendAnimation:
    case AssetLinkKind::EffectAnimation:
        return AssetType::Animation;
    case AssetLinkKind::PortraitImage:
    case AssetLinkKind::IconImage:
    case AssetLinkKind::BuildingImage:
    case AssetLinkKind::DialogImage:
        return AssetType::Image;
    case AssetLinkKind::SoundCandidate:
        return AssetType::Sound;
    case AssetLinkKind::Unknown:
        return AssetType::Unknown;
    }
    return AssetType::Unknown;
}

auto endpoint_order(const AssetLinkEndpoint& value) {
    return std::tie(value.kind, value.asset_id, value.table_asset_id, value.row_key);
}

auto link_order(const AssetLink& value) {
    return std::tuple{endpoint_order(value.source),
                      value.kind,
                      value.target_asset_id,
                      value.resolution,
                      value.confidence,
                      value.reason_code};
}

auto unresolved_order(const UnresolvedAssetReference& value) {
    return std::tuple{endpoint_order(value.source), value.kind, value.reason, value.reason_code,
                      value.candidate_asset_ids};
}

} // namespace

const char* to_string(AssetLinkEndpointKind value) noexcept {
    switch (value) {
    case AssetLinkEndpointKind::Asset:
        return "asset";
    case AssetLinkEndpointKind::DataRow:
        return "data_row";
    }
    return "unknown";
}

const char* to_string(AssetLinkKind value) noexcept {
    switch (value) {
    case AssetLinkKind::Unknown:
        return "unknown";
    case AssetLinkKind::IdleAnimation:
        return "idle_animation";
    case AssetLinkKind::MoveAnimation:
        return "move_animation";
    case AssetLinkKind::AttackAnimation:
        return "attack_animation";
    case AssetLinkKind::HitAnimation:
        return "hit_animation";
    case AssetLinkKind::DeathAnimation:
        return "death_animation";
    case AssetLinkKind::CastAnimation:
        return "cast_animation";
    case AssetLinkKind::DefendAnimation:
        return "defend_animation";
    case AssetLinkKind::PortraitImage:
        return "portrait_image";
    case AssetLinkKind::IconImage:
        return "icon_image";
    case AssetLinkKind::EffectAnimation:
        return "effect_animation";
    case AssetLinkKind::BuildingImage:
        return "building_image";
    case AssetLinkKind::DialogImage:
        return "dialog_image";
    case AssetLinkKind::SoundCandidate:
        return "sound_candidate";
    }
    return "unknown";
}

const char* to_string(AssetLinkResolution value) noexcept {
    switch (value) {
    case AssetLinkResolution::Confirmed:
        return "confirmed";
    case AssetLinkResolution::Heuristic:
        return "heuristic";
    }
    return "unknown";
}

const char* to_string(UnresolvedAssetReason value) noexcept {
    switch (value) {
    case UnresolvedAssetReason::NoCandidate:
        return "no_candidate";
    case UnresolvedAssetReason::Ambiguous:
        return "ambiguous";
    case UnresolvedAssetReason::WrongType:
        return "wrong_type";
    case UnresolvedAssetReason::UnsupportedMapping:
        return "unsupported_mapping";
    }
    return "unknown";
}

std::string asset_link_endpoint_key(const AssetLinkEndpoint& endpoint) {
    if (endpoint.kind == AssetLinkEndpointKind::Asset)
        return "asset:" + endpoint.asset_id;
    return "data_row:" + endpoint.table_asset_id + ":" + endpoint.row_key;
}

AssetLinkGraph AssetLinkManifest::load(const fs::path& asset_root, const AssetManifest& manifest,
                                       const std::vector<DataTable>& data_tables) {
    AssetLinkGraph graph;
    const fs::path path = asset_root / graph.sidecar_path;
    if (!fs::exists(path))
        return graph;

    std::ifstream input(path);
    if (!input)
        malformed("asset_links.json cannot be opened", "asset_links", graph.sidecar_path);
    Json root;
    try {
        input >> root;
    } catch (const Json::exception& error) {
        malformed(std::string("invalid asset_links.json: ") + error.what(), "asset_links",
                  graph.sidecar_path);
    }
    if (!root.is_object())
        malformed("asset-link root must be an object", "asset_links", graph.sidecar_path);

    const auto version = root.find("asset_links_schema_version");
    if (version == root.end() ||
        (!version->is_number_integer() && !version->is_number_unsigned())) {
        malformed("asset-link schema version must be an integer",
                  "asset_links.asset_links_schema_version", graph.sidecar_path);
    }
    const std::int64_t schema_version =
        version->is_number_unsigned() ? static_cast<std::int64_t>(version->get<std::uint64_t>())
                                      : version->get<std::int64_t>();
    if (std::cmp_not_equal(schema_version, asset_links_schema_version)) {
        throw AssetError(AssetErrorCode::UnsupportedAssetLinksSchema,
                         "unsupported asset-links schema version", std::to_string(schema_version),
                         graph.sidecar_path);
    }

    std::unordered_map<std::string, const AssetRecord*> assets;
    for (const AssetRecord& asset : manifest.assets())
        assets.emplace(asset.asset_id, &asset);
    std::unordered_map<std::string, const DataTable*> tables;
    for (const DataTable& table : data_tables)
        tables.emplace(table.data_table_asset_id, &table);

    const Json links =
        require_field(root, "links", Json::value_t::array, "asset_links", graph.sidecar_path);
    std::set<std::tuple<std::string, AssetLinkKind, std::string>> identities;
    std::set<std::pair<std::string, AssetLinkKind>>               successful_sources;
    graph.links.reserve(links.size());
    for (std::size_t index = 0; index < links.size(); ++index) {
        const std::string context = "asset_links.links[" + std::to_string(index) + "]";
        const Json&       json = links[index];
        AssetLink         link;
        link.source = parse_endpoint(
            require_field(json, "source", Json::value_t::object, context, graph.sidecar_path),
            context + ".source", graph.sidecar_path, assets, tables);
        link.target_asset_id = require_string(json, "target_asset_id", context, graph.sidecar_path);
        const auto target = assets.find(link.target_asset_id);
        if (target == assets.end()) {
            malformed("asset-link target asset does not exist", context + ".target_asset_id",
                      graph.sidecar_path);
        }
        link.kind = parse_link_kind(require_string(json, "link_kind", context, graph.sidecar_path),
                                    context + ".link_kind", graph.sidecar_path);
        const AssetType required_type = expected_type(link.kind);
        if (required_type != AssetType::Unknown && target->second->type != required_type) {
            malformed("asset-link target has the wrong asset type", context + ".target_asset_id",
                      graph.sidecar_path);
        }
        link.resolution =
            parse_resolution(require_string(json, "resolution", context, graph.sidecar_path),
                             context + ".resolution", graph.sidecar_path);
        const auto confidence = json.find("confidence");
        if (confidence == json.end() || !confidence->is_number_integer()) {
            malformed("asset-link confidence must be an integer", context + ".confidence",
                      graph.sidecar_path);
        }
        const std::int64_t confidence_value = confidence->get<std::int64_t>();
        if (confidence_value < 0 || confidence_value > 100) {
            malformed("asset-link confidence must be between 0 and 100", context + ".confidence",
                      graph.sidecar_path);
        }
        link.confidence = static_cast<std::uint8_t>(confidence_value);
        link.reason_code = require_string(json, "reason_code", context, graph.sidecar_path);
        if (link.resolution == AssetLinkResolution::Confirmed &&
            link.reason_code == "unit_animation_prefix") {
            malformed("confirmed asset link uses heuristic-only evidence", context + ".reason_code",
                      graph.sidecar_path);
        }
        link.evidence = parse_evidence(
            require_field(json, "evidence", Json::value_t::array, context, graph.sidecar_path),
            context + ".evidence", graph.sidecar_path);
        const auto identity =
            std::tuple{asset_link_endpoint_key(link.source), link.kind, link.target_asset_id};
        if (!identities.insert(identity).second) {
            throw AssetError(AssetErrorCode::DuplicateAssetLink, "duplicate asset-link identity",
                             context, graph.sidecar_path);
        }
        successful_sources.emplace(asset_link_endpoint_key(link.source), link.kind);
        graph.links.push_back(std::move(link));
    }
    if (!std::ranges::is_sorted(graph.links, {}, link_order)) {
        malformed("asset links are not in canonical order", "asset_links.links",
                  graph.sidecar_path);
    }

    const Json unresolved =
        require_field(root, "unresolved", Json::value_t::array, "asset_links", graph.sidecar_path);
    graph.unresolved.reserve(unresolved.size());
    for (std::size_t index = 0; index < unresolved.size(); ++index) {
        const std::string        context = "asset_links.unresolved[" + std::to_string(index) + "]";
        const Json&              json = unresolved[index];
        UnresolvedAssetReference reference;
        reference.source = parse_endpoint(
            require_field(json, "source", Json::value_t::object, context, graph.sidecar_path),
            context + ".source", graph.sidecar_path, assets, tables);
        reference.kind =
            parse_link_kind(require_string(json, "link_kind", context, graph.sidecar_path),
                            context + ".link_kind", graph.sidecar_path);
        reference.reason =
            parse_unresolved_reason(require_string(json, "reason", context, graph.sidecar_path),
                                    context + ".reason", graph.sidecar_path);
        reference.reason_code = require_string(json, "reason_code", context, graph.sidecar_path);
        reference.evidence = parse_evidence(
            require_field(json, "evidence", Json::value_t::array, context, graph.sidecar_path),
            context + ".evidence", graph.sidecar_path);
        const Json& candidates = require_field(json, "candidate_asset_ids", Json::value_t::array,
                                               context, graph.sidecar_path);
        for (std::size_t candidate_index = 0; candidate_index < candidates.size();
             ++candidate_index) {
            if (!candidates[candidate_index].is_string()) {
                malformed("unresolved candidate asset ID must be a string",
                          context + ".candidate_asset_ids[" + std::to_string(candidate_index) + "]",
                          graph.sidecar_path);
            }
            const std::string candidate = candidates[candidate_index].get<std::string>();
            if (!assets.contains(candidate)) {
                malformed("unresolved candidate asset does not exist",
                          context + ".candidate_asset_ids[" + std::to_string(candidate_index) + "]",
                          graph.sidecar_path);
            }
            reference.candidate_asset_ids.push_back(candidate);
        }
        if (!std::ranges::is_sorted(reference.candidate_asset_ids) ||
            std::ranges::adjacent_find(reference.candidate_asset_ids) !=
                reference.candidate_asset_ids.end()) {
            malformed("unresolved candidate asset IDs must be sorted and unique",
                      context + ".candidate_asset_ids", graph.sidecar_path);
        }
        if (successful_sources.contains(
                {asset_link_endpoint_key(reference.source), reference.kind})) {
            malformed("source and link kind are both resolved and unresolved", context,
                      graph.sidecar_path);
        }
        graph.unresolved.push_back(std::move(reference));
    }
    if (!std::ranges::is_sorted(graph.unresolved, {}, unresolved_order)) {
        malformed("unresolved asset references are not in canonical order",
                  "asset_links.unresolved", graph.sidecar_path);
    }

    const Json warnings =
        require_field(root, "warnings", Json::value_t::array, "asset_links", graph.sidecar_path);
    for (std::size_t index = 0; index < warnings.size(); ++index) {
        if (!warnings[index].is_string()) {
            malformed("asset-link warning must be a string",
                      "asset_links.warnings[" + std::to_string(index) + "]", graph.sidecar_path);
        }
        graph.warnings.push_back({warnings[index].get<std::string>()});
    }
    if (!std::ranges::is_sorted(graph.warnings, {}, &AssetLinkWarning::message)) {
        malformed("asset-link warnings are not in canonical order", "asset_links.warnings",
                  graph.sidecar_path);
    }

    const auto extensions = root.find("extensions");
    if (extensions != root.end() && !extensions->is_null())
        graph.extensions = parse_value(*extensions, "asset_links.extensions", graph.sidecar_path);
    return graph;
}

} // namespace d2asset
