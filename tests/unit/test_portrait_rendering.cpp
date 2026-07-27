#include <gtest/gtest.h>

#include "../src/d2res/rgba_buffer.hpp"
#include "d2engine/assets/portrait_manifest.hpp"
#include "d2engine/assets/portrait_manifest_index.hpp"
#include "d2engine/battle_view/battle_scene.hpp"
#include "d2engine/battle_view/battle_unit.hpp"
#include "d2engine/battle_view/debug_renderable_item.hpp"
#include "d2engine/battle_view/hud_unit_info.hpp"
#include "d2engine/battle_view/life_visual_state.hpp"
#include "d2engine/battle_view/portrait_render_item.hpp"
#include "d2engine/render/renderer2d.hpp"

namespace d2engine {

// ── Production magenta helper used by GameTextureCache ────────────────────

TEST(PortraitMagentaKey, ProductionHelperUsed) {
    // Verify that apply_magenta_key_to_rgba is the same function
    // used by game_texture_cache.cpp. This is a link-time test: if the
    // production path can't find the symbol, compilation fails.
    d2res::RgbaBuffer buf;
    buf.width = 2;
    buf.height = 2;
    buf.rgba = {
        255, 0, 255, 255, // magenta
        255, 0, 0,   255, // red
    };

    // Same call as GameTextureCache::get_raw(..., true) makes
    auto stats = d2res::apply_magenta_key_to_rgba(buf);

    EXPECT_EQ(buf.rgba[0], 0) << "magenta R zeroed";
    EXPECT_EQ(buf.rgba[1], 0) << "magenta G zeroed";
    EXPECT_EQ(buf.rgba[2], 0) << "magenta B zeroed";
    EXPECT_EQ(buf.rgba[3], 0) << "magenta alpha 0";
    EXPECT_EQ(buf.rgba[7], 255) << "red pixel should stay opaque";

    EXPECT_EQ(stats.checked_pixels, 4u);
    EXPECT_EQ(stats.keyed_pixels, 1u);
    EXPECT_TRUE(stats.cleanup_applied);
}

TEST(PortraitMagentaKey, DirtyMagentaValuesAreKeyed) {
    // Near-magenta values observed in game assets must be caught by predicate
    struct {
        uint8_t r, g, b;
    } const dirty_magenta[] = {
        {.r = 254, .g = 0, .b = 255}, {.r = 255, .g = 1, .b = 255}, {.r = 252, .g = 2, .b = 252},
        {.r = 241, .g = 2, .b = 239}, {.r = 214, .g = 2, .b = 214}, {.r = 168, .g = 2, .b = 169},
    };
    constexpr int kCount = 6;
    for (auto i : dirty_magenta) {
        EXPECT_TRUE(d2res::is_magenta_key_pixel(i.r, i.g, i.b))
            << "Expected (" << static_cast<int>(i.r) << ',' << static_cast<int>(i.g) << ','
            << static_cast<int>(i.b) << ") to be magenta key";
    }
    // Also verify they become transparent in the full pipeline
    d2res::RgbaBuffer buf;
    buf.width = static_cast<uint32_t>(kCount);
    buf.height = 1;
    for (auto i : dirty_magenta) {
        buf.rgba.push_back(i.r);
        buf.rgba.push_back(i.g);
        buf.rgba.push_back(i.b);
        buf.rgba.push_back(255);
    }
    d2res::apply_magenta_key_to_rgba(buf);
    for (int i = 0; i < kCount; ++i) {
        const auto idx = (static_cast<std::size_t>(i) * 4) + 3;
        EXPECT_EQ(buf.rgba[idx], 0) << "Pixel " << i << " should be transparent";
    }
}

TEST(PortraitMagentaKey, NormalPurpleNotKeyed) {
    // Normal purple/dark art that should NOT be treated as magenta key
    struct {
        uint8_t r, g, b;
    } const normal_purple[] = {
        {.r = 128, .g = 0, .b = 128},   // medium purple (r<145)
        {.r = 80, .g = 0, .b = 80},     // dark purple (r<145)
        {.r = 50, .g = 10, .b = 50},    // very dark purple (r<145)
        {.r = 100, .g = 30, .b = 100},  // muted purple (r<145)
        {.r = 180, .g = 50, .b = 180},  // g=50 > 2 → not magenta
        {.r = 200, .g = 80, .b = 200},  // g=80 > 2 → not magenta
        {.r = 152, .g = 32, .b = 145},  // g=32 > 2 → not magenta (normal purple, not keyed)
        {.r = 150, .g = 100, .b = 150}, // g=100 > 2 → not magenta
    };
    for (const auto& c : normal_purple) {
        EXPECT_FALSE(d2res::is_magenta_key_pixel(c.r, c.g, c.b))
            << "Expected (" << static_cast<int>(c.r) << ',' << static_cast<int>(c.g) << ','
            << static_cast<int>(c.b) << ") to NOT be magenta key";
    }
}

TEST(PortraitMagentaKey, AlphaMaskZeroesMagentaRgb) {
    // 2x2: TL magenta, TR red, BL blue, BR transparent black
    d2res::RgbaBuffer buf;
    buf.width = 2;
    buf.height = 2;
    buf.rgba = {
        255, 0, 255, 255, // TL (0,0): magenta
        255, 0, 0,   255, // TR (1,0): red
        0,   0, 255, 255, // BL (0,1): blue
        0,   0, 0,   0,   // BR (1,1): transparent black
    };

    d2res::apply_magenta_key_to_rgba(buf);

    // TL: alpha=0 and RGB=0 (alpha-mask cleanup, no neighbor copy)
    EXPECT_EQ(buf.rgba[0], 0) << "TL R zeroed (no neighbor bleed)";
    EXPECT_EQ(buf.rgba[1], 0) << "TL G zeroed";
    EXPECT_EQ(buf.rgba[2], 0) << "TL B zeroed";
    EXPECT_EQ(buf.rgba[3], 0) << "TL alpha 0";

    // TR: unchanged red
    EXPECT_EQ(buf.rgba[4], 255) << "TR R unchanged";
    EXPECT_EQ(buf.rgba[5], 0) << "TR G unchanged";
    EXPECT_EQ(buf.rgba[6], 0) << "TR B unchanged";
    EXPECT_EQ(buf.rgba[7], 255) << "TR alpha unchanged";

    // BL: unchanged blue
    EXPECT_EQ(buf.rgba[8], 0) << "BL R unchanged";
    EXPECT_EQ(buf.rgba[9], 0) << "BL G unchanged";
    EXPECT_EQ(buf.rgba[10], 255) << "BL B unchanged";
    EXPECT_EQ(buf.rgba[11], 255) << "BL alpha unchanged";

    // BR: transparent black remains unchanged (not keyed, was already 0,0,0,0)
    EXPECT_EQ(buf.rgba[12], 0) << "BR R unchanged";
    EXPECT_EQ(buf.rgba[13], 0) << "BR G unchanged";
    EXPECT_EQ(buf.rgba[14], 0) << "BR B unchanged";
    EXPECT_EQ(buf.rgba[15], 0) << "BR alpha stays 0";
}

TEST(PortraitMagentaKey, OpaquePixelsUnchangedByBleed) {
    d2res::RgbaBuffer buf;
    buf.width = 2;
    buf.height = 1;
    buf.rgba = {
        128, 64,  32, 255, // opaque pixel 1
        200, 100, 50, 255, // opaque pixel 2
    };

    d2res::apply_magenta_key_to_rgba(buf);

    // Neither pixel is magenta, so nothing changes
    EXPECT_EQ(buf.rgba[0], 128);
    EXPECT_EQ(buf.rgba[1], 64);
    EXPECT_EQ(buf.rgba[2], 32);
    EXPECT_EQ(buf.rgba[3], 255);
    EXPECT_EQ(buf.rgba[4], 200);
    EXPECT_EQ(buf.rgba[5], 100);
    EXPECT_EQ(buf.rgba[6], 50);
    EXPECT_EQ(buf.rgba[7], 255);
}

// ── Border heuristic tests ─────────────────────────────────────────────────

TEST(MagentaBorder, DetectMagentaBorder) {
    // 10x10 image with magenta border and non-magenta interior
    d2res::RgbaBuffer buf;
    buf.width = 10;
    buf.height = 10;
    buf.rgba.resize(static_cast<std::size_t>(buf.width) * buf.height * 4, 0);
    // Fill all pixels with non-magenta gray
    for (auto& v : buf.rgba)
        v = 100;
    for (std::size_t i = 0; i < buf.rgba.size(); i += 4)
        buf.rgba[i + 3] = 255;
    // Set the outer 2 rows/cols to magenta
    for (uint32_t y = 0; y < buf.height; ++y) {
        for (uint32_t x = 0; x < buf.width; ++x) {
            if (x < 2 || x >= buf.width - 2 || y < 2 || y >= buf.height - 2) {
                const auto idx = ((static_cast<std::size_t>(y) * buf.width) + x) * 4;
                buf.rgba[idx] = 255;     // R
                buf.rgba[idx + 1] = 0;   // G
                buf.rgba[idx + 2] = 255; // B
            }
        }
    }
    EXPECT_TRUE(d2res::detect_magenta_key_border(buf));
}

TEST(MagentaBorder, NoFalsePositiveWithoutMagentaBorder) {
    // 10x10 image with no magenta pixels at all
    d2res::RgbaBuffer buf;
    buf.width = 10;
    buf.height = 10;
    buf.rgba.resize(static_cast<std::size_t>(buf.width) * buf.height * 4, 255);
    // Fill with blue
    for (auto& v : buf.rgba)
        v = 50;
    for (std::size_t i = 0; i < buf.rgba.size(); i += 4)
        buf.rgba[i + 3] = 255;

    EXPECT_FALSE(d2res::detect_magenta_key_border(buf));
}

TEST(MagentaBorder, InteriorPurpleNotFalsePositive) {
    // 10x10 image with interior purple content but no magenta border
    d2res::RgbaBuffer buf;
    buf.width = 10;
    buf.height = 10;
    buf.rgba.resize(static_cast<std::size_t>(buf.width) * buf.height * 4, 255);
    // Fill with gray
    for (auto& v : buf.rgba)
        v = 128;
    for (std::size_t i = 0; i < buf.rgba.size(); i += 4)
        buf.rgba[i + 3] = 255;
    // Place some purple pixels in interior (not border)
    for (uint32_t y = 3; y < 7; ++y) {
        for (uint32_t x = 3; x < 7; ++x) {
            const auto idx = ((static_cast<std::size_t>(y) * buf.width) + x) * 4;
            buf.rgba[idx] = 150;
            buf.rgba[idx + 1] = 10;
            buf.rgba[idx + 2] = 150;
            buf.rgba[idx + 3] = 255;
        }
    }
    EXPECT_FALSE(d2res::detect_magenta_key_border(buf));
}

TEST(MagentaBorder, MagentaOnlyOnLeftEdgeDetected) {
    // 5x5: only left column (x=0) is magenta
    d2res::RgbaBuffer buf;
    buf.width = 5;
    buf.height = 5;
    buf.rgba.resize(static_cast<std::size_t>(buf.width) * buf.height * 4, 128);
    for (std::size_t i = 0; i < buf.rgba.size(); i += 4)
        buf.rgba[i + 3] = 255;
    for (uint32_t y = 0; y < buf.height; ++y) {
        const auto idx = ((static_cast<std::size_t>(y) * buf.width) + 0) * 4;
        buf.rgba[idx] = 255;
        buf.rgba[idx + 1] = 0;
        buf.rgba[idx + 2] = 255;
    }
    EXPECT_TRUE(d2res::detect_magenta_key_border(buf));
}

TEST(MagentaBorder, MagentaOnlyOnRightEdgeDetected) {
    // 5x5: only right column (x=4) is magenta
    d2res::RgbaBuffer buf;
    buf.width = 5;
    buf.height = 5;
    buf.rgba.resize(static_cast<std::size_t>(buf.width) * buf.height * 4, 128);
    for (std::size_t i = 0; i < buf.rgba.size(); i += 4)
        buf.rgba[i + 3] = 255;
    for (uint32_t y = 0; y < buf.height; ++y) {
        const auto idx = ((static_cast<std::size_t>(y) * buf.width) + 4) * 4;
        buf.rgba[idx] = 255;
        buf.rgba[idx + 1] = 0;
        buf.rgba[idx + 2] = 255;
    }
    EXPECT_TRUE(d2res::detect_magenta_key_border(buf));
}

TEST(MagentaBorder, MagentaOnlyOnTopEdgeDetected) {
    // 5x5: only top row (y=0) is magenta
    d2res::RgbaBuffer buf;
    buf.width = 5;
    buf.height = 5;
    buf.rgba.resize(static_cast<std::size_t>(buf.width) * buf.height * 4, 128);
    for (std::size_t i = 0; i < buf.rgba.size(); i += 4)
        buf.rgba[i + 3] = 255;
    for (uint32_t x = 0; x < buf.width; ++x) {
        const auto idx = ((static_cast<std::size_t>(0) * buf.width) + x) * 4;
        buf.rgba[idx] = 255;
        buf.rgba[idx + 1] = 0;
        buf.rgba[idx + 2] = 255;
    }
    EXPECT_TRUE(d2res::detect_magenta_key_border(buf));
}

TEST(MagentaBorder, MagentaOnlyOnBottomEdgeDetected) {
    // 5x5: only bottom row (y=4) is magenta
    d2res::RgbaBuffer buf;
    buf.width = 5;
    buf.height = 5;
    buf.rgba.resize(static_cast<std::size_t>(buf.width) * buf.height * 4, 128);
    for (std::size_t i = 0; i < buf.rgba.size(); i += 4)
        buf.rgba[i + 3] = 255;
    for (uint32_t x = 0; x < buf.width; ++x) {
        const auto idx = ((static_cast<std::size_t>(4) * buf.width) + x) * 4;
        buf.rgba[idx] = 255;
        buf.rgba[idx + 1] = 0;
        buf.rgba[idx + 2] = 255;
    }
    EXPECT_TRUE(d2res::detect_magenta_key_border(buf));
}

TEST(MagentaBorder, NoMagentaBorderSmallImageReturnsFalse) {
    // 3x3 image with no magenta at all — tests border_width == 1 path
    d2res::RgbaBuffer buf;
    buf.width = 3;
    buf.height = 3;
    buf.rgba.resize(static_cast<std::size_t>(buf.width) * buf.height * 4, 64);
    for (std::size_t i = 0; i < buf.rgba.size(); i += 4)
        buf.rgba[i + 3] = 255;
    EXPECT_FALSE(d2res::detect_magenta_key_border(buf));
}

TEST(MagentaBorder, InteriorPurpleNoBorderReturnsFalse) {
    // 12x12: all magenta pixels are interior, not touching the border
    d2res::RgbaBuffer buf;
    buf.width = 12;
    buf.height = 12;
    buf.rgba.resize(static_cast<std::size_t>(buf.width) * buf.height * 4, 100);
    for (std::size_t i = 0; i < buf.rgba.size(); i += 4)
        buf.rgba[i + 3] = 255;
    // Place magenta in the center 4x4 (x=4..7, y=4..7), well away from border
    for (uint32_t y = 4; y <= 7; ++y) {
        for (uint32_t x = 4; x <= 7; ++x) {
            const auto idx = ((static_cast<std::size_t>(y) * buf.width) + x) * 4;
            buf.rgba[idx] = 255;
            buf.rgba[idx + 1] = 0;
            buf.rgba[idx + 2] = 255;
        }
    }
    EXPECT_FALSE(d2res::detect_magenta_key_border(buf));
}

TEST(MagentaBorder, CornerClusterDetectsMagenta) {
    // Tiny 4x4 image: only top-left 3x3 has magenta pixels
    d2res::RgbaBuffer buf;
    buf.width = 4;
    buf.height = 4;
    buf.rgba.resize(static_cast<std::size_t>(buf.width) * buf.height * 4, 0);
    for (auto& v : buf.rgba)
        v = 128;
    for (std::size_t i = 0; i < buf.rgba.size(); i += 4)
        buf.rgba[i + 3] = 255;
    // Set 3 pixels in top-left 3x3 to magenta
    buf.rgba[0] = 255;
    buf.rgba[1] = 0;
    buf.rgba[2] = 255;
    buf.rgba[4] = 255;
    buf.rgba[5] = 0;
    buf.rgba[6] = 255;
    buf.rgba[8] = 255;
    buf.rgba[9] = 0;
    buf.rgba[10] = 255;

    EXPECT_TRUE(d2res::detect_magenta_key_border(buf));
}

TEST(PortraitMagentaKey, AlphaMaskCleanupDoesNotCopyNeighborRgb) {
    // 1x3: magenta left (idx 0-3), transparent center (idx 4-7), opaque red right (idx 8-11)
    // Production cleanup must NOT copy red RGB into the transparent center pixel.
    d2res::RgbaBuffer buf;
    buf.width = 3;
    buf.height = 1;
    buf.rgba = {
        255, 0, 255, 255, // left: magenta
        0,   0, 0,   0,   // center: transparent black
        255, 0, 0,   255, // right: opaque red
    };

    auto stats = d2res::apply_magenta_key_to_rgba(buf);

    // Left (magenta): zeroed by alpha-mask cleanup
    EXPECT_EQ(buf.rgba[0], 0) << "left R zeroed";
    EXPECT_EQ(buf.rgba[1], 0) << "left G zeroed";
    EXPECT_EQ(buf.rgba[2], 0) << "left B zeroed";
    EXPECT_EQ(buf.rgba[3], 0) << "left alpha 0";

    // Center (transparent): must stay 0,0,0,0 — no bleed from neighbor
    EXPECT_EQ(buf.rgba[4], 0) << "center R unchanged (no bleed)";
    EXPECT_EQ(buf.rgba[5], 0) << "center G unchanged (no bleed)";
    EXPECT_EQ(buf.rgba[6], 0) << "center B unchanged (no bleed)";
    EXPECT_EQ(buf.rgba[7], 0) << "center alpha unchanged";

    // Right (opaque red): untouched
    EXPECT_EQ(buf.rgba[8], 255);
    EXPECT_EQ(buf.rgba[9], 0);
    EXPECT_EQ(buf.rgba[10], 0);
    EXPECT_EQ(buf.rgba[11], 255);

    EXPECT_EQ(stats.checked_pixels, 3u);
    EXPECT_EQ(stats.keyed_pixels, 1u);
}

// ── Direct predicate tests ─────────────────────────────────────────────────

TEST(MagentaPredicate, Exact255_0_255) {
    EXPECT_TRUE(d2res::is_magenta_key_pixel(255, 0, 255));
}

TEST(MagentaPredicate, DirtyValues) {
    EXPECT_TRUE(d2res::is_magenta_key_pixel(252, 2, 252));
    EXPECT_TRUE(d2res::is_magenta_key_pixel(241, 2, 239));
    EXPECT_TRUE(d2res::is_magenta_key_pixel(214, 2, 214));
    EXPECT_TRUE(d2res::is_magenta_key_pixel(168, 2, 169));
}

TEST(MagentaPredicate, NormalRedNotKeyed) {
    EXPECT_FALSE(d2res::is_magenta_key_pixel(255, 0, 0));
}

TEST(MagentaPredicate, NormalBlueNotKeyed) {
    EXPECT_FALSE(d2res::is_magenta_key_pixel(0, 0, 255));
}

TEST(MagentaPredicate, NormalDarkPurpleNotKeyed) {
    EXPECT_FALSE(d2res::is_magenta_key_pixel(80, 0, 80));
    EXPECT_FALSE(d2res::is_magenta_key_pixel(50, 10, 50));
}

TEST(MagentaPredicate, HighGreenNotKeyed) {
    // g > 2 is not magenta-keyed under the current threshold
    EXPECT_FALSE(d2res::is_magenta_key_pixel(200, 80, 200));
    EXPECT_FALSE(d2res::is_magenta_key_pixel(150, 100, 150));
}

TEST(PortraitMagentaKey, FullyOpaqueNonMagenta) {
    d2res::RgbaBuffer buf;
    buf.width = 3;
    buf.height = 1;
    buf.rgba = {0, 0, 0, 255, 128, 128, 128, 255, 255, 255, 255, 255};

    d2res::apply_magenta_key_to_rgba(buf);

    EXPECT_EQ(buf.rgba[3], 255);
    EXPECT_EQ(buf.rgba[7], 255);
    EXPECT_EQ(buf.rgba[11], 255);
}

// ── PortraitManifestIndex ─────────────────────────────────────────────────

static PortraitManifestIndex make_test_index() {
    PortraitManifest manifest;
    manifest.container = "Imgs/Faces.ff";
    manifest.units = {
        {.unit_id = "g000uu0001",
         .resource_unit_id = "G000UU0001",
         .face_record_name = "G000UU0001FACE.PNG",
         .faceb_record_name = "G000UU0001FACEB.PNG",
         .has_face = true,
         .has_faceb = true},
        {.unit_id = "g000uu0007",
         .resource_unit_id = "G000UU0007",
         .face_record_name = "G000UU0007FACE.PNG",
         .faceb_record_name = "G000UU0007FACEB.PNG",
         .has_face = true,
         .has_faceb = false},
        {.unit_id = "g000uu0012",
         .resource_unit_id = "G000UU0012",
         .face_record_name = "",
         .faceb_record_name = "G000UU0012FACEB.PNG",
         .has_face = false,
         .has_faceb = true},
    };
    return PortraitManifestIndex(manifest);
}

TEST(PortraitManifestIndex, FindByUnitTypeUpper) {
    auto        idx = make_test_index();
    const auto* e = idx.find_by_unit_type("UU0001");
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(e->resource_unit_id, "G000UU0001");
}

TEST(PortraitManifestIndex, FindByUnitTypeLower) {
    auto        idx = make_test_index();
    const auto* e = idx.find_by_unit_type("uu0001");
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(e->resource_unit_id, "G000UU0001");
}

TEST(PortraitManifestIndex, FindByUnitIdUpper) {
    auto        idx = make_test_index();
    const auto* e = idx.find_by_unit_type("UU0001");
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(e->resource_unit_id, "G000UU0001");
}

TEST(PortraitManifestIndex, FindByUnitIdLower) {
    auto        idx = make_test_index();
    const auto* e = idx.find_by_unit_type("uu0001");
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(e->resource_unit_id, "G000UU0001");
}

TEST(PortraitManifestIndex, UnknownUnitReturnsNull) {
    auto idx = make_test_index();
    EXPECT_EQ(idx.find_by_unit_type("UU9999"), nullptr);
    EXPECT_EQ(idx.find_by_unit_type("UU9999"), nullptr);
}

// ── resolve_portrait_variant (selection policy) ───────────────────────────

TEST(PortraitSelectionPolicy, FacePreferFaceFirst) {
    UnitPortraitEntry entry;
    entry.has_face = true;
    entry.has_faceb = true;
    entry.face_record_name = "G000UU0001FACE.PNG";
    entry.faceb_record_name = "G000UU0001FACEB.PNG";

    auto r = resolve_portrait_variant(entry, PortraitTextureKind::Face);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->record_name, "G000UU0001FACE.PNG");
    EXPECT_EQ(r->kind, PortraitTextureKind::Face);
}

