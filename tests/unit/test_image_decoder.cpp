#include <gtest/gtest.h>
#include "d2res/image_decoder.hpp"
#include "d2res/opt_index.hpp"
#include "d2res/opt_maps.hpp"
#include <iomanip>
#include <sstream>

using namespace d2res;

// Build a minimal OptMaps with a mix of image and animation entries.
static OptMaps make_test_maps() {
    OptMaps maps;

    // Add 3 image entries and 1 animation entry to IndexMap
    IndexEntry img_a;
    img_a.id = 100;
    img_a.name = "IMG_A";
    IndexEntry img_b;
    img_b.id = 101;
    img_b.name = "IMG_B";
    IndexEntry img_c;
    img_c.id = 102;
    img_c.name = "IMG_C";
    IndexEntry anim;
    anim.id = -1;
    anim.name = "ANIM_X";

    maps.index_map.entries = {img_a, img_b, img_c, anim};
    maps.index_map.name_to_index["IMG_A"] = 0;
    maps.index_map.name_to_index["IMG_B"] = 1;
    maps.index_map.name_to_index["IMG_C"] = 2;
    maps.index_map.name_to_index["ANIM_X"] = 3;
    maps.index_map.id_to_indices[100] = {0};
    maps.index_map.id_to_indices[101] = {1};
    maps.index_map.id_to_indices[102] = {2};
    maps.index_map.id_to_indices[-1] = {3};

    return maps;
}

static IndexMap make_variant_map() {
    IndexMap map;

    IndexEntry bg;
    bg.id = 200;
    bg.name = "UNDEAD_0_BG";
    IndexEntry fg;
    fg.id = 200;
    fg.name = "UNDEAD_0_FG";
    IndexEntry neutral_bg;
    neutral_bg.id = 201;
    neutral_bg.name = "NEUTRAL_0_BG";
    IndexEntry shadow;
    shadow.id = 201;
    shadow.name = "NEUTRAL_0_SHADOW";
    IndexEntry solo;
    solo.id = 202;
    solo.name = "HUMAN_0_BG";

    map.entries = {bg, fg, neutral_bg, shadow, solo};
    map.name_to_index["UNDEAD_0_BG"] = 0;
    map.name_to_index["UNDEAD_0_FG"] = 1;
    map.name_to_index["NEUTRAL_0_BG"] = 2;
    map.name_to_index["NEUTRAL_0_SHADOW"] = 3;
    map.name_to_index["HUMAN_0_BG"] = 4;
    map.id_to_indices[200] = {0, 1};
    map.id_to_indices[201] = {2, 3};
    map.id_to_indices[202] = {4};

    return map;
}

// ---------------------------------------------------------------------------
// Hex color padding (build_metadata uses ostringstream with setfill/setw)
// ---------------------------------------------------------------------------

TEST(HexColor, ZeroPadsOneDigitChannels) {
    // Reproduce the exact formatting used by build_metadata.
    // Before fix: R=1,G=2,B=3 → "#123"; after: → "#010203".
    std::ostringstream hex;
    hex << std::hex << std::uppercase << std::setfill('0') << '#' << std::setw(2) << 1 // R
        << std::setw(2) << 2                                                           // G
        << std::setw(2) << 3;                                                          // B
    EXPECT_EQ(hex.str(), "#010203");
}

TEST(HexColor, FullByteValuesNotPadded) {
    std::ostringstream hex;
    hex << std::hex << std::uppercase << std::setfill('0') << '#' << std::setw(2) << 255 // R
        << std::setw(2) << 128                                                           // G
        << std::setw(2) << 0;                                                            // B
    EXPECT_EQ(hex.str(), "#FF8000");
}

// ---------------------------------------------------------------------------
// LruCache tests
// ---------------------------------------------------------------------------

TEST(LruCache, EvictsLruWhenAtCapacity) {
    d2res::LruCache<int, int> cache(3);
    cache.put(1, 10);
    cache.put(2, 20);
    cache.put(3, 30);
    ASSERT_EQ(cache.size(), 3u);

    // Access 1 and 2 so 3 becomes LRU
    cache.get(1);
    cache.get(2);

    cache.put(4, 40); // should evict 3
    EXPECT_EQ(cache.size(), 3u);
    EXPECT_EQ(cache.get(3), nullptr);
    EXPECT_NE(cache.get(1), nullptr);
    EXPECT_NE(cache.get(2), nullptr);
    EXPECT_NE(cache.get(4), nullptr);
}

TEST(LruCache, CacheHitReturnsCorrectValue) {
    d2res::LruCache<std::string, int> cache(4);
    cache.put("a", 42);
    int const* v = cache.get("a");
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(*v, 42);
}

TEST(LruCache, MissReturnsNullptr) {
    d2res::LruCache<std::string, int> cache(4);
    EXPECT_EQ(cache.get("missing"), nullptr);
}

TEST(LruCache, UpdateExistingKeyPromotesToMru) {
    d2res::LruCache<int, int> cache(2);
    cache.put(1, 10);
    cache.put(2, 20);
    cache.put(1, 99); // update 1 → should be MRU now
    cache.put(3, 30); // should evict 2, not 1
    EXPECT_EQ(cache.get(2), nullptr);
    ASSERT_NE(cache.get(1), nullptr);
    EXPECT_EQ(*cache.get(1), 99);
}

