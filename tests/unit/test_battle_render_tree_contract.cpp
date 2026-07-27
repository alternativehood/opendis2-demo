#include <gtest/gtest.h>

#include "d2engine/battle_view/battle_render_tree_contract.hpp"
#include "d2engine/battle_view/battle_renderer.hpp"
#include "d2engine/render/render_tree.hpp"

#include <cstddef>

namespace d2engine {

TEST(BattleRenderTreeContract, ValidatesRequiredBattleNodesThroughRenderTree) {
    RenderTree tree;
    tree.set_node("/battlefield", RenderTreeNode{.kind = "battlefield"});
    for (const BattleSlotCoord coord : kBattlefieldLayoutCoords) {
        tree.set_node(battlefield_slot_tree_path(coord), RenderTreeNode{.kind = "slot"});
        tree.set_node(battlefield_unit_tree_path(coord), RenderTreeNode{.kind = "unit"});
    }

    EXPECT_NO_THROW(validate_required_battle_nodes(tree));
}

TEST(BattleRenderLayer, PreservesBattleRenderPassOrder) {
    const auto order = BattleRenderer::pass_order();
    for (std::size_t i = 0; i < order.size(); ++i) {
        EXPECT_EQ(battle_render_layer(order[i]).order, static_cast<int>(i));
    }
}

} // namespace d2engine