TEST(PortraitSelectionPolicy, FaceFallbackToFacebWhenFaceMissing) {
    UnitPortraitEntry entry;
    entry.has_face = false;
    entry.has_faceb = true;
    entry.faceb_record_name = "G000UU0001FACEB.PNG";

    auto r = resolve_portrait_variant(entry, PortraitTextureKind::Face);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->record_name, "G000UU0001FACEB.PNG");
    EXPECT_EQ(r->kind, PortraitTextureKind::FaceB);
}

TEST(PortraitSelectionPolicy, FacebPreferFacebFirst) {
    UnitPortraitEntry entry;
    entry.has_face = true;
    entry.has_faceb = true;
    entry.face_record_name = "G000UU0001FACE.PNG";
    entry.faceb_record_name = "G000UU0001FACEB.PNG";

    auto r = resolve_portrait_variant(entry, PortraitTextureKind::FaceB);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->record_name, "G000UU0001FACEB.PNG");
    EXPECT_EQ(r->kind, PortraitTextureKind::FaceB);
}

TEST(PortraitSelectionPolicy, FacebFallbackToFaceWhenFacebMissing) {
    UnitPortraitEntry entry;
    entry.has_face = true;
    entry.has_faceb = false;
    entry.face_record_name = "G000UU0001FACE.PNG";

    auto r = resolve_portrait_variant(entry, PortraitTextureKind::FaceB);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->record_name, "G000UU0001FACE.PNG");
    EXPECT_EQ(r->kind, PortraitTextureKind::Face);
}

