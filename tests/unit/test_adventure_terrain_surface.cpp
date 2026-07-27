#include <d2adventure_render/terrain/adventure_terrain_surface.hpp>
#include <d2adventure_render/terrain/adventure_terrain_variant_hash.hpp>

#include <d2runtime/AdventureTerrainDecoder.hpp>

#include <opendis2_terrain_preview/terrain_preview_image.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

namespace {

std::string test_record_name(std::string_view prefix, int value, std::string_view suffix) {
    char buffer[32] = {};
    std::snprintf(buffer, sizeof(buffer), "%.*s_%02d%.*s", static_cast<int>(prefix.size()),
                  prefix.data(), value, static_cast<int>(suffix.size()), suffix.data());
    return buffer;
}

d2res::RgbaBuffer solid(std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    d2res::RgbaBuffer image;
    image.width = 64;
    image.height = 32;
    image.rgba.assign(static_cast<std::size_t>(image.width) * image.height * 4U, 0);
    for (std::size_t i = 0; i < image.rgba.size(); i += 4) {
        image.rgba[i] = r;
        image.rgba[i + 1] = g;
        image.rgba[i + 2] = b;
        image.rgba[i + 3] = 255;
    }
    return image;
}

d2res::RgbaBuffer gradient() {
    d2res::RgbaBuffer image;
    image.width = 128;
    image.height = 64;
    image.rgba.assign(static_cast<std::size_t>(image.width) * image.height * 4U, 0);
    for (std::uint32_t y = 0; y < image.height; ++y) {
        for (std::uint32_t x = 0; x < image.width; ++x) {
            auto* px = image.rgba.data() + (((static_cast<std::size_t>(y) * image.width) + x) * 4U);
            px[0] = static_cast<std::uint8_t>(x);
            px[1] = static_cast<std::uint8_t>(y);
            px[2] = 0;
            px[3] = 255;
        }
    }
    return image;
}

d2res::RgbaBuffer mask(std::uint8_t value) {
    return solid(value, value, value);
}

d2res::RgbaBuffer magenta_mask() {
    return solid(255, 0, 255);
}

std::string key(const std::string& record) {
    return "Imgs/Ground.ff/" + record;
}

std::string border_key(const std::string& record) {
    return "Imgs/GrBorder.ff/" + record;
}

d2runtime::AdventureTerrainTileDescriptor descriptor(uint32_t raw) {
    const d2runtime::AdventureTerrainDecoder decoder;
    return decoder.decode_tile(raw);
}

d2engine::AdventureTerrainSurfaceInput
input(int width, int height, std::vector<d2runtime::AdventureTerrainTileDescriptor> descriptors) {
    std::vector<d2engine::ResolvedAdventureTerrainTile> resolved;
    resolved.reserve(descriptors.size());
    for (const auto& desc : descriptors) {
        resolved.push_back({.descriptor = desc, .ground_asset_found = true});
    }
    return {.map_width = width,
            .map_height = height,
            .descriptors = std::move(descriptors),
            .resolved_tiles = std::move(resolved)};
}

d2engine::AdventureTerrainSurfaceImageMap base_images() {
    d2engine::AdventureTerrainSurfaceImageMap images;
    images.emplace(key("HU_00.PNG"), solid(10, 0, 0));
    images.emplace(key("HU_01.PNG"), solid(20, 0, 0));
    images.emplace(key("HU_02.PNG"), solid(30, 0, 0));
    images.emplace(key("HU_03.PNG"), solid(40, 0, 0));
    images.emplace(key("DW_00.PNG"), solid(0, 10, 10));
    images.emplace(key("DW_01.PNG"), solid(0, 20, 20));
    images.emplace(key("DW_02.PNG"), solid(0, 30, 30));
    images.emplace(key("DW_03.PNG"), solid(0, 40, 40));
    images.emplace(key("HE_00.PNG"), solid(60, 0, 10));
    images.emplace(key("HE_01.PNG"), solid(70, 0, 10));
    images.emplace(key("HE_02.PNG"), solid(80, 0, 10));
    images.emplace(key("HE_03.PNG"), solid(90, 0, 10));
    images.emplace(key("UN_00.PNG"), solid(30, 0, 30));
    images.emplace(key("UN_01.PNG"), solid(40, 0, 40));
    images.emplace(key("UN_02.PNG"), solid(50, 0, 50));
    images.emplace(key("UN_03.PNG"), solid(60, 0, 60));
    images.emplace(key("EL_00.PNG"), solid(0, 60, 20));
    images.emplace(key("EL_01.PNG"), solid(0, 70, 20));
    images.emplace(key("EL_02.PNG"), solid(0, 80, 20));
    images.emplace(key("EL_03.PNG"), solid(0, 90, 20));
    images.emplace(key("NE_00.PNG"), solid(0, 20, 0));
    images.emplace(key("NE_01.PNG"), solid(0, 30, 0));
    images.emplace(key("NE_02.PNG"), solid(0, 40, 0));
    images.emplace(key("NE_03.PNG"), solid(0, 50, 0));
    images.emplace(key("WA_00.PNG"), solid(0, 0, 80));
    images.emplace(key("BL_00.PNG"), solid(5, 5, 5));
    for (int shape = 1; shape <= 31; ++shape) {
        if (shape == 16) {
            continue;
        }
        // NOLINTNEXTLINE(misc-const-correctness)
        char record[16]{};
        std::snprintf(record, sizeof(record), "%02d_00.PNG", shape);
        images.emplace(border_key("NE_" + std::string(record)), mask(0));
        images.emplace(border_key("WA_" + std::string(record)), mask(0));
    }
    for (const auto shape : {1, 2, 4, 8, 11, 14}) {
        // NOLINTNEXTLINE(misc-const-correctness)
        char record[16]{};
        std::snprintf(record, sizeof(record), "%02d_01.PNG", shape);
        images.emplace(border_key("NE_" + std::string(record)), mask(0));
    }
    for (const auto shape : {1, 2, 3, 4, 5, 8, 9, 10, 12}) {
        for (const auto variant : {1, 2}) {
            char record[16]{};
            std::snprintf(record, sizeof(record), "%02d_%02d.PNG", shape, variant);
            images.emplace(border_key("WA_" + std::string(record)), mask(0));
        }
    }
    return images;
}

d2engine::TerrainAssetCatalog base_catalog() {
    d2engine::TerrainAssetCatalog catalog;
    // Ground variants
    for (const auto& code : {"HU", "DW", "HE", "UN", "EL", "NE", "WA"}) {
        for (int v = 0; v <= 3; ++v) {
            catalog.ground_variant_index[code].push_back(v);
        }
    }
    // Border variants: NE shapes 1-31 (excl 16), all with variant 0
    // plus shapes 1,2,4,8,11,14 with variant 1
    for (int shape = 1; shape <= 31; ++shape) {
        if (shape == 16)
            continue;
        catalog.border_variant_index[{"NE", shape}].push_back(0);
        catalog.border_assets.push_back(
            {"NE", shape, 0, "Imgs/GrBorder.ff", test_record_name("NE", shape, "_00.PNG"), 64, 32});
    }
    for (const auto shape : {1, 2, 4, 8, 11, 14}) {
        catalog.border_variant_index[{"NE", shape}].push_back(1);
        catalog.border_assets.push_back(
            {"NE", shape, 1, "Imgs/GrBorder.ff", test_record_name("NE", shape, "_01.PNG"), 64, 32});
    }
    // WA shapes 1-31 (excl 16), variant 0; plus listed shapes with variants 1,2
    for (int shape = 1; shape <= 31; ++shape) {
        if (shape == 16)
            continue;
        catalog.border_variant_index[{"WA", shape}].push_back(0);
        catalog.border_assets.push_back(
            {"WA", shape, 0, "Imgs/GrBorder.ff", test_record_name("WA", shape, "_00.PNG"), 64, 32});
    }
    for (const auto shape : {1, 2, 3, 4, 5, 8, 9, 10, 12}) {
        for (const auto variant : {1, 2}) {
            catalog.border_variant_index[{"WA", shape}].push_back(variant);
            char buf[32] = {};
            std::snprintf(buf, sizeof(buf), "WA_%02d_%02d.PNG", shape, variant);
            catalog.border_assets.push_back(
                {"WA", shape, variant, "Imgs/GrBorder.ff", buf, 64, 32});
        }
    }
    return catalog;
}

void override_all_wa_variants(d2engine::AdventureTerrainSurfaceImageMap& images,
                              const d2res::RgbaBuffer&                   buf) {
    for (int shape = 1; shape <= 31; ++shape) {
        if (shape == 16)
            continue;
        for (int variant = 0; variant <= 2; ++variant) {
            char record[16]{};
            std::snprintf(record, sizeof(record), "%02d_%02d.PNG", shape, variant);
            images[border_key("WA_" + std::string(record))] = buf;
        }
    }
}

void override_all_ne_variants(d2engine::AdventureTerrainSurfaceImageMap& images,
                              const d2res::RgbaBuffer&                   buf) {
    for (int shape = 1; shape <= 31; ++shape) {
        if (shape == 16)
            continue;
        for (int variant = 0; variant <= 2; ++variant) {
            char record[16]{};
            std::snprintf(record, sizeof(record), "%02d_%02d.PNG", shape, variant);
            images[border_key("NE_" + std::string(record))] = buf;
        }
    }
}

const d2engine::AdventureTerrainSurfacePixel&
pixel(const d2engine::AdventureTerrainSurface& surface, int x, int y) {
    return surface.pixels[(static_cast<std::size_t>(y) * static_cast<std::size_t>(surface.width)) +
                          static_cast<std::size_t>(x)];
}

d2res::RgbaBuffer border_with_pixel_at_center(std::uint8_t pr, std::uint8_t pg, std::uint8_t pb,
                                              std::uint8_t transparent_r,
                                              std::uint8_t transparent_g,
                                              std::uint8_t transparent_b) {
    d2res::RgbaBuffer image;
    image.width = 64;
    image.height = 32;
    image.rgba.assign(static_cast<std::size_t>(image.width) * image.height * 4U, 0);
    for (std::uint32_t y = 0; y < image.height; ++y) {
        for (std::uint32_t x = 0; x < image.width; ++x) {
            auto* px =
                image.rgba.data() +
                (((static_cast<std::size_t>(y) * image.width) + static_cast<std::size_t>(x)) * 4U);
            // NOLINTNEXTLINE(bugprone-branch-clone)
            if (x == 32 && y == 16) {
                px[0] = pr;
                px[1] = pg;
                px[2] = pb;
                px[3] = 255;
            } else {
                px[0] = transparent_r;
                px[1] = transparent_g;
                px[2] = transparent_b;
                px[3] = 255;
            }
        }
    }
    return image;
}

d2res::RgbaBuffer mask_with_white_at_center() {
    d2res::RgbaBuffer image;
    image.width = 64;
    image.height = 32;
    image.rgba.assign(static_cast<std::size_t>(image.width) * image.height * 4U, 0);
    for (std::uint32_t y = 0; y < image.height; ++y) {
        for (std::uint32_t x = 0; x < image.width; ++x) {
            auto* px =
                image.rgba.data() +
                (((static_cast<std::size_t>(y) * image.width) + static_cast<std::size_t>(x)) * 4U);
            if (x == 32 && y == 16) {
                px[0] = 255;
                px[1] = 255;
                px[2] = 255;
                px[3] = 255;
            } else {
                px[0] = 0;
                px[1] = 0;
                px[2] = 0;
                px[3] = 255;
            }
        }
    }
    return image;
}

d2engine::AdventureTerrainSurface
crop_tile_for_debug(const d2engine::AdventureTerrainSurfaceComposer& composer,
                    const d2engine::AdventureTerrainSurfaceInput& in, int tile_x, int tile_y,
                    const d2engine::AdventureTerrainSurfaceComposeOptions& options = {}) {
    auto                              full = composer.render_full_map(in, options);
    const int                         htw = options.tile_width / 2;
    const int                         hth = options.tile_height / 2;
    const int                         tile_canvas_x = (tile_x - tile_y + in.map_height - 1) * htw;
    const int                         tile_canvas_y = (tile_x + tile_y) * hth;
    d2engine::AdventureTerrainSurface tile;
    tile.width = options.tile_width;
    tile.height = options.tile_height;
    tile.pixels.resize(static_cast<std::size_t>(tile.width) *
                       static_cast<std::size_t>(tile.height));
    for (int ty = 0; ty < options.tile_height; ++ty) {
        auto canvas_row =
            static_cast<std::size_t>(tile_canvas_y + ty) * static_cast<std::size_t>(full.width);
        auto tile_row = static_cast<std::size_t>(ty) * static_cast<std::size_t>(tile.width);
        for (int tx = 0; tx < options.tile_width; ++tx) {
            tile.pixels[tile_row + static_cast<std::size_t>(tx)] =
                full.pixels[canvas_row + static_cast<std::size_t>(tile_canvas_x + tx)];
        }
    }
    return tile;
}

d2engine::AdventureTerrainSurfaceImageMap water_test_images() {
    auto images = base_images();
    for (int v = 0; v <= 3; ++v) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "HU_%02d.PNG", v);
        images[key(buf)] = solid(200, 0, 0);
    }
    images[key("WA_00.PNG")] = solid(0, 0, 200);
    for (int shape = 1; shape <= 31; ++shape) {
        if (shape == 16) {
            continue;
        }
        // NOLINTNEXTLINE(misc-const-correctness)
        char record[16]{};
        std::snprintf(record, sizeof(record), "%02d_00.PNG", shape);
        images[border_key("WA_" + std::string(record))] =
            border_with_pixel_at_center(0, 200, 0, 255, 0, 255);
        images[border_key("NE_" + std::string(record))] = mask_with_white_at_center();
    }
    for (const auto shape : {1, 2, 4, 8, 11, 14}) {
        char record[16]{};
        std::snprintf(record, sizeof(record), "%02d_01.PNG", shape);
        images[border_key("NE_" + std::string(record))] = mask_with_white_at_center();
    }
    for (const auto shape : {1, 2, 3, 4, 5, 8, 9, 10, 12}) {
        for (const auto variant : {1, 2}) {
            char record[16]{};
            std::snprintf(record, sizeof(record), "%02d_%02d.PNG", shape, variant);
            images[border_key("WA_" + std::string(record))] =
                border_with_pixel_at_center(0, 200, 0, 255, 0, 255);
        }
    }
    return images;
}

