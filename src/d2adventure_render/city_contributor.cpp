#include "city_contributor.hpp"

#include <d2adventure_render/map_geometry.hpp>
#include <d2adventure_render/preparation_context.hpp>
#include <d2runtime/AdventureWorldState.hpp>

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>

namespace d2engine::adventure_render {

namespace {

[[nodiscard]] std::string city_visual_tier_name(CityVisualTier tier) {
    switch (tier) {
    case CityVisualTier::Small:
        return "Small";
    case CityVisualTier::Medium:
        return "Medium";
    case CityVisualTier::Large:
        return "Large";
    }
    return "Unknown";
}

[[nodiscard]] CityVisualTier city_visual_tier_for_size(int size) {
    switch (size) {
    case 1:
        return CityVisualTier::Small;
    case 2:
        return CityVisualTier::Medium;
    case 3:
    case 4:
    case 5:
        return CityVisualTier::Large;
    default:
        throw std::runtime_error("make_city_contributor: unsupported city size=" +
                                 std::to_string(size));
    }
}

[[nodiscard]] int visual_width(const AnimatedCityLayer& layer) {
    int max_w = 0;
    for (const auto& frame : layer.animation.frames) {
        max_w = std::max(max_w, frame.canvas_width);
    }
    return max_w;
}

[[nodiscard]] int visual_height(const AnimatedCityLayer& layer) {
    int max_h = 0;
    for (const auto& frame : layer.animation.frames) {
        max_h = std::max(max_h, frame.canvas_height);
    }
    return max_h;
}

[[nodiscard]] std::string describe_footprint(const GridFootprint& fp) {
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

void populate_layer_primitive(PreparedAdventureRenderPrimitive& prim,
                              const AnimatedCityLayer& layer, const ScreenPoint& foot_anchor) {
    prim.container_path = layer.container_path;
    prim.animation = layer.animation;
    prim.draw_origin = {foot_anchor.x - layer.canvas_foot_x, foot_anchor.y - layer.canvas_foot_y};
    const int w = visual_width(layer);
    const int h = visual_height(layer);
    prim.visual_bounds = {prim.draw_origin.x, prim.draw_origin.y, prim.draw_origin.x + w,
                          prim.draw_origin.y + h};
    prim.src_width = layer.animation.native_canvas_w;
    prim.src_height = layer.animation.native_canvas_h;
    prim.alpha = 1.0f;
}

} // namespace

RenderContributor make_city_contributor(const CityAssetCatalog& catalog) {
    return [&catalog](const d2runtime::AdventureWorldState& world, PreparationContext& ctx) {
        const auto& geo = ctx.geometry();

        for (const auto& city : world.cities) {
            if (city.footprint.empty()) {
                ctx.add_diagnostic({PrepareDiagnosticKind::UnresolvedNoSprite, city.id, "City",
                                    city.size, city.ai_priority,
                                    "unresolved City id=" + city.id + " reason=empty_footprint" +
                                        " pos=(" + std::to_string(city.position.x) + "," +
                                        std::to_string(city.position.y) +
                                        ") size=" + std::to_string(city.size)});
                continue;
            }

            GridFootprint fp;
            fp.reserve(city.footprint.size());
            for (const auto& cell : city.footprint) {
                fp.push_back(cell);
            }

            const auto  anchor = AdventureMapGeometry::derive_depth_anchor(fp);
            const auto  foot = geo.cell_foot_anchor(anchor);
            const auto& visual = catalog.resolve_for_size(city.size);
            const auto  tier = city_visual_tier_for_size(city.size);

            const auto body_stable_id = stable_render_id("City:" + city.id + ":body");

            PreparedAdventureRenderPrimitive body;
            body.stable_id = body_stable_id;
            body.debug_label = "City:" + city.id + ":body";
            body.phase = AdventureRenderPhase::World;
            body.level = WorldRenderLevel::Structure;
            body.local_suborder = 0;
            body.footprint = fp;
            body.depth_anchor = anchor;
            populate_layer_primitive(body, visual.body, foot);
            ctx.add_primitive(std::move(body));

            if (visual.shadow.has_value()) {
                PreparedAdventureRenderPrimitive shadow;
                shadow.stable_id = stable_render_id("City:" + city.id + ":shadow");
                shadow.debug_label = "City:" + city.id + ":shadow";
                shadow.phase = AdventureRenderPhase::World;
                shadow.level = WorldRenderLevel::Structure;
                shadow.local_suborder = -1;
                shadow.footprint = fp;
                shadow.depth_anchor = anchor;
                populate_layer_primitive(shadow, *visual.shadow, foot);
                shadow.animation_sync_source_id = body_stable_id;
                shadow.alpha = 1.0f;
                ctx.add_primitive(std::move(shadow));
            }

            std::string shadow_animation = "none";
            if (visual.shadow.has_value()) {
                shadow_animation = visual.shadow->logical_animation;
            }

            ctx.add_diagnostic(
                {PrepareDiagnosticKind::Resolved, city.id, "City", city.size,
                 static_cast<int>(tier),
                 "resolved City id=" + city.id + " size=" + std::to_string(city.size) +
                     " tier=" + city_visual_tier_name(tier) + " body_animation=" +
                     visual.body.logical_animation + " shadow_animation=" + shadow_animation +
                     " footprint_cells=" + describe_footprint(fp) + " depth_anchor=(" +
                     std::to_string(anchor.x) + "," + std::to_string(anchor.y) + ")"});
        }
    };
}

} // namespace d2engine::adventure_render
