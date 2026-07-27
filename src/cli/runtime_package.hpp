#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

enum class PackageDiagnosticSeverity : std::uint8_t {
    Error,
    Warning,
};

struct PackageDiagnostic {
    PackageDiagnosticSeverity severity{PackageDiagnosticSeverity::Error};
    std::string               code;
    std::string               message;
    std::filesystem::path     path;
};

struct PackageValidationResult {
    bool                           valid{false};
    std::size_t                    asset_link_count{};
    std::size_t                    unresolved_asset_link_count{};
    std::vector<PackageDiagnostic> diagnostics;
};

struct RuntimePackageBuildOptions {
    std::filesystem::path    game_root;
    std::filesystem::path    output_root;
    std::vector<std::string> containers;
    int                      atlas_max_size{4096};
};

struct RuntimePackageBuildResult {
    bool                           published{false};
    std::vector<PackageDiagnostic> diagnostics;
};

[[nodiscard]] std::filesystem::path package_subtree_for_type(std::string_view type);
[[nodiscard]] bool is_safe_package_relative_path(const std::filesystem::path& path);

[[nodiscard]] PackageValidationResult
validate_runtime_package(const std::filesystem::path& asset_root,
                         const std::filesystem::path& report_path = {});

[[nodiscard]] RuntimePackageBuildResult
build_runtime_package(const RuntimePackageBuildOptions& options);

int cmd_build_runtime_assets(const std::string& game_dir, const std::string& out_dir,
                             const std::vector<std::string>& containers = {});
int cmd_validate_runtime_assets(const std::string& asset_root, const std::string& report_path);

// ── Internal detail API (exposed for testability) ─────────────────────────
namespace runtime_package_detail {

[[nodiscard]] std::filesystem::path
package_extracted_sound(const std::filesystem::path& extractor_sidecar,
                        std::string_view logical_name, std::string_view container_path,
                        std::string_view asset_id, const std::filesystem::path& package_root);

[[nodiscard]] std::filesystem::path
package_extracted_dbf(const std::filesystem::path& schema_json,
                      const std::filesystem::path& records_json, std::string_view logical_name,
                      std::string_view container_path, std::string_view asset_id,
                      const std::filesystem::path& package_root);

[[nodiscard]] std::filesystem::path
package_extracted_dat(const std::filesystem::path& extractor_json, std::string_view logical_name,
                      std::string_view container_path, std::string_view asset_id,
                      const std::filesystem::path& package_root);

[[nodiscard]] std::filesystem::path
package_extracted_dlg(const std::filesystem::path& extractor_json, std::string_view logical_name,
                      std::string_view container_path, std::string_view asset_id,
                      const std::filesystem::path& package_root);

} // namespace runtime_package_detail