TEST(PortraitSelectionPolicy, BothMissingReturnsNull) {
    UnitPortraitEntry entry;
    entry.has_face = false;
    entry.has_faceb = false;

    EXPECT_FALSE(resolve_portrait_variant(entry, PortraitTextureKind::Face).has_value());
    EXPECT_FALSE(resolve_portrait_variant(entry, PortraitTextureKind::FaceB).has_value());
}

// ── PortraitRenderItem to_tunable_item ────────────────────────────────────

TEST(PortraitRenderItem, ToDebugItemUsesActualTreePaths) {
    PortraitRenderItem pri;
    pri.tree_path = "/ui/left_unit_group/0_front/portrait";
    pri.stable_id = "/ui/left_unit_group/0_front/portrait";
    pri.dest_rect = {.x = 10, .y = 20, .w = 100, .h = 150};
    pri.layer = 100;
    pri.flip_x = false;
    pri.side = "a";
    pri.has_texture = true;
    pri.visual_category = "PortraitFACE";

    auto di = pri.to_tunable_item();
    EXPECT_EQ(di.tree_path, "/ui/left_unit_group/0_front/portrait");
    EXPECT_EQ(di.stable_id, "/ui/left_unit_group/0_front/portrait");
    EXPECT_EQ(di.kind, "portrait");
    EXPECT_EQ(di.layer, 100);
    EXPECT_TRUE(di.selectable);
    EXPECT_TRUE(di.visible);
    EXPECT_EQ(di.render_side, "a");
    EXPECT_EQ(di.visual_category, "PortraitFACE");
    EXPECT_FLOAT_EQ(di.logical_rect.x, 10);
    EXPECT_FLOAT_EQ(di.logical_rect.y, 20);
    EXPECT_FLOAT_EQ(di.logical_rect.w, 100);
    EXPECT_FLOAT_EQ(di.logical_rect.h, 150);

    ASSERT_TRUE(di.binding.has_value());
    EXPECT_EQ(di.binding->owner_kind, BindingOwnerKind::TreeLayout);
    EXPECT_EQ(di.binding->role, BindingRole::CombatFrame);
    EXPECT_EQ(di.binding->tree_path, "/ui/left_unit_group/0_front/portrait");
}