// ── Regression: NE diamond support ──────────────────────────────────────────

TEST(AdventureTerrainSurfaceComposer, CompositeNeMaskIsRestrictedToTileRasterSupport) {
    // NE mask 64x32, all opaque white
    d2res::RgbaBuffer full_white;
    full_white.width = 64;
    full_white.height = 32;
    full_white.rgba.assign(64U * 32U * 4U, 255);

    const auto individual = d2engine::prepare_ne_mask(full_white);

    const auto support = d2engine::prepare_d2_diamond_support(64, 32);

    d2engine::PreparedCompositeNeMaskKey key;
    key.records = {"NE_TEST"};

    std::map<std::string, d2engine::PreparedNeMask> masks;
    masks["NE_TEST"] = individual;

    const auto composite = d2engine::build_composite_ne_mask(key, masks, support);

    for (const auto& pp : composite.nonzero_pixels) {
        EXPECT_TRUE(d2engine::terrain_surface_diamond_contains(pp.x, pp.y, 64, 32))
            << "pixel (" << static_cast<int>(pp.x) << "," << static_cast<int>(pp.y)
            << ") must be inside tile support";
    }

    // Outside diamond pixels must be absent
    for (const auto& [x, y] : {std::pair{0, 0}, {63, 0}, {0, 31}, {63, 31}}) {
        bool found = false;
        for (const auto& pp : composite.nonzero_pixels) {
            if (pp.x == x && pp.y == y) {
                found = true;
                break;
            }
        }
        EXPECT_FALSE(found) << "pixel (" << x << "," << y
                            << ") outside diamond must NOT be in composite";
    }

    // Inside diamond pixels must exist
    int inside_count = 0;
    for (const auto& pp : composite.nonzero_pixels) {
        if (d2engine::terrain_surface_diamond_contains(pp.x, pp.y, 64, 32)) {
            ++inside_count;
        }
    }
    EXPECT_GT(inside_count, 0);
}

TEST(AdventureTerrainSurfaceComposer, CompositeCacheWorksWithSupport) {
    // Two tiles with equivalent NE record sets produce same canonical composite
    const auto support = d2engine::prepare_d2_diamond_support(64, 32);

    d2res::RgbaBuffer mask_a, mask_b;
    mask_a.width = 64;
    mask_a.height = 32;
    mask_a.rgba.assign(64U * 32U * 4U, 0);
    mask_a.rgba[((16U * 64U) + 32U) * 4U] = 64; // coverage=21 at (32,16)
    mask_a.rgba[((16U * 64U) + 32U) * 4U + 1] = 64;
    mask_a.rgba[((16U * 64U) + 32U) * 4U + 2] = 64;
    mask_a.rgba[((16U * 64U) + 32U) * 4U + 3] = 255;

    mask_b.width = 64;
    mask_b.height = 32;
    mask_b.rgba.assign(64U * 32U * 4U, 0);
    mask_b.rgba[((16U * 64U) + 32U) * 4U] = 192; // coverage=192 at (32,16)
    mask_b.rgba[((16U * 64U) + 32U) * 4U + 1] = 192;
    mask_b.rgba[((16U * 64U) + 32U) * 4U + 2] = 192;
    mask_b.rgba[((16U * 64U) + 32U) * 4U + 3] = 255;

    std::map<std::string, d2engine::PreparedNeMask> masks;
    masks["NE_X"] = d2engine::prepare_ne_mask(mask_a);
    masks["NE_Y"] = d2engine::prepare_ne_mask(mask_b);

    d2engine::PreparedCompositeNeMaskKey key_ab;
    key_ab.records = {"NE_X", "NE_Y"};
    std::ranges::sort(key_ab.records);
    d2engine::PreparedCompositeNeMaskKey key_ba;
    key_ba.records = {"NE_Y", "NE_X"};
    std::ranges::sort(key_ba.records);

    // Canonical keys must be identical after sort
    EXPECT_EQ(key_ab, key_ba);

    const auto composite = d2engine::build_composite_ne_mask(key_ab, masks, support);

    // Max(64, 192) = 192 at center pixel
    bool center_found = false;
    for (const auto& pp : composite.nonzero_pixels) {
        if (pp.x == 32 && pp.y == 16) {
            EXPECT_EQ(pp.coverage, 192);
            center_found = true;
            break;
        }
    }
    EXPECT_TRUE(center_found);

    // No pixel outside diamond support
    for (const auto& pp : composite.nonzero_pixels) {
        EXPECT_TRUE(d2engine::terrain_surface_diamond_contains(pp.x, pp.y, 64, 32));
    }
}

// ── Regression: WA not diamond-clipped ──────────────────────────────────────

TEST(AdventureTerrainSurfaceComposer, WaNotDiamondClipped) {
    // WA sprite 64x32 with green pixel at (0,0) — outside diamond
    d2res::RgbaBuffer wa_sprite;
    wa_sprite.width = 64;
    wa_sprite.height = 32;
    wa_sprite.rgba.assign(64U * 32U * 4U, 255);
    // (0,0): green, opaque
    wa_sprite.rgba[0] = 0;
    wa_sprite.rgba[1] = 200;
    wa_sprite.rgba[2] = 0;
    wa_sprite.rgba[3] = 255;

    const auto prepared = d2engine::prepare_wa_sprite(wa_sprite);

    bool found_outside = false;
    for (const auto& pp : prepared.opaque_pixels) {
        if (pp.x == 0 && pp.y == 0) {
            EXPECT_EQ(pp.pixel.r, 0);
            EXPECT_EQ(pp.pixel.g, 200);
            EXPECT_EQ(pp.pixel.b, 0);
            EXPECT_EQ(pp.pixel.a, 255);
            found_outside = true;
            break;
        }
    }
    EXPECT_TRUE(found_outside) << "WA sprite must preserve (0,0) outside diamond";
}

// ── Regression: WA extends coverage ─────────────────────────────────────────

TEST(AdventureTerrainSurfaceComposer, WaExtendsCoverage) {
    auto images = base_images();
    images[key("WA_00.PNG")] = solid(0, 0, 200);

    // WA sprite with visible pixel outside both diamonds at (63,0)
    // (63,0) in the WA sprite maps to canvas (95, 16) for WA tile at (1,0)
    // canvas (95, 16) is outside HU diamond AND outside WA diamond
    d2res::RgbaBuffer wa_border;
    wa_border.width = 64;
    wa_border.height = 32;
    wa_border.rgba.assign(64U * 32U * 4U, 255);
    // (63,0): green with alpha=128 (partial coverage)
    wa_border.rgba[((0U * 64U) + 63U) * 4U] = 0;
    wa_border.rgba[((0U * 64U) + 63U) * 4U + 1] = 200;
    wa_border.rgba[((0U * 64U) + 63U) * 4U + 2] = 0;
    wa_border.rgba[((0U * 64U) + 63U) * 4U + 3] = 128;

    override_all_wa_variants(images, wa_border);
    override_all_ne_variants(images, mask(0));

    d2engine::AdventureTerrainSurfaceComposer composer(images, base_catalog());
    // Need 2x1 HU+WA so WA overlay operations exist (land-water border)
    const auto in = input(2, 1, {descriptor(1), descriptor(29)});

    // Prepare once
    const auto prepared = composer.prepare_full_map(in);

    // WA sprite (63,0) placed at tile (1,0): canvas_x=32, canvas_y=16
    // → canvas (95, 16)
    constexpr int         canvas_w = 96; // (map_w + map_h) * htw = (2+1)*32 = 96
    constexpr std::size_t sprite_idx = 16 * canvas_w + 95;

    // WA sprite placement must exist for tile (1,0)
    bool found_placement = false;
    for (const auto& placement : prepared.wa_sprite_placements) {
        if (placement.tile_canvas_x == 32 && placement.tile_canvas_y == 16) {
            ASSERT_NE(placement.sprite, nullptr);
            bool found_pixel = false;
            for (const auto& wp : placement.sprite->opaque_pixels) {
                if (wp.x == 63 && wp.y == 0 && wp.pixel.g == 200) {
                    found_pixel = true;
                    break;
                }
            }
            EXPECT_TRUE(found_pixel) << "WA sprite must contain outside-diamond pixel (63,0)";
            found_placement = true;
            break;
        }
    }
    EXPECT_TRUE(found_placement) << "WA sprite placement must exist for tile (1,0)";

    // Render twice, must be identical
    const auto first = composer.render_prepared_full_map(prepared);
    const auto second = composer.render_prepared_full_map(prepared);
    ASSERT_EQ(first.pixels.size(), second.pixels.size());
    for (std::size_t i = 0; i < first.pixels.size(); ++i) {
        EXPECT_EQ(first.pixels[i].r, second.pixels[i].r);
        EXPECT_EQ(first.pixels[i].g, second.pixels[i].g);
        EXPECT_EQ(first.pixels[i].b, second.pixels[i].b);
        EXPECT_EQ(first.pixels[i].a, second.pixels[i].a);
    }

    // WA shoreline green at (95, 16) must be visible (no diamond overlap)
    const auto  full_output = composer.render_prepared_full_map(prepared);
    const auto& out_px = full_output.pixels[sprite_idx];
    EXPECT_EQ(out_px.g, 200);
    EXPECT_NE(out_px.a, 0);
}

// ── Source-over tests ───────────────────────────────────────────────────────

TEST(AdventureTerrainSurfaceComposer, SourceOverTransparentDstPartialSrc) {
    // transparent dst + partial src tested via render pipeline: WA coverage=128 with green
    auto images = base_images();
    images[key("WA_00.PNG")] = solid(0, 0, 200);
    images[key("HU_00.PNG")] = solid(200, 0, 0);

    // WA border with alpha=128 at center
    d2res::RgbaBuffer wa_border;
    wa_border.width = 64;
    wa_border.height = 32;
    wa_border.rgba.assign(64U * 32U * 4U, 255);
    wa_border.rgba[((16U * 64U) + 32U) * 4U] = 0;
    wa_border.rgba[((16U * 64U) + 32U) * 4U + 1] = 200;
    wa_border.rgba[((16U * 64U) + 32U) * 4U + 2] = 0;
    wa_border.rgba[((16U * 64U) + 32U) * 4U + 3] = 128;

    override_all_wa_variants(images, wa_border);
    override_all_ne_variants(images, mask(0));

    d2engine::AdventureTerrainSurfaceComposer composer(images, base_catalog());
    // 2x1 HU+WA so WA overlay operations exist; WA center (crop 32,16) is outside HU diamond
    const auto in = input(2, 1, {descriptor(1), descriptor(29)});
    const auto surface = crop_tile_for_debug(composer, in, 1, 0);
    // Center pixel: WA alpha=128 from coverage, WA green from sprite
    // source_over(clear, green*128) -> (0, 200, 0, 128)  (dst.a==0 shortcut)
    const auto& px = pixel(surface, 32, 16);
    EXPECT_EQ(px.g, 200);
    EXPECT_EQ(px.a, 128);
}

TEST(AdventureTerrainSurfaceComposer, SourceOverPartiallyTransparentBoth) {
    auto images = base_images();
    // Override all HU variants so ground texture is consistent regardless of patch variant
    for (int v = 0; v <= 3; ++v) {
        char buf[32]{};
        std::snprintf(buf, sizeof(buf), "HU_%02d.PNG", v);
        images[key(buf)] = solid(200, 0, 0);
    }
    images[key("WA_00.PNG")] = solid(0, 0, 200);

    d2res::RgbaBuffer wa_border;
    wa_border.width = 64;
    wa_border.height = 32;
    wa_border.rgba.assign(64U * 32U * 4U, 255);
    // center pixel: green, alpha=128
    wa_border.rgba[((16U * 64U) + 32U) * 4U] = 0;
    wa_border.rgba[((16U * 64U) + 32U) * 4U + 1] = 200;
    wa_border.rgba[((16U * 64U) + 32U) * 4U + 2] = 0;
    wa_border.rgba[((16U * 64U) + 32U) * 4U + 3] = 128;

    override_all_wa_variants(images, wa_border);
    override_all_ne_variants(images, mask(128));

    d2engine::AdventureTerrainSurfaceComposer composer(images, base_catalog());
    const auto                                in = input(2, 1, {descriptor(1), descriptor(29)});
    // NE (HU target) invades WA at center with coverage 128 via uniform NE mask.
    // WA coverage=128 (sprite override), WA field=green.
    // HU coverage=128 (from NE composite), HU field=red (all variants).
    // source_over(WA_green*128, HU_red*128) -> (100,100,0,~192)
    const auto  surface = crop_tile_for_debug(composer, in, 1, 0);
    const auto& px = pixel(surface, 32, 16);
    EXPECT_NEAR(px.r, 100, 2);
    EXPECT_NEAR(px.g, 100, 2);
    EXPECT_NE(px.a, 0);
}

TEST(AdventureTerrainSurfaceComposer, SourceOverOpaqueDstPartialSrc) {
    // opaque dst + partial src
    auto images = base_images();
    for (int v = 0; v <= 3; ++v) {
        char buf[32]{};
        std::snprintf(buf, sizeof(buf), "HU_%02d.PNG", v);
        images[key(buf)] = solid(200, 0, 0);
    }
    images[key("WA_00.PNG")] = solid(0, 0, 200);

    d2res::RgbaBuffer wa_border;
    wa_border.width = 64;
    wa_border.height = 32;
    wa_border.rgba.assign(64U * 32U * 4U, 255);
    // center pixel: green, alpha=64
    wa_border.rgba[((16U * 64U) + 32U) * 4U] = 0;
    wa_border.rgba[((16U * 64U) + 32U) * 4U + 1] = 200;
    wa_border.rgba[((16U * 64U) + 32U) * 4U + 2] = 0;
    wa_border.rgba[((16U * 64U) + 32U) * 4U + 3] = 64;

    override_all_wa_variants(images, wa_border);
    override_all_ne_variants(images, mask(255));

    d2engine::AdventureTerrainSurfaceComposer composer(images, base_catalog());
    const auto                                in = input(2, 1, {descriptor(1), descriptor(29)});
    // HU (full 255 coverage) invades WA center over WA (64 coverage, green)
    const auto  surface = crop_tile_for_debug(composer, in, 1, 0);
    const auto& px = pixel(surface, 32, 16);
    // source_over(WA_green*64, HU_red*255)
    // HU source_overs: r = (200*255 + 0*0)/255 = 200
    EXPECT_EQ(px.r, 200);
    EXPECT_EQ(px.a, 255);
}

} // namespace

