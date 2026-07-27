#include <gtest/gtest.h>

#include "d2engine/render/render_tree.hpp"

#include <nlohmann/json.hpp>

namespace d2engine {

TEST(RenderTree, ComposeWorksWithoutBattleLayoutTypes) {
    RenderTree tree;
    tree.set_node("/root", RenderTreeNode{.x = 10.0f, .y = 20.0f, .w = 100.0f, .h = 50.0f});
    tree.set_node("/root/child",
                  RenderTreeNode{.x = 3.0f, .y = 4.0f, .w = 20.0f, .h = 10.0f, .alpha = 0.5f});

    const Rect expected{.x = 13.0f, .y = 24.0f, .w = 20.0f, .h = 10.0f};
    EXPECT_EQ(tree.compose("/root/child"), expected);
}

TEST(RenderTree, PathLookupWorksWithoutBattleLayoutTypes) {
    RenderTree tree;
    tree.set_node("/ui/panel", RenderTreeNode{.kind = "panel"});

    EXPECT_TRUE(tree.has_node("/ui/panel"));
    EXPECT_EQ(tree.node("/ui/panel")->kind, "panel");
    EXPECT_FALSE(tree.node("/missing").has_value());
}

TEST(RenderTree, JsonRoundTripWorksWithoutBattleLayoutTypes) {
    RenderTree tree;
    tree.load(R"({"/text":{"kind":"text","x":1,"color":{"r":2,"g":3,"b":4,"a":5}}})"_json);

    nlohmann::json saved;
    tree.save(saved);

    RenderTree loaded;
    loaded.load(saved);
    ASSERT_TRUE(loaded.node("/text").has_value());
    EXPECT_EQ(loaded.node("/text")->color.r, 2);
}

} // namespace d2engine
