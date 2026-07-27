#include <d2adventure_render/terrain/adventure_terrain_border_shape_resolver.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <string>

namespace {} // namespace

TEST(AdventureTerrainBorderShapeResolver, Topology3x3LandUsesExplicitNeMapping) {
    const d2engine::AdventureTerrainBorderShapeResolver resolver;

    const auto operation = resolver.resolve_topology3x3_operation({
        .center_code = "HU",
        .target_code = "HE",
        .family = "NE",
        .cardinal_mask = 0x02,
        .diagonal_mask = 0x00,
    });

    ASSERT_TRUE(operation.has_value());
    EXPECT_TRUE(operation->drawable);
    EXPECT_EQ(operation->source, "explicit_ne_topology");
    EXPECT_EQ(operation->family, "NE");
    EXPECT_EQ(operation->logical_shape, 18);
    EXPECT_EQ(operation->record_shape, 18);
    EXPECT_EQ(operation->compose_mode, d2engine::AdventureTerrainBorderComposeMode::MaskBlend);
}

TEST(AdventureTerrainBorderShapeResolver, ExplicitNeDirectionBitsUseDisplaySpace) {
    const d2engine::AdventureTerrainBorderShapeResolver resolver;
    struct Case {
        std::uint8_t cardinal = 0;
        std::uint8_t diagonal = 0;
        std::uint8_t shape = 0;
    };
    static constexpr std::array cases = {
        Case{0x0, 0x1, 1},  Case{0x0, 0x2, 2},  Case{0x0, 0x4, 4},  Case{0x0, 0x8, 8},
        Case{0x1, 0x0, 17}, Case{0x2, 0x0, 18}, Case{0x4, 0x0, 20}, Case{0x8, 0x0, 24},
    };

    for (const auto& item : cases) {
        const auto operation = resolver.resolve_topology3x3_operation({
            .center_code = "NE",
            .target_code = "HU",
            .family = "NE",
            .cardinal_mask = item.cardinal,
            .diagonal_mask = item.diagonal,
        });

        ASSERT_TRUE(operation.has_value()) << static_cast<int>(item.shape);
        EXPECT_EQ(operation->record_shape, item.shape);
    }
}

TEST(AdventureTerrainBorderShapeResolver, ExplicitNeMappingCoversProductionTable) {
    const d2engine::AdventureTerrainBorderShapeResolver resolver;
    struct Case {
        std::uint8_t cardinal = 0;
        std::uint8_t diagonal = 0;
        std::uint8_t shape = 0;
    };
    static constexpr std::array cases = {
        Case{0x0, 0x1, 1},  Case{0x0, 0x2, 2},  Case{0x0, 0x3, 3},  Case{0x0, 0x4, 4},
        Case{0xF, 0x5, 5},  Case{0x7, 0x6, 6},  Case{0xF, 0x7, 7},  Case{0x0, 0x8, 8},
        Case{0xD, 0x9, 9},  Case{0xF, 0xA, 10}, Case{0xF, 0xB, 11}, Case{0xE, 0xC, 12},
        Case{0xF, 0xD, 13}, Case{0xF, 0xE, 14}, Case{0x0, 0xF, 15}, Case{0x1, 0x0, 17},
        Case{0x2, 0x0, 18}, Case{0x3, 0x0, 19}, Case{0x4, 0x0, 20}, Case{0x5, 0x0, 21},
        Case{0x6, 0x0, 22}, Case{0x7, 0x0, 23}, Case{0x8, 0x0, 24}, Case{0x9, 0x0, 25},
        Case{0xA, 0x0, 26}, Case{0xB, 0x0, 27}, Case{0xC, 0x0, 28}, Case{0xD, 0x0, 29},
        Case{0xE, 0x0, 30}, Case{0xF, 0x0, 31},
    };

    for (const auto& item : cases) {
        const auto operation = resolver.resolve_topology3x3_operation({
            .center_code = "NE",
            .target_code = "HU",
            .family = "NE",
            .cardinal_mask = item.cardinal,
            .diagonal_mask = item.diagonal,
        });

        ASSERT_TRUE(operation.has_value()) << static_cast<int>(item.shape);
        EXPECT_EQ(operation->source, "explicit_ne_topology");
        EXPECT_EQ(operation->record_shape, item.shape);
    }
}

TEST(AdventureTerrainBorderShapeResolver, Topology3x3ResolvesWaterColorKeyOverlayOperation) {
    const d2engine::AdventureTerrainBorderShapeResolver resolver;

    const auto operation = resolver.resolve_topology3x3_operation({
        .center_code = "WA",
        .target_code = "HU",
        .family = "WA",
        .cardinal_mask = 0x01,
        .diagonal_mask = 0x00,
    });

    ASSERT_TRUE(operation.has_value());
    EXPECT_TRUE(operation->drawable);
    EXPECT_EQ(operation->source, "explicit_wa_topology");
    EXPECT_EQ(operation->family, "WA");
    EXPECT_EQ(operation->logical_shape, 17);
    EXPECT_EQ(operation->record_shape, 17);
    EXPECT_EQ(operation->compose_mode,
              d2engine::AdventureTerrainBorderComposeMode::ColorKeyOverlay);
}