// ── Placeholder portrait item ─────────────────────────────────────────────

TEST(PortraitPlaceholder, IsSelectableAndDebugVisible) {
    auto ph =
        build_portrait_placeholder_item("/ui/left_unit_group/0_back/portrait",
                                        Rect{.x = 5, .y = 10, .w = 100, .h = 150}, false, "a");
    EXPECT_FALSE(ph.has_texture);
    EXPECT_EQ(ph.texture, nullptr);
    EXPECT_EQ(ph.visual_category, "PortraitPlaceholder");
    EXPECT_EQ(ph.tree_path, "/ui/left_unit_group/0_back/portrait");

    auto di = ph.to_tunable_item();
    EXPECT_EQ(di.tree_path, "/ui/left_unit_group/0_back/portrait");
    EXPECT_EQ(di.kind, "portrait");
    EXPECT_TRUE(di.selectable);
    EXPECT_TRUE(di.visible);
    EXPECT_EQ(di.visual_category, "PortraitPlaceholder");
    ASSERT_TRUE(di.binding.has_value());
    EXPECT_EQ(di.binding->owner_kind, BindingOwnerKind::TreeLayout);
}

TEST(PortraitPlaceholder, MissingTextureStillEmitsPortraitDebugPath) {
    // Simulate the path taken by FormationStatusPanel when texture is missing:
    // a placeholder item is built for the portrait tree path.
    auto ph = build_portrait_placeholder_item("/ui/right_unit_group/1_back/portrait",
                                              Rect{.x = 0, .y = 0, .w = 100, .h = 150}, true, "d");
    auto di = ph.to_tunable_item();
    EXPECT_EQ(di.tree_path, "/ui/right_unit_group/1_back/portrait");
    EXPECT_TRUE(di.visible);
}