TEST(AdventureTerrainSurfaceComposer, WaterTileRendersWaOverlayBeforeLandNeMask) {
    const auto                                images = water_test_images();
    d2engine::AdventureTerrainSurfaceComposer composer(images, base_catalog());
    const auto                                in = input(2, 1, {descriptor(1), descriptor(29)});

    const auto surface = crop_tile_for_debug(composer, in, 1, 0);

    const auto& px = pixel(surface, 32, 16);
    EXPECT_EQ(px.r, 200);
    EXPECT_EQ(px.g, 0);
}

TEST(AdventureTerrainSurfaceComposer, WaterBorderOnlyPassRendersNeAboveWaOverlay) {
    const auto                                images = water_test_images();
    d2engine::AdventureTerrainSurfaceComposer composer(images, base_catalog());
    const auto                                in = input(2, 1, {descriptor(1), descriptor(29)});

    const auto surface =
        crop_tile_for_debug(composer, in, 1, 0, {.include_base = false, .include_borders = true});

    const auto& px = pixel(surface, 32, 16);
    EXPECT_EQ(px.r, 200);
    EXPECT_EQ(px.g, 0);
    EXPECT_NE(px.a, 0);
}

TEST(AdventureTerrainSurfaceComposer, ColorKeyOverlayUsesSharedMagentaCleanup) {
    auto images = base_images();
    images[key("WA_00.PNG")] = solid(0, 0, 200);
    images[key("HU_00.PNG")] = solid(200, 0, 0);

    // NOLINTNEXTLINE(misc-const-correctness)
    d2res::RgbaBuffer wa_border;
    wa_border.width = 64;
    wa_border.height = 32;
    wa_border.rgba.assign(static_cast<std::size_t>(wa_border.width) * wa_border.height * 4U, 0);
    for (std::uint32_t y = 0; y < wa_border.height; ++y) {
        for (std::uint32_t x = 0; x < wa_border.width; ++x) {
            auto* px =
                wa_border.rgba.data() +
                (((static_cast<std::size_t>(y) * wa_border.width) + static_cast<std::size_t>(x)) *
                 4U);
            if (x == 30 && y == 16) {
                px[0] = 0;
                px[1] = 200;
                px[2] = 0;
                px[3] = 255;
            } else if (x == 32 && y == 16) {
                px[0] = 252;
                px[1] = 2;
                px[2] = 252;
                px[3] = 255;
            } else if (x == 34 && y == 16) {
                px[0] = 255;
                px[1] = 255;
                px[2] = 255;
                px[3] = 255;
            } else {
                px[0] = 255;
                px[1] = 0;
                px[2] = 255;
                px[3] = 255;
            }
        }
    }

    for (int shape = 1; shape <= 31; ++shape) {
        if (shape == 16)
            continue;
        // NOLINTNEXTLINE(misc-const-correctness)
        char record[16]{};
        std::snprintf(record, sizeof(record), "%02d_00.PNG", shape);
        images[border_key("WA_" + std::string(record))] = wa_border;
    }
    for (const auto shape : {1, 2, 3, 4, 5, 8, 9, 10, 12}) {
        for (const auto variant : {1, 2}) {
            char record[16]{};
            std::snprintf(record, sizeof(record), "%02d_%02d.PNG", shape, variant);
            images[border_key("WA_" + std::string(record))] = wa_border;
        }
    }

    d2engine::AdventureTerrainSurfaceComposer composer(images, base_catalog());
    const auto                                in = input(2, 1, {descriptor(1), descriptor(29)});
    const auto                                surface = crop_tile_for_debug(composer, in, 1, 0);

    {
        const auto& px = pixel(surface, 30, 16);
        EXPECT_EQ(px.r, 0);
        EXPECT_EQ(px.g, 200);
    }
    {
        const auto& px = pixel(surface, 32, 16);
        EXPECT_EQ(px.b, 200);
        EXPECT_EQ(px.r, 0);
    }
    {
        const auto& px = pixel(surface, 34, 16);
        EXPECT_EQ(px.b, 200);
        EXPECT_EQ(px.r, 0);
    }
    {
        const auto& px = pixel(surface, 36, 16);
        EXPECT_EQ(px.b, 200);
        EXPECT_EQ(px.r, 0);
    }
}

TEST(AdventureTerrainSurfaceComposer, WaterTileStillGetsNeAndWaOperations) {
    d2engine::AdventureTerrainSurfaceComposer composer(base_images(), base_catalog());
    const auto                                in = input(2, 1, {descriptor(1), descriptor(29)});

    const auto info = composer.describe_tile(in, 1, 0);

    // NOLINTNEXTLINE(misc-const-correctness)
    bool found_wa = false;
    // NOLINTNEXTLINE(misc-const-correctness)
    bool found_ne = false;
    for (const auto& op : info.border_operations) {
        if (op.family == "WA") {
            found_wa = true;
        }
        if (op.family == "NE") {
            found_ne = true;
        }
    }
    EXPECT_TRUE(found_wa);
    EXPECT_TRUE(found_ne);
}

TEST(AdventureTerrainSurfaceComposer, DiamondMaskCornersTransparentCenterOpaque) {
    d2engine::AdventureTerrainSurfaceComposer composer(base_images(), base_catalog());
    const auto                                in = input(1, 1, {descriptor(0x00000001)});

    const auto surface = crop_tile_for_debug(composer, in, 0, 0);

    EXPECT_EQ(pixel(surface, 0, 0).a, 0);
    EXPECT_EQ(pixel(surface, 63, 31).a, 0);
    EXPECT_EQ(pixel(surface, 32, 16).a, 255);
}

TEST(AdventureTerrainSurfaceComposer, GroundTextureFieldUsesNativeGroundPngDimensions) {
    // Create a large Ground PNG (128x64) with a marker at local (96,48) outside 64x32.
    auto images = base_images();
    auto g = gradient();
    {
        const auto marker_i = ((static_cast<std::size_t>(48) * static_cast<std::size_t>(g.width)) +
                               static_cast<std::size_t>(96)) *
                              4U;
        g.rgba[marker_i] = 123;
        g.rgba[marker_i + 1] = 45;
        g.rgba[marker_i + 2] = 67;
    }
    images[key("HU_00.PNG")] = g;
    images[key("HU_01.PNG")] = g;
    images[key("HU_02.PNG")] = g;
    images[key("HU_03.PNG")] = g;
    d2engine::AdventureTerrainSurfaceComposer composer(std::move(images), base_catalog());
    // 3x1 all-HU map. Tile (2,0): world origin=(64,32), crop center=(32,16) -> world=(96,48) ->
    // local=(96%128=96, 48%64=48)
    const auto surface = crop_tile_for_debug(
        composer,
        input(3, 1, {descriptor(0x00000001), descriptor(0x00000001), descriptor(0x00000001)}), 2,
        0);
    const auto& px = pixel(surface, 32, 16);
    EXPECT_EQ(px.r, 123);
    EXPECT_EQ(px.g, 45);
    EXPECT_EQ(px.b, 67);
}

TEST(AdventureTerrainSurfaceComposer, GroundTextureFieldUsesMultipleVariantsDeterministically) {
    auto images = base_images();
    // HU variants: 0=red solid(10,0,0), 1=red solid(20,0,0), 2=red solid(30,0,0), 3=red
    // solid(40,0,0) All are 64x32. The texture field for a 2x1 map canvas (96x48) should tile 64x32
    // patches. Patch cell 0 (world x 0..63, y 0..31) gets variant based on hash("HU",0,0). Patch
    // cell 1 (world x 64..127, y 0..31) gets variant based on hash("HU",1,0). With 4 variants, at
    // least 2 different variants should appear across the canvas.
    d2engine::AdventureTerrainSurfaceComposer composer(std::move(images), base_catalog());
    const auto                                surface = crop_tile_for_debug(
        composer, input(2, 1, {descriptor(0x00000001), descriptor(0x00000001)}), 0, 0);
    // Collect distinct r values from the surface
    std::set<int> seen_r;
    for (const auto& px : surface.pixels) {
        seen_r.insert(px.r);
    }
    // With 4 variants and 2+ patch cells, at least 2 distinct colors must appear
    EXPECT_GE(seen_r.size(), 2U);
}

TEST(AdventureTerrainSurfaceComposer, GroundTextureFieldIsDeterministic) {
    // Same input produces byte-identical texture field every time.
    d2engine::AdventureTerrainSurfaceComposer composer(base_images(), base_catalog());
    const auto input_data = input(2, 1, {descriptor(0x00000001), descriptor(0x00000001)});
    const auto first = crop_tile_for_debug(composer, input_data, 0, 0);
    const auto second = crop_tile_for_debug(composer, input_data, 0, 0);
    for (std::size_t i = 0; i < first.pixels.size(); ++i) {
        EXPECT_EQ(first.pixels[i].r, second.pixels[i].r);
        EXPECT_EQ(first.pixels[i].g, second.pixels[i].g);
        EXPECT_EQ(first.pixels[i].b, second.pixels[i].b);
        EXPECT_EQ(first.pixels[i].a, second.pixels[i].a);
    }
}

TEST(AdventureTerrainSurfaceComposer, ShapeZeroAndSixteenUseMaterialAOnly) {
    d2engine::AdventureTerrainSurfaceComposer composer(base_images(), base_catalog());

    // HU tile at (0,0): ground texture variant from patch(0,0) hash -> variant 1 -> HU_01=(20,0,0)
    EXPECT_EQ(pixel(crop_tile_for_debug(composer,
                                        input(2, 1, {descriptor(0x00000001), descriptor(5)}), 0, 0),
                    32, 16)
                  .r,
              20);
    const auto shape16 =
        composer.describe_tile(input(2, 1, {descriptor(0x40000001), descriptor(5)}), 0, 0);
    EXPECT_EQ(shape16.composer_border_kind,
              d2runtime::AdventureTerrainBorderKind::NonDrawableShape16);
    EXPECT_TRUE(shape16.border_asset_found);
    EXPECT_EQ(pixel(crop_tile_for_debug(composer,
                                        input(2, 1, {descriptor(0x40000001), descriptor(5)}), 0, 0),
                    32, 16)
                  .r,
              20);
}

TEST(AdventureTerrainSurfaceComposer, HuSideDoesNotEmitWaterBorderOperation) {
    d2engine::AdventureTerrainSurfaceComposer composer(base_images(), base_catalog());
    const auto                                in = input(2, 1, {descriptor(1), descriptor(29)});

    const auto hu = composer.describe_tile(in, 0, 0);
    const auto info = composer.describe_tile(in, 1, 0);

    EXPECT_EQ(hu.material_a_code, "HU");
    EXPECT_TRUE(hu.border_operations.empty());
    EXPECT_EQ(info.material_a_code, "WA");
    EXPECT_EQ(info.material_b_code, "HU");
    EXPECT_EQ(info.composer_border_family, "NE");
    EXPECT_EQ(info.border_shape_source, "explicit_ne_topology");
    EXPECT_FALSE(info.border_synthesized);
    EXPECT_EQ(info.resolved_record_shape, 1);
    EXPECT_EQ(info.composer_border_kind, d2runtime::AdventureTerrainBorderKind::Drawable);
    EXPECT_TRUE(info.neighbor_mask.west);
    ASSERT_EQ(info.border_operations.size(), 2U);
    EXPECT_EQ(info.border_operations[0].family, "NE");
    EXPECT_EQ(info.border_operations[1].family, "WA");
}

TEST(AdventureTerrainSurfaceComposer, WaterUsesAnyNonWaterNeighborForWaMapping) {
    d2engine::AdventureTerrainSurfaceComposer composer(base_images(), base_catalog());
    const auto in = input(2, 1, {descriptor(0x00000018), descriptor(29)});

    const auto info = composer.describe_tile(in, 1, 0);

    EXPECT_EQ(info.material_a_code, "WA");
    EXPECT_EQ(info.composer_border_family, "WA");
    EXPECT_EQ(info.border_shape_source, "explicit_wa_topology");
    EXPECT_EQ(info.resolved_record_shape, 1);
}

TEST(AdventureTerrainSurfaceComposer, TopologyPathDoesNotUseRawShapeFallbackForNormalTransition) {
    d2engine::AdventureTerrainSurfaceComposer composer(base_images(), base_catalog());
    const auto in = input(2, 1, {descriptor(0x04000005), descriptor(1)});

    const auto info = composer.describe_tile(in, 0, 0);

    EXPECT_EQ(info.border_shape_source, "explicit_ne_topology");
    EXPECT_FALSE(info.border_synthesized);
    EXPECT_EQ(info.composer_border_family, "NE");
    EXPECT_EQ(info.resolved_record_shape, 4);
}

TEST(AdventureTerrainSurfaceComposer, StrongerTileDoesNotEmitMirrorLandBorder) {
    d2engine::AdventureTerrainSurfaceComposer composer(base_images(), base_catalog());
    const auto                                in = input(2, 1, {descriptor(1), descriptor(5)});

    const auto strong = composer.describe_tile(in, 0, 0);
    const auto weak = composer.describe_tile(in, 1, 0);

    EXPECT_EQ(strong.material_a_code, "HU");
    EXPECT_TRUE(strong.border_operations.empty());
    EXPECT_EQ(weak.material_a_code, "NE");
    EXPECT_EQ(weak.border_shape_source, "explicit_ne_topology");
    EXPECT_EQ(weak.composer_border_family, "NE");
    EXPECT_EQ(weak.resolved_record_shape, 1);
}

