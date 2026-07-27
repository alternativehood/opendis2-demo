#include "asset_database.hpp"

#include "asset_error.hpp"
#include "asset_id.hpp"

#include <fstream>
#include <limits>
#include <sstream>
#include <unordered_set>
#include <utility>

#include <nlohmann/json.hpp>

namespace d2asset {
namespace {

namespace fs = std::filesystem;
using Json = nlohmann::json;

template <typename Ref> Ref make_ref(const AssetRecord& record) {
    return Ref{record.asset_id, record.logical_name, record.container_id, record.path};
}

template <typename Ref> AssetLookupResult<Ref> exact_result(const AssetRecord* record) {
    if (record == nullptr)
        return AssetLookupResult<Ref>::not_found();
    return AssetLookupResult<Ref>::found(make_ref<Ref>(*record));
}

template <typename Ref>
AssetLookupResult<Ref> name_result(const std::vector<const AssetRecord*>& records) {
    if (records.empty())
        return AssetLookupResult<Ref>::not_found();
    if (records.size() == 1)
        return AssetLookupResult<Ref>::found(make_ref<Ref>(*records.front()));

    std::vector<std::string> ids;
    ids.reserve(records.size());
    for (const AssetRecord* record : records)
        ids.push_back(record->asset_id);
    return AssetLookupResult<Ref>::ambiguous(std::move(ids));
}

std::optional<PixelSize> read_source_size(const fs::path& asset_root, const AssetRecord& image) {
    std::ifstream input(asset_root / image.path);
    if (!input) {
        throw AssetError(AssetErrorCode::MalformedAtlas, "image sidecar cannot be opened",
                         image.asset_id, image.path);
    }

    Json sidecar;
    try {
        input >> sidecar;
    } catch (const Json::exception& error) {
        throw AssetError(AssetErrorCode::MalformedAtlas,
                         std::string("invalid mapped image sidecar JSON: ") + error.what(),
                         image.asset_id, image.path);
    }
    const auto output_size = sidecar.find("output_size");
    if (output_size == sidecar.end())
        return std::nullopt;
    if (!output_size->is_object()) {
        throw AssetError(AssetErrorCode::MalformedAtlas, "image output_size must be an object",
                         image.asset_id + ".output_size", image.path);
    }
    const auto width = output_size->find("w");
    const auto height = output_size->find("h");
    if (width == output_size->end() || height == output_size->end() ||
        (!width->is_number_integer() && !width->is_number_unsigned()) ||
        (!height->is_number_integer() && !height->is_number_unsigned())) {
        throw AssetError(AssetErrorCode::MalformedAtlas,
                         "image output_size dimensions must be integers",
                         image.asset_id + ".output_size", image.path);
    }
    const auto positive_size = [](const Json& value) -> std::optional<std::uint32_t> {
        if (value.is_number_unsigned()) {
            const std::uint64_t number = value.get<std::uint64_t>();
            if (number == 0 || number > std::numeric_limits<std::uint32_t>::max())
                return std::nullopt;
            return static_cast<std::uint32_t>(number);
        }
        const std::int64_t number = value.get<std::int64_t>();
        if (number <= 0 || std::cmp_greater(number, std::numeric_limits<std::uint32_t>::max())) {
            return std::nullopt;
        }
        return static_cast<std::uint32_t>(number);
    };
    const std::optional<std::uint32_t> width_value = positive_size(*width);
    const std::optional<std::uint32_t> height_value = positive_size(*height);
    if (!width_value.has_value() || !height_value.has_value()) {
        throw AssetError(AssetErrorCode::MalformedAtlas,
                         "image output_size dimensions must be positive 32-bit integers",
                         image.asset_id + ".output_size", image.path);
    }
    return PixelSize{.width = *width_value, .height = *height_value};
}

} // namespace

AssetDatabase AssetDatabase::open(const std::filesystem::path& asset_root) {
    return {asset_root, AssetManifest::load(asset_root)};
}

AssetDatabase::AssetDatabase(std::filesystem::path asset_root, AssetManifest manifest)
    : asset_root_(std::move(asset_root)), manifest_(std::move(manifest)) {
    const auto& assets = manifest_.assets();
    id_index_.reserve(assets.size());
    logical_name_index_.reserve(assets.size());
    for (std::size_t i = 0; i < assets.size(); ++i) {
        id_index_.emplace(assets[i].asset_id, i);
        logical_name_index_[logical_key(assets[i].logical_name, assets[i].type)].push_back(i);
        if (assets[i].type == AssetType::Image) {
            container_image_index_[container_image_key(assets[i].container_id,
                                                       assets[i].logical_name)]
                .push_back(i);
        }
    }
    load_atlases();
    load_animations();
    load_sounds();
    load_data_tables();
    load_asset_links();
}

const AssetRecord* AssetDatabase::find_record_by_id(std::string_view asset_id) const {
    const auto it = id_index_.find(std::string(asset_id));
    return it == id_index_.end() ? nullptr : &manifest_.assets()[it->second];
}

const AssetRecord* AssetDatabase::find_record_by_id(std::string_view asset_id,
                                                    AssetType        type) const {
    const AssetRecord* record = find_record_by_id(asset_id);
    return record != nullptr && record->type == type ? record : nullptr;
}

std::vector<const AssetRecord*> AssetDatabase::find_records_by_name(std::string_view logical_name,
                                                                    AssetType        type) const {
    const auto it = logical_name_index_.find(logical_key(logical_name, type));
    if (it == logical_name_index_.end())
        return {};

    std::vector<const AssetRecord*> records;
    records.reserve(it->second.size());
    for (const std::size_t index : it->second)
        records.push_back(&manifest_.assets()[index]);
    return records;
}

std::string AssetDatabase::logical_key(std::string_view logical_name, AssetType type) {
    return std::to_string(static_cast<int>(type)) + ':' + normalize_ascii(logical_name);
}

std::string AssetDatabase::container_image_key(std::string_view container_id,
                                               std::string_view logical_name) {
    return std::string(container_id) + ':' + normalize_ascii(logical_name);
}

void AssetDatabase::load_atlases() {
    for (const AssetRecord& atlas_record : manifest_.assets()) {
        if (atlas_record.type != AssetType::Atlas)
            continue;

        AtlasManifest const atlas =
            AtlasManifest::load(asset_root_, atlas_record.path, atlas_record.asset_id);
        atlas_sheets_.insert(atlas_sheets_.end(), atlas.sheets().begin(), atlas.sheets().end());

        std::unordered_set<std::string> mapped_image_ids;
        for (const AtlasSpriteRegion& region : atlas.regions()) {
            const auto image_it = container_image_index_.find(
                container_image_key(atlas_record.container_id, region.logical_name));
            if (image_it == container_image_index_.end() || image_it->second.empty()) {
                throw AssetError(AssetErrorCode::MalformedAtlas,
                                 "atlas entry does not resolve to an image in its container",
                                 atlas_record.asset_id + ":" + region.logical_name,
                                 atlas_record.path);
            }
            if (image_it->second.size() != 1) {
                throw AssetError(AssetErrorCode::MalformedAtlas,
                                 "atlas entry resolves to multiple images in its container",
                                 atlas_record.asset_id + ":" + region.logical_name,
                                 atlas_record.path);
            }

            const AssetRecord& image = manifest_.assets()[image_it->second.front()];
            if (!mapped_image_ids.insert(image.asset_id).second) {
                throw AssetError(AssetErrorCode::DuplicateAtlasEntry,
                                 "multiple atlas entries map to the same image", image.asset_id,
                                 atlas_record.path);
            }

            const AtlasSheetInfo& sheet = atlas.sheets()[region.sheet_index];
            TextureRegion         texture_region{
                .atlas_asset_id = atlas_record.asset_id,
                .image_asset_id = image.asset_id,
                .logical_name = region.logical_name,
                .sheet_path = sheet.path,
                .sheet_index = region.sheet_index,
                .rectangle = region.rectangle,
                .source_size = read_source_size(asset_root_, image),
                .trimmed_size = region.trimmed_size,
                .trim_offset = region.trim_offset,
                .pivot = region.pivot,
                .anchor = region.anchor,
            };
            atlas_region_index_[image.asset_id].push_back(std::move(texture_region));
        }
    }
}

void AssetDatabase::load_animations() {
    const auto add_warning = [this](AnimationClip& clip, AnimationWarning warning) {
        animation_diagnostics_.push_back(warning);
        clip.warnings.push_back(std::move(warning));
    };

    for (const AssetRecord& animation_record : manifest_.assets()) {
        if (animation_record.type != AssetType::Animation)
            continue;

        AnimationClip clip =
            AnimationManifest::load(asset_root_, animation_record.path, animation_record.asset_id,
                                    animation_record.logical_name, animation_record.container_id);
        animation_diagnostics_.insert(animation_diagnostics_.end(), clip.warnings.begin(),
                                      clip.warnings.end());

        for (AnimationFrameRef& frame : clip.frames) {
            const auto image_it = container_image_index_.find(
                container_image_key(animation_record.container_id, frame.logical_name));
            if (image_it == container_image_index_.end() || image_it->second.empty()) {
                add_warning(
                    clip,
                    {.animation_asset_id = animation_record.asset_id,
                     .frame_index = frame.index,
                     .logical_name = frame.logical_name,
                     .message = "animation frame does not resolve to an image in its container",
                     .matching_asset_ids = {}});
            } else if (image_it->second.size() != 1) {
                std::vector<std::string> matching_ids;
                matching_ids.reserve(image_it->second.size());
                for (const std::size_t index : image_it->second)
                    matching_ids.push_back(manifest_.assets()[index].asset_id);
                add_warning(
                    clip,
                    {.animation_asset_id = animation_record.asset_id,
                     .frame_index = frame.index,
                     .logical_name = frame.logical_name,
                     .message = "animation frame resolves to multiple images in its container",
                     .matching_asset_ids = std::move(matching_ids)});
            } else {
                const AssetRecord& image = manifest_.assets()[image_it->second.front()];
                frame.image_asset_id = image.asset_id;
                const AssetLookupResult<TextureRegion> region =
                    find_atlas_region_by_image_id(image.asset_id);
                if (region.status == AssetLookupStatus::Found) {
                    frame.texture_region = region.value;
                } else if (region.status == AssetLookupStatus::Ambiguous) {
                    add_warning(clip, {.animation_asset_id = animation_record.asset_id,
                                       .frame_index = frame.index,
                                       .logical_name = frame.logical_name,
                                       .message = "animation frame image maps to multiple atlases",
                                       .matching_asset_ids = region.matching_asset_ids});
                }
            }

            if (!frame.resolved()) {
                add_warning(clip, {.animation_asset_id = animation_record.asset_id,
                                   .frame_index = frame.index,
                                   .logical_name = frame.logical_name,
                                   .message = "animation frame has no atlas region or fallback PNG",
                                   .matching_asset_ids = {}});
            }
        }

        animation_clip_index_.emplace(animation_record.asset_id, animation_clips_.size());
        animation_clips_.push_back(std::move(clip));
    }
}

void AssetDatabase::load_sounds() {
    for (const AssetRecord& sound_record : manifest_.assets()) {
        if (sound_record.type != AssetType::Sound)
            continue;
        SoundAsset sound =
            SoundManifest::load(asset_root_, sound_record.path, sound_record.asset_id,
                                sound_record.logical_name, sound_record.container_id);
        sound_diagnostics_.insert(sound_diagnostics_.end(), sound.warnings.begin(),
                                  sound.warnings.end());
        sound_asset_index_.emplace(sound_record.asset_id, sound_assets_.size());
        sound_assets_.push_back(std::move(sound));
    }
}

void AssetDatabase::load_data_tables() {
    for (const AssetRecord& table_record : manifest_.assets()) {
        if (table_record.type != AssetType::DataTable)
            continue;
        DataTable table =
            DataTableManifest::load(asset_root_, table_record.path, table_record.asset_id,
                                    table_record.logical_name, table_record.container_id);
        data_table_diagnostics_.insert(data_table_diagnostics_.end(), table.warnings.begin(),
                                       table.warnings.end());
        data_table_index_.emplace(table_record.asset_id, data_tables_.size());
        data_tables_.push_back(std::move(table));
    }
}

void AssetDatabase::load_asset_links() {
    asset_links_ = AssetLinkManifest::load(asset_root_, manifest_, data_tables_);
    for (std::size_t index = 0; index < asset_links_.links.size(); ++index) {
        const AssetLink& link = asset_links_.links[index];
        asset_link_source_index_[asset_link_endpoint_key(link.source)].push_back(index);
        asset_link_target_index_[link.target_asset_id].push_back(index);
    }
    for (std::size_t index = 0; index < asset_links_.unresolved.size(); ++index) {
        unresolved_source_index_[asset_link_endpoint_key(asset_links_.unresolved[index].source)]
            .push_back(index);
    }
}

AssetLookupResult<ImageAssetRef> AssetDatabase::find_image_by_id(std::string_view asset_id) const {
    return exact_result<ImageAssetRef>(find_record_by_id(asset_id, AssetType::Image));
}

AssetLookupResult<AnimationAssetRef>
AssetDatabase::find_animation_by_id(std::string_view asset_id) const {
    return exact_result<AnimationAssetRef>(find_record_by_id(asset_id, AssetType::Animation));
}

AssetLookupResult<SoundAssetRef> AssetDatabase::find_sound_by_id(std::string_view asset_id) const {
    return exact_result<SoundAssetRef>(find_record_by_id(asset_id, AssetType::Sound));
}

AssetLookupResult<AtlasAssetRef> AssetDatabase::find_atlas_by_id(std::string_view asset_id) const {
    return exact_result<AtlasAssetRef>(find_record_by_id(asset_id, AssetType::Atlas));
}

AssetLookupResult<DataTableRef>
AssetDatabase::find_data_table_by_id(std::string_view asset_id) const {
    return exact_result<DataTableRef>(find_record_by_id(asset_id, AssetType::DataTable));
}

AssetLookupResult<ImageAssetRef> AssetDatabase::find_image(std::string_view logical_name) const {
    return name_result<ImageAssetRef>(find_records_by_name(logical_name, AssetType::Image));
}

AssetLookupResult<SoundAssetRef> AssetDatabase::find_sound(std::string_view logical_name) const {
    return name_result<SoundAssetRef>(find_records_by_name(logical_name, AssetType::Sound));
}

AssetLookupResult<TextureRegion>
AssetDatabase::find_atlas_region_by_image_id(std::string_view image_asset_id) const {
    const auto it = atlas_region_index_.find(std::string(image_asset_id));
    if (it == atlas_region_index_.end() || it->second.empty())
        return AssetLookupResult<TextureRegion>::not_found();
    if (it->second.size() == 1)
        return AssetLookupResult<TextureRegion>::found(it->second.front());

    std::vector<std::string> atlas_ids;
    atlas_ids.reserve(it->second.size());
    for (const TextureRegion& region : it->second)
        atlas_ids.push_back(region.atlas_asset_id);
    return AssetLookupResult<TextureRegion>::ambiguous(std::move(atlas_ids));
}

AssetLookupResult<AnimationClip>
AssetDatabase::get_animation_clip(std::string_view animation_asset_id) const {
    const auto it = animation_clip_index_.find(std::string(animation_asset_id));
    if (it == animation_clip_index_.end())
        return AssetLookupResult<AnimationClip>::not_found();
    return AssetLookupResult<AnimationClip>::found(animation_clips_[it->second]);
}

AssetLookupResult<SoundAsset>
AssetDatabase::get_sound_asset(std::string_view sound_asset_id) const {
    const auto it = sound_asset_index_.find(std::string(sound_asset_id));
    if (it == sound_asset_index_.end())
        return AssetLookupResult<SoundAsset>::not_found();
    return AssetLookupResult<SoundAsset>::found(sound_assets_[it->second]);
}

AssetLookupResult<DataTable>
AssetDatabase::get_data_table(std::string_view data_table_asset_id) const {
    const auto it = data_table_index_.find(std::string(data_table_asset_id));
    if (it == data_table_index_.end())
        return AssetLookupResult<DataTable>::not_found();
    return AssetLookupResult<DataTable>::found(data_tables_[it->second]);
}

namespace {

std::vector<AssetLink> filter_links(const AssetLinkGraph&              graph,
                                    const std::vector<std::size_t>*    indexes,
                                    std::optional<AssetLinkKind>       kind,
                                    std::optional<AssetLinkResolution> resolution) {
    std::vector<AssetLink> links;
    if (indexes == nullptr)
        return links;
    links.reserve(indexes->size());
    for (const std::size_t index : *indexes) {
        const AssetLink& link = graph.links[index];
        if (kind.has_value() && link.kind != *kind)
            continue;
        if (resolution.has_value() && link.resolution != *resolution)
            continue;
        links.push_back(link);
    }
    return links;
}

std::vector<UnresolvedAssetReference> unresolved_for(const AssetLinkGraph&           graph,
                                                     const std::vector<std::size_t>* indexes) {
    std::vector<UnresolvedAssetReference> unresolved;
    if (indexes == nullptr)
        return unresolved;
    unresolved.reserve(indexes->size());
    for (const std::size_t index : *indexes)
        unresolved.push_back(graph.unresolved[index]);
    return unresolved;
}

} // namespace

AssetLookupResult<std::vector<AssetLink>>
AssetDatabase::links_from_asset(std::string_view asset_id, std::optional<AssetLinkKind> kind,
                                std::optional<AssetLinkResolution> resolution) const {
    if (find_record_by_id(asset_id) == nullptr)
        return AssetLookupResult<std::vector<AssetLink>>::not_found();
    const auto it = asset_link_source_index_.find("asset:" + std::string(asset_id));
    return AssetLookupResult<std::vector<AssetLink>>::found(
        filter_links(asset_links_, it == asset_link_source_index_.end() ? nullptr : &it->second,
                     kind, resolution));
}

AssetLookupResult<std::vector<AssetLink>>
AssetDatabase::links_from_data_row(std::string_view table_asset_id, std::string_view row_key,
                                   std::optional<AssetLinkKind>       kind,
                                   std::optional<AssetLinkResolution> resolution) const {
    const auto table = get_data_table(table_asset_id);
    if (!table.value.has_value() || !table.value->find_row(row_key).value.has_value())
        return AssetLookupResult<std::vector<AssetLink>>::not_found();
    const AssetLinkEndpoint endpoint{.kind = AssetLinkEndpointKind::DataRow,
                                     .asset_id = {},
                                     .table_asset_id = std::string(table_asset_id),
                                     .row_key = std::string(row_key)};
    const auto              it = asset_link_source_index_.find(asset_link_endpoint_key(endpoint));
    return AssetLookupResult<std::vector<AssetLink>>::found(
        filter_links(asset_links_, it == asset_link_source_index_.end() ? nullptr : &it->second,
                     kind, resolution));
}

AssetLookupResult<std::vector<AssetLink>>
AssetDatabase::links_to_asset(std::string_view target_asset_id, std::optional<AssetLinkKind> kind,
                              std::optional<AssetLinkResolution> resolution) const {
    if (find_record_by_id(target_asset_id) == nullptr)
        return AssetLookupResult<std::vector<AssetLink>>::not_found();
    const auto it = asset_link_target_index_.find(std::string(target_asset_id));
    return AssetLookupResult<std::vector<AssetLink>>::found(
        filter_links(asset_links_, it == asset_link_target_index_.end() ? nullptr : &it->second,
                     kind, resolution));
}

AssetLookupResult<std::vector<UnresolvedAssetReference>>
AssetDatabase::unresolved_from_asset(std::string_view asset_id) const {
    if (find_record_by_id(asset_id) == nullptr)
        return AssetLookupResult<std::vector<UnresolvedAssetReference>>::not_found();
    const auto it = unresolved_source_index_.find("asset:" + std::string(asset_id));
    return AssetLookupResult<std::vector<UnresolvedAssetReference>>::found(
        unresolved_for(asset_links_, it == unresolved_source_index_.end() ? nullptr : &it->second));
}

AssetLookupResult<std::vector<UnresolvedAssetReference>>
AssetDatabase::unresolved_from_data_row(std::string_view table_asset_id,
                                        std::string_view row_key) const {
    const auto table = get_data_table(table_asset_id);
    if (!table.value.has_value() || !table.value->find_row(row_key).value.has_value())
        return AssetLookupResult<std::vector<UnresolvedAssetReference>>::not_found();
    const AssetLinkEndpoint endpoint{.kind = AssetLinkEndpointKind::DataRow,
                                     .asset_id = {},
                                     .table_asset_id = std::string(table_asset_id),
                                     .row_key = std::string(row_key)};
    const auto              it = unresolved_source_index_.find(asset_link_endpoint_key(endpoint));
    return AssetLookupResult<std::vector<UnresolvedAssetReference>>::found(
        unresolved_for(asset_links_, it == unresolved_source_index_.end() ? nullptr : &it->second));
}

} // namespace d2asset