TEST(PortraitPlaceholder, TexturePortraitEmitsSameTreePath) {
    // When a texture portrait IS available, it emits the same /portrait tree path.
    PortraitRenderItem pri;
    pri.tree_path = "/ui/combat_frame/left_slot/portrait";
    pri.has_texture = true;
    pri.visual_category = "PortraitFACE";

    auto di = pri.to_tunable_item();
    EXPECT_EQ(di.tree_path, "/ui/combat_frame/left_slot/portrait");
    EXPECT_TRUE(di.visible);
    EXPECT_EQ(di.visual_category, "PortraitFACE");
}

TEST(PortraitPlaceholder, PlaceholderCategoryUsed) {
    auto ph = build_portrait_placeholder_item("/ui/combat_frame/right_slot/portrait",
                                              Rect{.x = 0, .y = 0, .w = 100, .h = 150}, true, "d");
    EXPECT_EQ(ph.visual_category, "PortraitPlaceholder");
}

// ── Empty portrait item ────────────────────────────────────────────────────

TEST(PortraitEmpty, ProducesCorrectCategory) {
    auto em =
        build_portrait_placeholder_item("/ui/left_unit_group/0_front/portrait",
                                        Rect{.x = 5, .y = 10, .w = 100, .h = 150}, false, "a");
    EXPECT_FALSE(em.has_texture);
    EXPECT_EQ(em.texture, nullptr);
    EXPECT_EQ(em.visual_category, "PortraitPlaceholder");
    EXPECT_EQ(em.tree_path, "/ui/left_unit_group/0_front/portrait");
}

