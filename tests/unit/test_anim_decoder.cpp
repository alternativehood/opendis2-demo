#include <gtest/gtest.h>
#include "d2res/opt_anims.hpp"
#include "d2res/opt_maps.hpp"

using namespace d2res;

// Build a minimal OptMaps with one 2-frame animation.
static OptMaps make_test_maps() {
    OptMaps maps;

    // IndexMap: 2 image entries + 1 animation entry
    IndexEntry img_xe;
    img_xe.id = 10;
    img_xe.name = "XE";
    IndexEntry img_ye;
    img_ye.id = 11;
    img_ye.name = "YE";
    IndexEntry anim;
    anim.id = -1;
    anim.name = "ANIM_A";
    maps.index_map.entries = {img_xe, img_ye, anim};
    maps.index_map.name_to_index["XE"] = 0;
    maps.index_map.name_to_index["YE"] = 1;
    maps.index_map.name_to_index["ANIM_A"] = 2;

    // AnimMap: one block with 2 frames
    AnimBlock block;
    block.name = "ANIM_A";
    block.frames = {"XE", "YE"};
    maps.anim_map.blocks = {block};
    maps.anim_map.name_to_block["ANIM_A"] = 0;

    return maps;
}

TEST(AnimDecoder, ListAnimationsReturnsBlockNames) {
    const OptMaps maps = make_test_maps();
    // Verify AnimMap has expected block
    ASSERT_EQ(maps.anim_map.blocks.size(), 1u);
    EXPECT_EQ(maps.anim_map.blocks[0].name, "ANIM_A");
    EXPECT_EQ(maps.anim_map.blocks[0].frames.size(), 2u);
}

TEST(AnimDecoder, FrameOrderMatchesAnimBlock) {
    const OptMaps maps = make_test_maps();
    const auto&   block = maps.anim_map.blocks[0];
    EXPECT_EQ(block.frames[0], "XE");
    EXPECT_EQ(block.frames[1], "YE");
}

TEST(AnimDecoder, AnimMapLookupByName) {
    const OptMaps maps = make_test_maps();
    auto          it = maps.anim_map.name_to_block.find("ANIM_A");
    ASSERT_NE(it, maps.anim_map.name_to_block.end());
    EXPECT_EQ(it->second, 0u);
}
