#include "runtime_package.hpp"

#include "commands_extract_atlas.hpp"
#include "commands_extract_dat.hpp"
#include "commands_extract_dbf.hpp"
#include "commands_extract_dlg.hpp"
#include "asset_reference_resolver.hpp"
#include "d2asset/asset_database.hpp"
#include "d2asset/asset_error.hpp"
#include "d2asset/data_table_manifest.hpp"
#include "d2res/game_scanner.hpp"
#include "extraction_orchestrator.hpp"
#include "manifest_builder.hpp"

#include <d2log/log.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <tuple>

#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

namespace {

auto kLog = d2log::get("d2.app"); // NOLINT(cert-err58-cpp)

constexpr int              kReportSchemaVersion = 1;
constexpr std::string_view kAtlasLogicalName = "__runtime_atlas";
constexpr std::string_view kAtlasType = "atlas";
std::atomic_uint64_t       staging_sequence{0};

class StagingDirectory {
public:
    explicit StagingDirectory(fs::path destination) : destination_(std::move(destination)) {
        if (fs::exists(destination_))
            throw std::runtime_error("destination already exists: " + destination_.string());

        fs::path parent = destination_.parent_path();
        if (parent.empty())
            parent = ".";
        std::error_code ec;
        fs::create_directories(parent, ec);
        if (ec)
            throw std::runtime_error("cannot create destination parent: " + ec.message());

        staging_ = parent / (destination_.filename().string() + ".staging-" +
                             std::to_string(++staging_sequence));
        fs::remove_all(staging_, ec);
        ec.clear();
        fs::create_directories(staging_, ec);
        if (ec)
            throw std::runtime_error("cannot create staging directory: " + ec.message());
    }

    StagingDirectory(const StagingDirectory&) = delete;
    StagingDirectory& operator=(const StagingDirectory&) = delete;
    StagingDirectory(StagingDirectory&&) = delete;
    StagingDirectory& operator=(StagingDirectory&&) = delete;

    ~StagingDirectory() {
        if (!published_) {
            std::error_code ec;
            fs::remove_all(staging_, ec);
        }
    }

    [[nodiscard]] const fs::path& path() const noexcept { return staging_; }