TEST(AdventureTerrainSurfaceComposer, ShapeSixteenDoesNotSynthesizeFromNeighbor) {
    d2engine::AdventureTerrainSurfaceComposer composer(base_images(), base_catalog());
    const auto in = input(2, 1, {descriptor(0x40000001), descriptor(29)});

    const auto info = composer.describe_tile(in, 0, 0);

    EXPECT_EQ(info.composer_border_kind, d2runtime::AdventureTerrainBorderKind::NonDrawableShape16);
    EXPECT_EQ(info.border_shape_source, "non_drawable_shape_16");
    EXPECT_FALSE(info.border_synthesized);
    EXPECT_TRUE(info.border_asset_found);
}

TEST(AdventureTerrainSurfaceComposer, LandAndWaterBorderFamilies) {
    d2engine::AdventureTerrainSurfaceComposer composer(base_images(), base_catalog());

    const auto land =
        composer.describe_tile(input(2, 1, {descriptor(0x04000005), descriptor(1)}), 0, 0);
    EXPECT_EQ(land.composer_border_family, "NE");
    EXPECT_EQ(land.resolved_record_shape, 4);

    const auto shore = composer.describe_tile(input(2, 1, {descriptor(1), descriptor(29)}), 1, 0);
    EXPECT_EQ(shore.composer_border_family, "NE");
    EXPECT_EQ(shore.resolved_record_shape, 1);

    const auto water_current =
        composer.describe_tile(input(2, 1, {descriptor(0x0400001D), descriptor(1)}), 0, 0);
    EXPECT_EQ(water_current.composer_border_family, "NE");
}

TEST(AdventureTerrainSurfaceComposer, MaterialBSelectionUsesWaterFrequencyAndTieOrder) {
    d2engine::AdventureTerrainSurfaceComposer composer(base_images(), base_catalog());

    EXPECT_EQ(composer
                  .describe_tile(
                      input(3, 1, {descriptor(2), descriptor(0x04000001), descriptor(5)}), 1, 0)
                  .material_b_code,
              "DW");
    EXPECT_EQ(composer
                  .describe_tile(input(3, 2,
                                       {descriptor(1), descriptor(0x04000001), descriptor(5),
                                        descriptor(5), descriptor(5), descriptor(2)}),
                                 1, 0)
                  .material_b_code,
              "DW");
    EXPECT_EQ(composer.describe_tile(input(2, 1, {descriptor(0x04000001), descriptor(29)}), 0, 0)
                  .material_b_code,
              "WA");
}

TEST(AdventureTerrainSurfaceComposer, WaterLowByteUsesWaterGroundAndColorKeyOverlay) {
    auto images = base_images();
    images[border_key("WA_01_00.PNG")] = magenta_mask();
    images[border_key("WA_01_01.PNG")] = magenta_mask();
    images[border_key("WA_01_02.PNG")] = magenta_mask();
    const auto center = ((static_cast<std::size_t>(16) * 64U) + 32U) * 4U;
    for (const auto record : {"WA_01_00.PNG", "WA_01_01.PNG", "WA_01_02.PNG"}) {
        images[border_key(record)].rgba[center] = 200;
        images[border_key(record)].rgba[center + 1] = 120;
        images[border_key(record)].rgba[center + 2] = 40;
        images[border_key(record)].rgba[center + 3] = 255;
    }
    d2engine::AdventureTerrainSurfaceComposer composer(images, base_catalog());
    const auto                                water = input(1, 1, {descriptor(0x0000001D)});

    EXPECT_EQ(composer.describe_tile(water, 0, 0).material_a_code, "WA");
    EXPECT_EQ(pixel(crop_tile_for_debug(composer, water, 0, 0), 32, 16).b, 80);

    const auto in = input(2, 1, {descriptor(1), descriptor(29)});
    const auto surface = crop_tile_for_debug(composer, in, 1, 0);
    EXPECT_EQ(pixel(surface, 32, 16).r, 200);
    EXPECT_EQ(pixel(surface, 32, 16).g, 120);
    EXPECT_EQ(pixel(surface, 32, 16).b, 40);
    EXPECT_EQ(pixel(surface, 32, 8).b, 80);
}

TEST(AdventureTerrainSurfaceComposer, ExplicitNeTopologyUsesMaskBlend) {
    auto                                      images = base_images();
    d2engine::AdventureTerrainSurfaceComposer composer(images, base_catalog());
    auto in = input(2, 1, {descriptor(0x04000005), descriptor(1)});

    // NE ground: hash("NE",0,0) -> variant 3 -> NE_03=(0,50,0)
    // HU ground: hash("HU",0,0) -> variant 1 -> HU_01=(20,0,0)
    images[border_key("NE_04_00.PNG")] = mask(255);
    images[border_key("NE_04_01.PNG")] = mask(255);
    composer = d2engine::AdventureTerrainSurfaceComposer(images, base_catalog());
    EXPECT_EQ(pixel(crop_tile_for_debug(composer, in, 0, 0), 32, 16).r, 20);

    images[border_key("NE_04_00.PNG")] = mask(128);
    images[border_key("NE_04_01.PNG")] = mask(128);
    composer = d2engine::AdventureTerrainSurfaceComposer(images, base_catalog());
    const auto gray = pixel(crop_tile_for_debug(composer, in, 0, 0), 32, 16);
    EXPECT_NEAR(gray.r, 10, 1);
    EXPECT_NEAR(gray.g, 24, 1);

    images[border_key("NE_04_00.PNG")] = magenta_mask();
    images[border_key("NE_04_01.PNG")] = magenta_mask();
    composer = d2engine::AdventureTerrainSurfaceComposer(images, base_catalog());
    EXPECT_EQ(pixel(crop_tile_for_debug(composer, in, 0, 0), 32, 16).g, 50);
}

TEST(AdventureTerrainSurfaceComposer, BorderOnlyNeMaskPreservesAntialiasAlpha) {
    auto images = base_images();
    images[border_key("NE_04_00.PNG")] = mask(9);
    images[border_key("NE_04_01.PNG")] = mask(9);
    d2engine::AdventureTerrainSurfaceComposer composer(images, base_catalog());
    const auto                                in = input(2, 1, {descriptor(5), descriptor(1)});

    // HU ground: hash("HU",0,0) -> variant 1 -> HU_01=(20,0,0)
    const auto dark = crop_tile_for_debug(composer, in, 0, 0, {.include_base = false});
    EXPECT_EQ(pixel(dark, 32, 16).r, 20);
    EXPECT_EQ(pixel(dark, 32, 16).a, 9);

    images[border_key("NE_04_00.PNG")] = mask(0);
    images[border_key("NE_04_01.PNG")] = mask(0);
    composer = d2engine::AdventureTerrainSurfaceComposer(images, base_catalog());
    EXPECT_EQ(pixel(crop_tile_for_debug(composer, in, 0, 0, {.include_base = false}), 32, 16).a, 0);

    images[border_key("NE_04_00.PNG")] = mask(255);
    images[border_key("NE_04_01.PNG")] = mask(255);
    composer = d2engine::AdventureTerrainSurfaceComposer(images, base_catalog());
    EXPECT_EQ(pixel(crop_tile_for_debug(composer, in, 0, 0, {.include_base = false}), 32, 16).a,
              255);
}

TEST(AdventureTerrainSurfaceComposer, NeMasksRemainPlainMaskBlendOnEdgePixels) {
    auto images = base_images();
    images[border_key("NE_04_00.PNG")] = mask(0);
    d2engine::AdventureTerrainSurfaceComposer composer(images, base_catalog());
    const auto                                in = input(2, 1, {descriptor(5), descriptor(1)});

    // NE ground: hash("NE",0,0) -> variant 3 -> NE_03=(0,50,0)
    const auto surface = crop_tile_for_debug(composer, in, 0, 0);

    EXPECT_EQ(pixel(surface, 60, 15).a, 255);
    EXPECT_EQ(pixel(surface, 60, 15).r, 0);
    EXPECT_EQ(pixel(surface, 60, 15).g, 50);
}

TEST(AdventureTerrainSurfaceComposer, NeMasksAreNotForceConvertedToWhiteOnEdgePixels) {
    auto images = base_images();
    images[border_key("NE_04_00.PNG")] = mask(128);
    images[border_key("NE_04_01.PNG")] = mask(128);
    d2engine::AdventureTerrainSurfaceComposer composer(images, base_catalog());
    // NE ground: NE_03=(0,50,0); HU ground: HU_01=(20,0,0)
    // NE mask=128 provides HU coverage=128 at tile center
    // Blend: r = (20*128 + 0*127)/255 = 10; g = (0*128 + 50*127)/255 = 24
    const auto surface =
        crop_tile_for_debug(composer, input(2, 1, {descriptor(5), descriptor(1)}), 0, 0);
    EXPECT_NEAR(pixel(surface, 60, 15).r, 10, 1);
    EXPECT_NEAR(pixel(surface, 60, 15).g, 24, 1);
}

TEST(AdventureTerrainSurfaceComposer, WaColorKeyOverlayIsUnaffectedByBaseCoverage) {
    auto images = base_images();
    images[border_key("WA_01_00.PNG")] = mask(0);
    images[border_key("WA_01_01.PNG")] = mask(0);
    images[border_key("WA_01_02.PNG")] = mask(0);
    d2engine::AdventureTerrainSurfaceComposer composer(images, base_catalog());
    const auto                                surface =
        crop_tile_for_debug(composer, input(2, 1, {descriptor(1), descriptor(29)}), 1, 0);

    EXPECT_EQ(pixel(surface, 3, 15).a, 255);
    EXPECT_EQ(pixel(surface, 3, 15).r, 0);
    EXPECT_EQ(pixel(surface, 3, 15).g, 0);
    EXPECT_EQ(pixel(surface, 3, 15).b, 0);
}

TEST(AdventureTerrainSurfaceComposer, DiamondMaskDoesNotChangeBaseFill) {
    d2engine::AdventureTerrainSurfaceComposer composer(base_images(), base_catalog());
    const auto surface = crop_tile_for_debug(composer, input(1, 1, {descriptor(1)}), 0, 0);

    EXPECT_EQ(pixel(surface, 1, 15).a, 255);
    EXPECT_EQ(pixel(surface, 61, 15).a, 255);
    EXPECT_EQ(pixel(surface, 31, 0).a, 255);
    EXPECT_EQ(pixel(surface, 31, 31).a, 255);
}

TEST(AdventureTerrainSurfaceComposer, Topology3x3ResultIsCoordinateIndependent) {
    d2engine::AdventureTerrainSurfaceComposer composer(base_images(), base_catalog());
    const auto                                make_input = [](int origin) {
        std::vector<d2runtime::AdventureTerrainTileDescriptor> descriptors(12 * 12, descriptor(1));
        descriptors[static_cast<std::size_t>(origin + origin * 12)] = descriptor(5);
        descriptors[static_cast<std::size_t>(origin + 1 + origin * 12)] = descriptor(1);
        return input(12, 12, descriptors);
    };

    const auto at_one = composer.describe_tile(make_input(1), 1, 1);
    const auto at_three = composer.describe_tile(make_input(3), 3, 3);
    const auto at_nine = composer.describe_tile(make_input(9), 9, 9);

    EXPECT_EQ(at_one.border_shape_source, "explicit_ne_topology");
    EXPECT_EQ(at_three.border_shape_source, at_one.border_shape_source);
    EXPECT_EQ(at_nine.border_shape_source, at_one.border_shape_source);
    EXPECT_EQ(at_three.resolved_border_record, at_one.resolved_border_record);
    EXPECT_EQ(at_nine.resolved_border_record, at_one.resolved_border_record);
}

TEST(AdventureTerrainSurfaceComposer, Topology3x3SeesAllEightNeighbors) {
    d2engine::AdventureTerrainSurfaceComposer composer(base_images(), base_catalog());
    const auto                                in =
        input(3, 3,
              {descriptor(1), descriptor(1), descriptor(1), descriptor(1), descriptor(5),
               descriptor(1), descriptor(1), descriptor(1), descriptor(1)});

    const auto info = composer.describe_tile(in, 1, 1);

    EXPECT_EQ(info.land_cardinal_mask, 0x0F);
    EXPECT_EQ(info.land_diagonal_mask, 0x0F);
    EXPECT_EQ(info.stronger_cardinal_mask, 0x0F);
    EXPECT_EQ(info.stronger_diagonal_mask, 0x0F);
    ASSERT_EQ(info.border_operations.size(), 1U);
    EXPECT_EQ(info.border_operations[0].source, "explicit_ne_topology");
}

TEST(AdventureTerrainSurfaceComposer, Topology3x3DirectionBitsAreScreenSpace) {
    d2engine::AdventureTerrainSurfaceComposer composer(base_images(), base_catalog());
    struct Case {
        int          dx = 0;
        int          dy = 0;
        std::uint8_t shape = 0;
    };
    static constexpr std::array cases = {
        Case{-1, 0, 1},   Case{0, -1, 2},  Case{1, 0, 4},  Case{0, 1, 8},
        Case{-1, -1, 17}, Case{1, -1, 18}, Case{1, 1, 20}, Case{-1, 1, 24},
    };

    for (const auto& item : cases) {
        std::vector<d2runtime::AdventureTerrainTileDescriptor> descriptors(9, descriptor(5));
        descriptors[4] = descriptor(5);
        descriptors[static_cast<std::size_t>(1 + item.dx + (1 + item.dy) * 3)] = descriptor(1);

        const auto info = composer.describe_tile(input(3, 3, descriptors), 1, 1);

        ASSERT_EQ(info.border_operations.size(), 1U) << static_cast<int>(item.shape);
        EXPECT_EQ(info.resolved_record_shape, item.shape);
    }
}

