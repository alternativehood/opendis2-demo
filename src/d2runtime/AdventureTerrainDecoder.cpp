#include "AdventureTerrainDecoder.hpp"

#include <cstddef>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>

namespace d2runtime {

namespace {

AdventureTerrainRawFields decode_raw_fields(uint32_t raw_value) {
    AdventureTerrainRawFields raw;
    raw.raw_value = raw_value;
    raw.low_byte = static_cast<uint8_t>(raw_value & 0xFFU);
    raw.byte1 = static_cast<uint8_t>((raw_value >> 8U) & 0xFFU);
    raw.byte2 = static_cast<uint8_t>((raw_value >> 16U) & 0xFFU);
    raw.high_byte = static_cast<uint8_t>((raw_value >> 24U) & 0xFFU);
    raw.low_word = static_cast<uint16_t>(raw_value & 0xFFFFU);
    raw.high_word = static_cast<uint16_t>((raw_value >> 16U) & 0xFFFFU);
    raw.family_id = static_cast<uint8_t>(raw.low_byte & 0x07U);
    raw.terrain_flags = static_cast<uint8_t>(raw.low_byte & 0x38U);
    raw.variant_bits = static_cast<uint8_t>((raw.low_byte >> 3U) & 0x07U);
    raw.border_shape = static_cast<uint8_t>((raw_value >> 26U) & 0x3FU);
    raw.has_border_shape = raw.border_shape > 0;
    raw.has_drawable_border_shape = raw.border_shape > 0 && raw.border_shape != 16;
    raw.has_unknown_high_bits = (raw_value & 0x03FFFF00U) != 0;
    return raw;
}

AdventureTerrainFamily family_for_material(AdventureTerrainMaterial material) {
    switch (material) {
    case AdventureTerrainMaterial::Black:
        return AdventureTerrainFamily::Black;
    case AdventureTerrainMaterial::Human:
        return AdventureTerrainFamily::Human;
    case AdventureTerrainMaterial::Dwarf:
        return AdventureTerrainFamily::Dwarf;
    case AdventureTerrainMaterial::Heretic:
        return AdventureTerrainFamily::Heretic;
    case AdventureTerrainMaterial::Undead:
        return AdventureTerrainFamily::Undead;
    case AdventureTerrainMaterial::Neutral:
        return AdventureTerrainFamily::Neutral;
    case AdventureTerrainMaterial::Elf:
        return AdventureTerrainFamily::Elf;
    case AdventureTerrainMaterial::Water:
        return AdventureTerrainFamily::Water;
    case AdventureTerrainMaterial::Unknown:
        return AdventureTerrainFamily::Unknown;
    }
    return AdventureTerrainFamily::Unknown;
}

std::pair<AdventureTerrainMaterial, const char*> material_for_low_byte(uint8_t low_byte) {
    switch (low_byte) {
    case 0:
        return {AdventureTerrainMaterial::Black, "BL"};
    case 1:
    case 9:
        return {AdventureTerrainMaterial::Human, "HU"};
    case 2:
    case 10:
        return {AdventureTerrainMaterial::Dwarf, "DW"};
    case 3:
    case 11:
        return {AdventureTerrainMaterial::Heretic, "HE"};
    case 4:
    case 12:
        return {AdventureTerrainMaterial::Undead, "UN"};
    case 5:
    case 13:
    case 37:
        return {AdventureTerrainMaterial::Neutral, "NE"};
    case 6:
    case 14:
        return {AdventureTerrainMaterial::Elf, "EL"};
    case 7:
    case 29:
        return {AdventureTerrainMaterial::Water, "WA"};
    default:
        return {AdventureTerrainMaterial::Unknown, "BL"};
    }
}

std::string record2(std::string_view prefix, int value, std::string_view suffix) {
    char buffer[32] = {};
    std::snprintf(buffer, sizeof(buffer), "%.*s_%02d%.*s", static_cast<int>(prefix.size()),
                  prefix.data(), value, static_cast<int>(suffix.size()), suffix.data());
    return buffer;
}

} // namespace

AdventureTerrainTileDescriptor
AdventureTerrainDecoder::decode_tile(uint32_t                             raw_value,
                                     const AdventureTerrainDecodeOptions& options) const {
    AdventureTerrainTileDescriptor tile;
    tile.raw_value = raw_value;
    tile.raw = decode_raw_fields(raw_value);
    const auto [material, terrain_code] = material_for_low_byte(tile.raw.low_byte);
    tile.material = material;
    tile.family = family_for_material(material);
    tile.terrain_code = terrain_code;
    tile.unknown_material = material == AdventureTerrainMaterial::Unknown;
    tile.is_forest = (tile.raw.low_byte >= 9 && tile.raw.low_byte <= 14);
    if (tile.terrain_code == "WA" || tile.terrain_code == "BL") {
        tile.ground_variant = 0;
    } else if (options.ground_variant != 0) {
        tile.ground_variant = options.ground_variant;
    } else {
        tile.ground_variant = tile.raw.variant_bits & 0x03;
    }
    tile.expected_ground_asset = {.container_path = "Imgs/Ground.ff",
                                  .record_name =
                                      record2(tile.terrain_code, tile.ground_variant, ".PNG")};

    if (tile.raw.has_border_shape) {
        AdventureTerrainBorderDescriptor border;
        border.shape = tile.raw.border_shape;
        border.family = tile.terrain_code == "WA" ? "WA" : "NE";
        if (tile.raw.has_drawable_border_shape) {
            border.kind = AdventureTerrainBorderKind::Drawable;
            border.expected_asset = AdventureTerrainAssetRef{
                .container_path = "Imgs/GrBorder.ff",
                .record_name = record2(border.family, border.shape, "_00.PNG")};
        } else {
            border.kind = AdventureTerrainBorderKind::NonDrawableShape16;
        }
        tile.border = std::move(border);
    }

    return tile;
}

AdventureTerrainMapDecoder::AdventureTerrainMapDecoder(const AdventureTerrainDecoder& tile_decoder)
    : tile_decoder_(tile_decoder) {}

std::vector<AdventureTerrainTileDescriptor>
AdventureTerrainMapDecoder::decode_grid(const AdventureTerrainGrid&          grid,
                                        const AdventureTerrainDecodeOptions& options) const {
    std::vector<AdventureTerrainTileDescriptor> result;
    if (grid.width <= 0 || grid.height <= 0) {
        return result;
    }
    result.reserve(static_cast<std::size_t>(grid.width) * static_cast<std::size_t>(grid.height));
    for (const auto& tile : grid.tiles) {
        result.push_back(tile_decoder_.decode_tile(tile.raw_value, options));
    }
    return result;
}

} // namespace d2runtime