    void publish() {
        std::error_code ec;
        fs::rename(staging_, destination_, ec);
        if (ec)
            throw std::runtime_error("cannot publish runtime package: " + ec.message());
        published_ = true;
    }

private:
    fs::path destination_;
    fs::path staging_;
    bool     published_{false};
};

PackageDiagnostic make_diagnostic(PackageDiagnosticSeverity severity, std::string code,
                                  std::string message, fs::path path = {}) {
    return {.severity = severity,
            .code = std::move(code),
            .message = std::move(message),
            .path = std::move(path)};
}

const char* severity_name(PackageDiagnosticSeverity severity) {
    return severity == PackageDiagnosticSeverity::Error ? "error" : "warning";
}

const char* asset_error_code(d2asset::AssetErrorCode code) {
    using enum d2asset::AssetErrorCode;
    switch (code) {
    case MissingManifest:
        return "missing_manifest";
    case InvalidJson:
        return "invalid_json";
    case UnsupportedSchema:
        return "unsupported_schema";
    case MalformedEntry:
        return "malformed_entry";
    case DuplicateId:
        return "duplicate_id";
    case UnsafePath:
        return "unsafe_path";
    case MissingFile:
        return "missing_file";
    case MalformedAtlas:
        return "malformed_atlas";
    case DuplicateAtlasEntry:
        return "duplicate_atlas_entry";
    case InvalidAtlasRectangle:
        return "invalid_atlas_rectangle";
    case MissingAtlasSheet:
        return "missing_atlas_sheet";
    case MalformedAnimation:
        return "malformed_animation";
    case MalformedSound:
        return "malformed_sound";
    case UnsupportedSoundSchema:
        return "unsupported_sound_schema";
    case MissingSoundPayload:
        return "missing_sound_payload";
    case SoundPayloadSizeMismatch:
        return "sound_payload_size_mismatch";
    case MalformedDataTable:
        return "malformed_data_table";
    case UnsupportedDataTableSchema:
        return "unsupported_data_table_schema";
    case MalformedDataValue:
        return "malformed_data_value";
    case DuplicateDataRow:
        return "duplicate_data_row";
    case UnsupportedAssetLinksSchema:
        return "unsupported_asset_links_schema";
    case MalformedAssetLink:
        return "malformed_asset_link";
    case DuplicateAssetLink:
        return "duplicate_asset_link";
    }
    return "asset_error";
}

void sort_diagnostics(std::vector<PackageDiagnostic>& diagnostics) {
    std::ranges::sort(diagnostics, [](const auto& lhs, const auto& rhs) {
        return std::tie(lhs.severity, lhs.code, lhs.path, lhs.message) <
               std::tie(rhs.severity, rhs.code, rhs.path, rhs.message);
    });
}

nlohmann::json report_json(const PackageValidationResult& result) {
    nlohmann::json errors = nlohmann::json::array();
    nlohmann::json warnings = nlohmann::json::array();
    for (const auto& diagnostic : result.diagnostics) {
        nlohmann::json item{
            {"code", diagnostic.code},
            {"message", diagnostic.message},
        };
        if (!diagnostic.path.empty())
            item["path"] = diagnostic.path.generic_string();
        (diagnostic.severity == PackageDiagnosticSeverity::Error ? errors : warnings)
            .push_back(std::move(item));
    }

    return {
        {"report_schema_version", kReportSchemaVersion},
        {"package_path", "."},
        {"valid", result.valid},
        {"errors", errors},
        {"warnings", warnings},
        {"summary",
         {{"asset_link_count", result.asset_link_count},
          {"error_count", errors.size()},
          {"unresolved_asset_link_count", result.unresolved_asset_link_count},
          {"warning_count", warnings.size()}}},
    };
}

bool write_json_file(const fs::path& path, const nlohmann::json& json) {
    std::error_code ec;
    if (!path.parent_path().empty())
        fs::create_directories(path.parent_path(), ec);
    if (ec)
        return false;
    std::ofstream output(path);
    if (!output)
        return false;
    output << json.dump(2) << '\n';
    return static_cast<bool>(output);
}

fs::path relative_inside(const fs::path& path, const fs::path& root) {
    std::error_code ec;
    const fs::path  relative = fs::relative(path, root, ec);
    return ec ? fs::path{} : relative;
}

void copy_file_checked(const fs::path& source, const fs::path& destination) {
    std::error_code ec;
    fs::create_directories(destination.parent_path(), ec);
    if (ec)
        throw std::runtime_error("cannot create package directory: " + ec.message());
    fs::copy_file(source, destination, fs::copy_options::overwrite_existing, ec);
    if (ec)
        throw std::runtime_error("cannot copy " + source.string() + ": " + ec.message());
}

fs::path canonical_container_path(std::string_view type, const std::string& container_path) {
    return package_subtree_for_type(type) / fs::path(container_path);
}

fs::path package_extracted_sound_impl(const fs::path&  extractor_sidecar,
                                      std::string_view logical_name,
                                      std::string_view container_path, std::string_view asset_id,
                                      const fs::path& package_root,
                                      const fs::path& destination_base) {
    nlohmann::json extractor;
    {
        std::ifstream input(extractor_sidecar);
        if (!input) {
            throw std::runtime_error("cannot open extracted sound sidecar: " +
                                     extractor_sidecar.string());
        }
        try {
            input >> extractor;
        } catch (const nlohmann::json::exception& error) {
            throw std::runtime_error("invalid extracted sound sidecar: " +
                                     std::string(error.what()));
        }
    }
    if (!extractor.is_object() || !extractor.contains("logical_name") ||
        !extractor["logical_name"].is_string() || !extractor.contains("payload_size") ||
        (!extractor["payload_size"].is_number_integer() &&
         !extractor["payload_size"].is_number_unsigned()) ||
        !extractor.contains("detected_format") || !extractor["detected_format"].is_string()) {
        throw std::runtime_error("extracted sound sidecar has invalid required fields: " +
                                 extractor_sidecar.string());
    }
    if (extractor["logical_name"].get<std::string>() != logical_name)
        throw std::runtime_error("extracted sound logical name does not match collected asset");
    const auto& extracted_size = extractor["payload_size"];
    if ((extracted_size.is_number_integer() && extracted_size.get<std::int64_t>() <= 0) ||
        (extracted_size.is_number_unsigned() && extracted_size.get<std::uint64_t>() == 0)) {
        throw std::runtime_error("extracted sound payload_size must be positive");
    }

    fs::path payload;
    for (const std::string_view extension : {".wav", ".bin"}) {
        const fs::path candidate = extractor_sidecar.parent_path() /
                                   (extractor_sidecar.stem().string() + std::string(extension));
        if (fs::is_regular_file(candidate)) {
            if (!payload.empty()) {
                throw std::runtime_error("multiple extracted sound payloads found for: " +
                                         extractor_sidecar.string());
            }
            payload = candidate;
        }
    }
    if (payload.empty()) {
        throw std::runtime_error("extracted sound payload is missing for: " +
                                 extractor_sidecar.string());
    }

    const fs::path destination_payload = destination_base / payload.filename();
    const fs::path destination_sidecar = destination_base / extractor_sidecar.filename();
    copy_file_checked(payload, destination_payload);
    const fs::path payload_relative = relative_inside(destination_payload, package_root);
    if (payload_relative.empty())
        throw std::runtime_error("cannot make sound payload package-relative");

    const std::string extractor_format = extractor["detected_format"].get<std::string>();
    std::string       runtime_format;
    if (extractor_format == "WAV") {
        runtime_format = "wave";
    } else if (extractor_format == "UNKNOWN") {
        runtime_format = "unknown";
    } else {
        throw std::runtime_error("unsupported extracted sound format label: " + extractor_format);
    }

    nlohmann::json canonical{
        {"sound_schema_version", d2asset::sound_schema_version},
        {"asset_id", std::string(asset_id)},
        {"logical_name", std::string(logical_name)},
        {"container_id", ManifestBuilder::make_id(std::string(container_path))},
        {"payload_path", payload_relative.generic_string()},
        {"payload_size", fs::file_size(destination_payload)},
        {"detected_format", runtime_format},
        {"format_tag", nullptr},
        {"channels", nullptr},
        {"sample_rate", nullptr},
        {"bit_depth", nullptr},
        {"duration_ms", nullptr},
        {"warnings", nlohmann::json::array()},
    };
    for (const std::string_view field :
         {"format_tag", "channels", "sample_rate", "bit_depth", "duration_ms"}) {
        const auto value = extractor.find(field);
        if (value != extractor.end())
            canonical[std::string(field)] = *value;
    }
    const auto source = extractor.find("source_container");
    if (source != extractor.end() && source->is_string())
        canonical["source_container"] = *source;
    if (!write_json_file(destination_sidecar, canonical)) {
        throw std::runtime_error("cannot write canonical sound sidecar: " +
                                 destination_sidecar.string());
    }
    return relative_inside(destination_sidecar, package_root);
}

nlohmann::json read_json_checked(const fs::path& path, std::string_view description) {
    std::ifstream input(path);
    if (!input)
        throw std::runtime_error("cannot open " + std::string(description) + ": " + path.string());
    nlohmann::json value;
    try {
        input >> value;
    } catch (const nlohmann::json::exception& error) {
        throw std::runtime_error("invalid " + std::string(description) + ": " + error.what());
    }
    return value;
}

nlohmann::json canonical_column(std::string name) {
    return {{"name", std::move(name)},
            {"source_type", nullptr},
            {"width", nullptr},
            {"decimal_count", nullptr},
            {"extensions", nullptr}};
}

nlohmann::json canonical_cell(const std::string& name, const nlohmann::json& value) {
    return {{"name", name}, {"value", value}};
}

std::string indexed_row_key(std::size_t index) {
    std::ostringstream output;
    output << std::setfill('0') << std::setw(8) << index;
    return output.str();
}

fs::path write_canonical_data_table(std::string_view kind, std::string_view logical_name,
                                    std::string_view container_path, std::string_view asset_id,
                                    nlohmann::json columns, nlohmann::json rows,
                                    nlohmann::json warnings, nlohmann::json extensions,
                                    const fs::path& package_root) {
    const fs::path destination_base =
        package_root / canonical_container_path("data_table", std::string(container_path));
    const fs::path destination_sidecar = destination_base / (std::string(logical_name) + ".json");
    const nlohmann::json canonical{
        {"data_table_schema_version", d2asset::data_table_schema_version},
        {"asset_id", std::string(asset_id)},
        {"logical_name", std::string(logical_name)},
        {"container_id", ManifestBuilder::make_id(std::string(container_path))},
        {"kind", std::string(kind)},
        {"columns", std::move(columns)},
        {"rows", std::move(rows)},
        {"warnings", std::move(warnings)},
        {"extensions", std::move(extensions)},
    };
    if (!write_json_file(destination_sidecar, canonical)) {
        throw std::runtime_error("cannot write canonical data-table sidecar: " +
                                 destination_sidecar.string());
    }
    return relative_inside(destination_sidecar, package_root);
}

fs::path package_extracted_dbf_impl(const fs::path& schema_json, const fs::path& records_json,
                                    std::string_view logical_name, std::string_view container_path,
                                    std::string_view asset_id, const fs::path& package_root) {
    const nlohmann::json schema = read_json_checked(schema_json, "extracted DBF schema JSON");
    const nlohmann::json records = read_json_checked(records_json, "extracted DBF records JSON");
    if (!schema.is_object() || !schema.contains("fields") || !schema["fields"].is_array() ||
        !records.is_array()) {
        throw std::runtime_error("extracted DBF JSON has invalid root structure");
    }

    nlohmann::json           columns = nlohmann::json::array();
    std::vector<std::string> column_names;
    for (const auto& field : schema["fields"]) {
        if (!field.is_object() || !field.contains("name") || !field["name"].is_string() ||
            field["name"].get<std::string>().empty()) {
            throw std::runtime_error("extracted DBF field has invalid name");
        }
        const std::string name = field["name"].get<std::string>();
        nlohmann::json    column = canonical_column(name);
        for (const std::string_view key : {"type", "length", "decimal_count"}) {
            const auto value = field.find(key);
            if (value == field.end())
                continue;
            if (key == "type") {
                column["source_type"] = *value;
            } else if (key == "length") {
                column["width"] = *value;
            } else {
                column["decimal_count"] = *value;
            }
        }
        columns.push_back(std::move(column));
        column_names.push_back(name);
    }

    nlohmann::json rows = nlohmann::json::array();
    for (std::size_t index = 0; index < records.size(); ++index) {
        if (!records[index].is_object())
            throw std::runtime_error("extracted DBF record must be an object");
        nlohmann::json values = nlohmann::json::array();
        for (const std::string& name : column_names) {
            const auto value = records[index].find(name);
            values.push_back(canonical_cell(
                name, value == records[index].end() ? nlohmann::json(nullptr) : *value));
        }
        for (auto it = records[index].begin(); it != records[index].end(); ++it) {
            if (std::ranges::find(column_names, it.key()) == column_names.end())
                values.push_back(canonical_cell(it.key(), it.value()));
        }
        rows.push_back({{"row_key", indexed_row_key(index)}, {"values", std::move(values)}});
    }

    nlohmann::json extensions = nlohmann::json::object();
    const auto     source = schema.find("source");
    if (source != schema.end())
        extensions["source"] = *source;
    return write_canonical_data_table("dbf", logical_name, container_path, asset_id,
                                      std::move(columns), std::move(rows), nlohmann::json::array(),
                                      std::move(extensions), package_root);
}

fs::path package_extracted_dat_impl(const fs::path& extractor_json, std::string_view logical_name,
                                    std::string_view container_path, std::string_view asset_id,
                                    const fs::path& package_root) {
    const nlohmann::json extracted = read_json_checked(extractor_json, "extracted DAT JSON");
    if (!extracted.is_object())
        throw std::runtime_error("extracted DAT JSON must be an object");
    nlohmann::json columns =
        nlohmann::json::array({canonical_column("key"), canonical_column("value")});
    nlohmann::json           rows = nlohmann::json::array();
    std::vector<std::string> keys;
    keys.reserve(extracted.size());
    for (auto it = extracted.begin(); it != extracted.end(); ++it)
        keys.push_back(it.key());
    std::ranges::sort(keys);
    for (const std::string& key : keys) {
        rows.push_back(
            {{"row_key", key},
             {"values", nlohmann::json::array({canonical_cell("key", key),
                                               canonical_cell("value", extracted.at(key))})}});
    }
    return write_canonical_data_table("dat", logical_name, container_path, asset_id,
                                      std::move(columns), std::move(rows), nlohmann::json::array(),
                                      nullptr, package_root);
}

fs::path package_extracted_dlg_impl(const fs::path& extractor_json, std::string_view logical_name,
                                    std::string_view container_path, std::string_view asset_id,
                                    const fs::path& package_root) {
    const nlohmann::json extracted = read_json_checked(extractor_json, "extracted DLG JSON");
    if (!extracted.is_array())
        throw std::runtime_error("extracted DLG JSON must be an array");
    std::vector<std::string> column_names;
    for (const auto& dialog : extracted) {
        if (!dialog.is_object())
            throw std::runtime_error("extracted DLG dialog must be an object");
        for (auto it = dialog.begin(); it != dialog.end(); ++it) {
            if (std::ranges::find(column_names, it.key()) == column_names.end())
                column_names.push_back(it.key());
        }
    }
    nlohmann::json columns = nlohmann::json::array();
    for (const std::string& name : column_names)
        columns.push_back(canonical_column(name));

    nlohmann::json        rows = nlohmann::json::array();
    nlohmann::json        warnings = nlohmann::json::array();
    std::set<std::string> used_keys;
    for (std::size_t index = 0; index < extracted.size(); ++index) {
        const auto  id = extracted[index].find("id");
        std::string row_key;
        if (id != extracted[index].end() && id->is_string() && !id->get<std::string>().empty() &&
            used_keys.insert(id->get<std::string>()).second) {
            row_key = id->get<std::string>();
        } else {
            row_key = indexed_row_key(index);
            while (!used_keys.insert(row_key).second) {
                row_key.insert(row_key.begin(), '_');
            }
            warnings.push_back("dialog row " + std::to_string(index) +
                               " uses a deterministic fallback row key");
        }
        nlohmann::json values = nlohmann::json::array();
        for (const std::string& name : column_names) {
            const auto value = extracted[index].find(name);
            if (value != extracted[index].end())
                values.push_back(canonical_cell(name, *value));
        }
        rows.push_back({{"row_key", std::move(row_key)}, {"values", std::move(values)}});
    }
    return write_canonical_data_table("dlg", logical_name, container_path, asset_id,
                                      std::move(columns), std::move(rows), std::move(warnings),
                                      nullptr, package_root);
}

fs::path assemble_asset(const ExtractedAsset& asset, const std::string& container_path,
                        const std::string& asset_id, const fs::path& package_root) {
    const std::string_view type = to_string(asset.kind);
    const fs::path         relative_base = canonical_container_path(type, container_path);
    const fs::path         destination_base = package_root / relative_base;

    if (asset.kind == ExtractedAssetKind::Animation) {
        const fs::path destination_dir =
            destination_base / asset.sidecar_path.parent_path().filename();
        std::error_code ec;
        fs::create_directories(destination_dir, ec);
        if (ec)
            throw std::runtime_error("cannot create animation package directory: " + ec.message());
        fs::copy(asset.sidecar_path.parent_path(), destination_dir,
                 fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
        if (ec)
            throw std::runtime_error("cannot copy animation output: " + ec.message());
        return relative_inside(destination_dir / "anim.json", package_root);
    }

    if (asset.kind == ExtractedAssetKind::Sound) {
        return package_extracted_sound_impl(asset.sidecar_path, asset.logical_name, container_path,
                                            asset_id, package_root, destination_base);
    }

    const fs::path destination_sidecar = destination_base / asset.sidecar_path.filename();
    copy_file_checked(asset.sidecar_path, destination_sidecar);

    const fs::path payload =
        asset.sidecar_path.parent_path() / (asset.sidecar_path.stem().string() + ".png");
    if (fs::is_regular_file(payload))
        copy_file_checked(payload, destination_base / payload.filename());

    return relative_inside(destination_sidecar, package_root);
}

void create_package_directories(const fs::path& root) {
    for (const std::string_view directory :
         {"images", "animations", "atlases", "sounds", "data", "reports"}) {
        std::error_code ec;
        fs::create_directories(root / directory, ec);
        if (ec) {
            throw std::runtime_error("cannot create package directory " + std::string(directory) +
                                     ": " + ec.message());
        }
    }
}

void append_build_report(const fs::path&                       package_root,
                         const std::vector<PackageDiagnostic>& diagnostics, bool success) {
    PackageValidationResult result;
    result.valid = success;
    result.diagnostics = diagnostics;
    sort_diagnostics(result.diagnostics);
    if (!write_json_file(package_root / "reports/build_report.json", report_json(result)))
        throw std::runtime_error("cannot write build report");
}

} // namespace

namespace runtime_package_detail {

fs::path package_extracted_sound(const fs::path& extractor_sidecar, std::string_view logical_name,
                                 std::string_view container_path, std::string_view asset_id,
                                 const fs::path& package_root) {
    const fs::path destination_base =
        package_root / canonical_container_path("sound", std::string(container_path));
    return package_extracted_sound_impl(extractor_sidecar, logical_name, container_path, asset_id,
                                        package_root, destination_base);
}

fs::path package_extracted_dbf(const fs::path& schema_json, const fs::path& records_json,
                               std::string_view logical_name, std::string_view container_path,
                               std::string_view asset_id, const fs::path& package_root) {
    return package_extracted_dbf_impl(schema_json, records_json, logical_name, container_path,
                                      asset_id, package_root);
}

fs::path package_extracted_dat(const fs::path& extractor_json, std::string_view logical_name,
                               std::string_view container_path, std::string_view asset_id,
                               const fs::path& package_root) {
    return package_extracted_dat_impl(extractor_json, logical_name, container_path, asset_id,
                                      package_root);
}

fs::path package_extracted_dlg(const fs::path& extractor_json, std::string_view logical_name,
                               std::string_view container_path, std::string_view asset_id,
                               const fs::path& package_root) {
    return package_extracted_dlg_impl(extractor_json, logical_name, container_path, asset_id,
                                      package_root);
}

} // namespace runtime_package_detail

fs::path package_subtree_for_type(std::string_view type) {
    if (type == "image")
        return "images";
    if (type == "animation")
        return "animations";
    if (type == "atlas")
        return "atlases";
    if (type == "sound")
        return "sounds";
    if (type == "data_table")
        return "data";
    return {};
}

bool is_safe_package_relative_path(const fs::path& path) {
    if (path.empty() || path.is_absolute())
        return false;
    const fs::path normalized = path.lexically_normal();
    if (normalized.empty() || normalized == ".")
        return false;
    for (const auto& component : normalized) {
        if (component == "..")
            return false;
    }
    return true;
}

PackageValidationResult validate_runtime_package(const fs::path& asset_root,
                                                 const fs::path& report_path) {
    PackageValidationResult result;
    const std::array        required_entries{
        fs::path{"game_manifest.json"},
        fs::path{"images"},
        fs::path{"animations"},
        fs::path{"atlases"},
        fs::path{"sounds"},
        fs::path{"data"},
        fs::path{"reports"},
    };
    for (const auto& relative : required_entries) {
        if (!fs::exists(asset_root / relative)) {
            result.diagnostics.push_back(
                make_diagnostic(PackageDiagnosticSeverity::Error, "missing_required_entry",
                                "required package entry is missing", relative));
        }
    }

    const fs::path manifest_path = asset_root / "game_manifest.json";
    nlohmann::json manifest;
    if (fs::is_regular_file(manifest_path)) {
        try {
            std::ifstream input(manifest_path);
            input >> manifest;
            if (manifest.contains("assets") && manifest["assets"].is_array()) {
                for (const auto& asset : manifest["assets"]) {
                    if (!asset.is_object() || !asset.contains("path") ||
                        !asset["path"].is_string()) {
                        continue;
                    }
                    const fs::path relative = asset["path"].get<std::string>();
                    if (!is_safe_package_relative_path(relative)) {
                        result.diagnostics.push_back(make_diagnostic(
                            PackageDiagnosticSeverity::Error, "unsafe_asset_path",
                            "asset path is not a safe package-relative path", relative));
                    } else if (!fs::is_regular_file(asset_root / relative)) {
                        result.diagnostics.push_back(
                            make_diagnostic(PackageDiagnosticSeverity::Error, "missing_asset_file",
                                            "manifest asset file is missing", relative));
                    }
                }
            }
        } catch (const nlohmann::json::exception& error) {
            result.diagnostics.push_back(make_diagnostic(PackageDiagnosticSeverity::Error,
                                                         "invalid_manifest_json", error.what(),
                                                         "game_manifest.json"));
        }
    }

    const bool structural_errors = std::ranges::any_of(result.diagnostics, [](const auto& item) {
        return item.severity == PackageDiagnosticSeverity::Error;
    });
    if (!structural_errors) {
        try {
            const d2asset::AssetDatabase database = d2asset::AssetDatabase::open(asset_root);
            result.asset_link_count = database.asset_links().links.size();
            result.unresolved_asset_link_count = database.asset_links().unresolved.size();
            for (const auto& warning : database.manifest().warnings()) {
                result.diagnostics.push_back(make_diagnostic(PackageDiagnosticSeverity::Warning,
                                                             "manifest_warning", warning));
            }
            for (const auto& warning : database.animation_diagnostics()) {
                result.diagnostics.push_back(make_diagnostic(PackageDiagnosticSeverity::Warning,
                                                             "animation_warning", warning.message));
            }
            for (const auto& warning : database.sound_diagnostics()) {
                result.diagnostics.push_back(make_diagnostic(PackageDiagnosticSeverity::Warning,
                                                             "sound_warning", warning.message));
            }
            for (const auto& warning : database.data_table_diagnostics()) {
                result.diagnostics.push_back(make_diagnostic(
                    PackageDiagnosticSeverity::Warning, "data_table_warning", warning.message));
            }
        } catch (const d2asset::AssetError& error) {
            result.diagnostics.push_back(make_diagnostic(PackageDiagnosticSeverity::Error,
                                                         asset_error_code(error.code()),
                                                         error.what(), error.path()));
        } catch (const std::exception& error) {
            result.diagnostics.push_back(make_diagnostic(PackageDiagnosticSeverity::Error,
                                                         "database_open_failed", error.what()));
        }
    }

    sort_diagnostics(result.diagnostics);
    result.valid = std::ranges::none_of(result.diagnostics, [](const auto& item) {
        return item.severity == PackageDiagnosticSeverity::Error;
    });

    const fs::path output_report =
        report_path.empty() ? asset_root / "reports/validation_report.json" : report_path;
    if (!write_json_file(output_report, report_json(result))) {
        result.diagnostics.push_back(
            make_diagnostic(PackageDiagnosticSeverity::Error, "report_write_failed",
                            "cannot write validation report", output_report));
        result.valid = false;
        sort_diagnostics(result.diagnostics);
    }
    return result;
}

RuntimePackageBuildResult build_runtime_package(const RuntimePackageBuildOptions& options) {
    RuntimePackageBuildResult result;
    if (!fs::is_directory(options.game_root)) {
        result.diagnostics.push_back(
            make_diagnostic(PackageDiagnosticSeverity::Error, "invalid_game_root",
                            "game root is not a directory", options.game_root));
        return result;
    }
    if (fs::exists(options.output_root)) {
        result.diagnostics.push_back(
            make_diagnostic(PackageDiagnosticSeverity::Error, "destination_exists",
                            "destination already exists", options.output_root));
        return result;
    }

    try {
        StagingDirectory staging(options.output_root);
        const fs::path&  package_root = staging.path();
        create_package_directories(package_root);
        const fs::path work_root = package_root / ".work";
        fs::create_directories(work_root);

        ManifestBuilder builder;
        builder.set_game_root(options.game_root.string());
        std::set<std::string> asset_ids;

        const d2res::ScanResult scan = d2res::GameScanner::scan(options.game_root.string());
        for (const auto& warning : scan.warnings) {
            result.diagnostics.push_back(
                make_diagnostic(PackageDiagnosticSeverity::Warning, "scan_warning", warning));
            builder.add_warning(warning);
        }

        const std::set<std::string> filter_containers(options.containers.begin(),
                                                      options.containers.end());
        const std::size_t           total_containers = scan.containers.size();
        std::size_t                 container_index = 0;
        for (const auto& container : scan.containers) {
            if (!filter_containers.empty() && !filter_containers.contains(container.path)) {
                continue;
            }
            ++container_index;
            kLog->info("processing_container index={} total={} path={}", container_index,
                       total_containers, container.path);

            if (!has_supported_runtime_content(container)) {
                kLog->warn("skipping_unsupported_container path={}", container.path);
                const std::string warning = "unsupported container: " + container.path;
                result.diagnostics.push_back(make_diagnostic(PackageDiagnosticSeverity::Warning,
                                                             "unsupported_container", warning,
                                                             container.path));
                builder.add_warning(warning);
                continue;
            }

            const fs::path    work_path = work_root / container.path;
            ExtractionOptions extraction_options;
            extraction_options.animation_frame_atlases = false;
            extraction_options.atlas_max_size = options.atlas_max_size;
            const ContainerExtraction outcome = extract_container_assets(
                options.game_root, container, work_path, extraction_options);
            kLog->info("container_extracted path={}", container.path);
            builder.add_container(container.path, outcome.content_kinds);
            if (!outcome.succeeded) {
                const std::string message =
                    "failed to extract " + container.path + ": " + outcome.error;
                result.diagnostics.push_back(make_diagnostic(PackageDiagnosticSeverity::Warning,
                                                             "container_extraction_failed", message,
                                                             container.path));
                builder.add_warning(message);
                continue;
            }

            for (const auto& asset : outcome.assets) {
                const std::string asset_id = ManifestBuilder::make_id(container.path) + "/" +
                                             ManifestBuilder::make_id(asset.logical_name);
                if (!asset_ids.insert(asset_id).second)
                    throw std::runtime_error("duplicate package asset ID: " + asset_id);
                const fs::path relative =
                    assemble_asset(asset, container.path, asset_id, package_root);
                builder.add_asset(to_string(asset.kind), asset.logical_name, container.path,
                                  relative.generic_string());
            }

            for (const auto& warning : outcome.warnings) {
                result.diagnostics.push_back(make_diagnostic(PackageDiagnosticSeverity::Warning,
                                                             "extraction_warning", warning,
                                                             container.path));
                builder.add_warning(container.path + ": " + warning);
            }

            if (std::ranges::find(container.likely_content, d2res::ContentKind::Images) !=
                container.likely_content.end()) {
                const std::string atlas_asset_id = ManifestBuilder::make_id(container.path) + "/" +
                                                   ManifestBuilder::make_id(kAtlasLogicalName);
                if (!asset_ids.insert(atlas_asset_id).second) {
                    throw std::runtime_error("reserved atlas asset ID collision: " +
                                             atlas_asset_id);
                }

                const fs::path atlas_relative =
                    canonical_container_path(kAtlasType, container.path);
                const fs::path atlas_output = package_root / atlas_relative;
                const fs::path source_container = options.game_root / container.path;
                kLog->info("atlas_generation container={}", container.path);
                if (cmd_extract_atlas(source_container.string(), atlas_output.string(), "", true,
                                      options.atlas_max_size) == 0) {
                    kLog->info("atlas_generation_ok container={}", container.path);
                    builder.add_asset(std::string(kAtlasType), std::string(kAtlasLogicalName),
                                      container.path,
                                      (atlas_relative / "atlas.json").generic_string());
                    const fs::path warnings_path = atlas_output / "warnings.txt";
                    if (fs::is_regular_file(warnings_path)) {
                        std::ifstream input(warnings_path);
                        for (std::string line; std::getline(input, line);) {
                            if (!line.empty()) {
                                result.diagnostics.push_back(
                                    make_diagnostic(PackageDiagnosticSeverity::Warning,
                                                    "atlas_warning", line, atlas_relative));
                            }
                        }
                    }
                } else {
                    kLog->error("atlas_generation_failed container={}", container.path);
                    asset_ids.erase(atlas_asset_id);
                    const std::string message = "atlas generation failed for " + container.path;
                    result.diagnostics.push_back(make_diagnostic(PackageDiagnosticSeverity::Error,
                                                                 "atlas_generation_failed", message,
                                                                 container.path));
                    builder.add_warning(message);
                }
            }
        }

        const auto total_other_files =
            static_cast<std::size_t>(std::ranges::count_if(scan.other_files, [](const auto& f) {
                return f.type == "dbf" || f.type == "dat" || f.type == "dlg";
            }));
        std::size_t other_file_index = 0;
        for (const auto& file : scan.other_files) {
            if (file.type != "dbf" && file.type != "dat" && file.type != "dlg")
                continue;
            ++other_file_index;
            kLog->info("processing_data_table index={} total={} path={}", other_file_index,
                       total_other_files, file.path);
            const fs::path    source = options.game_root / file.path;
            const fs::path    extracted_dir = work_root / file.path;
            const std::string logical_name = source.stem().string();
            const std::string container_id = ManifestBuilder::make_id(file.path);
            const std::string asset_id =
                container_id + "/" + ManifestBuilder::make_id(logical_name);
            if (!asset_ids.insert(asset_id).second)
                throw std::runtime_error("duplicate package asset ID: " + asset_id);
            fs::create_directories(extracted_dir);

            fs::path relative;
            if (file.type == "dbf") {
                if (cmd_extract_dbf(source.string(), extracted_dir.string()) != 0)
                    throw std::runtime_error("DBF extraction failed: " + file.path);
                relative =
                    package_extracted_dbf_impl(extracted_dir / (logical_name + ".schema.json"),
                                               extracted_dir / (logical_name + ".records.json"),
                                               logical_name, file.path, asset_id, package_root);
            } else if (file.type == "dat") {
                if (cmd_extract_dat(source.string(), extracted_dir.string()) != 0)
                    throw std::runtime_error("DAT extraction failed: " + file.path);
                relative =
                    package_extracted_dat_impl(extracted_dir / (logical_name + ".json"),
                                               logical_name, file.path, asset_id, package_root);
            } else {
                if (cmd_extract_dlg(source.string(), extracted_dir.string()) != 0)
                    throw std::runtime_error("DLG extraction failed: " + file.path);
                relative =
                    package_extracted_dlg_impl(extracted_dir / (logical_name + ".json"),
                                               logical_name, file.path, asset_id, package_root);
            }
            builder.add_container(file.path, {"data"});
            builder.add_asset("data_table", logical_name, file.path, relative.generic_string());
            kLog->info("data_table_extracted path={}", file.path);
        }

        builder.write(package_root);
        AssetReferenceResolver::write(package_root);
        std::error_code ec;
        fs::remove_all(work_root, ec);
        const bool build_has_errors =
            std::ranges::any_of(result.diagnostics, [](const auto& diagnostic) {
                return diagnostic.severity == PackageDiagnosticSeverity::Error;
            });
        append_build_report(package_root, result.diagnostics, !build_has_errors);
        if (build_has_errors)
            return result;

        const PackageValidationResult validation = validate_runtime_package(package_root);
        result.diagnostics.insert(result.diagnostics.end(), validation.diagnostics.begin(),
                                  validation.diagnostics.end());
        if (!validation.valid)
            return result;

        staging.publish();
        result.published = true;
    } catch (const std::exception& error) {
        result.diagnostics.push_back(
            make_diagnostic(PackageDiagnosticSeverity::Error, "build_failed", error.what()));
    }
    sort_diagnostics(result.diagnostics);
    return result;
}

int cmd_build_runtime_assets(const std::string& game_dir, const std::string& out_dir,
                             const std::vector<std::string>& containers) {
    RuntimePackageBuildOptions options;
    options.game_root = game_dir;
    options.output_root = out_dir;
    options.containers = containers;
    const RuntimePackageBuildResult result = build_runtime_package(options);
    for (const auto& diagnostic : result.diagnostics) {
        const std::string path_suffix =
            diagnostic.path.empty() ? "" : " (" + diagnostic.path.string() + ")";
        kLog->info("build_diagnostic severity={} code={} message={}{}",
                   severity_name(diagnostic.severity), diagnostic.code, diagnostic.message,
                   path_suffix);
    }
    return result.published ? 0 : 1;
}

int cmd_validate_runtime_assets(const std::string& asset_root, const std::string& report_path) {
    const fs::path output_report = report_path.empty() ? fs::path{} : fs::path(report_path);
    const PackageValidationResult result = validate_runtime_package(asset_root, output_report);
    for (const auto& diagnostic : result.diagnostics) {
        const std::string path_suffix =
            diagnostic.path.empty() ? "" : " (" + diagnostic.path.string() + ")";
        kLog->info("validate_diagnostic severity={} code={} message={}{}",
                   severity_name(diagnostic.severity), diagnostic.code, diagnostic.message,
                   path_suffix);
    }
    return result.valid ? 0 : 1;
}
