#include "engine_contract.hpp"

#include "asset_error.hpp"

#include <algorithm>
#include <tuple>
#include <utility>

#include <nlohmann/json.hpp>

namespace d2asset {
namespace {

using Json = nlohmann::json;

Json pixel_size_json(const PixelSize& size) {
    return {{"height", size.height}, {"width", size.width}};
}

Json pixel_point_json(const PixelPoint& point) {
    return {{"x", point.x}, {"y", point.y}};
}

Json optional_pixel_size_json(const std::optional<PixelSize>& value) {
    return value.has_value() ? pixel_size_json(*value) : Json(nullptr);
}

Json optional_pixel_point_json(const std::optional<PixelPoint>& value) {
    return value.has_value() ? pixel_point_json(*value) : Json(nullptr);
}

Json texture_region_json(const TextureRegion& region) {
    return {
        {"anchor", optional_pixel_point_json(region.anchor)},
        {"atlas_asset_id", region.atlas_asset_id},
        {"image_asset_id", region.image_asset_id},
        {"logical_name", region.logical_name},
        {"pivot", optional_pixel_point_json(region.pivot)},
        {"rectangle",
         {{"height", region.rectangle.height},
          {"width", region.rectangle.width},
          {"x", region.rectangle.x},
          {"y", region.rectangle.y}}},
        {"sheet_index", region.sheet_index},
        {"sheet_path", package_path_string(region.sheet_path)},
        {"source_size", optional_pixel_size_json(region.source_size)},
        {"trim_offset", optional_pixel_point_json(region.trim_offset)},
        {"trimmed_size", optional_pixel_size_json(region.trimmed_size)},
    };
}

Json warning_json(const AnimationWarning& warning) {
    std::vector<std::string> matching_ids = warning.matching_asset_ids;
    std::ranges::sort(matching_ids);
    return {
        {"animation_asset_id", warning.animation_asset_id},
        {"frame_index",
         warning.frame_index.has_value() ? Json(*warning.frame_index) : Json(nullptr)},
        {"logical_name", warning.logical_name},
        {"matching_asset_ids", matching_ids},
        {"message", warning.message},
    };
}

Json frame_json(const AnimationFrameRef& frame) {
    return {
        {"fallback_path", frame.fallback_path.has_value()
                              ? Json(package_path_string(*frame.fallback_path))
                              : Json(nullptr)},
        {"image_asset_id",
         frame.image_asset_id.has_value() ? Json(*frame.image_asset_id) : Json(nullptr)},
        {"index", frame.index},
        {"logical_name", frame.logical_name},
        {"resolved", frame.resolved()},
        {"source_size", pixel_size_json(frame.source_size)},
        {"texture_region", frame.texture_region.has_value()
                               ? texture_region_json(*frame.texture_region)
                               : Json(nullptr)},
        {"timing",
         {{"duration_ms", frame.timing.duration_ms}, {"source", to_string(frame.timing.source)}}},
    };
}

InspectionError lookup_error(InspectionExitCode exit_code, std::string code, std::string message,
                             std::string requested_asset_id) {
    return {.exit_code = exit_code,
            .code = std::move(code),
            .message = std::move(message),
            .requested_asset_id = std::move(requested_asset_id),
            .detail_code = {},
            .context = {},
            .path = {}};
}

Json sound_json(const SoundAsset& sound) {
    std::vector<SoundWarning> warnings = sound.warnings;
    std::ranges::sort(warnings, {}, &SoundWarning::message);
    Json warning_array = Json::array();
    for (const SoundWarning& warning : warnings)
        warning_array.push_back(warning.message);
    return {
        {"asset_id", sound.sound_asset_id},
        {"bit_depth", sound.bit_depth.has_value() ? Json(*sound.bit_depth) : Json(nullptr)},
        {"channels", sound.channels.has_value() ? Json(*sound.channels) : Json(nullptr)},
        {"container_id", sound.container_id},
        {"duration_ms", sound.duration_ms.has_value() ? Json(*sound.duration_ms) : Json(nullptr)},
        {"format", to_string(sound.format)},
        {"format_tag", sound.format_tag.has_value() ? Json(*sound.format_tag) : Json(nullptr)},
        {"logical_name", sound.logical_name},
        {"payload_path", package_path_string(sound.payload_path)},
        {"payload_size", sound.payload_size},
        {"sample_rate", sound.sample_rate.has_value() ? Json(*sound.sample_rate) : Json(nullptr)},
        {"sidecar_path", package_path_string(sound.sidecar_path)},
        {"warnings", std::move(warning_array)},
    };
}

Json data_value_json(const DataValue& value) { // NOLINT(misc-no-recursion)
    Json result{{"kind", to_string(value.kind())}};
    // NOLINTNEXTLINE(bugprone-branch-clone)
    switch (value.kind()) {
    case DataValueKind::Null:
        result["value"] = nullptr;
        break;
    case DataValueKind::Boolean:
        result["value"] = std::get<bool>(value.value);
        break;
    case DataValueKind::Integer:
        result["value"] = std::get<std::int64_t>(value.value);
        break;
    case DataValueKind::FloatingPoint:
        result["value"] = std::get<double>(value.value);
        break;
    case DataValueKind::String:
        result["value"] = std::get<std::string>(value.value);
        break;
    case DataValueKind::Array: {
        Json array = Json::array();
        for (const DataValue& item : std::get<DataValue::Array>(value.value))
            array.push_back(data_value_json(item));
        result["value"] = std::move(array);
        break;
    }
    case DataValueKind::Object: {
        Json members = Json::array();
        for (const auto& [name, member] : std::get<DataValue::Object>(value.value)) {
            members.push_back({{"name", name}, {"value", data_value_json(member)}});
        }
        result["value"] = std::move(members);
        break;
    }
    }
    return result;
}

Json data_table_json(const DataTable& table, const DataRow* selected_row) {
    Json columns = Json::array();
    for (const DataColumn& column : table.columns) {
        columns.push_back(
            {{"decimal_count",
              column.decimal_count.has_value() ? Json(*column.decimal_count) : Json(nullptr)},
             {"name", column.name},
             {"source_type",
              column.source_type.has_value() ? Json(*column.source_type) : Json(nullptr)},
             {"width", column.width.has_value() ? Json(*column.width) : Json(nullptr)}});
    }
    std::vector<DataTableWarning> warnings = table.warnings;
    std::ranges::sort(warnings, {}, &DataTableWarning::message);
    Json warning_array = Json::array();
    for (const DataTableWarning& warning : warnings)
        warning_array.push_back(warning.message);

    Json row = nullptr;
    if (selected_row != nullptr) {
        Json values = Json::array();
        for (const DataCell& cell : selected_row->values) {
            values.push_back({{"name", cell.name}, {"value", data_value_json(cell.value)}});
        }
        row = {{"row_key", selected_row->row_key}, {"values", std::move(values)}};
    }
    return {
        {"asset_id", table.data_table_asset_id},
        {"columns", std::move(columns)},
        {"container_id", table.container_id},
        {"kind", to_string(table.kind)},
        {"logical_name", table.logical_name},
        {"row_count", table.rows.size()},
        {"selected_row", std::move(row)},
        {"sidecar_path", package_path_string(table.sidecar_path)},
        {"warnings", std::move(warning_array)},
    };
}

Json link_endpoint_json(const AssetLinkEndpoint& endpoint) {
    if (endpoint.kind == AssetLinkEndpointKind::Asset)
        return {{"asset_id", endpoint.asset_id}, {"kind", to_string(endpoint.kind)}};
    return {{"kind", to_string(endpoint.kind)},
            {"row_key", endpoint.row_key},
            {"table_asset_id", endpoint.table_asset_id}};
}

Json link_evidence_json(const std::vector<AssetLinkEvidence>& evidence) {
    Json result = Json::array();
    for (const AssetLinkEvidence& item : evidence) {
        result.push_back({{"field", item.field},
                          {"source_value", item.source_value},
                          {"target_value", item.target_value}});
    }
    return result;
}

Json link_json(const AssetLink& link) {
    return {{"confidence", link.confidence},
            {"evidence", link_evidence_json(link.evidence)},
            {"link_kind", to_string(link.kind)},
            {"reason_code", link.reason_code},
            {"resolution", to_string(link.resolution)},
            {"source", link_endpoint_json(link.source)},
            {"target_asset_id", link.target_asset_id}};
}

Json unresolved_reference_json(const UnresolvedAssetReference& reference) {
    return {{"candidate_asset_ids", reference.candidate_asset_ids},
            {"evidence", link_evidence_json(reference.evidence)},
            {"link_kind", to_string(reference.kind)},
            {"reason", to_string(reference.reason)},
            {"reason_code", reference.reason_code},
            {"source", link_endpoint_json(reference.source)}};
}

Json references_json(const std::vector<AssetLink>& outgoing, const std::vector<AssetLink>& incoming,
                     const std::vector<UnresolvedAssetReference>& unresolved) {
    Json outgoing_json = Json::array();
    for (const AssetLink& link : outgoing)
        outgoing_json.push_back(link_json(link));
    Json incoming_json = Json::array();
    for (const AssetLink& link : incoming)
        incoming_json.push_back(link_json(link));
    Json unresolved_json = Json::array();
    for (const UnresolvedAssetReference& reference : unresolved)
        unresolved_json.push_back(unresolved_reference_json(reference));
    return {{"incoming", std::move(incoming_json)},
            {"outgoing", std::move(outgoing_json)},
            {"unresolved", std::move(unresolved_json)}};
}

Json success_json(const InspectionRequest& request, const AnimationClip& clip,
                  const std::optional<SoundAsset>& sound,
                  const std::optional<DataTable>& data_table, const DataRow* selected_row) {
    std::vector<AnimationFrameRef> frames = clip.frames;
    std::ranges::sort(frames, {}, &AnimationFrameRef::index);

    std::vector<AnimationWarning> warnings = clip.warnings;
    std::ranges::sort(warnings, [](const AnimationWarning& left, const AnimationWarning& right) {
        return std::tie(left.frame_index, left.logical_name, left.message,
                        left.matching_asset_ids) < std::tie(right.frame_index, right.logical_name,
                                                            right.message,
                                                            right.matching_asset_ids);
    });

    Json frame_array = Json::array();
    for (const AnimationFrameRef& frame : frames)
        frame_array.push_back(frame_json(frame));

    Json warning_array = Json::array();
    for (const AnimationWarning& warning : warnings)
        warning_array.push_back(warning_json(warning));

    Json animation = {
        {"asset_id", clip.animation_asset_id},
        {"classification",
         {{"matched_token", clip.classification.matched_token},
          {"reason", clip.classification.reason},
          {"role", to_string(clip.classification.role)}}},
        {"container_id", clip.container_id},
        {"facing_direction", to_string(clip.facing_direction)},
        {"frame_count", frames.size()},
        {"frames", std::move(frame_array)},
        {"logical_name", clip.logical_name},
        {"loop_mode", to_string(clip.loop_mode)},
        {"sidecar_path", package_path_string(clip.sidecar_path)},
        {"warnings", std::move(warning_array)},
    };

    Json report = {
        {"animation", std::move(animation)},
        {"inspection_schema_version", inspection_schema_version},
        {"request",
         {{"animation_asset_id", request.animation_asset_id},
          {"sound_asset_id",
           request.sound_asset_id.has_value() ? Json(*request.sound_asset_id) : Json(nullptr)},
          {"data_table_asset_id", request.data_table_asset_id.has_value()
                                      ? Json(*request.data_table_asset_id)
                                      : Json(nullptr)},
          {"row_key", request.row_key.has_value() ? Json(*request.row_key) : Json(nullptr)}}},
        {"status", "ok"},
    };
    if (sound.has_value())
        report["sound"] = sound_json(*sound);
    if (data_table.has_value())
        report["data_table"] = data_table_json(*data_table, selected_row);
    return report;
}

} // namespace

const char* to_string(TimingSource value) noexcept {
    switch (value) {
    case TimingSource::ProvisionalSidecar:
        return "provisional_sidecar";
    case TimingSource::FallbackDefault:
        return "fallback_default";
    }
    return "unknown";
}

const char* to_string(LoopMode value) noexcept {
    switch (value) {
    case LoopMode::Unknown:
        return "unknown";
    case LoopMode::Once:
        return "once";
    case LoopMode::Loop:
        return "loop";
    }
    return "unknown";
}

const char* to_string(AnimationRole value) noexcept {
    switch (value) {
    case AnimationRole::Unknown:
        return "unknown";
    case AnimationRole::Idle:
        return "idle";
    case AnimationRole::Move:
        return "move";
    case AnimationRole::Attack:
        return "attack";
    case AnimationRole::Hit:
        return "hit";
    case AnimationRole::Death:
        return "death";
    case AnimationRole::Cast:
        return "cast";
    case AnimationRole::Defend:
        return "defend";
    }
    return "unknown";
}

const char* to_string(FacingDirection value) noexcept {
    switch (value) {
    case FacingDirection::Unknown:
        return "unknown";
    }
    return "unknown";
}

const char* to_string(AssetErrorCode value) noexcept {
    switch (value) {
    case AssetErrorCode::MissingManifest:
        return "missing_manifest";
    case AssetErrorCode::InvalidJson:
        return "invalid_json";
    case AssetErrorCode::UnsupportedSchema:
        return "unsupported_schema";
    case AssetErrorCode::MalformedEntry:
        return "malformed_entry";
    case AssetErrorCode::DuplicateId:
        return "duplicate_id";
    case AssetErrorCode::UnsafePath:
        return "unsafe_path";
    case AssetErrorCode::MissingFile:
        return "missing_file";
    case AssetErrorCode::MalformedAtlas:
        return "malformed_atlas";
    case AssetErrorCode::DuplicateAtlasEntry:
        return "duplicate_atlas_entry";
    case AssetErrorCode::InvalidAtlasRectangle:
        return "invalid_atlas_rectangle";
    case AssetErrorCode::MissingAtlasSheet:
        return "missing_atlas_sheet";
    case AssetErrorCode::MalformedAnimation:
        return "malformed_animation";
    case AssetErrorCode::MalformedSound:
        return "malformed_sound";
    case AssetErrorCode::UnsupportedSoundSchema:
        return "unsupported_sound_schema";
    case AssetErrorCode::MissingSoundPayload:
        return "missing_sound_payload";
    case AssetErrorCode::SoundPayloadSizeMismatch:
        return "sound_payload_size_mismatch";
    case AssetErrorCode::MalformedDataTable:
        return "malformed_data_table";
    case AssetErrorCode::UnsupportedDataTableSchema:
        return "unsupported_data_table_schema";
    case AssetErrorCode::MalformedDataValue:
        return "malformed_data_value";
    case AssetErrorCode::DuplicateDataRow:
        return "duplicate_data_row";
    case AssetErrorCode::UnsupportedAssetLinksSchema:
        return "unsupported_asset_links_schema";
    case AssetErrorCode::MalformedAssetLink:
        return "malformed_asset_link";
    case AssetErrorCode::DuplicateAssetLink:
        return "duplicate_asset_link";
    }
    return "unknown_asset_error";
}

std::string package_path_string(const std::filesystem::path& path) {
    return path.lexically_normal().generic_string();
}

InspectionResult inspect_runtime_assets(const InspectionRequest& request) {
    const bool has_reference_table = request.reference_table_asset_id.has_value();
    const bool has_reference_row = request.reference_row_key.has_value();
    const bool has_reference_target = request.reference_target_asset_id.has_value();
    if (has_reference_table != has_reference_row || (has_reference_table && has_reference_target)) {
        return {.report = std::nullopt,
                .error = lookup_error(
                    InspectionExitCode::Usage, "invalid_arguments",
                    "reference source requires table and row and conflicts with target", {})};
    }
    try {
        const AssetDatabase database = AssetDatabase::open(request.asset_root);
        const auto          clip_result = database.get_animation_clip(request.animation_asset_id);
        if (clip_result.status != AssetLookupStatus::Found || !clip_result.value.has_value()) {
            return {.report = std::nullopt,
                    .error = lookup_error(InspectionExitCode::Animation, "animation_not_found",
                                          "animation asset was not found or has the wrong type",
                                          request.animation_asset_id)};
        }

        std::optional<SoundAsset> sound;
        if (request.sound_asset_id.has_value()) {
            const std::string& sound_asset_id = request.sound_asset_id.value();
            const auto         sound_result = database.get_sound_asset(sound_asset_id);
            if (sound_result.status != AssetLookupStatus::Found ||
                !sound_result.value.has_value()) {
                return {.report = std::nullopt,
                        .error = lookup_error(InspectionExitCode::Sound, "sound_not_found",
                                              "sound asset was not found or has the wrong type",
                                              sound_asset_id)};
            }
            sound = sound_result.value.value();
        }

        std::optional<DataTable> data_table;
        const DataRow*           selected_row = nullptr;
        if (request.data_table_asset_id.has_value()) {
            const auto table_result = database.get_data_table(*request.data_table_asset_id);
            if (table_result.status != AssetLookupStatus::Found ||
                !table_result.value.has_value()) {
                return {.report = std::nullopt,
                        .error =
                            lookup_error(InspectionExitCode::DataTable, "data_table_not_found",
                                         "data-table asset was not found or has the wrong type",
                                         *request.data_table_asset_id)};
            }
            data_table = table_result.value.value();
            if (request.row_key.has_value()) {
                const auto row_result = data_table->find_row(*request.row_key);
                if (!row_result.value.has_value()) {
                    return {.report = std::nullopt,
                            .error =
                                lookup_error(InspectionExitCode::DataRow, "data_row_not_found",
                                             "data-table row was not found", *request.row_key)};
                }
                selected_row = *row_result.value;
            }
        }
        Json report =
            success_json(request, clip_result.value.value(), sound, data_table, selected_row);
        if (request.reference_table_asset_id.has_value()) {
            const auto outgoing = database.links_from_data_row(*request.reference_table_asset_id,
                                                               *request.reference_row_key);
            const auto unresolved = database.unresolved_from_data_row(
                *request.reference_table_asset_id, *request.reference_row_key);
            if (!outgoing.value.has_value() || !unresolved.value.has_value()) {
                return {.report = std::nullopt,
                        .error = lookup_error(
                            InspectionExitCode::ReferenceSource, "reference_source_not_found",
                            "reference source table or row was not found",
                            *request.reference_table_asset_id + ":" + *request.reference_row_key)};
            }
            report["references"] = references_json(*outgoing.value, {}, *unresolved.value);
            report["request"]["reference_row_key"] = *request.reference_row_key;
            report["request"]["reference_table_asset_id"] = *request.reference_table_asset_id;
        } else if (request.reference_target_asset_id.has_value()) {
            const auto incoming = database.links_to_asset(*request.reference_target_asset_id);
            if (!incoming.value.has_value()) {
                return {.report = std::nullopt,
                        .error = lookup_error(InspectionExitCode::ReferenceTarget,
                                              "reference_target_not_found",
                                              "reference target asset was not found",
                                              *request.reference_target_asset_id)};
            }
            report["references"] = references_json({}, *incoming.value, {});
            report["request"]["reference_target_asset_id"] = *request.reference_target_asset_id;
        }
        return {.report = std::move(report), .error = std::nullopt};
    } catch (const AssetError& error) {
        std::filesystem::path error_path = error.path();
        if (error_path.is_absolute()) {
            const std::filesystem::path relative =
                error_path.lexically_relative(request.asset_root.lexically_normal());
            if (!relative.empty() && *relative.begin() != "..")
                error_path = relative;
        }
        return {.report = std::nullopt,
                .error = InspectionError{
                    .exit_code = InspectionExitCode::Package,
                    .code = "package_open_failed",
                    .message = error.what(),
                    .requested_asset_id = {},
                    .detail_code = to_string(error.code()),
                    .context = error.context(),
                    .path = std::move(error_path),
                }};
    } catch (const std::filesystem::filesystem_error& error) {
        static_cast<void>(error);
        return {.report = std::nullopt,
                .error = InspectionError{
                    .exit_code = InspectionExitCode::Package,
                    .code = "package_open_failed",
                    .message = "filesystem error while opening package",
                    .requested_asset_id = {},
                    .detail_code = "filesystem_error",
                    .context = {},
                    .path = {},
                }};
    }
}

Json inspection_error_json(const InspectionError& error) {
    return {
        {"error",
         {{"code", error.code},
          {"context", error.context.empty() ? Json(nullptr) : Json(error.context)},
          {"detail_code", error.detail_code.empty() ? Json(nullptr) : Json(error.detail_code)},
          {"message", error.message},
          {"path", error.path.empty() ? Json(nullptr) : Json(package_path_string(error.path))},
          {"requested_asset_id",
           error.requested_asset_id.empty() ? Json(nullptr) : Json(error.requested_asset_id)}}},
        {"inspection_schema_version", inspection_schema_version},
        {"status", "error"},
    };
}

std::string serialize_inspection_json(const Json& value) {
    return value.dump(2) + '\n';
}

int run_d2asset_inspect(const std::vector<std::string>& arguments, std::ostream& output) {
    InspectionRequest request;
    bool              has_animation = false;
    if (arguments.empty()) {
        const InspectionError error{
            .exit_code = InspectionExitCode::Usage,
            .code = "invalid_arguments",
            .message = "usage: opendis2-dev-extractor inspect <asset_root> --animation "
                       "<asset_id> [--sound <asset_id>] [--data-table "
                       "<asset_id> [--row <row_key>]] [--reference-table "
                       "<asset_id> --reference-row <row_key> | "
                       "--reference-target <asset_id>]",
            .requested_asset_id = {},
            .detail_code = {},
            .context = {},
            .path = {}};
        output << serialize_inspection_json(inspection_error_json(error));
        return static_cast<int>(error.exit_code);
    }
    request.asset_root = arguments.front();
    // NOLINTNEXTLINE(bugprone-branch-clone)
    for (std::size_t index = 1; index < arguments.size(); ++index) {
        if (arguments[index] == "--animation" && index + 1 < arguments.size()) {
            request.animation_asset_id = arguments[++index];
            has_animation = !request.animation_asset_id.empty();
        } else if (arguments[index] == "--sound" && index + 1 < arguments.size()) {
            request.sound_asset_id = arguments[++index];
            if (request.sound_asset_id->empty())
                request.sound_asset_id.reset();
        } else if (arguments[index] == "--data-table" && index + 1 < arguments.size()) {
            request.data_table_asset_id = arguments[++index];
            if (request.data_table_asset_id->empty())
                request.data_table_asset_id.reset();
        } else if (arguments[index] == "--row" && index + 1 < arguments.size()) {
            request.row_key = arguments[++index];
            if (request.row_key->empty())
                request.row_key.reset();
        } else if (arguments[index] == "--reference-table" && index + 1 < arguments.size()) {
            request.reference_table_asset_id = arguments[++index];
            if (request.reference_table_asset_id->empty())
                request.reference_table_asset_id.reset();
        } else if (arguments[index] == "--reference-row" && index + 1 < arguments.size()) {
            request.reference_row_key = arguments[++index];
            if (request.reference_row_key->empty())
                request.reference_row_key.reset();
        } else if (arguments[index] == "--reference-target" && index + 1 < arguments.size()) {
            request.reference_target_asset_id = arguments[++index];
            if (request.reference_target_asset_id->empty())
                request.reference_target_asset_id.reset();
        } else {
            const InspectionError error{.exit_code = InspectionExitCode::Usage,
                                        .code = "invalid_arguments",
                                        .message = "unknown or incomplete argument",
                                        .requested_asset_id = {},
                                        .detail_code = {},
                                        .context = arguments[index],
                                        .path = {}};
            output << serialize_inspection_json(inspection_error_json(error));
            return static_cast<int>(error.exit_code);
        }
    }
    if (!has_animation) {
        const InspectionError error{.exit_code = InspectionExitCode::Usage,
                                    .code = "invalid_arguments",
                                    .message = "--animation <asset_id> is required",
                                    .requested_asset_id = {},
                                    .detail_code = {},
                                    .context = {},
                                    .path = {}};
        output << serialize_inspection_json(inspection_error_json(error));
        return static_cast<int>(error.exit_code);
    }
    if (request.row_key.has_value() && !request.data_table_asset_id.has_value()) {
        const InspectionError error{.exit_code = InspectionExitCode::Usage,
                                    .code = "invalid_arguments",
                                    .message = "--row requires --data-table <asset_id>",
                                    .requested_asset_id = {},
                                    .detail_code = {},
                                    .context = *request.row_key,
                                    .path = {}};
        output << serialize_inspection_json(inspection_error_json(error));
        return static_cast<int>(error.exit_code);
    }
    const bool has_reference_table = request.reference_table_asset_id.has_value();
    const bool has_reference_row = request.reference_row_key.has_value();
    const bool has_reference_target = request.reference_target_asset_id.has_value();
    if (has_reference_table != has_reference_row || (has_reference_table && has_reference_target)) {
        const InspectionError error{
            .exit_code = InspectionExitCode::Usage,
            .code = "invalid_arguments",
            .message = "reference source requires both --reference-table and --reference-row, "
                       "and cannot be combined with --reference-target",
            .requested_asset_id = {},
            .detail_code = {},
            .context = {},
            .path = {}};
        output << serialize_inspection_json(inspection_error_json(error));
        return static_cast<int>(error.exit_code);
    }

    const InspectionResult result = inspect_runtime_assets(request);
    if (result.report.has_value()) {
        output << serialize_inspection_json(result.report.value());
        return static_cast<int>(InspectionExitCode::Success);
    }
    if (!result.error.has_value()) {
        const InspectionError error{.exit_code = InspectionExitCode::Package,
                                    .code = "internal_error",
                                    .message = "inspection produced no report or error",
                                    .requested_asset_id = {},
                                    .detail_code = {},
                                    .context = {},
                                    .path = {}};
        output << serialize_inspection_json(inspection_error_json(error));
        return static_cast<int>(error.exit_code);
    }
    const InspectionError& error = result.error.value();
    output << serialize_inspection_json(inspection_error_json(error));
    return static_cast<int>(error.exit_code);
}

} // namespace d2asset