TEST(AdventureTerrainSurfaceComposer, BorderVariantsUseCatalogAndDeterministicHash) {
    auto                          images = base_images();
    d2engine::TerrainAssetCatalog catalog;
    catalog.border_variant_index.emplace(std::make_pair("NE", 1), std::vector<int>{0, 1, 2});
    catalog.border_variant_index.emplace(std::make_pair("WA", 1), std::vector<int>{0, 1, 2});
    // Exact border assets must exist in catalog for find_border_asset to resolve names.
    for (const auto v : {0, 1, 2}) {
        char buf[32] = {};
        std::snprintf(buf, sizeof(buf), "NE_%02d_%02d.PNG", 1, v);
        catalog.border_assets.push_back({"NE", 1, v, "Imgs/GrBorder.ff", buf, 64, 32});
        std::snprintf(buf, sizeof(buf), "WA_%02d_%02d.PNG", 1, v);
        catalog.border_assets.push_back({"WA", 1, v, "Imgs/GrBorder.ff", buf, 64, 32});
    }
    d2engine::AdventureTerrainSurfaceComposer composer(images, catalog);
    const auto                                make_ne_input = [](int x, int y) {
        std::vector<d2runtime::AdventureTerrainTileDescriptor> descriptors(25, descriptor(5));
        descriptors[static_cast<std::size_t>(x + y * 5)] = descriptor(5);
        descriptors[static_cast<std::size_t>(x - 1 + y * 5)] = descriptor(1);
        return input(5, 5, descriptors);
    };
    const auto make_wa_input = [](int x, int y) {
        std::vector<d2runtime::AdventureTerrainTileDescriptor> descriptors(25, descriptor(29));
        descriptors[static_cast<std::size_t>(x + y * 5)] = descriptor(29);
        descriptors[static_cast<std::size_t>(x - 1 + y * 5)] = descriptor(1);
        return input(5, 5, descriptors);
    };

    // Deterministic hash produces different variants for different tiles.
    // WA-center with land neighbor currently also yields an NE border (legacy behavior);
    // resolved_border_record reflects the first drawable operation.
    EXPECT_EQ(composer.describe_tile(make_ne_input(1, 0), 1, 0).resolved_border_record,
              "NE_01_02.PNG");
    EXPECT_EQ(composer.describe_tile(make_ne_input(4, 0), 4, 0).resolved_border_record,
              "NE_01_01.PNG");
    EXPECT_EQ(composer.describe_tile(make_wa_input(1, 0), 1, 0).resolved_border_record,
              "NE_01_02.PNG");
    EXPECT_EQ(composer.describe_tile(make_wa_input(4, 0), 4, 0).resolved_border_record,
              "NE_01_01.PNG");
    EXPECT_EQ(composer.describe_tile(make_wa_input(3, 0), 3, 0).resolved_border_record,
              "NE_01_00.PNG");
}

TEST(AdventureTerrainSurfaceComposer, WeakCenterSurroundedByStrongerTerrainUsesSuppressedShape) {
    d2engine::AdventureTerrainSurfaceComposer composer(base_images(), base_catalog());
    // Display-space NW/NE/SE/SW (grid axis offsets) are stronger HU;
    // display-space N/E/S/W (grid diagonal offsets) are same NE.
    const auto in =
        input(3, 3,
              {descriptor(5), descriptor(1), descriptor(5), descriptor(1), descriptor(5),
               descriptor(1), descriptor(5), descriptor(1), descriptor(5)});

    const auto info = composer.describe_tile(in, 1, 1);

    ASSERT_EQ(info.border_operations.size(), 1U);
    EXPECT_EQ(info.material_a_code, "NE");
    EXPECT_EQ(info.stronger_cardinal_mask, 0x00);
    EXPECT_EQ(info.stronger_diagonal_mask, 0x0F);
    EXPECT_EQ(info.resolved_record_shape, 15);
    EXPECT_EQ(info.resolved_border_record, "NE_15_00.PNG");
}

TEST(AdventureTerrainSurfaceComposer, FourCornersPlusExtraSidesStillUsesOnlyNe15) {
    d2engine::AdventureTerrainSurfaceComposer composer(base_images(), base_catalog());
    // All eight neighbors stronger; full diagonal quartet suppresses cardinal bits.
    const auto in =
        input(3, 3,
              {descriptor(1), descriptor(1), descriptor(1), descriptor(1), descriptor(5),
               descriptor(1), descriptor(1), descriptor(1), descriptor(1)});

    const auto info = composer.describe_tile(in, 1, 1);

    ASSERT_EQ(info.border_operations.size(), 1U);
    EXPECT_EQ(info.resolved_record_shape, 15);
    EXPECT_EQ(info.resolved_border_record, "NE_15_00.PNG");
}

TEST(AdventureTerrainSurfaceComposer, AllEightStrongerStillUsesNe15) {
    d2engine::AdventureTerrainSurfaceComposer composer(base_images(), base_catalog());
    const auto                                in =
        input(3, 3,
              {descriptor(1), descriptor(1), descriptor(1), descriptor(1), descriptor(5),
               descriptor(1), descriptor(1), descriptor(1), descriptor(1)});

    const auto info = composer.describe_tile(in, 1, 1);

    ASSERT_EQ(info.border_operations.size(), 1U);
    EXPECT_EQ(info.resolved_record_shape, 15);
    EXPECT_EQ(info.resolved_border_record, "NE_15_00.PNG");
}

TEST(AdventureTerrainSurfaceComposer, ThreeCornerNeighborsDoNotUseNe15) {
    d2engine::AdventureTerrainSurfaceComposer composer(base_images(), base_catalog());
    // Only three display diagonal neighbors are stronger.
    const auto in =
        input(3, 3,
              {descriptor(5), descriptor(1), descriptor(5), descriptor(1), descriptor(5),
               descriptor(5), descriptor(5), descriptor(1), descriptor(5)});

    const auto info = composer.describe_tile(in, 1, 1);
    EXPECT_NE(info.resolved_record_shape, 15);
}

TEST(AdventureTerrainSurfaceComposer, FourSidesButNoCornersDoNotUseNe15) {
    d2engine::AdventureTerrainSurfaceComposer composer(base_images(), base_catalog());
    // Only display N/E/S/W (grid diagonal offsets) are stronger.
    const auto in =
        input(3, 3,
              {descriptor(1), descriptor(5), descriptor(1), descriptor(5), descriptor(5),
               descriptor(5), descriptor(1), descriptor(5), descriptor(1)});

    const auto info = composer.describe_tile(in, 1, 1);
    EXPECT_EQ(info.resolved_record_shape, 31);
}

TEST(AdventureTerrainSurfaceComposer, WaFourCornerLandNeighborsUseWa15) {
    d2engine::AdventureTerrainSurfaceComposer composer(base_images(), base_catalog());
    // Display-space corners (grid axis offsets) are land; sides are water.
    const auto in =
        input(3, 3,
              {descriptor(29), descriptor(1), descriptor(29), descriptor(1), descriptor(29),
               descriptor(1), descriptor(29), descriptor(1), descriptor(29)});

    const auto info = composer.describe_tile(in, 1, 1);

    const auto wa15 = std::ranges::find_if(info.border_operations, [](const auto& op) {
        return op.record_shape == 15 && op.family == "WA";
    });
    EXPECT_NE(wa15, info.border_operations.end());
}

TEST(AdventureTerrainSurfaceComposer, WaFourCornersPlusExtraSidesStillUsesOnlyWa15ForTarget) {
    d2engine::AdventureTerrainSurfaceComposer composer(base_images(), base_catalog());
    const auto                                in =
        input(3, 3,
              {descriptor(1), descriptor(1), descriptor(1), descriptor(1), descriptor(29),
               descriptor(1), descriptor(1), descriptor(1), descriptor(1)});

    const auto info = composer.describe_tile(in, 1, 1);

    const auto wa15 = std::ranges::find_if(info.border_operations, [](const auto& op) {
        return op.record_shape == 15 && op.family == "WA";
    });
    EXPECT_NE(wa15, info.border_operations.end());
}

TEST(AdventureTerrainSurfaceComposer, WaAllEightLandStillUsesWa15) {
    d2engine::AdventureTerrainSurfaceComposer composer(base_images(), base_catalog());
    const auto                                in =
        input(3, 3,
              {descriptor(1), descriptor(1), descriptor(1), descriptor(1), descriptor(29),
               descriptor(1), descriptor(1), descriptor(1), descriptor(1)});

    const auto info = composer.describe_tile(in, 1, 1);

    const auto wa15 = std::ranges::find_if(info.border_operations, [](const auto& op) {
        return op.record_shape == 15 && op.family == "WA";
    });
    EXPECT_NE(wa15, info.border_operations.end());
}

TEST(AdventureTerrainSurfaceComposer, WaThreeCornerLandDoesNotUseWa15) {
    d2engine::AdventureTerrainSurfaceComposer composer(base_images(), base_catalog());
    const auto                                in =
        input(3, 3,
              {descriptor(29), descriptor(1), descriptor(29), descriptor(1), descriptor(29),
               descriptor(29), descriptor(29), descriptor(1), descriptor(29)});

    const auto info = composer.describe_tile(in, 1, 1);
    const auto wa15 = std::ranges::find_if(info.border_operations, [](const auto& op) {
        return op.record_shape == 15 && op.family == "WA";
    });
    EXPECT_EQ(wa15, info.border_operations.end());
}

TEST(AdventureTerrainSurfaceComposer, WaFourSidesWithoutCornerQuartetDoNotUseWa15) {
    d2engine::AdventureTerrainSurfaceComposer composer(base_images(), base_catalog());
    // Only display sides (grid diagonal offsets) are land; corners are water.
    const auto in =
        input(3, 3,
              {descriptor(1), descriptor(29), descriptor(1), descriptor(29), descriptor(1),
               descriptor(29), descriptor(1), descriptor(29), descriptor(1)});

    const auto info = composer.describe_tile(in, 1, 1);
    const auto wa15 = std::ranges::find_if(info.border_operations, [](const auto& op) {
        return op.record_shape == 15 && op.family == "WA";
    });
    EXPECT_EQ(wa15, info.border_operations.end());
}

TEST(AdventureTerrainSurfaceComposer, Topology3x3DecomposesCoveredExtraBits) {
    d2engine::AdventureTerrainSurfaceComposer composer(base_images(), base_catalog());
    const auto                                make_info = [&](std::initializer_list<int> indexes) {
        std::vector<d2runtime::AdventureTerrainTileDescriptor> descriptors(9, descriptor(5));
        for (const auto index : indexes) {
            descriptors[static_cast<std::size_t>(index)] = descriptor(1);
        }
        return composer.describe_tile(input(3, 3, descriptors), 1, 1);
    };

    const auto north_west_corner = make_info({0, 3, 6});
    ASSERT_EQ(north_west_corner.border_operations.size(), 2U);
    EXPECT_EQ(north_west_corner.border_operations[0].record_name, "NE_25_00.PNG");
    EXPECT_EQ(north_west_corner.border_operations[1].record_shape, 1);
    EXPECT_EQ((north_west_corner.border_operations[0].cardinal_mask |
               north_west_corner.border_operations[1].cardinal_mask),
              0x09);
    EXPECT_EQ((north_west_corner.border_operations[0].diagonal_mask |
               north_west_corner.border_operations[1].diagonal_mask),
              0x01);

    const auto north_east_corner = make_info({0, 1, 2});
    ASSERT_EQ(north_east_corner.border_operations.size(), 2U);
    EXPECT_EQ(north_east_corner.border_operations[0].record_name, "NE_19_00.PNG");
    EXPECT_EQ(north_east_corner.border_operations[1].record_shape, 2);

    const auto south_east_corner = make_info({2, 5, 8});
    ASSERT_EQ(south_east_corner.border_operations.size(), 2U);
    EXPECT_EQ(south_east_corner.border_operations[0].record_name, "NE_22_00.PNG");
    EXPECT_EQ(south_east_corner.border_operations[1].record_shape, 4);

    const auto north_cap = make_info({0, 1, 2, 3});
    ASSERT_EQ(north_cap.border_operations.size(), 2U);
    EXPECT_EQ(north_cap.border_operations[0].record_name, "NE_19_00.PNG");
    EXPECT_EQ(north_cap.border_operations[1].record_name, "NE_03_00.PNG");
}

TEST(AdventureTerrainSurfaceComposer, DiagonalDifferencesAffectTopologyKey) {
    d2engine::AdventureTerrainSurfaceComposer composer(base_images(), base_catalog());
    const auto                                north_west =
        input(3, 3,
              {descriptor(5), descriptor(1), descriptor(1), descriptor(1), descriptor(1),
               descriptor(1), descriptor(1), descriptor(1), descriptor(1)});
    const auto north_east =
        input(3, 3,
              {descriptor(1), descriptor(1), descriptor(5), descriptor(1), descriptor(1),
               descriptor(1), descriptor(1), descriptor(1), descriptor(1)});

    const auto left_key = composer.describe_tile(north_west, 1, 1).topology_key;
    const auto right_key = composer.describe_tile(north_east, 1, 1).topology_key;

    EXPECT_NE(left_key, right_key);
}

TEST(AdventureTerrainSurfaceComposer, GenericLandTransitionsUseNeFamily) {
    d2engine::AdventureTerrainSurfaceComposer composer(base_images(), base_catalog());
    const auto                                in = input(2, 1, {descriptor(3), descriptor(2)});

    const auto info = composer.describe_tile(in, 0, 0);

    EXPECT_EQ(info.material_a_code, "HE");
    EXPECT_EQ(info.material_b_code, "DW");
    EXPECT_EQ(info.composer_border_family, "NE");
    EXPECT_EQ(info.border_shape_source, "explicit_ne_topology");
    EXPECT_FALSE(info.border_synthesized);
}

TEST(AdventureTerrainSurfaceComposer, MultipleLandNeighborMaterialsCreateMultipleOperations) {
    d2engine::AdventureTerrainSurfaceComposer composer(base_images(), base_catalog());
    const auto in = input(3, 1, {descriptor(2), descriptor(5), descriptor(3)});

    const auto info = composer.describe_tile(in, 1, 0);

    ASSERT_EQ(info.border_operations.size(), 2U);
    EXPECT_EQ(info.border_operations[0].family, "NE");
    EXPECT_EQ(info.border_operations[0].record_shape, 1);
    EXPECT_EQ(info.border_operations[1].family, "NE");
    EXPECT_EQ(info.border_operations[1].record_shape, 4);
}