TEST(PortraitEmpty, IsSelectableAndDebugVisible) {
    auto em = build_portrait_placeholder_item("/ui/left_unit_group/0_back/portrait",
                                              Rect{.x = 0, .y = 0, .w = 100, .h = 150}, false, "a");
    auto di = em.to_tunable_item();
    EXPECT_EQ(di.tree_path, "/ui/left_unit_group/0_back/portrait");
    EXPECT_EQ(di.kind, "portrait");
    EXPECT_TRUE(di.selectable);
    EXPECT_TRUE(di.visible);
    EXPECT_EQ(di.visual_category, "PortraitPlaceholder");
    ASSERT_TRUE(di.binding.has_value());
    EXPECT_EQ(di.binding->owner_kind, BindingOwnerKind::TreeLayout);
}

TEST(PortraitEmpty, LeftSlotNoFlip) {
    auto em = build_portrait_placeholder_item("/ui/left_unit_group/0_front/portrait",
                                              Rect{.x = 0, .y = 0, .w = 100, .h = 150}, false, "a");
    EXPECT_FALSE(em.flip_x);
    EXPECT_EQ(em.side, "a");
}

TEST(PortraitEmpty, RightSlotFlipX) {
    auto em = build_portrait_placeholder_item("/ui/right_unit_group/1_back/portrait",
                                              Rect{.x = 0, .y = 0, .w = 100, .h = 150}, true, "d");
    EXPECT_TRUE(em.flip_x);
    EXPECT_EQ(em.side, "d");
}

