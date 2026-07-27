#pragma once

#include "animation_manifest.hpp"
#include "asset_link_manifest.hpp"
#include "asset_manifest.hpp"
#include "atlas_manifest.hpp"
#include "data_table_manifest.hpp"
#include "sound_manifest.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace d2asset {

template <typename Tag> struct TypedAssetRef {
    std::string           asset_id;
    std::string           logical_name;
    std::string           container_id;
    std::filesystem::path path;
};

struct ImageAssetTag;
struct AnimationAssetTag;
struct SoundAssetTag;
struct AtlasAssetTag;
struct DataTableTag;

using ImageAssetRef = TypedAssetRef<ImageAssetTag>;
using AnimationAssetRef = TypedAssetRef<AnimationAssetTag>;
using SoundAssetRef = TypedAssetRef<SoundAssetTag>;
using AtlasAssetRef = TypedAssetRef<AtlasAssetTag>;
using DataTableRef = TypedAssetRef<DataTableTag>;

enum class AssetLookupStatus : std::uint8_t {
    Found,
    NotFound,
    Ambiguous,
};

template <typename T> struct AssetLookupResult {
    AssetLookupStatus        status{AssetLookupStatus::NotFound};
    std::optional<T>         value;
    std::vector<std::string> matching_asset_ids;

    [[nodiscard]] static AssetLookupResult found(T asset) {
        AssetLookupResult result;
        result.status = AssetLookupStatus::Found;
        result.value = std::move(asset);
        return result;
    }

    [[nodiscard]] static AssetLookupResult not_found() { return {}; }

    [[nodiscard]] static AssetLookupResult ambiguous(std::vector<std::string> ids) {
        AssetLookupResult result;
        result.status = AssetLookupStatus::Ambiguous;
        result.matching_asset_ids = std::move(ids);
        return result;
    }
};

class AssetDatabase {
public:
    [[nodiscard]] static AssetDatabase open(const std::filesystem::path& asset_root);

    [[nodiscard]] AssetLookupResult<ImageAssetRef>
    find_image_by_id(std::string_view asset_id) const;
    [[nodiscard]] AssetLookupResult<AnimationAssetRef>
    find_animation_by_id(std::string_view asset_id) const;
    [[nodiscard]] AssetLookupResult<SoundAssetRef>
    find_sound_by_id(std::string_view asset_id) const;
    [[nodiscard]] AssetLookupResult<AtlasAssetRef>
    find_atlas_by_id(std::string_view asset_id) const;
    [[nodiscard]] AssetLookupResult<DataTableRef>
    find_data_table_by_id(std::string_view asset_id) const;

    [[nodiscard]] AssetLookupResult<ImageAssetRef> find_image(std::string_view logical_name) const;
    [[nodiscard]] AssetLookupResult<SoundAssetRef> find_sound(std::string_view logical_name) const;
    [[nodiscard]] AssetLookupResult<TextureRegion>
    find_atlas_region_by_image_id(std::string_view image_asset_id) const;
    [[nodiscard]] AssetLookupResult<AnimationClip>
    get_animation_clip(std::string_view animation_asset_id) const;
    [[nodiscard]] AssetLookupResult<SoundAsset>
    get_sound_asset(std::string_view sound_asset_id) const;
    [[nodiscard]] AssetLookupResult<DataTable>
    get_data_table(std::string_view data_table_asset_id) const;
    [[nodiscard]] AssetLookupResult<std::vector<AssetLink>>
    links_from_asset(std::string_view asset_id, std::optional<AssetLinkKind> kind = std::nullopt,
                     std::optional<AssetLinkResolution> resolution = std::nullopt) const;
    [[nodiscard]] AssetLookupResult<std::vector<AssetLink>>
    links_from_data_row(std::string_view table_asset_id, std::string_view row_key,
                        std::optional<AssetLinkKind>       kind = std::nullopt,
                        std::optional<AssetLinkResolution> resolution = std::nullopt) const;
    [[nodiscard]] AssetLookupResult<std::vector<AssetLink>>
    links_to_asset(std::string_view                   target_asset_id,
                   std::optional<AssetLinkKind>       kind = std::nullopt,
                   std::optional<AssetLinkResolution> resolution = std::nullopt) const;
    [[nodiscard]] AssetLookupResult<std::vector<UnresolvedAssetReference>>
    unresolved_from_asset(std::string_view asset_id) const;
    [[nodiscard]] AssetLookupResult<std::vector<UnresolvedAssetReference>>
    unresolved_from_data_row(std::string_view table_asset_id, std::string_view row_key) const;

    [[nodiscard]] const std::filesystem::path& asset_root() const noexcept { return asset_root_; }
    [[nodiscard]] const AssetManifest&         manifest() const noexcept { return manifest_; }
    [[nodiscard]] const std::vector<AtlasSheetInfo>& atlas_sheets() const noexcept {
        return atlas_sheets_;
    }
    [[nodiscard]] const std::vector<AnimationWarning>& animation_diagnostics() const noexcept {
        return animation_diagnostics_;
    }
    [[nodiscard]] const std::vector<SoundWarning>& sound_diagnostics() const noexcept {
        return sound_diagnostics_;
    }
    [[nodiscard]] const std::vector<DataTableWarning>& data_table_diagnostics() const noexcept {
        return data_table_diagnostics_;
    }
    [[nodiscard]] const AssetLinkGraph& asset_links() const noexcept { return asset_links_; }

private:
    AssetDatabase(std::filesystem::path asset_root, AssetManifest manifest);

    [[nodiscard]] const AssetRecord* find_record_by_id(std::string_view asset_id,
                                                       AssetType        type) const;
    [[nodiscard]] const AssetRecord* find_record_by_id(std::string_view asset_id) const;
    [[nodiscard]] std::vector<const AssetRecord*>
    find_records_by_name(std::string_view logical_name, AssetType type) const;
    [[nodiscard]] static std::string logical_key(std::string_view logical_name, AssetType type);
    [[nodiscard]] static std::string container_image_key(std::string_view container_id,
                                                         std::string_view logical_name);
    void                             load_atlases();
    void                             load_animations();
    void                             load_sounds();
    void                             load_data_tables();
    void                             load_asset_links();

    std::filesystem::path                                       asset_root_;
    AssetManifest                                               manifest_;
    std::unordered_map<std::string, std::size_t>                id_index_;
    std::unordered_map<std::string, std::vector<std::size_t>>   logical_name_index_;
    std::unordered_map<std::string, std::vector<std::size_t>>   container_image_index_;
    std::vector<AtlasSheetInfo>                                 atlas_sheets_;
    std::unordered_map<std::string, std::vector<TextureRegion>> atlas_region_index_;
    std::vector<AnimationClip>                                  animation_clips_;
    std::unordered_map<std::string, std::size_t>                animation_clip_index_;
    std::vector<AnimationWarning>                               animation_diagnostics_;
    std::vector<SoundAsset>                                     sound_assets_;
    std::unordered_map<std::string, std::size_t>                sound_asset_index_;
    std::vector<SoundWarning>                                   sound_diagnostics_;
    std::vector<DataTable>                                      data_tables_;
    std::unordered_map<std::string, std::size_t>                data_table_index_;
    std::vector<DataTableWarning>                               data_table_diagnostics_;
    AssetLinkGraph                                              asset_links_;
    std::unordered_map<std::string, std::vector<std::size_t>>   asset_link_source_index_;
    std::unordered_map<std::string, std::vector<std::size_t>>   asset_link_target_index_;
    std::unordered_map<std::string, std::vector<std::size_t>>   unresolved_source_index_;
};

} // namespace d2asset
