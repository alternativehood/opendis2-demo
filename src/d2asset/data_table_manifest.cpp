#include "data_table_manifest.hpp"

#include "asset_error.hpp"
#include "asset_id.hpp"

#include <charconv>
#include <cmath>
#include <fstream>
#include <limits>
#include <unordered_set>

#include <nlohmann/json.hpp>

namespace d2asset {
namespace {

namespace fs = std::filesystem;
using Json = nlohmann::ordered_json;

[[noreturn]] void malformed(AssetErrorCode code, const std::string& message,
                            const std::string& context, const fs::path& path) {
    throw AssetError(code, message, context, path);
}

const Json& require_field(const Json& object, std::string_view field, Json::value_t type,
                          const std::string& context, const fs::path& path) {
    if (!object.is_object()) {
        malformed(AssetErrorCode::MalformedDataTable, "data-table value must be an object", context,
                  path);
    }
    const auto it = object.find(field);
    if (it == object.end() || it->type() != type) {
        malformed(AssetErrorCode::MalformedDataTable, "required data-table field has invalid type",
                  context + "." + std::string(field), path);
    }
    return *it;
}

std::string require_string(const Json& object, std::string_view field, const std::string& context,
                           const fs::path& path) {
    const std::string value =
        require_field(object, field, Json::value_t::string, context, path).get<std::string>();
    if (value.empty()) {
        malformed(AssetErrorCode::MalformedDataTable, "required data-table string is empty",
                  context + "." + std::string(field), path);
    }
    return value;
}

std::uint64_t positive_integer(const Json& object, std::string_view field,
                               const std::string& context, const fs::path& path) {
    const auto it = object.find(field);
    if (it == object.end() || (!it->is_number_integer() && !it->is_number_unsigned())) {
        malformed(AssetErrorCode::MalformedDataTable,
                  "data-table schema version must be an integer",
                  context + "." + std::string(field), path);
    }
    const std::int64_t  signed_value = it->is_number_unsigned() ? 0 : it->get<std::int64_t>();
    const std::uint64_t value = it->is_number_unsigned() ? it->get<std::uint64_t>()
                                                         : static_cast<std::uint64_t>(signed_value);
    if ((!it->is_number_unsigned() && signed_value <= 0) || value == 0) {
        malformed(AssetErrorCode::MalformedDataTable, "data-table schema version must be positive",
                  context + "." + std::string(field), path);
    }
    return value;
}

std::optional<std::uint32_t> optional_u32(const Json& object, std::string_view field,
                                          const std::string& context, const fs::path& path) {
    const auto it = object.find(field);
    if (it == object.end() || it->is_null())
        return std::nullopt;
    if (!it->is_number_integer() && !it->is_number_unsigned()) {
        malformed(AssetErrorCode::MalformedDataTable, "column metadata must be an integer",
                  context + "." + std::string(field), path);
    }
    if (it->is_number_integer() && it->get<std::int64_t>() < 0) {
        malformed(AssetErrorCode::MalformedDataTable, "column metadata must not be negative",
                  context + "." + std::string(field), path);
    }
    const std::uint64_t value = it->is_number_unsigned()
                                    ? it->get<std::uint64_t>()
                                    : static_cast<std::uint64_t>(it->get<std::int64_t>());
    if (value > std::numeric_limits<std::uint32_t>::max()) {
        malformed(AssetErrorCode::MalformedDataTable, "column metadata is out of range",
                  context + "." + std::string(field), path);
    }
    return static_cast<std::uint32_t>(value);
}

// NOLINTNEXTLINE(misc-no-recursion)
DataValue parse_value(const Json& json, const std::string& context, const fs::path& path) {
    if (json.is_null())
        return {.value = std::monostate{}};
    if (json.is_boolean())
        return {.value = json.get<bool>()};
    if (json.is_number_unsigned()) {
        const std::uint64_t value = json.get<std::uint64_t>();
        if (value > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
            malformed(AssetErrorCode::MalformedDataValue,
                      "unsigned data value exceeds the supported integer range", context, path);
        }
        return {.value = static_cast<std::int64_t>(value)};
    }
    if (json.is_number_integer())
        return {.value = json.get<std::int64_t>()};
    if (json.is_number_float()) {
        const double value = json.get<double>();
        if (!std::isfinite(value)) {
            malformed(AssetErrorCode::MalformedDataValue,
                      "floating-point data value must be finite", context, path);
        }
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
        for (auto it = json.begin(); it != json.end(); ++it) {
            result.emplace_back(it.key(), parse_value(it.value(), context + "." + it.key(), path));
        }
        return {.value = std::move(result)};
    }
    malformed(AssetErrorCode::MalformedDataValue, "unsupported JSON data value", context, path);
}

void validate_identity(const std::string& actual, const std::string& expected,
                       std::string_view field, const std::string& context, const fs::path& path,
                       bool case_insensitive) {
    const bool matches = case_insensitive ? normalize_ascii(actual) == normalize_ascii(expected)
                                          : actual == expected;
    if (!matches) {
        malformed(AssetErrorCode::MalformedDataTable,
                  "data-table sidecar identity does not match manifest",
                  context + "." + std::string(field), path);
    }
}

DataAccessError access_error(DataAccessErrorCode code, const DataTable& table, std::string row_key,
                             std::string column_name, std::string message) {
    return {.code = code,
            .table_asset_id = table.data_table_asset_id,
            .row_key = std::move(row_key),
            .column_name = std::move(column_name),
            .message = std::move(message)};
}

template <typename T>
DataAccessResult<T> conversion_error(const DataTable& table, std::string_view row_key,
                                     std::string_view column_name) {
    return DataAccessResult<T>::failed(
        access_error(DataAccessErrorCode::TypeConversion, table, std::string(row_key),
                     std::string(column_name), "data value cannot be converted to requested type"));
}

std::optional<std::string_view> string_storage(const DataValue& value) {
    if (const auto* text = std::get_if<std::string>(&value.value))
        return *text;
    return std::nullopt;
}

} // namespace

DataValueKind DataValue::kind() const noexcept {
    return static_cast<DataValueKind>(value.index());
}

const DataValue* DataRow::find(std::string_view column_name) const noexcept {
    for (const DataCell& cell : values) {
        if (cell.name == column_name)
            return &cell.value;
    }
    return nullptr;
}

DataAccessResult<const DataRow*> DataTable::find_row(std::string_view row_key) const {
    for (const DataRow& row : rows) {
        if (row.row_key == row_key)
            return DataAccessResult<const DataRow*>::found(&row);
    }
    return DataAccessResult<const DataRow*>::failed(access_error(DataAccessErrorCode::RowNotFound,
                                                                 *this, std::string(row_key), {},
                                                                 "data-table row was not found"));
}

DataAccessResult<const DataValue*> DataTable::find_value(std::string_view row_key,
                                                         std::string_view column_name) const {
    const auto row = find_row(row_key);
    if (!row.value.has_value()) {
        return DataAccessResult<const DataValue*>::failed(row.error.value_or(
            access_error(DataAccessErrorCode::RowNotFound, *this, std::string(row_key), {},
                         "data-table row was not found")));
    }
    const DataValue* value = (*row.value)->find(column_name);
    if (value == nullptr) {
        return DataAccessResult<const DataValue*>::failed(
            access_error(DataAccessErrorCode::ColumnNotFound, *this, std::string(row_key),
                         std::string(column_name), "data-table column was not found"));
    }
    return DataAccessResult<const DataValue*>::found(value);
}

DataAccessResult<std::string> DataTable::string_value(std::string_view row_key,
                                                      std::string_view column_name) const {
    const auto raw = find_value(row_key, column_name);
    if (!raw.value.has_value()) {
        return DataAccessResult<std::string>::failed(raw.error.value_or(
            access_error(DataAccessErrorCode::ColumnNotFound, *this, std::string(row_key),
                         std::string(column_name), "data-table value was not found")));
    }
    const DataValue& value = **raw.value;
    if (const auto* text = std::get_if<std::string>(&value.value))
        return DataAccessResult<std::string>::found(*text);
    if (const auto* number = std::get_if<std::int64_t>(&value.value))
        return DataAccessResult<std::string>::found(std::to_string(*number));
    if (const auto* number = std::get_if<double>(&value.value))
        return DataAccessResult<std::string>::found(std::to_string(*number));
    if (const auto* boolean = std::get_if<bool>(&value.value))
        return DataAccessResult<std::string>::found(*boolean ? "true" : "false");
    return conversion_error<std::string>(*this, row_key, column_name);
}

DataAccessResult<std::int64_t> DataTable::integer_value(std::string_view row_key,
                                                        std::string_view column_name) const {
    const auto raw = find_value(row_key, column_name);
    if (!raw.value.has_value()) {
        return DataAccessResult<std::int64_t>::failed(raw.error.value_or(
            access_error(DataAccessErrorCode::ColumnNotFound, *this, std::string(row_key),
                         std::string(column_name), "data-table value was not found")));
    }
    const DataValue& value = **raw.value;
    if (const auto* number = std::get_if<std::int64_t>(&value.value))
        return DataAccessResult<std::int64_t>::found(*number);
    const auto text = string_storage(value);
    if (text.has_value()) {
        std::int64_t parsed{};
        const char*  begin = text->data();
        const char*  end = begin + text->size();
        const auto [position, error] = std::from_chars(begin, end, parsed);
        if (error == std::errc{} && position == end)
            return DataAccessResult<std::int64_t>::found(parsed);
    }
    return conversion_error<std::int64_t>(*this, row_key, column_name);
}

DataAccessResult<double> DataTable::floating_value(std::string_view row_key,
                                                   std::string_view column_name) const {
    const auto raw = find_value(row_key, column_name);
    if (!raw.value.has_value()) {
        return DataAccessResult<double>::failed(raw.error.value_or(
            access_error(DataAccessErrorCode::ColumnNotFound, *this, std::string(row_key),
                         std::string(column_name), "data-table value was not found")));
    }
    const DataValue& value = **raw.value;
    if (const auto* number = std::get_if<double>(&value.value))
        return DataAccessResult<double>::found(*number);
    if (const auto* number = std::get_if<std::int64_t>(&value.value))
        return DataAccessResult<double>::found(static_cast<double>(*number));
    const auto text = string_storage(value);
    if (text.has_value()) {
        double      parsed{};
        const char* begin = text->data();
        const char* end = begin + text->size();
        const auto [position, error] = std::from_chars(begin, end, parsed);
        if (error == std::errc{} && position == end && std::isfinite(parsed))
            return DataAccessResult<double>::found(parsed);
    }
    return conversion_error<double>(*this, row_key, column_name);
}

DataAccessResult<bool> DataTable::boolean_value(std::string_view row_key,
                                                std::string_view column_name) const {
    const auto raw = find_value(row_key, column_name);
    if (!raw.value.has_value()) {
        return DataAccessResult<bool>::failed(raw.error.value_or(
            access_error(DataAccessErrorCode::ColumnNotFound, *this, std::string(row_key),
                         std::string(column_name), "data-table value was not found")));
    }
    const DataValue& value = **raw.value;
    if (const auto* boolean = std::get_if<bool>(&value.value))
        return DataAccessResult<bool>::found(*boolean);
    const auto text = string_storage(value);
    if (text == "true" || text == "1")
        return DataAccessResult<bool>::found(true);
    if (text == "false" || text == "0")
        return DataAccessResult<bool>::found(false);
    return conversion_error<bool>(*this, row_key, column_name);
}

const char* to_string(DataTableKind value) noexcept {
    switch (value) {
    case DataTableKind::Dbf:
        return "dbf";
    case DataTableKind::Dat:
        return "dat";
    case DataTableKind::Dlg:
        return "dlg";
    }
    return "unknown";
}

const char* to_string(DataValueKind value) noexcept {
    switch (value) {
    case DataValueKind::Null:
        return "null";
    case DataValueKind::Boolean:
        return "boolean";
    case DataValueKind::Integer:
        return "integer";
    case DataValueKind::FloatingPoint:
        return "floating_point";
    case DataValueKind::String:
        return "string";
    case DataValueKind::Array:
        return "array";
    case DataValueKind::Object:
        return "object";
    }
    return "unknown";
}

const char* to_string(DataAccessErrorCode value) noexcept {
    switch (value) {
    case DataAccessErrorCode::None:
        return "none";
    case DataAccessErrorCode::RowNotFound:
        return "row_not_found";
    case DataAccessErrorCode::ColumnNotFound:
        return "column_not_found";
    case DataAccessErrorCode::TypeConversion:
        return "type_conversion";
    }
    return "unknown";
}

DataTable DataTableManifest::load(const fs::path& asset_root, const fs::path& sidecar_path,
                                  const std::string& data_table_asset_id,
                                  const std::string& logical_name,
                                  const std::string& container_id) {
    std::ifstream input(asset_root / sidecar_path);
    if (!input) {
        throw AssetError(AssetErrorCode::MalformedDataTable, "data-table sidecar cannot be opened",
                         data_table_asset_id, sidecar_path);
    }
    Json root;
    try {
        input >> root;
    } catch (const Json::exception& error) {
        throw AssetError(AssetErrorCode::MalformedDataTable,
                         std::string("invalid data-table JSON: ") + error.what(),
                         data_table_asset_id, sidecar_path);
    }
    if (!root.is_object()) {
        malformed(AssetErrorCode::MalformedDataTable, "data-table root must be an object",
                  data_table_asset_id, sidecar_path);
    }
    const std::uint64_t version =
        positive_integer(root, "data_table_schema_version", data_table_asset_id, sidecar_path);
    if (version != data_table_schema_version) {
        throw AssetError(AssetErrorCode::UnsupportedDataTableSchema,
                         "unsupported data-table sidecar schema version",
                         data_table_asset_id + ".data_table_schema_version", sidecar_path);
    }

    validate_identity(require_string(root, "asset_id", data_table_asset_id, sidecar_path),
                      data_table_asset_id, "asset_id", data_table_asset_id, sidecar_path, false);
    validate_identity(require_string(root, "logical_name", data_table_asset_id, sidecar_path),
                      logical_name, "logical_name", data_table_asset_id, sidecar_path, true);
    validate_identity(require_string(root, "container_id", data_table_asset_id, sidecar_path),
                      container_id, "container_id", data_table_asset_id, sidecar_path, false);

    const std::string kind_name = require_string(root, "kind", data_table_asset_id, sidecar_path);
    DataTableKind     kind{};
    if (kind_name == "dbf") {
        kind = DataTableKind::Dbf;
    } else if (kind_name == "dat") {
        kind = DataTableKind::Dat;
    } else if (kind_name == "dlg") {
        kind = DataTableKind::Dlg;
    } else {
        malformed(AssetErrorCode::MalformedDataTable, "unknown data-table kind",
                  data_table_asset_id + ".kind", sidecar_path);
    }

    DataTable table{
        .data_table_asset_id = data_table_asset_id,
        .logical_name = logical_name,
        .container_id = container_id,
        .sidecar_path = sidecar_path,
        .kind = kind,
        .columns = {},
        .rows = {},
        .warnings = {},
        .extensions = std::nullopt,
    };

    const Json& columns =
        require_field(root, "columns", Json::value_t::array, data_table_asset_id, sidecar_path);
    std::unordered_set<std::string> column_names;
    for (std::size_t index = 0; index < columns.size(); ++index) {
        const std::string context = data_table_asset_id + ".columns[" + std::to_string(index) + "]";
        const Json&       column = columns[index];
        DataColumn        parsed{
            .name = require_string(column, "name", context, sidecar_path),
            .source_type = std::nullopt,
            .width = std::nullopt,
            .decimal_count = std::nullopt,
            .extensions = std::nullopt,
        };
        if (!column_names.insert(parsed.name).second) {
            malformed(AssetErrorCode::MalformedDataTable, "duplicate data-table column",
                      context + ".name", sidecar_path);
        }
        const auto source_type = column.find("source_type");
        if (source_type != column.end() && !source_type->is_null()) {
            if (!source_type->is_string()) {
                malformed(AssetErrorCode::MalformedDataTable,
                          "column source_type must be a string or null", context + ".source_type",
                          sidecar_path);
            }
            parsed.source_type = source_type->get<std::string>();
        }
        parsed.width = optional_u32(column, "width", context, sidecar_path);
        parsed.decimal_count = optional_u32(column, "decimal_count", context, sidecar_path);
        const auto extensions = column.find("extensions");
        if (extensions != column.end() && !extensions->is_null())
            parsed.extensions = parse_value(*extensions, context + ".extensions", sidecar_path);
        table.columns.push_back(std::move(parsed));
    }

    const Json& rows =
        require_field(root, "rows", Json::value_t::array, data_table_asset_id, sidecar_path);
    std::unordered_set<std::string> row_keys;
    for (std::size_t index = 0; index < rows.size(); ++index) {
        const std::string context = data_table_asset_id + ".rows[" + std::to_string(index) + "]";
        const Json&       row = rows[index];
        DataRow           parsed{.row_key = require_string(row, "row_key", context, sidecar_path),
                                 .values = {}};
        if (!row_keys.insert(parsed.row_key).second) {
            throw AssetError(AssetErrorCode::DuplicateDataRow, "duplicate data-table row key",
                             context + ".row_key", sidecar_path);
        }
        const Json& values =
            require_field(row, "values", Json::value_t::array, context, sidecar_path);
        std::unordered_set<std::string> value_names;
        for (std::size_t value_index = 0; value_index < values.size(); ++value_index) {
            const std::string value_context =
                context + ".values[" + std::to_string(value_index) + "]";
            const Json& cell = values[value_index];
            DataCell    parsed_cell{
                .name = require_string(cell, "name", value_context, sidecar_path),
                .value = {},
            };
            if (!value_names.insert(parsed_cell.name).second) {
                malformed(AssetErrorCode::MalformedDataTable,
                          "duplicate column value in data-table row", value_context + ".name",
                          sidecar_path);
            }
            const auto value = cell.find("value");
            if (value == cell.end()) {
                malformed(AssetErrorCode::MalformedDataValue, "data-table cell value is missing",
                          value_context + ".value", sidecar_path);
            }
            parsed_cell.value = parse_value(*value, value_context + ".value", sidecar_path);
            parsed.values.push_back(std::move(parsed_cell));
        }
        table.rows.push_back(std::move(parsed));
    }

    const Json& warnings =
        require_field(root, "warnings", Json::value_t::array, data_table_asset_id, sidecar_path);
    for (std::size_t index = 0; index < warnings.size(); ++index) {
        if (!warnings[index].is_string()) {
            malformed(AssetErrorCode::MalformedDataTable, "data-table warning must be a string",
                      data_table_asset_id + ".warnings[" + std::to_string(index) + "]",
                      sidecar_path);
        }
        table.warnings.push_back({.data_table_asset_id = data_table_asset_id,
                                  .message = warnings[index].get<std::string>()});
    }
    const auto extensions = root.find("extensions");
    if (extensions != root.end() && !extensions->is_null()) {
        table.extensions =
            parse_value(*extensions, data_table_asset_id + ".extensions", sidecar_path);
    }
    return table;
}

} // namespace d2asset