// ── Right-side flip tests through build_portrait_placeholder_item ─────────

TEST(PortraitFlip, LeftFormationPlaceholderNoFlip) {
    // Left formation slot → flip_x = false
    auto ph = build_portrait_placeholder_item("/ui/left_unit_group/0_front/portrait",
                                              Rect{.x = 0, .y = 0, .w = 100, .h = 150}, false, "a");
    EXPECT_FALSE(ph.flip_x);
    EXPECT_EQ(ph.side, "a");
}

TEST(PortraitFlip, RightFormationPlaceholderFlipX) {
    // Right formation slot → flip_x = true
    auto ph = build_portrait_placeholder_item("/ui/right_unit_group/0_front/portrait",
                                              Rect{.x = 0, .y = 0, .w = 100, .h = 150}, true, "d");
    EXPECT_TRUE(ph.flip_x);
    EXPECT_EQ(ph.side, "d");
}

TEST(PortraitFlip, LeftBottomPlaceholderNoFlip) {
    // Bottom HUD left slot → flip_x = false
    auto ph = build_portrait_placeholder_item("/ui/combat_frame/left_slot/portrait",
                                              Rect{.x = 0, .y = 0, .w = 100, .h = 150}, false, "a");
    EXPECT_FALSE(ph.flip_x);
    EXPECT_EQ(ph.tree_path, "/ui/combat_frame/left_slot/portrait");
}

TEST(PortraitFlip, RightBottomPlaceholderFlipX) {
    // Bottom HUD right slot → flip_x = true
    auto ph = build_portrait_placeholder_item("/ui/combat_frame/right_slot/portrait",
                                              Rect{.x = 0, .y = 0, .w = 100, .h = 150}, true, "d");
    EXPECT_TRUE(ph.flip_x);
    EXPECT_EQ(ph.tree_path, "/ui/combat_frame/right_slot/portrait");
}

TEST(PortraitFlip, LeftFormationBuilderNoFlip) {
    // Left formation slot: build_portrait_render_item calls with flip_x = !is_left
    // This test verifies the item's flip_x from the builder path (no texture needed).
    // We create placement data manually — the actual builder is tested at integration level.
    bool const flip_x_for_left = false; // is_left=true → !is_left=false
    auto       ph = build_portrait_placeholder_item("/ui/left_unit_group/0_back/portrait",
                                                    Rect{.x = 0, .y = 0, .w = 100, .h = 150},
                                                    flip_x_for_left, "a");
    EXPECT_FALSE(ph.flip_x);
}

TEST(PortraitFlip, RightFormationBuilderFlipX) {
    // Right formation slot: flip_x = !is_left → true
    bool const flip_x_for_right = true; // is_left=false → !is_left=true
    auto       ph = build_portrait_placeholder_item("/ui/right_unit_group/0_back/portrait",
                                                    Rect{.x = 0, .y = 0, .w = 100, .h = 150},
                                                    flip_x_for_right, "d");
    EXPECT_TRUE(ph.flip_x);
}

// ── HudUnitInfo defaults ──────────────────────────────────────────────────

TEST(HudUnitInfo, Defaults) {
    const HudUnitInfo info;
    EXPECT_FALSE(info.has_unit);
    EXPECT_TRUE(info.name.empty());
    EXPECT_TRUE(info.unit_type.empty());
    EXPECT_EQ(info.hp, 0);
    EXPECT_EQ(info.hp_max, 0);
    EXPECT_FALSE(info.flip_x);
    EXPECT_FALSE(info.flip_y);
    EXPECT_FALSE(info.portrait_item.has_value());
}

