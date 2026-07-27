#include "ruin_contributor.hpp"

#include "adventure_banner_primitive_builder.hpp"
#include <d2adventure_render/map_geometry.hpp>
#include <d2runtime/AdventureWorldState.hpp>

#include <cstddef>
#include <algorithm>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace d2engine::adventure_render {

namespace {

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

[[nodiscard]] std::string placement_name(d2runtime::AdventureSurfacePlacement placement) {
    return placement == d2runtime::AdventureSurfacePlacement::Water ? "water" : "land";
}

} // namespace

RenderContributor make_ruin_contributor(const RuinAssetCatalog&        catalog,
                                        const StackBannerAssetCatalog& banner_catalog) {
    return [&catalog, &banner_catalog](const d2runtime::AdventureWorldState& world,
                                       PreparationContext&                   ctx) {
        const auto& geo = ctx.geometry();

        for (const auto& ruin : world.ruins) {
            const auto* visual = catalog.find(ruin.image, ruin.placement);
            if (visual == nullptr) {
                ctx.add_diagnostic(
                    {PrepareDiagnosticKind::UnresolvedNoSprite, ruin.id, "Ruin", ruin.image, -1,
                     "unresolved Ruin id=" + ruin.id + " image=" + std::to_string(ruin.image) +
                         " placement=" + placement_name(ruin.placement) + " position=(" +
                         std::to_string(ruin.position.x) + "," + std::to_string(ruin.position.y) +
                         ") expected_range=0..10"});
                continue;
            }

            if (ruin.footprint.empty()) {
                const auto asset_name = std::visit(
                    [](const auto& candidate) -> const std::string& {
                        if constexpr (std::is_same_v<std::decay_t<decltype(candidate)>,
                                                     StaticRuinVisual>)
                            return candidate.logical_sprite;
                        else
                            return candidate.logical_animation;
                    },
                    *visual);
                ctx.add_diagnostic(
                    {PrepareDiagnosticKind::UnresolvedNoSprite, ruin.id, "Ruin", ruin.image, -1,
                     "unresolved Ruin id=" + ruin.id + " image=" + std::to_string(ruin.image) +
                         " asset=" + asset_name + " reason=empty_footprint"});
                continue;
            }

            GridFootprint footprint;
            footprint.reserve(ruin.footprint.size());
            for (const auto& cell : ruin.footprint) {
                footprint.push_back(cell);
            }

            const auto  depth_anchor = AdventureMapGeometry::derive_depth_anchor(footprint);
            const auto  foot_anchor = geo.cell_foot_anchor(depth_anchor);
            const auto& banner_asset = banner_catalog.resolve_banner(4);

            PreparedAdventureRenderPrimitive prim;
            prim.stable_id = stable_render_id("Ruin:" + ruin.id);
            prim.debug_label = "Ruin:" + ruin.id;
            prim.semantic_role = AdventurePrimitiveRole::RuinBody;
            prim.semantic_object_id = ruin.id;
            prim.phase = AdventureRenderPhase::World;
            prim.level = WorldRenderLevel::Structure;
            prim.local_suborder = 0;
            prim.footprint = footprint;
            prim.depth_anchor = depth_anchor;
            prim.alpha = 1.0f;
            std::string asset_description;
            std::visit(
                [&](const auto& candidate) {
                    using Visual = std::decay_t<decltype(candidate)>;
                    prim.container_path = candidate.container_path;
                    if constexpr (std::is_same_v<Visual, StaticRuinVisual>) {
                        prim.record_name = candidate.logical_sprite;
                        prim.draw_origin = {foot_anchor.x - candidate.canvas_foot_x,
                                            foot_anchor.y - candidate.canvas_foot_y};
                        prim.visual_bounds = {prim.draw_origin.x, prim.draw_origin.y,
                                              prim.draw_origin.x + candidate.canvas_width,
                                              prim.draw_origin.y + candidate.canvas_height};
                        prim.src_width = candidate.canvas_width;
                        prim.src_height = candidate.canvas_height;
                        asset_description = "static=" + candidate.logical_sprite;
                    } else {
                        prim.record_name = candidate.animation.frames.front().record_name;
                        prim.animation = candidate.animation;
                        int max_width = 0;
                        int max_height = 0;
                        for (const auto& frame : candidate.animation.frames) {
                            max_width = std::max(max_width, frame.canvas_width);
                            max_height = std::max(max_height, frame.canvas_height);
                        }
                        prim.draw_origin = {foot_anchor.x - candidate.canvas_foot_x,
                                            foot_anchor.y - candidate.canvas_foot_y};
                        prim.visual_bounds = {prim.draw_origin.x, prim.draw_origin.y,
                                              prim.draw_origin.x + max_width,
                                              prim.draw_origin.y + max_height};
                        prim.src_width = candidate.animation.frames.front().canvas_width;
                        prim.src_height = candidate.animation.frames.front().canvas_height;
                        asset_description =
                            "animated=" + candidate.logical_animation +
                            " frame_count=" + std::to_string(candidate.animation.frames.size());
                    }
                },
                *visual);
            const auto content_bounds = std::visit(
                [](const auto& candidate) {
                    using Visual = std::decay_t<decltype(candidate)>;
                    if constexpr (std::is_same_v<Visual, StaticRuinVisual>) {
                        return candidate.content_bounds;
                    } else {
                        return candidate.animation.frames.front().content_bounds;
                    }
                },
                *visual);
            const auto banner = build_adventure_banner_primitive(
                prim, content_bounds, banner_asset, AdventureBannerDockSide::LeftOfReference,
                stable_render_id("RuinBanner:" + ruin.id), "RuinBanner:" + ruin.id + ":4",
                AdventurePrimitiveRole::RuinBanner, ruin.id, WorldRenderLevel::Structure, 1,
                footprint, depth_anchor);
            ctx.add_primitive(std::move(prim));
            ctx.add_primitive(std::move(banner));

            ctx.add_diagnostic(
                {PrepareDiagnosticKind::Resolved, ruin.id, "Ruin", ruin.image,
                 static_cast<int>(footprint.size()),
                 "resolved Ruin id=" + ruin.id + " image=" + std::to_string(ruin.image) +
                     " placement=" + placement_name(ruin.placement) + " " + asset_description +
                     " footprint_cells=" + describe_cells(footprint) + " depth_anchor=(" +
                     std::to_string(depth_anchor.x) + "," + std::to_string(depth_anchor.y) + ")"});
        }
    };
}

} // namespace d2engine::adventure_render
