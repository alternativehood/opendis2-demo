#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace d2runtime {

enum class AdventureTerrainFamily : uint8_t {
    Black = 0,
    Human = 1,
    Dwarf = 2,
    Heretic = 3,
    Undead = 4,
    Neutral = 5,
    Elf = 6,
    Water = 7,
    Unknown = 255,
};

enum class AdventureTerrainMaterial : uint8_t {
    Black = 0,
    Human = 1,
    Dwarf = 2,
    Heretic = 3,
    Undead = 4,
    Neutral = 5,
    Elf = 6,
    Water = 7,
    Unknown = 255,
};

struct AdventureTerrainRawFields {
    uint32_t raw_value = 0;

    uint8_t low_byte = 0;
    uint8_t byte1 = 0;
    uint8_t byte2 = 0;
    uint8_t high_byte = 0;

    uint16_t low_word = 0;
    uint16_t high_word = 0;

    uint8_t family_id = 0;
    uint8_t terrain_flags = 0;
    uint8_t variant_bits = 0;
    uint8_t border_shape = 0;

    bool has_border_shape = false;
    bool has_drawable_border_shape = false;
    bool has_unknown_high_bits = false;
};

struct AdventureTerrainAssetRef {
    std::string container_path;
    std::string record_name;
};

enum class AdventureTerrainBorderKind : uint8_t {
    None = 0,
    Drawable = 1,
    NonDrawableShape16 = 2,
};

struct AdventureTerrainBorderDescriptor {
    AdventureTerrainBorderKind              kind = AdventureTerrainBorderKind::None;
    uint8_t                                 shape = 0;
    std::string                             family;
    std::optional<AdventureTerrainAssetRef> expected_asset;
};

struct AdventureTerrainTileDescriptor {
    uint32_t                  raw_value = 0;
    AdventureTerrainRawFields raw;

    AdventureTerrainFamily   family = AdventureTerrainFamily::Unknown;
    AdventureTerrainMaterial material = AdventureTerrainMaterial::Unknown;
    std::string              terrain_code;
    bool                     unknown_material = false;
    bool                     is_forest = false;

    int                      ground_variant = 0;
    AdventureTerrainAssetRef expected_ground_asset;

    std::optional<AdventureTerrainBorderDescriptor> border;
};

struct AdventureTerrainDecodeOptions {
    int ground_variant = 0;
};

[[nodiscard]] const char* terrain_border_kind_name(AdventureTerrainBorderKind kind);

} // namespace d2runtime
