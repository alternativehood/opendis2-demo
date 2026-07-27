#include "treasure_contributor.hpp"

#include <d2adventure_render/map_geometry.hpp>
#include <d2runtime/AdventureWorldState.hpp>

#include <cstddef>
#include <string>
#include <utility>

namespace d2engine::adventure_render {

namespace {

[[nodiscard]] const char* placement_name(d2runtime::AdventureTreasurePlacement placement) {
    switch (placement) {
    case d2runtime::AdventureTreasurePlacement::Land:
        return "Land";
    case d2runtime::AdventureTreasurePlacement::Water:
        return "Water";
    }
    return "Land";
}

[[nodiscard]] std::string describe_cells(const GridFootprint& fp) {
    std::string out = "[";
    for (std::size_t i = 0; i < fp.size(); ++i) {
        if (i > 0) {
            out += ",";
        }
        out += "(" + std::to_string(fp[i].x) + "," + std::to_string(fp[i].y) + ")";
    }
    out += "]";
    return out;
}

} // namespace

RenderContributor make_treasure_contributor(const TreasureAssetCatalog& catalog) {
    return [&catalog](const d2runtime::AdventureWorldState& world, PreparationContext& ctx) {
        const auto& geo = ctx.geometry();

        for (const auto& treasure : world.treasures) {
            const auto* visual = catalog.find(treasure.placement, treasure.image);
            if (visual == nullptr) {
                ctx.add_diagnostic({PrepareDiagnosticKind::UnresolvedNoSprite, treasure.id,
                                    "Treasure", treasure.image, -1,
                                    "unresolved Treasure id=" + treasure.id +
                                        " placement=" + placement_name(treasure.placement) +
                                        " image=" + std::to_string(treasure.image)});
                continue;
            }

            if (treasure.footprint.empty()) {
                ctx.add_diagnostic({PrepareDiagnosticKind::UnresolvedNoSprite, treasure.id,
                                    "Treasure", treasure.image, -1,
                                    "unresolved Treasure id=" + treasure.id +
                                        " image=" + std::to_string(treasure.image) +
                                        " placement=" + placement_name(treasure.placement) +
                                        " sprite=" + visual->logical_sprite +
                                        " reason=empty_footprint"});
                continue;
            }

            GridFootprint footprint;
            footprint.reserve(treasure.footprint.size());
            for (const auto& cell : treasure.footprint) {
                footprint.push_back(cell);
            }

            const auto depth_anchor = AdventureMapGeometry::derive_depth_anchor(footprint);
            const auto foot_anchor = geo.cell_foot_anchor(depth_anchor);

            PreparedAdventureRenderPrimitive prim;
            prim.stable_id = stable_render_id("Treasure:" + treasure.id);
            prim.debug_label = "Treasure:" + treasure.id;
            prim.phase = AdventureRenderPhase::World;
            prim.level = WorldRenderLevel::GroundObject;
            prim.local_suborder = 0;
            prim.container_path = visual->container_path;
            prim.record_name = visual->logical_sprite;
            prim.draw_origin = {foot_anchor.x - visual->canvas_foot_x,
                                foot_anchor.y - visual->canvas_foot_y};
            prim.visual_bounds = {prim.draw_origin.x, prim.draw_origin.y,
                                  prim.draw_origin.x + visual->canvas_width,
                                  prim.draw_origin.y + visual->canvas_height};
            prim.footprint = footprint;
            prim.depth_anchor = depth_anchor;
            prim.src_width = visual->canvas_width;
            prim.src_height = visual->canvas_height;
            prim.alpha = 1.0f;
            ctx.add_primitive(std::move(prim));

            ctx.add_diagnostic(
                {PrepareDiagnosticKind::Resolved, treasure.id, "Treasure", treasure.image,
                 static_cast<int>(footprint.size()),
                 "resolved Treasure id=" + treasure.id +
                     " placement=" + placement_name(treasure.placement) + " image=" +
                     std::to_string(treasure.image) + " sprite=" + visual->logical_sprite +
                     " footprint_cells=" + describe_cells(footprint) + " depth_anchor=(" +
                     std::to_string(depth_anchor.x) + "," + std::to_string(depth_anchor.y) + ")"});
        }
    };
}

} // namespace d2engine::adventure_render
