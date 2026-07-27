#include "resource_node_contributor.hpp"

#include <d2adventure_render/adventure_render_types.hpp>
#include <d2adventure_render/map_geometry.hpp>
#include <d2adventure_render/preparation_context.hpp>
#include <d2runtime/AdventureWorldState.hpp>

#include <algorithm>
#include <stdexcept>
#include <string>

namespace d2engine::adventure_render {

RenderContributor make_resource_node_contributor(const ResourceNodeAssetCatalog& catalog) {
    return [&catalog](const d2runtime::AdventureWorldState& world, PreparationContext& ctx) {
        const auto& geo = ctx.geometry();

        for (const auto& node : world.resource_nodes) {
            const auto& visual = catalog.resolve(node.resource_kind);

            GridFootprint fp;
            fp.reserve(node.footprint.size());
            for (const auto& cell : node.footprint)
                fp.push_back(cell);

            const auto anchor = AdventureMapGeometry::derive_depth_anchor(fp);
            const auto foot = geo.cell_foot_anchor(anchor);

            const auto stable_id = stable_render_id("ResourceNode:" + node.id);

            std::string debug_label;
            switch (node.resource_kind) {
            case d2runtime::AdventureResourceKind::GoldMine:
                debug_label = "GoldMine";
                break;
            case d2runtime::AdventureResourceKind::RedMana:
                debug_label = "RedMana";
                break;
            case d2runtime::AdventureResourceKind::YellowMana:
                debug_label = "YellowMana";
                break;
            case d2runtime::AdventureResourceKind::OrangeMana:
                debug_label = "OrangeMana";
                break;
            case d2runtime::AdventureResourceKind::WhiteMana:
                debug_label = "WhiteMana";
                break;
            case d2runtime::AdventureResourceKind::BlueMana:
                debug_label = "BlueMana";
                break;
            }

            PreparedAdventureRenderPrimitive prim;
            prim.stable_id = stable_id;
            prim.debug_label = debug_label + ":" + node.id;
            prim.phase = AdventureRenderPhase::World;
            prim.level = WorldRenderLevel::Structure;
            prim.footprint = fp;
            prim.depth_anchor = anchor;

            if (auto* sv = std::get_if<StaticResourceNodeVisual>(&visual)) {
                prim.container_path = sv->container_path;
                prim.record_name = sv->logical_sprite;
                prim.draw_origin = {foot.x - sv->canvas_foot_x, foot.y - sv->canvas_foot_y};
                prim.visual_bounds = {prim.draw_origin.x, prim.draw_origin.y,
                                      prim.draw_origin.x + sv->canvas_width,
                                      prim.draw_origin.y + sv->canvas_height};
                prim.src_width = sv->canvas_width;
                prim.src_height = sv->canvas_height;
            } else if (auto* av = std::get_if<AnimatedResourceNodeVisual>(&visual)) {
                const auto& frames = av->animation_data.frames;
                int         max_w = 0;
                int         max_h = 0;
                for (const auto& f : frames) {
                    max_w = std::max(max_w, f.canvas_width);
                    max_h = std::max(max_h, f.canvas_height);
                }

                prim.container_path = av->container_path;
                prim.record_name = frames.empty() ? "" : frames[0].record_name;
                prim.animation = av->animation_data;
                prim.draw_origin = {foot.x - av->canvas_foot_x, foot.y - av->canvas_foot_y};
                prim.visual_bounds = {prim.draw_origin.x, prim.draw_origin.y,
                                      prim.draw_origin.x + max_w, prim.draw_origin.y + max_h};
                prim.src_width = frames.empty() ? 0 : frames[0].canvas_width;
                prim.src_height = frames.empty() ? 0 : frames[0].canvas_height;
            }

            ctx.add_primitive(std::move(prim));

            // Diagnostic
            std::string sprite_info;
            if (auto* sv = std::get_if<StaticResourceNodeVisual>(&visual)) {
                sprite_info = sv->logical_sprite;
            } else if (auto* av = std::get_if<AnimatedResourceNodeVisual>(&visual)) {
                sprite_info = av->logical_animation + " (anim, " +
                              std::to_string(av->animation_data.frames.size()) + " frames)";
            }

            ctx.add_diagnostic(
                {PrepareDiagnosticKind::Resolved, node.id, "ResourceNode",
                 static_cast<int>(node.resource_kind), node.raw_resource,
                 "resolved ResourceNode id=" + node.id + " kind=" + debug_label + " pos=(" +
                     std::to_string(node.position.x) + "," + std::to_string(node.position.y) + ")" +
                     " raw_resource=" + std::to_string(node.raw_resource) +
                     " visual=" + sprite_info + " depth_anchor=(" + std::to_string(anchor.x) + "," +
                     std::to_string(anchor.y) + ")"});
        }
    };
}

} // namespace d2engine::adventure_render
