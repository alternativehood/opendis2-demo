#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace d2asset {

inline constexpr std::uint32_t data_table_schema_version = 1;

enum class DataTableKind : std::uint8_t {
    Dbf,
    Dat,
    Dlg,
};

enum class DataValueKind : std::uint8_t {
    Null,
    Boolean,
    Integer,
    FloatingPoint,
    String,
    Array,
    Object,
};

enum class DataAccessErrorCode : std::uint8_t {
    None,
    RowNotFound,
    ColumnNotFound,
    TypeConversion,
};

struct DataValue {
    using Array = std::vector<DataValue>;
    using Object = std::vector<std::pair<std::string, DataValue>>;
    using Storage =
        std::variant<std::monostate, bool, std::int64_t, double, std::string, Array, Object>;

    Storage value;

    [[nodiscard]] DataValueKind kind() const noexcept;
};

struct DataColumn {
    std::string                  name;
    std::optional<std::string>   source_type;
    std::optional<std::uint32_t> width;
    std::optional<std::uint32_t> decimal_count;
    std::optional<DataValue>     extensions;
};

struct DataCell {
    std::string name;
    DataValue   value;
};

struct DataRow {
    std::string           row_key;
    std::vector<DataCell> values;

    [[nodiscard]] const DataValue* find(std::string_view column_name) const noexcept;
};

struct DataTableWarning {
    std::string data_table_asset_id;
    std::string message;
};

struct DataAccessError {
    DataAccessErrorCode code{DataAccessErrorCode::None};
    std::string         table_asset_id;
    std::string         row_key;
    std::string         column_name;
    std::string         message;
};

template <typename T> struct DataAccessResult {
    std::optional<T>               value;
    std::optional<DataAccessError> error;

    [[nodiscard]] static DataAccessResult found(T result) {
        return {.value = std::move(result), .error = std::nullopt};
    }

    [[nodiscard]] static DataAccessResult failed(const DataAccessError& failure) {
        return {.value = std::nullopt, .error = failure};
    }
};

struct DataTable {
    std::string                   data_table_asset_id;
    std::string                   logical_name;
    std::string                   container_id;
    std::filesystem::path         sidecar_path;
    DataTableKind                 kind{DataTableKind::Dbf};
    std::vector<DataColumn>       columns;
    std::vector<DataRow>          rows;
    std::vector<DataTableWarning> warnings;
    std::optional<DataValue>      extensions;

    [[nodiscard]] DataAccessResult<const DataRow*>   find_row(std::string_view row_key) const;
    [[nodiscard]] DataAccessResult<const DataValue*> find_value(std::string_view row_key,
                                                                std::string_view column_name) const;
    [[nodiscard]] DataAccessResult<std::string>  string_value(std::string_view row_key,
                                                              std::string_view column_name) const;
    [[nodiscard]] DataAccessResult<std::int64_t> integer_value(std::string_view row_key,
                                                               std::string_view column_name) const;
    [[nodiscard]] DataAccessResult<double>       floating_value(std::string_view row_key,
                                                                std::string_view column_name) const;
    [[nodiscard]] DataAccessResult<bool>         boolean_value(std::string_view row_key,
                                                               std::string_view column_name) const;
};

[[nodiscard]] const char* to_string(DataTableKind value) noexcept;
[[nodiscard]] const char* to_string(DataValueKind value) noexcept;
[[nodiscard]] const char* to_string(DataAccessErrorCode value) noexcept;

class DataTableManifest {
public:
    [[nodiscard]] static DataTable load(const std::filesystem::path& asset_root,
                                        const std::filesystem::path& sidecar_path,
                                        const std::string&           data_table_asset_id,
                                        const std::string&           logical_name,
                                        const std::string&           container_id);
};

} // namespace d2asset
