#pragma once

#include "asset_manifest.hpp"
#include "data_table_manifest.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace d2asset {

inline constexpr std::uint32_t asset_links_schema_version = 1;

enum class AssetLinkEndpointKind : std::uint8_t {
    Asset,
    DataRow,
};

enum class AssetLinkKind : std::uint8_t {
    Unknown,
    IdleAnimation,
    MoveAnimation,
    AttackAnimation,
    HitAnimation,
    DeathAnimation,
    CastAnimation,
    DefendAnimation,
    PortraitImage,
    IconImage,
    EffectAnimation,
    BuildingImage,
    DialogImage,
    SoundCandidate,
};

enum class AssetLinkResolution : std::uint8_t {
    Confirmed,
    Heuristic,
};

enum class UnresolvedAssetReason : std::uint8_t {
    NoCandidate,
    Ambiguous,
    WrongType,
    UnsupportedMapping,
};

struct AssetLinkEndpoint {
    AssetLinkEndpointKind kind{AssetLinkEndpointKind::Asset};
    std::string           asset_id;
    std::string           table_asset_id;
    std::string           row_key;
};

struct AssetLinkEvidence {
    std::string field;
    std::string source_value;
    std::string target_value;
};

struct AssetLink {
    AssetLinkEndpoint              source;
    std::string                    target_asset_id;
    AssetLinkKind                  kind{AssetLinkKind::Unknown};
    AssetLinkResolution            resolution{AssetLinkResolution::Heuristic};
    std::uint8_t                   confidence{};
    std::string                    reason_code;
    std::vector<AssetLinkEvidence> evidence;
};

struct UnresolvedAssetReference {
    AssetLinkEndpoint              source;
    AssetLinkKind                  kind{AssetLinkKind::Unknown};
    UnresolvedAssetReason          reason{UnresolvedAssetReason::NoCandidate};
    std::string                    reason_code;
    std::vector<AssetLinkEvidence> evidence;
    std::vector<std::string>       candidate_asset_ids;
};

struct AssetLinkWarning {
    std::string message;
};

struct AssetLinkGraph {
    std::filesystem::path                 sidecar_path{"asset_links.json"};
    std::vector<AssetLink>                links;
    std::vector<UnresolvedAssetReference> unresolved;
    std::vector<AssetLinkWarning>         warnings;
    std::optional<DataValue>              extensions;
};

[[nodiscard]] const char* to_string(AssetLinkEndpointKind value) noexcept;
[[nodiscard]] const char* to_string(AssetLinkKind value) noexcept;
[[nodiscard]] const char* to_string(AssetLinkResolution value) noexcept;
[[nodiscard]] const char* to_string(UnresolvedAssetReason value) noexcept;
[[nodiscard]] std::string asset_link_endpoint_key(const AssetLinkEndpoint& endpoint);

class AssetLinkManifest {
public:
    [[nodiscard]] static AssetLinkGraph load(const std::filesystem::path&  asset_root,
                                             const AssetManifest&          manifest,
                                             const std::vector<DataTable>& data_tables);
};

} // namespace d2asset