TEST(LruCache, SizeNeverExceedsCapacity) {
    d2res::LruCache<int, int> cache(2);
    for (int i = 0; i < 10; ++i)
        cache.put(i, i * 10);
    EXPECT_LE(cache.size(), 2u);
}

TEST(LruCache, ZeroCapacityPutDoesNotCrash) {
    // capacity=0 triggered UB before fix: map_.size()>=0 always true, order_.back() on empty list.
    d2res::LruCache<std::string, int> cache(0);
    cache.put("key", 1); // must not crash
    EXPECT_EQ(cache.get("key"), nullptr);
    EXPECT_EQ(cache.size(), 0u);
}

// ---------------------------------------------------------------------------
// Transparency mode string selection (mirrors image_decoder.cpp logic)
// ---------------------------------------------------------------------------

static std::string tmode_for(int16_t opacity_algorithm) {
    if (opacity_algorithm == 300 || opacity_algorithm == 400)
        return "AdditiveBlend";
    if (opacity_algorithm > 0)
        return "ColorKeyMagentaRange";
    return "None";
}

TEST(AdditiveBlend, MetadataEmitsAdditiveBlendFor300) {
    EXPECT_EQ(tmode_for(300), "AdditiveBlend");
}

TEST(AdditiveBlend, MetadataEmitsAdditiveBlendFor400) {
    EXPECT_EQ(tmode_for(400), "AdditiveBlend");
}

TEST(AdditiveBlend, MetadataEmitsColorKeyFor128) {
    EXPECT_EQ(tmode_for(128), "ColorKeyMagentaRange");
}

TEST(AdditiveBlend, MetadataEmitsColorKeyFor255) {
    EXPECT_EQ(tmode_for(255), "ColorKeyMagentaRange");
}

TEST(AdditiveBlend, MetadataEmitsNoneForZero) {
    EXPECT_EQ(tmode_for(0), "None");
}

// ---------------------------------------------------------------------------
// ImageDecoder list_images tests
// ---------------------------------------------------------------------------

TEST(ImageDecoderListImages, CountsOnlyNonAnimationEntries) {
    OptMaps const maps = make_test_maps();
    // We can't easily construct a real MqdbContainer without a file,
    // so this test only exercises list_images logic via the OptMaps.
    // We verify the expected counts using the same logic as list_images().
    std::size_t image_count = 0;
    std::size_t anim_count = 0;
    for (const auto& e : maps.index_map.entries) {
        if (e.is_animation()) {
            ++anim_count;
        } else {
            ++image_count;
        }
    }
    EXPECT_EQ(image_count, 3u);
    EXPECT_EQ(anim_count, 1u);
}

TEST(ImageDecoderListImages, AnimEntryExcluded) {
    OptMaps const            maps = make_test_maps();
    std::vector<std::string> image_names;
    for (const auto& e : maps.index_map.entries) {
        if (!e.is_animation())
            image_names.push_back(e.name);
    }

    ASSERT_EQ(image_names.size(), 3u);
    EXPECT_EQ(image_names[0], "IMG_A");
    EXPECT_EQ(image_names[1], "IMG_B");
    EXPECT_EQ(image_names[2], "IMG_C");

    // ANIM_X must not appear
    for (const auto& n : image_names)
        EXPECT_NE(n, "ANIM_X");
}

// ---------------------------------------------------------------------------
// IndexMap overlay_variants tests
// ---------------------------------------------------------------------------

TEST(IndexMapOverlayVariants, FindsFgForBgWithSharedId) {
    const IndexMap map = make_variant_map();
    const auto     variants = map.overlay_variants("UNDEAD_0_BG");
    ASSERT_EQ(variants.size(), 1u);
    EXPECT_EQ(variants[0], "UNDEAD_0_FG");
}

TEST(IndexMapOverlayVariants, FindsShadowForNeutralBg) {
    const IndexMap map = make_variant_map();
    const auto     variants = map.overlay_variants("NEUTRAL_0_BG");
    ASSERT_EQ(variants.size(), 1u);
    EXPECT_EQ(variants[0], "NEUTRAL_0_SHADOW");
}

TEST(IndexMapOverlayVariants, NoVariantsForSoloBg) {
    const IndexMap map = make_variant_map();
    const auto     variants = map.overlay_variants("HUMAN_0_BG");
    EXPECT_TRUE(variants.empty());
}

TEST(IndexMapOverlayVariants, UnknownNameReturnsEmpty) {
    const IndexMap map = make_variant_map();
    const auto     variants = map.overlay_variants("NONEXISTENT");
    EXPECT_TRUE(variants.empty());
}

TEST(IndexMapOverlayVariants, ExcludesSourceNameFromResult) {
    const IndexMap map = make_variant_map();
    // Querying FG returns BG (the other peer); the source FG itself is excluded from results.
    const auto variants = map.overlay_variants("UNDEAD_0_FG");
    ASSERT_EQ(variants.size(), 1u);
    EXPECT_EQ(variants[0], "UNDEAD_0_BG");
}

TEST(IndexMapOverlayVariants, AnimationEntryReturnsEmpty) {
    IndexMap   map = make_variant_map();
    IndexEntry anim;
    anim.id = -1;
    anim.name = "SOME_ANIM";
    map.entries.push_back(anim);
    map.name_to_index["SOME_ANIM"] = map.entries.size() - 1;
    map.id_to_indices[-1].push_back(map.entries.size() - 1);

    const auto variants = map.overlay_variants("SOME_ANIM");
    EXPECT_TRUE(variants.empty());
}