TEST(AdventureTerrainSurfaceComposer, OutOfBoundsAloneIsNotDrawableTransition) {
    d2engine::AdventureTerrainSurfaceComposer composer(base_images(), base_catalog());
    const auto                                in = input(1, 1, {descriptor(5)});

    const auto info = composer.describe_tile(in, 0, 0);

    EXPECT_EQ(info.composer_border_kind, d2runtime::AdventureTerrainBorderKind::None);
    EXPECT_EQ(info.composer_border_record, "");
}

// ── New tests for full-map material-layer renderer ──────────────────────────

d2engine::AdventureTerrainSurfaceImageMap ne_layer_test_images() {
    auto images = base_images();
    // Override all NE and HU variants to deterministic colors for coverage tests
    for (int v = 0; v <= 3; ++v) {
        // NOLINTNEXTLINE(misc-const-correctness)
        char buf[16]{};
        std::snprintf(buf, sizeof(buf), "NE_%02d.PNG", v);
        images[key(buf)] = solid(0, 200, 0);
        // NOLINTNEXTLINE(misc-const-correctness)
        std::snprintf(buf, sizeof(buf), "HU_%02d.PNG", v);
        images[key(buf)] = solid(200, 0, 0);
    }
    for (int shape = 1; shape <= 31; ++shape) {
        if (shape == 16)
            continue;
        // NOLINTNEXTLINE(misc-const-correctness)
        char record[16]{};
        std::snprintf(record, sizeof(record), "%02d_00.PNG", shape);
        images[border_key("NE_" + std::string(record))] = mask(0);
    }
    for (const auto shape : {1, 2, 4, 8, 11, 14}) {
        char record[16]{};
        std::snprintf(record, sizeof(record), "%02d_01.PNG", shape);
        images[border_key("NE_" + std::string(record))] = mask(0);
    }
    return images;
}

TEST(AdventureTerrainSurfaceComposer, FullMapWaSpriteBelongsToWaLayer) {
    auto images = base_images();
    images[key("WA_00.PNG")] = solid(0, 0, 200);
    images[key("HU_00.PNG")] = solid(200, 0, 0);
    for (int shape = 1; shape <= 31; ++shape) {
        if (shape == 16)
            continue;
        // NOLINTNEXTLINE(misc-const-correctness)
        char record[16]{};
        std::snprintf(record, sizeof(record), "%02d_00.PNG", shape);
        images[border_key("WA_" + std::string(record))] =
            border_with_pixel_at_center(0, 200, 0, 0, 0, 0);
        images[border_key("NE_" + std::string(record))] = mask(0);
    }
    for (const auto shape : {1, 2, 4, 8, 11, 14}) {
        char record[16]{};
        std::snprintf(record, sizeof(record), "%02d_01.PNG", shape);
        images[border_key("NE_" + std::string(record))] = mask(0);
    }
    for (const auto shape : {1, 2, 3, 4, 5, 8, 9, 10, 12}) {
        for (const auto variant : {1, 2}) {
            char record[16]{};
            std::snprintf(record, sizeof(record), "%02d_%02d.PNG", shape, variant);
            images[border_key("WA_" + std::string(record))] =
                border_with_pixel_at_center(0, 200, 0, 0, 0, 0);
        }
    }

    d2engine::AdventureTerrainSurfaceComposer composer(images, base_catalog());
    const auto                                in = input(2, 1, {descriptor(1), descriptor(29)});

    // WA tile center: WA shoreline (green) must appear when WA base layer exists
    const auto  full = crop_tile_for_debug(composer, in, 1, 0);
    const auto& px = pixel(full, 32, 16);
    EXPECT_EQ(px.r, 0);
    EXPECT_EQ(px.g, 200);

    // Without base layer: WA shoreline has no destination layer -> must not appear
    const auto border_only =
        crop_tile_for_debug(composer, in, 1, 0, {.include_base = false, .include_borders = true});
    const auto& bp = pixel(border_only, 32, 16);
    EXPECT_EQ(bp.a, 0);
}

TEST(AdventureTerrainSurfaceComposer, FullMapLandNeCompositesAboveWa) {
    const auto                                images = water_test_images();
    d2engine::AdventureTerrainSurfaceComposer composer(images, base_catalog());
    const auto                                in = input(2, 1, {descriptor(1), descriptor(29)});

    const auto surface = crop_tile_for_debug(composer, in, 1, 0);

    const auto& px = pixel(surface, 32, 16);
    EXPECT_EQ(px.r, 200);
    EXPECT_EQ(px.g, 0);
    EXPECT_EQ(px.b, 0);
}

TEST(AdventureTerrainSurfaceComposer, NeCoverageTreatsTransparentAndMagentaAsZero) {
    auto images = ne_layer_test_images();

    // Custom NE mask: (30,16)=transparent-white, (34,16)=magenta,
    // (32,16)=white(full invade), (36,16)=black, (38,16)=gray
    const auto ne_mask = []() -> d2res::RgbaBuffer {
        d2res::RgbaBuffer buf;
        buf.width = 64;
        buf.height = 32;
        buf.rgba.assign(static_cast<std::size_t>(buf.width) * buf.height * 4U, 0);
        for (std::uint32_t y = 0; y < buf.height; ++y) {
            for (std::uint32_t x = 0; x < buf.width; ++x) {
                auto* px =
                    buf.rgba.data() +
                    (((static_cast<std::size_t>(y) * buf.width) + static_cast<std::size_t>(x)) *
                     4U);
                if (x == 30 && y == 16) {
                    px[0] = 255;
                    px[1] = 255;
                    px[2] = 255;
                    px[3] = 0;
                } else if (x == 34 && y == 16) {
                    px[0] = 255;
                    px[1] = 0;
                    px[2] = 255;
                    px[3] = 255;
                } else if (x == 32 && y == 16) {
                    px[0] = 255;
                    px[1] = 255;
                    px[2] = 255;
                    px[3] = 255;
                } else if (x == 38 && y == 16) {
                    px[0] = 128;
                    px[1] = 128;
                    px[2] = 128;
                    px[3] = 255;
                } else {
                    px[0] = 0;
                    px[1] = 0;
                    px[2] = 0;
                    px[3] = 255;
                }
            }
        }
        return buf;
    }();
    images[border_key("NE_04_00.PNG")] = ne_mask;
    images[border_key("NE_04_01.PNG")] = ne_mask;

    d2engine::AdventureTerrainSurfaceComposer composer(images, base_catalog());
    const auto                                in = input(2, 1, {descriptor(5), descriptor(1)});

    const auto surface = crop_tile_for_debug(composer, in, 0, 0);

    // P1: alpha=0 with white RGB -> coverage=0 -> NE green shows
    {
        const auto& p1 = pixel(surface, 30, 16);
        EXPECT_EQ(p1.g, 200);
        EXPECT_EQ(p1.r, 0);
    }
    // P2: magenta -> coverage=0 -> NE green shows
    {
        const auto& p2 = pixel(surface, 34, 16);
        EXPECT_EQ(p2.g, 200);
        EXPECT_EQ(p2.r, 0);
    }
    // P3: white -> full invade -> HU red shows
    {
        const auto& p3 = pixel(surface, 32, 16);
        EXPECT_EQ(p3.r, 200);
        EXPECT_EQ(p3.g, 0);
    }
    // P4: black (0,0,0) -> zero coverage -> NE green shows
    {
        const auto& p4 = pixel(surface, 36, 16);
        EXPECT_EQ(p4.g, 200);
        EXPECT_EQ(p4.r, 0);
    }
    // P5: gray (128) -> partial invade -> blended
    {
        const auto& p5 = pixel(surface, 38, 16);
        EXPECT_NEAR(p5.r, (200.0 * 128.0) / 255.0, 2);
        EXPECT_NE(p5.g, 0);
    }
}

TEST(AdventureTerrainSurfaceComposer, WaWhiteKeyPreparedOnce) {
    auto images = base_images();
    images[key("WA_00.PNG")] = solid(0, 0, 200);
    images[key("HU_00.PNG")] = solid(200, 0, 0);

    // WA border sprite: (32,16)=white, (30,16)=green, (34,16)=dirty-magenta
    const auto wa_border = []() -> d2res::RgbaBuffer {
        d2res::RgbaBuffer buf;
        buf.width = 64;
        buf.height = 32;
        buf.rgba.assign(static_cast<std::size_t>(buf.width) * buf.height * 4U, 0);
        for (std::uint32_t y = 0; y < buf.height; ++y) {
            for (std::uint32_t x = 0; x < buf.width; ++x) {
                auto* px =
                    buf.rgba.data() +
                    (((static_cast<std::size_t>(y) * buf.width) + static_cast<std::size_t>(x)) *
                     4U);
                if (x == 32 && y == 16) {
                    px[0] = 255;
                    px[1] = 255;
                    px[2] = 255;
                    px[3] = 255;
                } else if (x == 30 && y == 16) {
                    px[0] = 0;
                    px[1] = 200;
                    px[2] = 0;
                    px[3] = 255;
                } else if (x == 34 && y == 16) {
                    px[0] = 252;
                    px[1] = 3;
                    px[2] = 252;
                    px[3] = 255;
                } else {
                    px[0] = 255;
                    px[1] = 0;
                    px[2] = 255;
                    px[3] = 255;
                }
            }
        }
        return buf;
    }();

    auto key_fmt = [](const std::string& prefix, const char* fmt, auto... args) -> std::string {
        char buf[16]{};
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
        std::snprintf(buf, sizeof(buf), fmt, args...);
#pragma clang diagnostic pop
        return prefix + buf;
    };

    for (int shape = 1; shape <= 31; ++shape) {
        if (shape == 16)
            continue;
        images[border_key(key_fmt("WA_", "%02d_00.PNG", shape))] = wa_border;
        images[border_key(key_fmt("NE_", "%02d_00.PNG", shape))] = mask(0);
    }
    for (const auto shape : {1, 2, 4, 8, 11, 14}) {
        images[border_key(key_fmt("NE_", "%02d_01.PNG", shape))] = mask(0);
    }
    for (const auto shape : {1, 2, 3, 4, 5, 8, 9, 10, 12}) {
        for (const auto variant : {1, 2}) {
            images[border_key(key_fmt("WA_", "%02d_%02d.PNG", shape, variant))] = wa_border;
        }
    }

    d2engine::AdventureTerrainSurfaceComposer composer(images, base_catalog());
    const auto                                in = input(2, 1, {descriptor(1), descriptor(29)});

    const auto surface = crop_tile_for_debug(composer, in, 1, 0);

    // Green pixel (30,16) must render
    {
        const auto& px = pixel(surface, 30, 16);
        EXPECT_EQ(px.r, 0);
        EXPECT_EQ(px.g, 200);
        EXPECT_EQ(px.b, 0);
    }

    // White pixel (32,16) must be transparent -> WA ground blue shows
    {
        const auto& px = pixel(surface, 32, 16);
        EXPECT_EQ(px.r, 0);
        EXPECT_EQ(px.g, 0);
        EXPECT_EQ(px.b, 200);
    }

    // Dirty magenta pixel (34,16) with g=3 is not a color key pixel (threshold g<=2)
    // It should render as the WA sprite's dirty magenta, not transparent
    {
        const auto& px = pixel(surface, 34, 16);
        EXPECT_EQ(px.r, 252);
        EXPECT_EQ(px.g, 3);
        EXPECT_EQ(px.b, 252);
    }
}

TEST(AdventureTerrainSurfaceComposer, GroundVariantsComeOnlyFromCatalog) {
    d2engine::TerrainAssetCatalog catalog;
    catalog.ground_variant_index["HU"] = {0, 3};
    auto                                      images = base_images();
    d2engine::AdventureTerrainSurfaceComposer composer(images, catalog);
    const auto surface = crop_tile_for_debug(composer, input(1, 1, {descriptor(0x00000001)}), 0, 0);
    // HU_00 and HU_03 exist in base_images; HU_01/HU_02 should never be requested
    // because catalog only lists [0,3]. Pixel color should be either variant 0 (r=10)
    // or variant 3 (r=40), never variant 1 (r=20) or 2 (r=30).
    std::set<int> seen_r;
    for (const auto& px : surface.pixels) {
        if (px.a > 0) {
            seen_r.insert(px.r);
        }
    }
    EXPECT_TRUE(seen_r.count(10) > 0 || seen_r.count(40) > 0);
    EXPECT_EQ(seen_r.count(20), 0);
    EXPECT_EQ(seen_r.count(30), 0);
}

TEST(AdventureTerrainSurfaceComposer, GroundVariantIndexSupportsNonContiguousVariants) {
    d2engine::TerrainAssetCatalog catalog;
    catalog.ground_variant_index["HU"] = {0, 3, 7};
    auto images = base_images();
    // Add synthetic HU_07.PNG so variant 7 is loadable
    images.emplace(key("HU_07.PNG"), solid(55, 0, 0));
    d2engine::AdventureTerrainSurfaceComposer composer(images, catalog);
    const auto surface = crop_tile_for_debug(composer, input(1, 1, {descriptor(0x00000001)}), 0, 0);
    std::set<int> seen_r;
    for (const auto& px : surface.pixels) {
        if (px.a > 0) {
            seen_r.insert(px.r);
        }
    }
    // Only 0,3,7 should ever be selected; 1,2,4,5,6 never appear
    EXPECT_EQ(seen_r.count(20), 0); // variant 1
    EXPECT_EQ(seen_r.count(30), 0); // variant 2
    EXPECT_EQ(seen_r.count(10) + seen_r.count(40) + seen_r.count(55), 1);
}

TEST(AdventureTerrainSurfaceComposer, MissingBorderShapeDoesNotFabricate00) {
    d2engine::TerrainAssetCatalog catalog;
    // Catalog has no WA shape 12 at all
    d2engine::AdventureTerrainSurfaceComposer composer(base_images(), catalog);
    const auto shore = composer.describe_tile(input(2, 1, {descriptor(1), descriptor(29)}), 1, 0);
    // WA border operations for missing shapes should be skipped entirely
    for (const auto& op : shore.border_operations) {
        EXPECT_NE(op.record_name, "WA_12_00.PNG");
    }
}

