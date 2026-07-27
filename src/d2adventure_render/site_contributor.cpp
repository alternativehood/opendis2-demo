#include "site_contributor.hpp"

#include "adventure_banner_primitive_builder.hpp"
#include <d2adventure_render/map_geometry.hpp>
#include <d2runtime/AdventureWorldState.hpp>

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

namespace d2engine::adventure_render {

namespace {

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

[[nodiscard]] std::string site_kind_name(d2runtime::AdventureSiteKind kind) {
    switch (kind) {
    case d2runtime::AdventureSiteKind::Mage:
        return "Mage";
    case d2runtime::AdventureSiteKind::Merchant:
        return "Merchant";
    case d2runtime::AdventureSiteKind::Mercenary:
        return "Mercenary";
    case d2runtime::AdventureSiteKind::Trainer:
        return "Trainer";
    }
    return "Unknown";
}

void populate_static_primitive(PreparedAdventureRenderPrimitive& prim,
                               const StaticSiteVisual& visual, const ScreenPoint& foot_anchor) {
    prim.container_path = visual.container_path;
    prim.record_name = visual.logical_sprite;
    prim.draw_origin = {foot_anchor.x - visual.canvas_foot_x, foot_anchor.y - visual.canvas_foot_y};
    prim.visual_bounds = {prim.draw_origin.x, prim.draw_origin.y,
                          prim.draw_origin.x + visual.canvas_width,
                          prim.draw_origin.y + visual.canvas_height};
    prim.content_bounds = visual.content_bounds;
    prim.src_width = visual.canvas_width;
    prim.src_height = visual.canvas_height;
    prim.alpha = 1.0f;
}

void populate_animated_layer(PreparedAdventureRenderPrimitive& prim, const AnimatedSiteLayer& layer,
                             const ScreenPoint& foot_anchor) {
    prim.container_path = layer.container_path;
    prim.record_name =
        layer.animation.frames.empty() ? std::string{} : layer.animation.frames.front().record_name;
    prim.animation = layer.animation;
    prim.draw_origin = {foot_anchor.x - layer.canvas_foot_x, foot_anchor.y - layer.canvas_foot_y};

    prim.visual_bounds = {prim.draw_origin.x, prim.draw_origin.y,
                          prim.draw_origin.x + layer.animation.native_canvas_w,
                          prim.draw_origin.y + layer.animation.native_canvas_h};
    prim.content_bounds = layer.content_bounds;
    prim.src_width = layer.animation.native_canvas_w;
    prim.src_height = layer.animation.native_canvas_h;
    prim.alpha = 1.0f;
}

[[nodiscard]] int site_banner_index(d2runtime::AdventureSiteKind kind) {
    switch (kind) {
    case d2runtime::AdventureSiteKind::Mage:
        return 0;
    case d2runtime::AdventureSiteKind::Mercenary:
        return 1;
    case d2runtime::AdventureSiteKind::Merchant:
        return 2;
    case d2runtime::AdventureSiteKind::Trainer:
        return 3;
    }
    return 0;
}

} // namespace

RenderContributor make_site_contributor(const SiteAssetCatalog&        catalog,
                                        const StackBannerAssetCatalog& banner_catalog) {
    return [&catalog, &banner_catalog](const d2runtime::AdventureWorldState& world,
                                       PreparationContext&                   ctx) {
        const auto& geo = ctx.geometry();

        for (const auto& site : world.sites) {
            const auto* visual = catalog.find(site.kind, site.image_iso);
            if (visual == nullptr) {
                ctx.add_diagnostic({PrepareDiagnosticKind::UnresolvedNoSprite, site.id, "Site",
                                    site.image_iso, static_cast<int>(site.kind),
                                    "unresolved Site id=" + site.id +
                                        " kind=" + site_kind_name(site.kind) +
                                        " IMG_ISO=" + std::to_string(site.image_iso) +
                                        " reason=unsupported_visual"});
                continue;
            }

            if (site.footprint.empty()) {
                ctx.add_diagnostic(
                    {PrepareDiagnosticKind::UnresolvedNoSprite, site.id, "Site", site.image_iso,
                     static_cast<int>(site.kind),
                     "unresolved Site id=" + site.id + " kind=" + site_kind_name(site.kind) +
                         " IMG_ISO=" + std::to_string(site.image_iso) + " reason=empty_footprint"});
                continue;
            }

            GridFootprint footprint;
            footprint.reserve(site.footprint.size());
            for (const auto& cell : site.footprint) {
                footprint.push_back(cell);
            }

            const auto  depth_anchor = AdventureMapGeometry::derive_depth_anchor(footprint);
            const auto  foot_anchor = geo.cell_foot_anchor(depth_anchor);
            const auto  stable_body_id = stable_render_id("Site:" + site.id + ":body");
            const auto  banner_index = site_banner_index(site.kind);
            const auto& banner_asset = banner_catalog.resolve_banner(banner_index);

            PreparedAdventureRenderPrimitive body;
            body.stable_id = stable_body_id;
            body.debug_label = "Site:" + site.id + ":body";
            body.semantic_role = AdventurePrimitiveRole::SiteBody;
            body.semantic_object_id = site.id;
            body.phase = AdventureRenderPhase::World;
            body.level = WorldRenderLevel::Structure;
            body.local_suborder = 0;
            body.footprint = footprint;
            body.depth_anchor = depth_anchor;

            std::optional<std::string> shadow_logical;

            if (const auto* static_visual = std::get_if<StaticSiteVisual>(visual)) {
                populate_static_primitive(body, *static_visual, foot_anchor);
                const auto banner = build_adventure_banner_primitive(
                    body, static_visual->content_bounds, banner_asset,
                    AdventureBannerDockSide::LeftOfReference,
                    stable_render_id("SiteBanner:" + site.id),
                    "SiteBanner:" + site.id + ":" + site_kind_name(site.kind) + ":" +
                        std::to_string(banner_index),
                    AdventurePrimitiveRole::SiteBanner, site.id, WorldRenderLevel::Structure, 1,
                    footprint, depth_anchor);
                ctx.add_primitive(std::move(body));
                ctx.add_primitive(std::move(banner));
                ctx.add_diagnostic(
                    {PrepareDiagnosticKind::Resolved, site.id, "Site", site.image_iso,
                     static_cast<int>(site.kind),
                     "resolved Site id=" + site.id + " kind=" + site_kind_name(site.kind) +
                         " IMG_ISO=" + std::to_string(site.image_iso) +
                         " visual=static body_sprite=" + static_visual->logical_sprite +
                         " shadow=none footprint=" + describe_footprint(footprint) +
                         " depth_anchor=(" + std::to_string(depth_anchor.x) + "," +
                         std::to_string(depth_anchor.y) + ")"});
                continue;
            }

            const auto* animated_visual = std::get_if<AnimatedSiteVisual>(visual);
            if (animated_visual == nullptr) {
                ctx.add_diagnostic({PrepareDiagnosticKind::UnresolvedNoSprite, site.id, "Site",
                                    site.image_iso, static_cast<int>(site.kind),
                                    "unresolved Site id=" + site.id +
                                        " kind=" + site_kind_name(site.kind) +
                                        " IMG_ISO=" + std::to_string(site.image_iso) +
                                        " reason=unsupported_visual"});
                continue;
            }

            populate_animated_layer(body, animated_visual->body, foot_anchor);
            const auto banner = build_adventure_banner_primitive(
                body, animated_visual->body.content_bounds, banner_asset,
                AdventureBannerDockSide::LeftOfReference, stable_render_id("SiteBanner:" + site.id),
                "SiteBanner:" + site.id + ":" + site_kind_name(site.kind) + ":" +
                    std::to_string(banner_index),
                AdventurePrimitiveRole::SiteBanner, site.id, WorldRenderLevel::Structure, 1,
                footprint, depth_anchor);
            ctx.add_primitive(std::move(body));
            ctx.add_primitive(std::move(banner));

            shadow_logical =
                animated_visual->shadow.has_value()
                    ? std::optional<std::string>{animated_visual->shadow->logical_animation}
                    : std::nullopt;

            if (animated_visual->shadow.has_value()) {
                PreparedAdventureRenderPrimitive shadow;
                shadow.stable_id = stable_render_id("Site:" + site.id + ":shadow");
                shadow.debug_label = "Site:" + site.id + ":shadow";
                shadow.phase = AdventureRenderPhase::World;
                shadow.level = WorldRenderLevel::Structure;
                shadow.local_suborder = -1;
                shadow.footprint = footprint;
                shadow.depth_anchor = depth_anchor;
                populate_animated_layer(shadow, *animated_visual->shadow, foot_anchor);
                shadow.animation_sync_source_id = stable_body_id;
                shadow.alpha = 1.0f;
                ctx.add_primitive(std::move(shadow));
            }

            ctx.add_diagnostic(
                {PrepareDiagnosticKind::Resolved, site.id, "Site", site.image_iso,
                 static_cast<int>(site.kind),
                 "resolved Site id=" + site.id + " kind=" + site_kind_name(site.kind) +
                     " IMG_ISO=" + std::to_string(site.image_iso) +
                     " visual=animated body_animation=" + animated_visual->body.logical_animation +
                     " shadow_animation=" +
                     (shadow_logical.has_value() ? *shadow_logical : "none") +
                     " footprint=" + describe_footprint(footprint) + " depth_anchor=(" +
                     std::to_string(depth_anchor.x) + "," + std::to_string(depth_anchor.y) + ")"});
        }
    };
}

} // namespace d2engine::adventure_render