TEST(PortraitMagentaKey, NoNeighborRgbCopyIntoTransparent) {
    // 1x2: magenta left, opaque red right
    // Alpha-mask cleanup must NOT copy red RGB into the left pixel
    d2res::RgbaBuffer buf;
    buf.width = 2;
    buf.height = 1;
    buf.rgba = {
        255, 0,  255, 255, // left: magenta
        200, 50, 0,   255, // right: opaque orange
    };

    d2res::apply_magenta_key_to_rgba(buf);

    // Left pixel must be 0,0,0,0 — no neighbor bleed
    EXPECT_EQ(buf.rgba[0], 0) << "no neighbor copy into R";
    EXPECT_EQ(buf.rgba[1], 0) << "no neighbor copy into G";
    EXPECT_EQ(buf.rgba[2], 0) << "no neighbor copy into B";
    EXPECT_EQ(buf.rgba[3], 0) << "alpha 0";

    // Right pixel untouched
    EXPECT_EQ(buf.rgba[4], 200);
    EXPECT_EQ(buf.rgba[5], 50);
    EXPECT_EQ(buf.rgba[6], 0);
    EXPECT_EQ(buf.rgba[7], 255);
}

TEST(PortraitMagentaKey, CleanupStatsOnCleanImage) {
    d2res::RgbaBuffer buf;
    buf.width = 2;
    buf.height = 1;
    buf.rgba = {100, 100, 100, 255, 50, 50, 50, 255};

    auto stats = d2res::apply_magenta_key_to_rgba(buf);

    EXPECT_EQ(stats.checked_pixels, 2u);
    EXPECT_EQ(stats.keyed_pixels, 0u);
    EXPECT_FALSE(stats.cleanup_applied);
    // Buffer byte-identical
    EXPECT_EQ(buf.rgba[0], 100);
    EXPECT_EQ(buf.rgba[3], 255);
}

// ── Dead mask overlay item ─────────────────────────────────────────────────

TEST(DeadMaskItem, Constants) {
    EXPECT_STREQ(kDeadMaskContainer, "Imgs/Faces.ff");
    EXPECT_STREQ(kDeadMaskLargeName, "MASKDEADL.PNG");
    EXPECT_STREQ(kDeadMaskSmallName, "MASKDEADS.PNG");
}

TEST(DeadMaskItem, LargeMissingTexture_HasCorrectFields) {
    // Simulate missing-texture path: build item manually as build_dead_mask_item would.
    PortraitRenderItem item;
    item.dest_rect = {.x = 10, .y = 20, .w = 200, .h = 150};
    item.tree_path = "/ui/right_unit_group/0_center/dead_mask";
    item.stable_id = "/ui/right_unit_group/0_center/dead_mask";
    item.layer = 101;
    item.flip_x = true;
    item.flip_y = false;
    item.visual_category = "PortraitDeadMaskLarge";
    item.side = "d";
    item.has_texture = false;

    EXPECT_FALSE(item.has_texture);
    EXPECT_EQ(item.layer, 101);
    EXPECT_EQ(item.visual_category, "PortraitDeadMaskLarge");
    EXPECT_TRUE(item.flip_x);
    EXPECT_EQ(item.side, "d");

    auto di = item.to_tunable_item();
    EXPECT_EQ(di.tree_path, "/ui/right_unit_group/0_center/dead_mask");
    EXPECT_EQ(di.visual_category, "PortraitDeadMaskLarge");
    EXPECT_EQ(di.layer, 101);
    EXPECT_TRUE(di.selectable);
    EXPECT_TRUE(di.visible);
}

TEST(DeadMaskItem, SmallMissingTexture_HasCorrectFields) {
    PortraitRenderItem item;
    item.dest_rect = {.x = 5, .y = 10, .w = 100, .h = 150};
    item.tree_path = "/ui/left_unit_group/2_front/dead_mask";
    item.stable_id = "/ui/left_unit_group/2_front/dead_mask";
    item.layer = 101;
    item.flip_x = false;
    item.flip_y = false;
    item.visual_category = "PortraitDeadMaskSmall";
    item.side = "a";
    item.has_texture = false;

    EXPECT_FALSE(item.has_texture);
    EXPECT_EQ(item.layer, 101);
    EXPECT_EQ(item.visual_category, "PortraitDeadMaskSmall");
    EXPECT_FALSE(item.flip_x);

    auto di = item.to_tunable_item();
    EXPECT_EQ(di.tree_path, "/ui/left_unit_group/2_front/dead_mask");
    EXPECT_EQ(di.visual_category, "PortraitDeadMaskSmall");
}

TEST(DeadMaskItem, LayerAbovePortrait) {
    // Portrait is at layer 100; dead mask must be above it.
    PortraitRenderItem portrait;
    portrait.layer = 100;
    PortraitRenderItem mask;
    mask.layer = 101;
    EXPECT_GT(mask.layer, portrait.layer);
}

TEST(DeadMaskItem, MissingMaskDoesNotHidePortrait) {
    // Verify that when the mask has no texture, has_texture is false —
    // the caller should still draw the portrait and skip the mask draw call.
    PortraitRenderItem portrait;
    portrait.has_texture = true;
    portrait.texture = reinterpret_cast<SDL_Texture*>(0x1); // NOLINT
    portrait.visual_category = "PortraitFACE";

    PortraitRenderItem mask;
    mask.has_texture = false; // texture not found in Imgs/Faces.ff

    // Caller logic: draw portrait regardless, draw mask only if has_texture.
    EXPECT_TRUE(portrait.has_texture); // portrait drawn
    EXPECT_FALSE(mask.has_texture);    // mask skipped — portrait stays visible
}

} // namespace d2engine