TEST(AdventureTerrainSurfaceComposer, GroundFieldUsesRealSharedHandle) {
    d2engine::AdventureTerrainSurfaceComposer composer(base_images(), base_catalog());
    const auto                                in = input(1, 1, {descriptor(0x00000001)});
    const auto                                prep = composer.prepare_full_map(
        in, {.tile_width = 64, .tile_height = 32, .include_base = true, .include_borders = false});
    ASSERT_TRUE(prep.ground_fields.contains("HU"));
    const auto& field = prep.ground_fields.at("HU");
    // At least one buffer must be a real shared_ptr holding actual RGBA data
    ASSERT_FALSE(field.buffers.empty());
    ASSERT_TRUE(field.buffers[0] != nullptr);
    EXPECT_FALSE(field.buffers[0]->rgba.empty());
}

TEST(TerrainAssetCatalog, BLExcludedFromGroundCodeList) {
    // BL must not be accepted as a normal ground terrain code
    EXPECT_FALSE(d2engine::parse_ground_texture_record_name("BL_00.PNG").has_value());
}

TEST(AdventureTerrainSurfaceComposer, BLExcludedFromDominanceRender) {
    // Even if a synthetic BL ground image exists, BL must not appear in render output
    d2engine::TerrainAssetCatalog catalog;
    catalog.ground_variant_index["BL"] = {0};
    auto images = base_images();
    images.emplace(key("BL_01.PNG"), solid(99, 99, 99));
    d2engine::AdventureTerrainSurfaceComposer composer(images, catalog);
    const auto surface = crop_tile_for_debug(composer, input(1, 1, {descriptor(0x00000000)}), 0, 0);
    // BL tile center pixel: should not be magenta fallback (255,0,255) because
    // BL is not a renderable material. But since no coverage_map is created for BL
    // in normal dominance order (BL removed), the canvas stays transparent.
    const auto& px = pixel(surface, 32, 16);
    EXPECT_EQ(px.a, 0);
}

TEST(AdventureTerrainSurfaceComposer, GroundFieldClippedToBounds) {
    d2engine::AdventureTerrainSurfaceComposer composer(base_images(), base_catalog());
    // Small 3x3 HU island on large 5x5 WA map
    std::vector<d2runtime::AdventureTerrainTileDescriptor> descriptors(25, descriptor(29));
    descriptors[6] = descriptor(1);  // (1,1) = HU
    descriptors[7] = descriptor(1);  // (2,1) = HU
    descriptors[8] = descriptor(1);  // (3,1) = HU
    descriptors[11] = descriptor(1); // (1,2) = HU
    descriptors[12] = descriptor(1); // (2,2) = HU
    descriptors[13] = descriptor(1); // (3,2) = HU
    descriptors[16] = descriptor(1); // (1,3) = HU
    descriptors[17] = descriptor(1); // (2,3) = HU
    descriptors[18] = descriptor(1); // (3,3) = HU
    const auto prep = composer.prepare_full_map(
        input(5, 5, descriptors),
        {.tile_width = 64, .tile_height = 32, .include_base = true, .include_borders = false});
    ASSERT_TRUE(prep.ground_fields.contains("HU"));
    const auto& field = prep.ground_fields.at("HU");
    // HU field grid is limited to the 3x3 island bbox. Without clipping it would tile
    // the entire canvas. The grid cell count must be drastically smaller.
    const auto hu_cell_count =
        static_cast<std::size_t>(field.columns) * static_cast<std::size_t>(field.rows);
    EXPECT_LT(hu_cell_count, 50U);
}

TEST(AdventureTerrainVariantHash, CoordinateKeyUniqueness) {
    std::set<std::uint64_t> keys;
    for (int y = -128; y < 128; ++y) {
        for (int x = -128; x < 128; ++x) {
            const auto k = d2engine::coordinate_key(x, y);
            EXPECT_EQ(keys.count(k), 0U) << "duplicate key for (" << x << "," << y << ")";
            keys.insert(k);
        }
    }
}

TEST(AdventureTerrainVariantHash, HashGoldenValues) {
    // Known hash outputs for specific coordinates, verified against the
    // SplitMix64-style mixer constants.
    EXPECT_EQ(d2engine::terrain_variant_hash(0, 0), 0xE220A8397B1DCDAFULL);
    EXPECT_EQ(d2engine::terrain_variant_hash(1, 0), 0xC42C5A1AA3820138ULL);
    EXPECT_EQ(d2engine::terrain_variant_hash(0, 1), 0x910A2DEC89025CC1ULL);
    EXPECT_EQ(d2engine::terrain_variant_hash(1, 1), 0x204391A6FD59956FULL);
    EXPECT_EQ(d2engine::terrain_variant_hash(-1, 0), 0x219FC13D6BC5B015ULL);
    EXPECT_EQ(d2engine::terrain_variant_hash(0, -1), 0x73B13BA2AFF181C0ULL);
    EXPECT_EQ(d2engine::terrain_variant_hash(-1, -1), 0xE4D971771B652C20ULL);
    EXPECT_EQ(d2engine::terrain_variant_hash(123, 456), 0x36EFD072E6A495DDULL);
}

TEST(AdventureTerrainVariantHash, BucketDistributionOn256x256) {
    std::array<std::size_t, 4> counts{};
    for (int y = 0; y < 256; ++y) {
        for (int x = 0; x < 256; ++x) {
            const auto bucket = d2engine::terrain_variant_bucket(x, y);
            ASSERT_LT(bucket, 4U);
            ++counts[bucket];
        }
    }
    const auto total = static_cast<double>(256 * 256);
    for (std::size_t i = 0; i < 4; ++i) {
        const auto ratio = static_cast<double>(counts[i]) / total;
        EXPECT_GT(ratio, 0.22) << "bucket " << i << " too low";
        EXPECT_LT(ratio, 0.28) << "bucket " << i << " too high";
    }
}

TEST(AdventureTerrainVariantHash, FixedLineCoverage) {
    // Scanning a fixed row (y=42) across 1024 x-values must hit all 4 buckets.
    std::array<bool, 4> seen{};
    for (int x = 0; x < 1024; ++x) {
        const auto bucket = d2engine::terrain_variant_bucket(x, 42);
        ASSERT_LT(bucket, 4U);
        seen[bucket] = true;
    }
    for (std::size_t i = 0; i < 4; ++i) {
        EXPECT_TRUE(seen[i]) << "bucket " << i << " missing on row y=42";
    }
}

TEST(AdventureTerrainVariantHash, VariantCountReachability) {
    // N=2: only bucket 3 maps to 1; all others map to 0.
    EXPECT_EQ(d2engine::select_border_variant_index_from_bucket(0, 2), 0U);
    EXPECT_EQ(d2engine::select_border_variant_index_from_bucket(1, 2), 0U);
    EXPECT_EQ(d2engine::select_border_variant_index_from_bucket(2, 2), 0U);
    EXPECT_EQ(d2engine::select_border_variant_index_from_bucket(3, 2), 1U);

    // N=3: [0,0,1,2]
    EXPECT_EQ(d2engine::select_border_variant_index_from_bucket(0, 3), 0U);
    EXPECT_EQ(d2engine::select_border_variant_index_from_bucket(1, 3), 0U);
    EXPECT_EQ(d2engine::select_border_variant_index_from_bucket(2, 3), 1U);
    EXPECT_EQ(d2engine::select_border_variant_index_from_bucket(3, 3), 2U);

    // N=4: direct mapping
    EXPECT_EQ(d2engine::select_border_variant_index_from_bucket(0, 4), 0U);
    EXPECT_EQ(d2engine::select_border_variant_index_from_bucket(1, 4), 1U);
    EXPECT_EQ(d2engine::select_border_variant_index_from_bucket(2, 4), 2U);
    EXPECT_EQ(d2engine::select_border_variant_index_from_bucket(3, 4), 3U);

    // N=1: everything maps to 0
    EXPECT_EQ(d2engine::select_border_variant_index_from_bucket(0, 1), 0U);
    EXPECT_EQ(d2engine::select_border_variant_index_from_bucket(3, 1), 0U);

    // N=0: throws invalid_argument
    EXPECT_THROW(d2engine::select_border_variant_index_from_bucket(0, 0), std::invalid_argument);

    // N>4: throws logic_error
    EXPECT_THROW(d2engine::select_border_variant_index_from_bucket(2, 5), std::logic_error);
}

TEST(AdventureTerrainVariantHash, DistributionTwoVariantsWithinBounds) {
    std::array<std::size_t, 2> counts{};
    for (int y = 0; y < 256; ++y) {
        for (int x = 0; x < 256; ++x) {
            const auto bucket = d2engine::terrain_variant_bucket(x, y);
            const auto idx = d2engine::select_border_variant_index_from_bucket(bucket, 2);
            ASSERT_LT(idx, 2U);
            ++counts[idx];
        }
    }
    const auto total = static_cast<double>(256 * 256);
    // ~75/25 split => variant 0 ~ 0.75, variant 1 ~ 0.25
    EXPECT_GT(static_cast<double>(counts[0]) / total, 0.72);
    EXPECT_LT(static_cast<double>(counts[0]) / total, 0.78);
    EXPECT_GT(static_cast<double>(counts[1]) / total, 0.22);
    EXPECT_LT(static_cast<double>(counts[1]) / total, 0.28);
}

TEST(AdventureTerrainVariantHash, DistributionThreeVariantsWithinBounds) {
    std::array<std::size_t, 3> counts{};
    for (int y = 0; y < 256; ++y) {
        for (int x = 0; x < 256; ++x) {
            const auto bucket = d2engine::terrain_variant_bucket(x, y);
            const auto idx = d2engine::select_border_variant_index_from_bucket(bucket, 3);
            ASSERT_LT(idx, 3U);
            ++counts[idx];
        }
    }
    const auto total = static_cast<double>(256 * 256);
    // ~50/25/25 split
    EXPECT_GT(static_cast<double>(counts[0]) / total, 0.47);
    EXPECT_LT(static_cast<double>(counts[0]) / total, 0.53);
    EXPECT_GT(static_cast<double>(counts[1]) / total, 0.22);
    EXPECT_LT(static_cast<double>(counts[1]) / total, 0.28);
    EXPECT_GT(static_cast<double>(counts[2]) / total, 0.22);
    EXPECT_LT(static_cast<double>(counts[2]) / total, 0.28);
}

TEST(AdventureTerrainVariantHash, DistributionFourVariantsWithinBounds) {
    std::array<std::size_t, 4> counts{};
    for (int y = 0; y < 256; ++y) {
        for (int x = 0; x < 256; ++x) {
            const auto bucket = d2engine::terrain_variant_bucket(x, y);
            const auto idx = d2engine::select_border_variant_index_from_bucket(bucket, 4);
            ASSERT_LT(idx, 4U);
            ++counts[idx];
        }
    }
    const auto total = static_cast<double>(256 * 256);
    // ~25/25/25/25 split
    for (std::size_t i = 0; i < 4; ++i) {
        EXPECT_GT(static_cast<double>(counts[i]) / total, 0.22) << "variant " << i << " too low";
        EXPECT_LT(static_cast<double>(counts[i]) / total, 0.28) << "variant " << i << " too high";
    }
}

TEST(AdventureTerrainVariantHash, FixedColumnCoverage) {
    // Scanning a fixed column (x=7) across 1024 y-values must hit all 4 buckets.
    std::array<bool, 4> seen{};
    for (int y = 0; y < 1024; ++y) {
        const auto bucket = d2engine::terrain_variant_bucket(7, y);
        ASSERT_LT(bucket, 4U);
        seen[bucket] = true;
    }
    for (std::size_t i = 0; i < 4; ++i) {
        EXPECT_TRUE(seen[i]) << "bucket " << i << " missing on column x=7";
    }
}

TEST(AdventureTerrainSurfaceComposer, TwoExactBorderRecordsReachable) {
    // With a catalog that only has NE_01_00.PNG and NE_01_01.PNG, both must be
    // reachable by deterministic hash across interior coordinates on a safe map.
    auto                          images = base_images();
    d2engine::TerrainAssetCatalog catalog;
    catalog.border_variant_index.emplace(std::make_pair("NE", 1), std::vector<int>{0, 1});
    catalog.border_assets.push_back({"NE", 1, 0, "Imgs/GrBorder.ff", "NE_01_00.PNG", 64, 32});
    catalog.border_assets.push_back({"NE", 1, 1, "Imgs/GrBorder.ff", "NE_01_01.PNG", 64, 32});
    d2engine::AdventureTerrainSurfaceComposer composer(images, catalog);

    const auto in = [](int x, int y) {
        std::vector<d2runtime::AdventureTerrainTileDescriptor> descriptors(18 * 18, descriptor(5));
        descriptors[static_cast<std::size_t>((x - 1) + y * 18)] = descriptor(1);
        return input(18, 18, descriptors);
    };

    std::set<std::string> seen;
    for (int y = 1; y < 17; ++y) {
        for (int x = 1; x < 17; ++x) {
            const auto rec = composer.describe_tile(in(x, y), x, y).resolved_border_record;
            if (!rec.empty()) {
                seen.insert(rec);
            }
        }
    }
    EXPECT_TRUE(seen.count("NE_01_00.PNG")) << "NE_01_00.PNG not reachable";
    EXPECT_TRUE(seen.count("NE_01_01.PNG")) << "NE_01_01.PNG not reachable";
}