TEST(AdventureTerrainBorderShapeResolver, ExplicitWaMappingUsesSameDisplayBitsAsNe) {
    const d2engine::AdventureTerrainBorderShapeResolver resolver;

    const auto west = resolver.resolve_topology3x3_operation({
        .center_code = "WA",
        .target_code = "HU",
        .family = "WA",
        .cardinal_mask = 0x08,
    });
    const auto south = resolver.resolve_topology3x3_operation({
        .center_code = "WA",
        .target_code = "HU",
        .family = "WA",
        .cardinal_mask = 0x04,
    });
    const auto north_west = resolver.resolve_topology3x3_operation({
        .center_code = "WA",
        .target_code = "HU",
        .family = "WA",
        .diagonal_mask = 0x01,
    });

    ASSERT_TRUE(west.has_value());
    ASSERT_TRUE(south.has_value());
    ASSERT_TRUE(north_west.has_value());
    EXPECT_EQ(west->record_shape, 24);
    EXPECT_EQ(south->record_shape, 20);
    EXPECT_EQ(north_west->record_shape, 1);
    EXPECT_EQ(north_west->source, "explicit_wa_topology");
}

TEST(AdventureTerrainBorderShapeResolver, Topology3x3NeverRequestsShape16) {
    const d2engine::AdventureTerrainBorderShapeResolver resolver;
    struct Case {
        std::uint8_t cardinal = 0;
        std::uint8_t diagonal = 0;
    };
    static constexpr std::array cases = {
        Case{0x0, 0x1}, Case{0x0, 0x2}, Case{0x0, 0x3}, Case{0x0, 0x4}, Case{0xF, 0x5},
        Case{0x7, 0x6}, Case{0xF, 0x7}, Case{0x0, 0x8}, Case{0xD, 0x9}, Case{0xF, 0xA},
        Case{0xF, 0xB}, Case{0xE, 0xC}, Case{0xF, 0xD}, Case{0xF, 0xE}, Case{0x0, 0xF},
        Case{0x1, 0x0}, Case{0x2, 0x0}, Case{0x3, 0x0}, Case{0x4, 0x0}, Case{0x5, 0x0},
        Case{0x6, 0x0}, Case{0x7, 0x0}, Case{0x8, 0x0}, Case{0x9, 0x0}, Case{0xA, 0x0},
        Case{0xB, 0x0}, Case{0xC, 0x0}, Case{0xD, 0x0}, Case{0xE, 0x0}, Case{0xF, 0x0},
    };

    for (const auto& item : cases) {
        const auto operation = resolver.resolve_topology3x3_operation({
            .center_code = "HU",
            .target_code = "NE",
            .family = "NE",
            .cardinal_mask = item.cardinal,
            .diagonal_mask = item.diagonal,
        });
        ASSERT_TRUE(operation.has_value());
        EXPECT_NE(operation->record_shape, 16);
    }
}

TEST(AdventureTerrainBorderShapeResolver, FourCornerDirectionsGiveShape15) {
    const d2engine::AdventureTerrainBorderShapeResolver resolver;

    const auto ne = resolver.resolve_topology3x3_operation({
        .center_code = "NE",
        .target_code = "HU",
        .family = "NE",
        .cardinal_mask = 0x00,
        .diagonal_mask = 0x0F,
    });
    ASSERT_TRUE(ne.has_value());
    EXPECT_EQ(ne->record_shape, 15);
    EXPECT_EQ(ne->source, "explicit_ne_topology");

    const auto wa = resolver.resolve_topology3x3_operation({
        .center_code = "WA",
        .target_code = "HU",
        .family = "WA",
        .cardinal_mask = 0x00,
        .diagonal_mask = 0x0F,
    });
    ASSERT_TRUE(wa.has_value());
    EXPECT_EQ(wa->record_shape, 15);
    EXPECT_EQ(wa->source, "explicit_wa_topology");
}

TEST(AdventureTerrainBorderShapeResolver, FourSideDirectionsGiveShape31) {
    const d2engine::AdventureTerrainBorderShapeResolver resolver;
    const auto operation = resolver.resolve_topology3x3_operation({
        .center_code = "NE",
        .target_code = "HU",
        .family = "NE",
        .cardinal_mask = 0x0F,
        .diagonal_mask = 0x00,
    });
    ASSERT_TRUE(operation.has_value());
    EXPECT_EQ(operation->record_shape, 31);
}

TEST(AdventureTerrainBorderShapeResolver, NeResolverDoesNotUseOldLogicalShapeFallback) {
    // This test verifies that the NE resolver uses explicit topology, not old fallback
    const d2engine::AdventureTerrainBorderShapeResolver resolver;
    const auto operation = resolver.resolve_topology3x3_operation({
        .center_code = "HU",
        .target_code = "NE",
        .family = "NE",
        .cardinal_mask = 0x01,
    });
    ASSERT_TRUE(operation.has_value());
    EXPECT_EQ(operation->source, "explicit_ne_topology");
}

TEST(AdventureTerrainBorderShapeResolver, DefaultConstructedResolverWorks) {
    const d2engine::AdventureTerrainBorderShapeResolver resolver;
    EXPECT_FALSE(resolver
                     .resolve_topology3x3_operation({
                         .center_code = "HU",
                         .target_code = "NE",
                         .family = "XX",
                         .cardinal_mask = 0x01,
                     })
                     .has_value());
}
