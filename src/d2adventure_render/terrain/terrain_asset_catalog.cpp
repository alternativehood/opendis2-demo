#include "terrain_asset_catalog.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <string>
#include <tuple>
#include <vector>

namespace d2engine {
namespace {

[[nodiscard]] bool is_allowed_ground_code(std::string_view code) {
    static constexpr std::array<std::string_view, 7> codes = {"HU", "NE", "DW", "UN",
                                                              "HE", "EL", "WA"};
    return std::ranges::find(codes, code) != codes.end();
}

[[nodiscard]] bool is_allowed_border_family(std::string_view family) {
    return family == "NE" || family == "WA";
}

[[nodiscard]] std::optional<int> parse_two_digits(std::string_view value) {
    if (value.size() != 2 || !std::isdigit(static_cast<unsigned char>(value[0])) ||
        !std::isdigit(static_cast<unsigned char>(value[1]))) {
        return std::nullopt;
    }
    int        result = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if (parsed.ec != std::errc{}) {
        return std::nullopt;
    }
    return result;
}

} // namespace

std::optional<ParsedGroundTextureName>
parse_ground_texture_record_name(std::string_view record_name) {
    if (record_name.size() != 9 || record_name[2] != '_' || record_name.substr(5) != ".PNG") {
        return std::nullopt;
    }
    const std::string code(record_name.substr(0, 2));
    if (!is_allowed_ground_code(code)) {
        return std::nullopt;
    }
    const auto variant = parse_two_digits(record_name.substr(3, 2));
    if (!variant.has_value()) {
        return std::nullopt;
    }
    return ParsedGroundTextureName{.terrain_code = code, .variant = *variant};
}

std::optional<ParsedBorderAssetName> parse_border_asset_record_name(std::string_view record_name) {
    if (record_name.size() != 12 || record_name[2] != '_' || record_name[5] != '_' ||
        record_name.substr(8) != ".PNG") {
        return std::nullopt;
    }
    const std::string family(record_name.substr(0, 2));
    if (!is_allowed_border_family(family)) {
        return std::nullopt;
    }
    const auto shape = parse_two_digits(record_name.substr(3, 2));
    const auto variant = parse_two_digits(record_name.substr(6, 2));
    if (!shape.has_value() || !variant.has_value()) {
        return std::nullopt;
    }
    return ParsedBorderAssetName{.family = family, .shape = *shape, .variant = *variant};
}

std::optional<std::string> parse_iso_logical_family(std::string_view logical_name) {
    std::string family;
    for (char ch : logical_name) {
        const auto c = static_cast<unsigned char>(ch);
        if (std::isdigit(c)) {
            return family.empty() ? std::nullopt : std::optional<std::string>{family};
        }
        if (!std::isupper(c)) {
            return std::nullopt;
        }
        family.push_back(ch);
    }
    return std::nullopt;
}

void sort_terrain_asset_catalog(TerrainAssetCatalog& catalog) {
    std::ranges::sort(catalog.ground_textures, {}, [](const auto& asset) {
        return std::tie(asset.terrain_code, asset.variant, asset.record_name);
    });
    std::ranges::sort(catalog.border_assets, {}, [](const auto& asset) {
        return std::tie(asset.family, asset.shape, asset.variant, asset.record_name);
    });
    std::ranges::sort(catalog.terrain_overlays, {},
                      [](const auto& asset) { return std::tie(asset.family, asset.logical_name); });
    std::ranges::sort(catalog.static_assets, {}, [](const auto& asset) {
        return std::tie(asset.container_path, asset.family, asset.logical_name);
    });
}

std::optional<TerrainBorderAsset>
TerrainAssetCatalog::find_border_asset(std::string_view family, int shape, int variant) const {
    const auto it = std::ranges::find_if(border_assets, [&](const auto& asset) {
        return asset.family == family && asset.shape == shape && asset.variant == variant;
    });
    if (it == border_assets.end()) {
        return std::nullopt;
    }
    return *it;
}

} // namespace d2engine
