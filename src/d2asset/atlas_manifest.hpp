#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace d2asset {

struct PixelSize {
    std::uint32_t width{};
    std::uint32_t height{};
};

struct PixelPoint {
    std::int32_t x{};
    std::int32_t y{};
};

struct SpriteRect {
    std::uint32_t x{};
    std::uint32_t y{};
    std::uint32_t width{};
    std::uint32_t height{};
};

struct AtlasSheetInfo {
    std::string           atlas_asset_id;
    std::uint32_t         index{};
    std::filesystem::path path;
    PixelSize             size;
};

struct AtlasSpriteRegion {
    std::string               logical_name;
    std::uint32_t             sheet_index{};
    SpriteRect                rectangle;
    std::optional<PixelSize>  source_size;
    std::optional<PixelSize>  trimmed_size;
    std::optional<PixelPoint> trim_offset;
    std::optional<PixelPoint> pivot;
    std::optional<PixelPoint> anchor;
};

struct TextureRegion {
    std::string               atlas_asset_id;
    std::string               image_asset_id;
    std::string               logical_name;
    std::filesystem::path     sheet_path;
    std::uint32_t             sheet_index{};
    SpriteRect                rectangle;
    std::optional<PixelSize>  source_size;
    std::optional<PixelSize>  trimmed_size;
    std::optional<PixelPoint> trim_offset;
    std::optional<PixelPoint> pivot;
    std::optional<PixelPoint> anchor;
};

class AtlasManifest {
public:
    [[nodiscard]] static AtlasManifest load(const std::filesystem::path& asset_root,
                                            const std::filesystem::path& sidecar_path,
                                            const std::string&           atlas_asset_id);

    [[nodiscard]] const std::vector<AtlasSheetInfo>&    sheets() const noexcept { return sheets_; }
    [[nodiscard]] const std::vector<AtlasSpriteRegion>& regions() const noexcept {
        return regions_;
    }

private:
    std::uint32_t                  max_sheet_size_{};
    std::uint32_t                  total_sprites_{};
    std::uint32_t                  skipped_sprites_{};
    std::vector<AtlasSheetInfo>    sheets_;
    std::vector<AtlasSpriteRegion> regions_;
};

} // namespace d2asset
