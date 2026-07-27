#pragma once

#include "asset_database.hpp"
#include "asset_error.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace d2asset {

inline constexpr int inspection_schema_version = 1;

enum class InspectionExitCode : std::uint8_t {
    Success = 0,
    Usage = 2,
    Package = 3,
    Animation = 4,
    Sound = 5,
    DataTable = 6,
    DataRow = 7,
    ReferenceSource = 8,
    ReferenceTarget = 9,
};

struct InspectionRequest {
    std::filesystem::path      asset_root;
    std::string                animation_asset_id;
    std::optional<std::string> sound_asset_id;
    std::optional<std::string> data_table_asset_id;
    std::optional<std::string> row_key;
    std::optional<std::string> reference_table_asset_id;
    std::optional<std::string> reference_row_key;
    std::optional<std::string> reference_target_asset_id;
};

struct InspectionError {
    InspectionExitCode    exit_code{InspectionExitCode::Usage};
    std::string           code;
    std::string           message;
    std::string           requested_asset_id;
    std::string           detail_code;
    std::string           context;
    std::filesystem::path path;
};

struct InspectionResult {
    std::optional<nlohmann::json>  report;
    std::optional<InspectionError> error;
};

[[nodiscard]] const char* to_string(TimingSource value) noexcept;
[[nodiscard]] const char* to_string(LoopMode value) noexcept;
[[nodiscard]] const char* to_string(AnimationRole value) noexcept;
[[nodiscard]] const char* to_string(FacingDirection value) noexcept;
[[nodiscard]] const char* to_string(AssetErrorCode value) noexcept;

[[nodiscard]] std::string      package_path_string(const std::filesystem::path& path);
[[nodiscard]] InspectionResult inspect_runtime_assets(const InspectionRequest& request);
[[nodiscard]] nlohmann::json   inspection_error_json(const InspectionError& error);
[[nodiscard]] std::string      serialize_inspection_json(const nlohmann::json& value);

int run_d2asset_inspect(const std::vector<std::string>& arguments, std::ostream& output);

} // namespace d2asset
