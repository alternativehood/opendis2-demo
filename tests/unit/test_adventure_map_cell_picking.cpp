#include <d2adventure_render/map_geometry.hpp>

#include <gtest/gtest.h>

TEST(AdventureMapCellPicking, ResolvesCellCentersWithoutTranspose) {
    const auto geometry = d2engine::adventure_render::AdventureMapGeometry::from_source(9, 5);
    for (int y = 0; y < geometry.map_height; ++y) {
        for (int x = 0; x < geometry.map_width; ++x) {
            const auto origin = geometry.cell_canvas_origin({x, y});
            EXPECT_EQ(geometry.canvas_to_cell(origin.x + geometry.half_tile_width,
                                              origin.y + geometry.half_tile_height),
                      (d2runtime::MapCellCoord{x, y}));
        }
    }
}

TEST(AdventureMapCellPicking, ResolvesOneByOneCenterAndMapBoundaries) {
    const auto one = d2engine::adventure_render::AdventureMapGeometry::from_source(1, 1);
    const auto origin = one.cell_canvas_origin({0, 0});
    EXPECT_EQ(one.canvas_to_cell(origin.x + one.half_tile_width, origin.y + one.half_tile_height),
              (d2runtime::MapCellCoord{0, 0}));

    const auto geometry = d2engine::adventure_render::AdventureMapGeometry::from_source(9, 5);
    EXPECT_EQ(geometry.canvas_to_cell(geometry.cell_canvas_origin({0, 0}).x + 32,
                                      geometry.cell_canvas_origin({0, 0}).y + 16),
              (d2runtime::MapCellCoord{0, 0}));
    EXPECT_EQ(geometry.canvas_to_cell(geometry.cell_canvas_origin({8, 4}).x + 32,
                                      geometry.cell_canvas_origin({8, 4}).y + 16),
              (d2runtime::MapCellCoord{8, 4}));
}

TEST(AdventureMapCellPicking, InteriorAndOutsidePointsFollowDiamondMembership) {
    const auto geometry = d2engine::adventure_render::AdventureMapGeometry::from_source(1, 1);
    const auto origin = geometry.cell_canvas_origin({0, 0});
    EXPECT_EQ(geometry.canvas_to_cell(origin.x + geometry.half_tile_width, origin.y + 1),
              (d2runtime::MapCellCoord{0, 0}));
    EXPECT_EQ(geometry.canvas_to_cell(origin.x + 1, origin.y + geometry.half_tile_height),
              (d2runtime::MapCellCoord{0, 0}));
    EXPECT_FALSE(geometry.canvas_to_cell(origin.x, origin.y));
}

TEST(AdventureMapCellPicking, SharedBoundaryUsesFrontmostDeterministicCell) {
    const auto geometry = d2engine::adventure_render::AdventureMapGeometry::from_source(2, 2);
    const auto first = geometry.cell_canvas_origin({0, 0});
    const int  edge_x = first.x + geometry.tile_width;
    const int  edge_y = first.y + geometry.half_tile_height;
    EXPECT_EQ(geometry.canvas_to_cell(edge_x, edge_y), (d2runtime::MapCellCoord{1, 0}));
    EXPECT_EQ(geometry.canvas_to_cell(edge_x, edge_y), geometry.canvas_to_cell(edge_x, edge_y));
}

TEST(AdventureMapCellPicking, SharedVertexUsesDepthXAndYTieBreak) {
    const auto geometry = d2engine::adventure_render::AdventureMapGeometry::from_source(3, 3);
    const auto origin = geometry.cell_canvas_origin({1, 1});
    const int  vertex_x = origin.x + geometry.half_tile_width;
    const int  vertex_y = origin.y;
    const auto expected = geometry.canvas_to_cell(vertex_x, vertex_y);
    ASSERT_TRUE(expected.has_value());
    EXPECT_EQ(*expected, geometry.canvas_to_cell(vertex_x, vertex_y));
    EXPECT_EQ(*expected, (d2runtime::MapCellCoord{1, 1}));
}

TEST(AdventureMapCellPicking, SideInteriorAndOutsideBoundingBoxAreStable) {
    const auto geometry = d2engine::adventure_render::AdventureMapGeometry::from_source(1, 1);
    const auto origin = geometry.cell_canvas_origin({0, 0});
    EXPECT_EQ(geometry.canvas_to_cell(origin.x + geometry.half_tile_width, origin.y + 1),
              (d2runtime::MapCellCoord{0, 0}));
    EXPECT_EQ(geometry.canvas_to_cell(origin.x + geometry.half_tile_width,
                                      origin.y + geometry.tile_height - 1),
              (d2runtime::MapCellCoord{0, 0}));
    EXPECT_FALSE(geometry.canvas_to_cell(origin.x + 1, origin.y + 1));
    EXPECT_FALSE(geometry.canvas_to_cell(-1, -1));
    EXPECT_FALSE(geometry.canvas_to_cell(geometry.canvas_width, geometry.canvas_height));
}

TEST(AdventureMapCellPicking, NonDefaultTileDimensionsRoundTripEveryCell) {
    const auto geometry =
        d2engine::adventure_render::AdventureMapGeometry::from_source(9, 5, 80, 40);
    for (int y = 0; y < geometry.map_height; ++y) {
        for (int x = 0; x < geometry.map_width; ++x) {
            const auto origin = geometry.cell_canvas_origin({x, y});
            EXPECT_EQ(geometry.canvas_to_cell(origin.x + geometry.half_tile_width,
                                              origin.y + geometry.half_tile_height),
                      (d2runtime::MapCellCoord{x, y}));
        }
    }
}

TEST(AdventureMapCellPicking, InvalidGeometryAndOutsideCanvasAreRejected) {
    EXPECT_FALSE(
        d2engine::adventure_render::AdventureMapGeometry::from_source(0, 1).canvas_to_cell(0, 0));
    EXPECT_FALSE(d2engine::adventure_render::AdventureMapGeometry::from_source(1, 1, 0, 32)
                     .canvas_to_cell(0, 0));
    const auto geometry = d2engine::adventure_render::AdventureMapGeometry::from_source(1, 1);
    EXPECT_FALSE(geometry.canvas_to_cell(-100, -100));
    EXPECT_FALSE(geometry.canvas_to_cell(1000, 1000));
}