TEST(AdventureTerrainSurfaceComposer, WaFiveExactBorderRecordsReachable) {
    // With a catalog that has WA_05_00.PNG / WA_05_01.PNG / WA_05_02.PNG,
    // all three must be reachable by deterministic hash via real WA shape-5 topology.
    // Shape 5 mask 0x5F requires:
    //   grid-diagonal neighbors (cardinal bits): (-1,-1), (1,-1), (1,1), (-1,1)
    //   grid-axis neighbors     (diagonal bits): (-1,0), (1,0)
    auto                          images = base_images();
    d2engine::TerrainAssetCatalog catalog;
    catalog.border_variant_index.emplace(std::make_pair("WA", 5), std::vector<int>{0, 1, 2});
    catalog.border_assets.push_back({"WA", 5, 0, "Imgs/GrBorder.ff", "WA_05_00.PNG", 64, 32});
    catalog.border_assets.push_back({"WA", 5, 1, "Imgs/GrBorder.ff", "WA_05_01.PNG", 64, 32});
    catalog.border_assets.push_back({"WA", 5, 2, "Imgs/GrBorder.ff", "WA_05_02.PNG", 64, 32});
    d2engine::AdventureTerrainSurfaceComposer composer(images, catalog);

    const auto in = [](int x, int y) {
        std::vector<d2runtime::AdventureTerrainTileDescriptor> descriptors(18 * 18, descriptor(29));
        descriptors[static_cast<std::size_t>((x - 1) + (y - 1) * 18)] = descriptor(1);
        descriptors[static_cast<std::size_t>((x + 1) + (y - 1) * 18)] = descriptor(1);
        descriptors[static_cast<std::size_t>((x + 1) + (y + 1) * 18)] = descriptor(1);
        descriptors[static_cast<std::size_t>((x - 1) + (y + 1) * 18)] = descriptor(1);
        descriptors[static_cast<std::size_t>((x - 1) + y * 18)] = descriptor(1);
        descriptors[static_cast<std::size_t>((x + 1) + y * 18)] = descriptor(1);
        return input(18, 18, descriptors);
    };

    std::set<std::string> seen;
    for (int y = 1; y < 17; ++y) {
        for (int x = 1; x < 17; ++x) {
            const auto info = composer.describe_tile(in(x, y), x, y);
            for (const auto& op : info.border_operations) {
                if (op.family == "WA" && op.record_shape == 5 && !op.record_name.empty()) {
                    seen.insert(op.record_name);
                }
            }
        }
    }
    EXPECT_TRUE(seen.count("WA_05_00.PNG")) << "WA_05_00.PNG not reachable";
    EXPECT_TRUE(seen.count("WA_05_01.PNG")) << "WA_05_01.PNG not reachable";
    EXPECT_TRUE(seen.count("WA_05_02.PNG")) << "WA_05_02.PNG not reachable";
}

// ── WA/land render-ordering regression tests ─────────────────────────────────
//
// In the old render order (all base → WA sprites → NE), WA border sprites could
// overwrite land base pixels at the tile boundary. The correct order is:
//   WA base → WA sprites → land base → NE transitions (dominance order).
// Land base must always win over WA sprites at overlapping pixel positions.

TEST(AdventureTerrainSurfaceComposer, WaAndLandCommandsAreSeparatedInPrepare) {
    auto images = base_images();
    images[key("WA_00.PNG")] = solid(0, 0, 200);
    images[key("HU_00.PNG")] = solid(200, 0, 0);
    d2engine::AdventureTerrainSurfaceComposer composer(images, base_catalog());

    // 2×1 map: land(0,0) = HU, WA(1,0)
    const auto in = input(2, 1, {descriptor(1), descriptor(29)});
    const auto prep = composer.prepare_full_map(
        in, {.tile_width = 64, .tile_height = 32, .include_base = true, .include_borders = true});

    // WA base: at least one WA tile present
    EXPECT_FALSE(prep.wa_base_commands.empty());

    // WA sprite: shoreline border present at the WA/land boundary
    EXPECT_FALSE(prep.wa_sprite_placements.empty());

    // Land base: at least one land tile present
    EXPECT_FALSE(prep.land_base_commands.empty());

    // NE transition: material-index 2+ should have commands (NE composites at WA→land boundary)
    bool has_ne = false;
    for (std::size_t mi = 2; mi < prep.ne_transition_commands_by_material.size(); ++mi) {
        if (!prep.ne_transition_commands_by_material[mi].empty()) {
            has_ne = true;
            break;
        }
    }
    EXPECT_TRUE(has_ne);

    // Sanity: total base commands = wa + land
    EXPECT_EQ(prep.wa_base_commands.size() + prep.land_base_commands.size(),
              prep.placements.size());
}

TEST(AdventureTerrainSurfaceComposer, WaSpriteRendersAtTileCenterAndNeDependsOnWaBase) {
    // Adjacent WA/land diamonds only touch at the edge — they never strictly
    // overlap.  This test verifies that the WA border sprite renders at the WA
    // tile centre, and that its visibility depends on the WA base layer
    // (color-key overlay semantics: no destination → no visible sprite).
    auto images = base_images();
    images[key("WA_00.PNG")] = solid(0, 0, 200);
    images[key("HU_00.PNG")] = solid(200, 0, 0);

    // WA border sprite with a green centre pixel (32,16) and a red pixel at
    // (63,0) — the red pixel is outside both WA and HU diamonds.
    const auto wa_sprite = []() {
        d2res::RgbaBuffer buf;
        buf.width = 64;
        buf.height = 32;
        buf.rgba.assign(static_cast<std::size_t>(buf.width) * buf.height * 4U, 255);
        auto px_at = [&](int x, int y) -> std::uint8_t* {
            return buf.rgba.data() +
                   ((static_cast<std::size_t>(y) * 64U + static_cast<std::size_t>(x)) * 4U);
        };
        // Centre: green (within WA diamond)
        std::fill_n(px_at(32, 16), 4, std::uint8_t{0});
        px_at(32, 16)[1] = 200;
        px_at(32, 16)[3] = 255;
        // Corner pixel (63,0): red, outside both WA and HU diamonds
        std::fill_n(px_at(63, 0), 4, std::uint8_t{0});
        px_at(63, 0)[0] = 255;
        px_at(63, 0)[3] = 255;
        return buf;
    }();

    for (int shape = 1; shape <= 31; ++shape) {
        if (shape == 16)
            continue;
        char record[16]{};
        std::snprintf(record, sizeof(record), "%02d_00.PNG", shape);
        images[border_key("WA_" + std::string(record))] = wa_sprite;
    }
    for (const auto shape : {1, 2, 3, 4, 5, 8, 9, 10, 12}) {
        for (const auto variant : {1, 2}) {
            char record[16]{};
            std::snprintf(record, sizeof(record), "%02d_%02d.PNG", shape, variant);
            images[border_key("WA_" + std::string(record))] = wa_sprite;
        }
    }
    for (int shape = 1; shape <= 31; ++shape) {
        if (shape == 16)
            continue;
        char record[16]{};
        std::snprintf(record, sizeof(record), "%02d_00.PNG", shape);
        images[border_key("NE_" + std::string(record))] = mask(0);
    }
    for (const auto shape : {1, 2, 4, 8, 11, 14}) {
        char record[16]{};
        std::snprintf(record, sizeof(record), "%02d_01.PNG", shape);
        images[border_key("NE_" + std::string(record))] = mask(0);
    }

    d2engine::AdventureTerrainSurfaceComposer composer(images, base_catalog());
    const auto                                in = input(2, 1, {descriptor(1), descriptor(29)});

    // With base layer: WA sprite centre pixel is visible (green)
    {
        const auto  surface = crop_tile_for_debug(composer, in, 1, 0);
        const auto& px = pixel(surface, 32, 16);
        EXPECT_EQ(px.r, 0);
        EXPECT_EQ(px.g, 200);
        EXPECT_EQ(px.b, 0);
    }

    // Without base layer: WA sprite has no destination (color-key overlay
    // requires the WA base as destination).  Centre pixel must be transparent.
    {
        const auto  surface = crop_tile_for_debug(composer, in, 1, 0,
                                                  {.include_base = false, .include_borders = true});
        const auto& px = pixel(surface, 32, 16);
        EXPECT_EQ(px.a, 0);
    }

    // Corner pixel (63,0) — outside both WA diamond (|63-32|+2*|0-16|=63>32)
    // and HU diamond (|63+32-32|+2*|16-16|=63>32).  WA sprite is not
    // diamond-clipped, so it renders wherever its opaque pixels lie.
    {
        const auto  surface = crop_tile_for_debug(composer, in, 1, 0);
        const auto& px = pixel(surface, 63, 0);
        EXPECT_EQ(px.r, 255);
        EXPECT_EQ(px.g, 0);
        EXPECT_EQ(px.b, 0);
    }
}

// ── Ground-field grid bounds regression test ─────────────────────────────────
//
// build_ground_fields computes the patch grid bbox from tile placements.
// bbox.max_x/max_y are exclusive (half-open), so the last covered pixel is
// max − 1.  The old code used  floor_div(max + patch_w − 1, patch_w)  which
// over-counted by one patch.  Fixed to  floor_div(max − 1, patch_w).

TEST(AdventureTerrainSurfaceComposer, GroundFieldSingleTileIsOnePatch) {
    d2engine::AdventureTerrainSurfaceComposer composer(base_images(), base_catalog());
    const auto                                prep = composer.prepare_full_map(
        input(1, 1, {descriptor(0x00000001)}),
        {.tile_width = 64, .tile_height = 32, .include_base = true, .include_borders = false});

    ASSERT_TRUE(prep.ground_fields.contains("HU"));
    const auto& field = prep.ground_fields.at("HU");

    // Ground textures are 64×32.  A single tile at (0,0) with diamond 64×32:
    //   bbox  [0, 64) × [0, 32)
    //   → first_patch_x=0, last_patch_x = floor_div(63,64) = 0  →  1 column
    //   → first_patch_y=0, last_patch_y = floor_div(31,32) = 0  →  1 row
    EXPECT_EQ(field.columns, 1) << "single tile needs exactly 1 column of 64-wide patches";
    EXPECT_EQ(field.rows, 1) << "single tile needs exactly 1 row of 32-tall patches";
}

TEST(AdventureTerrainSurfaceComposer, GroundFieldTwoAdjacentTilesHasCorrectPatchCount) {
    d2engine::AdventureTerrainSurfaceComposer composer(base_images(), base_catalog());
    // 2×1 HU tiles at (0,0) and (1,0)
    const auto prep = composer.prepare_full_map(
        input(2, 1, {descriptor(0x00000001), descriptor(0x00000001)}),
        {.tile_width = 64, .tile_height = 32, .include_base = true, .include_borders = false});

    ASSERT_TRUE(prep.ground_fields.contains("HU"));
    const auto& field = prep.ground_fields.at("HU");

    // Tile(0,0) spans world [0,64) × [0,32); tile(1,0) spans [32,96) × [16,48)
    // Combined bbox: [0,96) × [0,48)
    //   columns: floor_div(95,64) − floor_div(0,64) + 1 = 1 − 0 + 1 = 2
    //   rows:    floor_div(47,32) − floor_div(0,32) + 1 = 1 − 0 + 1 = 2
    EXPECT_EQ(field.columns, 2) << "two adjacent tiles need exactly 2 columns";
    EXPECT_EQ(field.rows, 2) << "two adjacent tiles need exactly 2 rows";
}

TEST(AdventureTerrainSurfaceComposer, GroundFieldThreeInLineDoesNotOverExtend) {
    d2engine::AdventureTerrainSurfaceComposer composer(base_images(), base_catalog());
    // 3×1 HU tiles at (0,0), (1,0), (2,0)
    const auto prep = composer.prepare_full_map(
        input(3, 1, {descriptor(0x00000001), descriptor(0x00000001), descriptor(0x00000001)}),
        {.tile_width = 64, .tile_height = 32, .include_base = true, .include_borders = false});

    ASSERT_TRUE(prep.ground_fields.contains("HU"));
    const auto& field = prep.ground_fields.at("HU");

    // Tile positions: (0,0) → [0,64)×[0,32), (1,0) → [32,96)×[16,48),
    //                (2,0) → [64,128)×[32,64)
    // Combined bbox: [0,128) × [0,64)
    //   columns: floor_div(127,64) − floor_div(0,64) + 1 = 1 − 0 + 1 = 2
    //   rows:    floor_div(63,32)  − floor_div(0,32) + 1  = 1 − 0 + 1 = 2
    //
    // Old buggy code: floor_div(128+64-1,64)=2 → 3 columns, which is wrong.
    EXPECT_EQ(field.columns, 2) << "three tiles span 128px, exactly 2 columns of 64-wide patches";
    EXPECT_EQ(field.rows, 2) << "three tiles span 64px, exactly 2 rows of 32-tall patches";
}

// Regression: raw SG terrain storage uses transposed indexing relative to
// canonical map coordinates.  normalize_raw_sg_terrain applies this once at
// the import boundary.  After normalization, canonical terrain is fed directly
// to the composer.
TEST(AdventureTerrainSurfaceComposer, CanonicalTerrainRendersWithoutAdditionalOrientation) {
    auto images = base_images();
    for (int v = 0; v <= 3; ++v) {
        char key_buf[32];
        std::snprintf(key_buf, sizeof(key_buf), "Imgs/Ground.ff/HU_%02d.PNG", v);
        images[key_buf] = solid(200, 0, 0);
        std::snprintf(key_buf, sizeof(key_buf), "Imgs/Ground.ff/DW_%02d.PNG", v);
        images[key_buf] = solid(0, 200, 0);
    }

    d2engine::AdventureTerrainSurfaceComposer composer(images, base_catalog());

    // Canonical 2×1 terrain after normalize_raw_sg_terrain from raw SG 1×2.
    const auto canonical_input = input(2, 1, {descriptor(0x00000001), descriptor(0x00000002)});

    ASSERT_EQ(canonical_input.map_width, 2);
    ASSERT_EQ(canonical_input.map_height, 1);
    ASSERT_EQ(canonical_input.resolved_tiles.size(), 2U);

    EXPECT_EQ(canonical_input.resolved_tiles[0].descriptor.raw.low_byte, 1);
    EXPECT_EQ(canonical_input.resolved_tiles[1].descriptor.raw.low_byte, 2);

    const auto surface = composer.render_full_map(
        canonical_input,
        {.tile_width = 64, .tile_height = 32, .include_base = true, .include_borders = false});

    ASSERT_GT(surface.width, 0);
}
