#include "asset_reference_resolver.hpp"

#include "d2asset/asset_database.hpp"
#include "d2asset/asset_id.hpp"

#include <algorithm>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>
#include <unordered_map>

#include <nlohmann/json.hpp>

namespace {

namespace fs = std::filesystem;
using Json = nlohmann::ordered_json;
using namespace d2asset;

AssetLinkEndpoint row_endpoint(const DataTable& table, const DataRow& row) {
    return {.kind = AssetLinkEndpointKind::DataRow,
            .asset_id = {},
            .table_asset_id = table.data_table_asset_id,
            .row_key = row.row_key};
}

AssetLinkKind animation_link_kind(AnimationRole role) {
    switch (role) {
    case AnimationRole::Idle:
        return AssetLinkKind::IdleAnimation;
    case AnimationRole::Move:
        return AssetLinkKind::MoveAnimation;
    case AnimationRole::Attack:
        return AssetLinkKind::AttackAnimation;
    case AnimationRole::Hit:
        return AssetLinkKind::HitAnimation;
    case AnimationRole::Death:
        return AssetLinkKind::DeathAnimation;
    case AnimationRole::Cast:
        return AssetLinkKind::CastAnimation;
    case AnimationRole::Defend:
        return AssetLinkKind::DefendAnimation;
    case AnimationRole::Unknown:
        return AssetLinkKind::EffectAnimation;
    }
    return AssetLinkKind::EffectAnimation;
}

std::string unit_animation_prefix(std::string unit_id) {
    unit_id = normalize_ascii(unit_id);
    if (unit_id.size() >= 6 && unit_id[4] == 'u' && unit_id[5] == 'n')
        unit_id[5] = 'u';
    return unit_id;
}

// NOLINTNEXTLINE(misc-no-recursion)
void collect_strings(const DataValue& value, std::string field,
                     std::vector<std::pair<std::string, std::string>>& output) {
    switch (value.kind()) {
    case DataValueKind::String:
        output.emplace_back(std::move(field), std::get<std::string>(value.value));
        return;
    case DataValueKind::Array: {
        const auto& array = std::get<DataValue::Array>(value.value);
        for (std::size_t index = 0; index < array.size(); ++index)
            collect_strings(array[index], field + "[" + std::to_string(index) + "]", output);
        return;
    }
    case DataValueKind::Object:
        for (const auto& [name, member] : std::get<DataValue::Object>(value.value)) {
            std::string child_field = field;
            if (!child_field.empty())
                child_field.push_back('.');
            child_field.append(name);
            collect_strings(member, std::move(child_field), output);
        }
        return;
    case DataValueKind::Null:
    case DataValueKind::Boolean:
    case DataValueKind::Integer:
    case DataValueKind::FloatingPoint:
        return;
    }
}

bool image_field(std::string_view field) {
    const std::string normalized = normalize_ascii(field);
    return normalized == "images" || normalized.ends_with(".images") ||
           normalized.find("image") != std::string::npos;
}

bool sound_field(std::string_view field) {
    return normalize_ascii(field).find("sound") != std::string::npos;
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

Json endpoint_json(const AssetLinkEndpoint& endpoint) {
    if (endpoint.kind == AssetLinkEndpointKind::Asset)
        return {{"kind", to_string(endpoint.kind)}, {"asset_id", endpoint.asset_id}};
    return {{"kind", to_string(endpoint.kind)},
            {"table_asset_id", endpoint.table_asset_id},
            {"row_key", endpoint.row_key}};
}

Json evidence_json(const std::vector<AssetLinkEvidence>& evidence) {
    Json result = Json::array();
    for (const AssetLinkEvidence& item : evidence) {
        result.push_back({{"field", item.field},
                          {"source_value", item.source_value},
                          {"target_value", item.target_value}});
    }
    return result;
}

Json graph_json(const AssetLinkGraph& graph) {
    Json links = Json::array();
    for (const AssetLink& link : graph.links) {
        links.push_back({{"source", endpoint_json(link.source)},
                         {"target_asset_id", link.target_asset_id},
                         {"link_kind", to_string(link.kind)},
                         {"resolution", to_string(link.resolution)},
                         {"confidence", link.confidence},
                         {"reason_code", link.reason_code},
                         {"evidence", evidence_json(link.evidence)}});
    }
    Json unresolved = Json::array();
    for (const UnresolvedAssetReference& reference : graph.unresolved) {
        unresolved.push_back({{"source", endpoint_json(reference.source)},
                              {"link_kind", to_string(reference.kind)},
                              {"reason", to_string(reference.reason)},
                              {"reason_code", reference.reason_code},
                              {"evidence", evidence_json(reference.evidence)},
                              {"candidate_asset_ids", reference.candidate_asset_ids}});
    }
    Json warnings = Json::array();
    for (const AssetLinkWarning& warning : graph.warnings)
        warnings.push_back(warning.message);
    return {{"asset_links_schema_version", asset_links_schema_version},
            {"links", std::move(links)},
            {"unresolved", std::move(unresolved)},
            {"warnings", std::move(warnings)},
            {"extensions", nullptr}};
}

} // namespace

d2asset::AssetLinkGraph AssetReferenceResolver::resolve(const fs::path& package_root) {
    const AssetDatabase database = AssetDatabase::open(package_root);
    AssetLinkGraph      graph;

    std::unordered_map<std::string, const AssetRecord*> assets_by_id;
    std::vector<const AssetRecord*>                     images;
    std::vector<const AssetRecord*>                     animations;
    for (const AssetRecord& asset : database.manifest().assets()) {
        assets_by_id.emplace(asset.asset_id, &asset);
        if (asset.type == AssetType::Image)
            images.push_back(&asset);
        if (asset.type == AssetType::Animation)
            animations.push_back(&asset);
    }
    const auto by_id = [](const AssetRecord* left, const AssetRecord* right) {
        return left->asset_id < right->asset_id;
    };
    std::ranges::sort(images, by_id);
    std::ranges::sort(animations, by_id);

    for (const AssetRecord& table_record : database.manifest().assets()) {
        if (table_record.type != AssetType::DataTable)
            continue;
        const auto table_result = database.get_data_table(table_record.asset_id);
        if (!table_result.value.has_value())
            continue;
        const DataTable& table = *table_result.value;
        for (const DataRow& row : table.rows) {
            const AssetLinkEndpoint                          source = row_endpoint(table, row);
            std::vector<std::pair<std::string, std::string>> strings;
            for (const DataCell& cell : row.values)
                collect_strings(cell.value, cell.name, strings);

            std::set<std::tuple<AssetLinkKind, std::string, std::string>> emitted;
            for (const auto& [field, value] : strings) {
                if (value.empty())
                    continue;
                const auto explicit_target = assets_by_id.find(value);
                const bool typed_field = image_field(field) || sound_field(field);
                if (!typed_field && explicit_target != assets_by_id.end() &&
                    emitted.emplace(AssetLinkKind::Unknown, value, "explicit_asset_id").second) {
                    graph.links.push_back(
                        {.source = source,
                         .target_asset_id = value,
                         .kind = AssetLinkKind::Unknown,
                         .resolution = AssetLinkResolution::Confirmed,
                         .confidence = 100,
                         .reason_code = "explicit_asset_id",
                         .evidence = {
                             {.field = field, .source_value = value, .target_value = value}}});
                }

                if (table.kind == DataTableKind::Dlg && image_field(field)) {
                    if (explicit_target != assets_by_id.end()) {
                        if (explicit_target->second->type == AssetType::Image) {
                            graph.links.push_back({.source = source,
                                                   .target_asset_id = value,
                                                   .kind = AssetLinkKind::DialogImage,
                                                   .resolution = AssetLinkResolution::Confirmed,
                                                   .confidence = 100,
                                                   .reason_code = "explicit_asset_id",
                                                   .evidence = {{.field = field,
                                                                 .source_value = value,
                                                                 .target_value = value}}});
                        } else {
                            graph.unresolved.push_back({.source = source,
                                                        .kind = AssetLinkKind::DialogImage,
                                                        .reason = UnresolvedAssetReason::WrongType,
                                                        .reason_code = "image_asset_wrong_type",
                                                        .evidence = {{.field = field,
                                                                      .source_value = value,
                                                                      .target_value = "image"}},
                                                        .candidate_asset_ids = {value}});
                        }
                        continue;
                    }
                    std::vector<const AssetRecord*> candidates;
                    for (const AssetRecord* image : images) {
                        if (normalize_ascii(image->logical_name) == normalize_ascii(value))
                            candidates.push_back(image);
                    }
                    if (candidates.size() == 1 &&
                        emitted
                            .emplace(AssetLinkKind::DialogImage, candidates.front()->asset_id,
                                     "exact_logical_name")
                            .second) {
                        graph.links.push_back(
                            {.source = source,
                             .target_asset_id = candidates.front()->asset_id,
                             .kind = AssetLinkKind::DialogImage,
                             .resolution = AssetLinkResolution::Confirmed,
                             .confidence = 100,
                             .reason_code = "exact_logical_name",
                             .evidence = {{.field = field,
                                           .source_value = value,
                                           .target_value = candidates.front()->logical_name}}});
                    } else {
                        std::vector<std::string> candidate_ids;
                        candidate_ids.reserve(candidates.size());
                        for (const AssetRecord* candidate : candidates)
                            candidate_ids.push_back(candidate->asset_id);
                        graph.unresolved.push_back(
                            {.source = source,
                             .kind = AssetLinkKind::DialogImage,
                             .reason = candidates.empty() ? UnresolvedAssetReason::NoCandidate
                                                          : UnresolvedAssetReason::Ambiguous,
                             .reason_code = candidates.empty() ? "image_name_not_found"
                                                               : "image_name_ambiguous",
                             .evidence = {{.field = field,
                                           .source_value = value,
                                           .target_value = normalize_ascii(value)}},
                             .candidate_asset_ids = std::move(candidate_ids)});
                    }
                }

                if (sound_field(field)) {
                    if (explicit_target == assets_by_id.end()) {
                        graph.unresolved.push_back(
                            {.source = source,
                             .kind = AssetLinkKind::SoundCandidate,
                             .reason = UnresolvedAssetReason::UnsupportedMapping,
                             .reason_code = "symbolic_sound_mapping_unavailable",
                             .evidence = {{.field = field,
                                           .source_value = value,
                                           .target_value = "canonical_sound_asset_id"}},
                             .candidate_asset_ids = {}});
                    } else if (explicit_target->second->type == AssetType::Sound) {
                        graph.links.push_back(
                            {.source = source,
                             .target_asset_id = value,
                             .kind = AssetLinkKind::SoundCandidate,
                             .resolution = AssetLinkResolution::Confirmed,
                             .confidence = 100,
                             .reason_code = "explicit_asset_id",
                             .evidence = {
                                 {.field = field, .source_value = value, .target_value = value}}});
                    } else {
                        graph.unresolved.push_back({.source = source,
                                                    .kind = AssetLinkKind::SoundCandidate,
                                                    .reason = UnresolvedAssetReason::WrongType,
                                                    .reason_code = "sound_asset_wrong_type",
                                                    .evidence = {{.field = field,
                                                                  .source_value = value,
                                                                  .target_value = "sound"}},
                                                    .candidate_asset_ids = {value}});
                    }
                }
            }

            const DataValue* unit_id_value = row.find("UNIT_ID");
            if (table.kind != DataTableKind::Dbf || unit_id_value == nullptr ||
                unit_id_value->kind() != DataValueKind::String)
                continue;
            const auto&       unit_id = std::get<std::string>(unit_id_value->value);
            const std::string prefix = unit_animation_prefix(unit_id);
            std::map<AssetLinkKind, std::vector<const AssetRecord*>> candidates_by_kind;
            for (const AssetRecord* animation : animations) {
                if (!normalize_ascii(animation->logical_name).starts_with(prefix))
                    continue;
                const auto clip = database.get_animation_clip(animation->asset_id);
                if (clip.value.has_value()) {
                    candidates_by_kind[animation_link_kind(clip.value->classification.role)]
                        .push_back(animation);
                }
            }
            if (candidates_by_kind.empty()) {
                graph.unresolved.push_back({.source = source,
                                            .kind = AssetLinkKind::EffectAnimation,
                                            .reason = UnresolvedAssetReason::NoCandidate,
                                            .reason_code = "unit_animation_prefix_not_found",
                                            .evidence = {{.field = "UNIT_ID",
                                                          .source_value = unit_id,
                                                          .target_value = prefix}},
                                            .candidate_asset_ids = {}});
            }
            for (const auto& [kind, candidates] : candidates_by_kind) {
                if (candidates.size() == 1) {
                    graph.links.push_back(
                        {.source = source,
                         .target_asset_id = candidates.front()->asset_id,
                         .kind = kind,
                         .resolution = AssetLinkResolution::Heuristic,
                         .confidence = 80,
                         .reason_code = "unit_animation_prefix",
                         .evidence = {{.field = "UNIT_ID",
                                       .source_value = unit_id,
                                       .target_value = candidates.front()->logical_name}}});
                } else {
                    std::vector<std::string> candidate_ids;
                    candidate_ids.reserve(candidates.size());
                    for (const AssetRecord* candidate : candidates)
                        candidate_ids.push_back(candidate->asset_id);
                    graph.unresolved.push_back({.source = source,
                                                .kind = kind,
                                                .reason = UnresolvedAssetReason::Ambiguous,
                                                .reason_code = "unit_animation_prefix_ambiguous",
                                                .evidence = {{.field = "UNIT_ID",
                                                              .source_value = unit_id,
                                                              .target_value = prefix}},
                                                .candidate_asset_ids = std::move(candidate_ids)});
                }
            }
        }
    }

    std::set<std::pair<std::string, AssetLinkKind>> resolved_source_kinds;
    for (const AssetLink& link : graph.links)
        resolved_source_kinds.emplace(asset_link_endpoint_key(link.source), link.kind);
    std::erase_if(graph.unresolved, [&resolved_source_kinds](const auto& reference) {
        return resolved_source_kinds.contains(
            {asset_link_endpoint_key(reference.source), reference.kind});
    });

    std::ranges::sort(graph.links, {}, link_order);
    std::ranges::sort(graph.unresolved, {}, unresolved_order);
    std::ranges::sort(graph.warnings, {}, &AssetLinkWarning::message);
    return graph;
}

void AssetReferenceResolver::write(const fs::path& package_root) {
    const AssetLinkGraph graph = resolve(package_root);
    std::ofstream        output(package_root / graph.sidecar_path);
    if (!output)
        throw std::runtime_error("cannot write asset_links.json");
    output << graph_json(graph).dump(2) << '\n';
    if (!output)
        throw std::runtime_error("cannot finish asset_links.json");
}
