#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace d2asset {

enum class AssetType : std::uint8_t {
    Image,
    Animation,
    Sound,
    Atlas,
    DataTable,
    Unknown,
};

struct ContainerRecord {
    std::string              container_id;
    std::string              path;
    std::vector<std::string> content_kinds;
};

struct AssetRecord {
    AssetType             type{AssetType::Unknown};
    std::string           type_name;
    std::string           asset_id;
    std::string           logical_name;
    std::string           container_id;
    std::filesystem::path path;
};

class AssetManifest {
public:
    [[nodiscard]] static AssetManifest load(const std::filesystem::path& asset_root);

    [[nodiscard]] const std::vector<ContainerRecord>& containers() const noexcept {
        return containers_;
    }
    [[nodiscard]] const std::vector<AssetRecord>& assets() const noexcept { return assets_; }
    [[nodiscard]] const std::vector<std::string>& warnings() const noexcept { return warnings_; }

private:
    int                          schema_version_{};
    std::vector<ContainerRecord> containers_;
    std::vector<AssetRecord>     assets_;
    std::vector<std::string>     warnings_;
};

[[nodiscard]] AssetType   asset_type_from_string(const std::string& type_name);
[[nodiscard]] const char* to_string(AssetType type) noexcept;

} // namespace d2asset
