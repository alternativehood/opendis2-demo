#include "map_preparer.hpp"
#include "adventure_actor_primitive_builder.hpp"
#include "adventure_banner_primitive_builder.hpp"
#include "map_stack_presentation_ids.hpp"

#include "tree_hash.hpp"

#include <d2runtime/AdventureWorldState.hpp>

namespace d2runtime {
// Pull in AdventureMapObjectKind and AdventureMapObject for readability.
// These are used extensively in kind_ordinal(), kind_name(), and contributors.
} // namespace d2runtime

using namespace d2runtime;

#include <cstdint>
#include <cstdio>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace d2engine::adventure_render {

// ── AdventureMapPreparer ───────────────────────────────────────────────

AdventureMapPreparer::AdventureMapPreparer(const AdventureMapGeometry& geometry)
    : geometry_(&geometry) {}

// cppcheck-suppress unusedFunction
void AdventureMapPreparer::set_terrain_composer(const AdventureTerrainSurfaceComposer& composer,
                                                const TerrainAssetCatalog&             catalog) {
    composer_ = &composer;
    catalog_ = &catalog;
}

void AdventureMapPreparer::add_contributor(RenderContributor contributor) {
    contributors_.push_back(std::move(contributor));
}

PrepareResult AdventureMapPreparer::prepare(const d2runtime::AdventureWorldState& world) const {
    AdventureRenderGraphBuilder builder;
    PreparationContext          ctx(*geometry_, builder);

    // Run each contributor
    for (const auto& contributor : contributors_) {
        contributor(world, ctx);
    }

    // Finalize the render graph with depth resolver
    IsoDepthResolver resolver(*geometry_);
    auto             graph = builder.finalize(resolver);

    PrepareResult result;
    result.graph = std::move(graph);
    result.pick_entries = ctx.take_pick_entries();
    result.diagnostics = ctx.take_diagnostics();
    return result;
}

PreparedAdventureMap
// cppcheck-suppress unusedFunction
AdventureMapPreparer::prepare_full(const d2runtime::AdventureWorldState&        world,
                                   const AdventureTerrainSurfaceInput&          terrain_input,
                                   const AdventureTerrainSurfaceComposeOptions& options) const {
    PreparedAdventureMap result;
    result.geometry = *geometry_;

    // Prepare terrain if composer available
    if (composer_ != nullptr) {
        auto prepared = composer_->prepare_full_map(terrain_input, options);
        if (prepared.canvas_width > 0 && prepared.canvas_height > 0) {
            result.terrain = std::move(prepared);
        }
    }

    // Prepare world primitives
    auto world_result = prepare(world);
    result.world_graph = std::move(world_result.graph);
    result.pick_entries = std::move(world_result.pick_entries);
    result.diagnostics = std::move(world_result.diagnostics);

    return result;
}

// ── Road contributor ───────────────────────────────────────────────────
//
// Each MidRoad/AdventureRoad maps to exactly one PreparedAdventureRenderPrimitive
// when its INDEX resolves through the RoadAssetCatalog.
// Roads render in GroundOverlay phase (after terrain, before World).
// VAR is preserved in diagnostics/metadata but does not alter sprite selection.
RenderContributor make_road_contributor(const RoadAssetCatalog& catalog) {
    return [&catalog](const d2runtime::AdventureWorldState& world, PreparationContext& ctx) {
        const auto& geo = ctx.geometry();

        for (const auto& road : world.roads) {
            const auto* visual = catalog.find(road.index);

            if (visual == nullptr) {
                std::string expected;
                if (road.index >= 0 && road.index <= 15) {
                    char buf[16];
                    std::snprintf(buf, sizeof(buf), "ROAD%02d00", road.index);
                    expected = buf;
                }
                ctx.add_diagnostic(
                    {PrepareDiagnosticKind::UnresolvedNoSprite, road.id, "Road", road.index,
                     road.variant,
                     "unresolved Road id=" + road.id + " pos=(" + std::to_string(road.position.x) +
                         "," + std::to_string(road.position.y) + ") index=" +
                         std::to_string(road.index) + " variant=" + std::to_string(road.variant) +
                         (expected.empty() ? std::string() : " expected=" + expected)});
                continue;
            }

            const auto stable_id = stable_render_id("Road:" + road.id);

            const auto tile_origin = geo.cell_canvas_origin(road.position);

            const int offset_x = (geo.tile_width - visual->width) / 2;
            const int offset_y = (geo.tile_height - visual->height) / 2;

            PreparedAdventureRenderPrimitive prim;
            prim.stable_id = stable_id;
            prim.debug_label = "Road:" + road.id;
            prim.phase = AdventureRenderPhase::GroundOverlay;
            prim.level = WorldRenderLevel::GroundObject;
            prim.container_path = catalog.container;
            prim.record_name = visual->logical_sprite;
            prim.draw_origin = {tile_origin.x + offset_x, tile_origin.y + offset_y};
            prim.visual_bounds = {prim.draw_origin.x, prim.draw_origin.y,
                                  prim.draw_origin.x + visual->width,
                                  prim.draw_origin.y + visual->height};
            prim.footprint.push_back(road.position);
            prim.depth_anchor = road.position;
            prim.src_width = visual->width;
            prim.src_height = visual->height;
            ctx.add_primitive(std::move(prim));

            ctx.add_diagnostic(
                {PrepareDiagnosticKind::Resolved, road.id, "Road", road.index, road.variant,
                 "resolved Road id=" + road.id + " pos=(" + std::to_string(road.position.x) + "," +
                     std::to_string(road.position.y) + ") index=" + std::to_string(road.index) +
                     " variant=" + std::to_string(road.variant) +
                     " sprite=" + visual->logical_sprite});
        }
    };
}

// ── Mountain contributor ───────────────────────────────────────────────
//
// Each MidMountains entry maps to exactly one AdventureMountain, which maps
// to exactly one PreparedAdventureRenderPrimitive.  No greedy packing, no
// fallback to smaller/larger sizes, no guessing unsupported races.
RenderContributor make_mountain_contributor(const MountainAssetCatalog& catalog) {
    return [&catalog](const d2runtime::AdventureWorldState& world, PreparationContext& ctx) {
        const auto& geo = ctx.geometry();

        for (const auto& mt : world.mountains) {
            GridFootprint fp;
            fp.reserve(mt.footprint.size());
            for (const auto& cell : mt.footprint)
                fp.push_back(cell);
            const auto anchor = AdventureMapGeometry::derive_depth_anchor(fp);

            const auto* visual = catalog.find(mt.race, mt.size_x, mt.size_y, mt.image);

            if (visual == nullptr) {
                std::string expected_key;
                if (mt.race == 4) {
                    char buf[16];
                    std::snprintf(buf, sizeof(buf), "MOMNE%02d%02d", mt.size_x, mt.image);
                    expected_key = buf;
                }
                ctx.add_diagnostic(
                    {PrepareDiagnosticKind::UnresolvedNoSprite, mt.id, "Mountain", mt.image, -1,
                     "unresolved Mountain id=" + mt.id +
                         " id_mount=" + std::to_string(mt.id_mount) + " pos=(" +
                         std::to_string(mt.position.x) + "," + std::to_string(mt.position.y) +
                         ") size=" + std::to_string(mt.size_x) + "x" + std::to_string(mt.size_y) +
                         " image=" + std::to_string(mt.image) + " race=" + std::to_string(mt.race) +
                         (expected_key.empty() ? std::string() : " expected=" + expected_key) +
                         " footprint_cells=" + std::to_string(fp.size()) + " depth_anchor=(" +
                         std::to_string(anchor.x) + "," + std::to_string(anchor.y) + ")"});
                continue;
            }

            const auto stable_id = stable_render_id("Mountain:" + mt.id);

            const auto foot = geo.cell_foot_anchor(anchor);

            PreparedAdventureRenderPrimitive prim;
            prim.stable_id = stable_id;
            prim.debug_label = "Mountain:" + mt.id;
            prim.phase = AdventureRenderPhase::World;
            prim.level = WorldRenderLevel::Structure;
            prim.container_path = catalog.container;
            prim.record_name = visual->logical_sprite;
            prim.draw_origin = {foot.x - visual->canvas_foot_x, foot.y - visual->canvas_foot_y};
            prim.visual_bounds = {prim.draw_origin.x, prim.draw_origin.y,
                                  prim.draw_origin.x + visual->width,
                                  prim.draw_origin.y + visual->height};
            prim.footprint = fp;
            prim.depth_anchor = anchor;
            prim.src_width = visual->width;
            prim.src_height = visual->height;
            ctx.add_primitive(std::move(prim));

            ctx.add_diagnostic(
                {PrepareDiagnosticKind::Resolved, mt.id, "Mountain", mt.image, -1,
                 "resolved Mountain id=" + mt.id + " id_mount=" + std::to_string(mt.id_mount) +
                     " pos=(" + std::to_string(mt.position.x) + "," +
                     std::to_string(mt.position.y) + ") size=" + std::to_string(mt.size_x) + "x" +
                     std::to_string(mt.size_y) + " race=4" + " image=" + std::to_string(mt.image) +
                     " sprite=" + visual->logical_sprite + " depth_anchor=(" +
                     std::to_string(anchor.x) + "," + std::to_string(anchor.y) + ")"});
        }
    };
}

// ── Landmark contributor ────────────────────────────────────────────────
//
// Each Landmark instance maps to exactly one render entity (static sprite or
// animation).  Unresolved types produce diagnostics; no fallback.
RenderContributor make_landmark_contributor(const LandmarkAssetCatalog& catalog) {
    return [&catalog](const d2runtime::AdventureWorldState& world, PreparationContext& ctx) {
        const auto& geo = ctx.geometry();

        for (const auto& lm : world.landmarks) {
            GridFootprint fp;
            fp.reserve(lm.footprint.size());
            for (const auto& cell : lm.footprint)
                fp.push_back(cell);
            const auto anchor = AdventureMapGeometry::derive_depth_anchor(fp);

            const auto* visual = catalog.find(lm.type_id);

            if (visual == nullptr) {
                ctx.add_diagnostic(
                    {PrepareDiagnosticKind::UnresolvedNoSprite, lm.id, "Landmark", -1, -1,
                     "unresolved Landmark id=" + lm.id + " type=" + lm.type_id +
                         " map_gfx=" + lm.map_gfx_id + " image=" + lm.image + " pos=(" +
                         std::to_string(lm.position.x) + "," + std::to_string(lm.position.y) + ")" +
                         " footprint_cells=" + std::to_string(fp.size()) + " key=" + lm.type_id});
                continue;
            }

            const auto stable_id = stable_render_id("Landmark:" + lm.id);

            const auto foot = geo.cell_foot_anchor(anchor);

            PreparedAdventureRenderPrimitive prim;
            prim.stable_id = stable_id;
            prim.debug_label = "Landmark:" + lm.id;
            prim.phase = AdventureRenderPhase::World;
            prim.level = WorldRenderLevel::Structure;
            prim.footprint = fp;
            prim.depth_anchor = anchor;

            bool        is_static = false;
            std::string visual_container;
            if (auto* sv = std::get_if<StaticLandmarkVisual>(visual)) {
                prim.container_path = sv->container_path;
                prim.record_name = sv->logical_sprite;
                prim.draw_origin = {foot.x - sv->canvas_foot_x, foot.y - sv->canvas_foot_y};
                prim.visual_bounds = {prim.draw_origin.x, prim.draw_origin.y,
                                      prim.draw_origin.x + sv->width,
                                      prim.draw_origin.y + sv->height};
                prim.src_width = sv->width;
                prim.src_height = sv->height;
                visual_container = sv->container_path;
                is_static = true;
            } else if (auto* av = std::get_if<AnimatedLandmarkVisual>(visual)) {
                prim.container_path = av->container_path;
                prim.animation = av->animation_data;
                prim.draw_origin = {foot.x - av->canvas_foot_x, foot.y - av->canvas_foot_y};
                prim.visual_bounds = {prim.draw_origin.x, prim.draw_origin.y,
                                      prim.draw_origin.x + av->animation_data.native_canvas_w,
                                      prim.draw_origin.y + av->animation_data.native_canvas_h};
                prim.src_width = av->animation_data.native_canvas_w;
                prim.src_height = av->animation_data.native_canvas_h;
                visual_container = av->container_path;
            }

            ctx.add_primitive(std::move(prim));

            const char* kind_str = is_static ? "static" : "animated";
            ctx.add_diagnostic({PrepareDiagnosticKind::Resolved, lm.id, "Landmark", -1, -1,
                                "resolved Landmark id=" + lm.id + " type=" + lm.type_id +
                                    " kind=" + kind_str + " pos=(" + std::to_string(lm.position.x) +
                                    "," + std::to_string(lm.position.y) + ")" + " depth_anchor=(" +
                                    std::to_string(anchor.x) + "," + std::to_string(anchor.y) +
                                    ")" + " container=" + visual_container});
        }
    };
}

// ── Shared layer validation helper ──────────────────────────────────────
namespace {

void validate_layer(const std::string& layer_label, const AdventureActorVisualLayer& layer,
                    const std::string& stack_id, const std::string& leader_id,
                    const std::string& leader_type_id) {
    if (layer.container_path.empty()) {
        std::string msg = "malformed_unit_visual " + layer_label + " stack=" + stack_id;
        msg += " leader=" + leader_id;
        msg += " type=" + leader_type_id;
        msg += " reason=empty_container_path";
        msg += " animation=" + layer.logical_animation_name;
        throw std::runtime_error(msg);
    }
    if (layer.logical_animation_name.empty()) {
        std::string msg = "malformed_unit_visual " + layer_label + " stack=" + stack_id;
        msg += " leader=" + leader_id;
        msg += " type=" + leader_type_id;
        msg += " reason=empty_animation_name";
        throw std::runtime_error(msg);
    }
    if (layer.frames.empty()) {
        std::string msg = "malformed_unit_visual " + layer_label + " stack=" + stack_id;
        msg += " leader=" + leader_id;
        msg += " type=" + leader_type_id;
        msg += " reason=zero_frames";
        msg += " animation=" + layer.logical_animation_name;
        throw std::runtime_error(msg);
    }
    if (layer.native_canvas_w <= 0 || layer.native_canvas_h <= 0) {
        std::string msg = "malformed_unit_visual " + layer_label + " stack=" + stack_id;
        msg += " leader=" + leader_id;
        msg += " type=" + leader_type_id;
        msg += " reason=invalid_canvas";
        msg += " canvas=" + std::to_string(layer.native_canvas_w) + "x" +
               std::to_string(layer.native_canvas_h);
        msg += " animation=" + layer.logical_animation_name;
        throw std::runtime_error(msg);
    }
    for (std::size_t fi = 0; fi < layer.frames.size(); ++fi) {
        const auto& f = layer.frames[fi];
        if (f.record_name.empty()) {
            std::string msg;
            msg += "malformed_unit_visual ";
            msg += layer_label;
            msg += " stack=";
            msg += stack_id;
            msg += " leader=" + leader_id;
            msg += " type=" + leader_type_id;
            msg += " reason=empty_frame_record";
            msg += " frame=" + std::to_string(fi);
            msg += " animation=" + layer.logical_animation_name;
            throw std::runtime_error(msg);
        }
        if (f.canvas_width <= 0 || f.canvas_height <= 0) {
            std::string msg;
            msg += "malformed_unit_visual ";
            msg += layer_label;
            msg += " stack=";
            msg += stack_id;
            msg += " leader=" + leader_id;
            msg += " type=" + leader_type_id;
            msg += " reason=invalid_frame_dimensions";
            msg += " frame=" + std::to_string(fi);
            msg +=
                " dims=" + std::to_string(f.canvas_width) + "x" + std::to_string(f.canvas_height);
            msg += " record=" + f.record_name;
            msg += " animation=" + layer.logical_animation_name;
            throw std::runtime_error(msg);
        }
    }
}

} // namespace

// ── Actor visual validation ─────────────────────────────────────────────
void validate_adventure_actor_visual_or_throw(const d2runtime::AdventureStack&        stack,
                                              const d2runtime::AdventureUnitInstance& leader,
                                              const AdventureActorVisual&             visual) {
    validate_layer("body", visual.body, stack.id, stack.leader_id, leader.type_id);

    if (visual.shadow.has_value()) {
        validate_layer("shadow", *visual.shadow, stack.id, stack.leader_id, leader.type_id);
    }
}

// ── Stack actor contributor ────────────────────────────────────────────
RenderContributor make_stack_actor_contributor(StackActorVisualResolver       resolver,
                                               const StackBannerAssetCatalog* banner_catalog) {
    return [resolver = std::move(resolver),
            banner_catalog](const d2runtime::AdventureWorldState& world, PreparationContext& ctx) {
        for (const auto& stack : world.stacks) {
            if (!d2runtime::is_stack_on_adventure_map(stack))
                continue;

            const auto* leader = world.find_unit(stack.leader_id);
            if (leader == nullptr) {
                ctx.add_diagnostic({PrepareDiagnosticKind::UnresolvedNoSprite, stack.id, "Stack",
                                    -1, -1,
                                    "unresolved Stack id=" + stack.id +
                                        " missing leader unit=" + stack.leader_id});
                continue;
            }

            if (!resolver) {
                ctx.add_diagnostic(
                    {PrepareDiagnosticKind::UnresolvedNoSprite, stack.id, "Stack", -1, -1,
                     "unresolved Stack id=" + stack.id + " no actor visual resolver leader=" +
                         stack.leader_id + " type=" + leader->type_id});
                continue;
            }

            const auto visual = resolver(stack, *leader);
            if (!visual.has_value()) {
                ctx.add_diagnostic({PrepareDiagnosticKind::UnresolvedNoSprite, stack.id, "Stack",
                                    -1, -1,
                                    "unresolved Stack id=" + stack.id +
                                        " leader=" + stack.leader_id + " type=" + leader->type_id});
                continue;
            }

            // Malformed resolved visuals are fatal — no silent fallback
            validate_adventure_actor_visual_or_throw(stack, *leader, *visual);

            const auto& geo = ctx.geometry();
            const auto  foot = geo.cell_foot_anchor(stack.position);

            const auto ids = map_stack_presentation_render_ids(stack.id);
            ctx.add_pick_entry(PickEntry{
                .stable_id = ids.body,
                .kind = PickEntryKind::Stack,
                .object_id = stack.id,
            });

            const auto actor_primitives = build_adventure_actor_primitives(
                stack, *visual, geo, foot, stack.position, ids.body, ids.shadow,
                "StackLeader:" + stack.id, "StackShadow:" + stack.id,
                {.frame_duration_ms = AdventureActorIdlePlaybackPolicy::frame_duration_ms,
                 .is_looping = AdventureActorIdlePlaybackPolicy::is_looping,
                 .timing_source = AdventureActorIdlePlaybackPolicy::timing_source});
            ctx.add_primitive(actor_primitives.body);
            if (actor_primitives.shadow.has_value())
                ctx.add_primitive(*actor_primitives.shadow);

            if (banner_catalog != nullptr) {
                const auto* subrace = world.find_subrace(stack.subrace);
                if (subrace == nullptr) {
                    ctx.add_diagnostic(
                        {PrepareDiagnosticKind::UnresolvedNoSprite, stack.id, "Stack", -1, -1,
                         "unresolved Stack id=" + stack.id + " missing subrace=" + stack.subrace});
                    continue;
                }
                const auto& banner_asset = banner_catalog->resolve_banner(subrace->banner);
                const auto  banner = build_adventure_banner_primitive(
                    actor_primitives.body, visual->body.content_bounds, banner_asset,
                    AdventureBannerDockSide::RightOfReference, ids.banner,
                    "StackBanner:" + stack.id + ":" + std::to_string(subrace->banner),
                    AdventurePrimitiveRole::MapStackBanner, stack.id, WorldRenderLevel::Actor, 1,
                    {stack.position}, stack.position);
                ctx.add_primitive(banner);
            }

            // Add diagnostic
            std::string diag_msg = "resolved Stack id=" + stack.id + " leader=" + stack.leader_id +
                                   " type=" + leader->type_id +
                                   " body_frames=" + std::to_string(visual->body.frames.size()) +
                                   " body_anim=" + visual->body.logical_animation_name;
            if (visual->shadow.has_value()) {
                diag_msg += " shadow_frames=" + std::to_string(visual->shadow->frames.size()) +
                            " shadow_anim=" + visual->shadow->logical_animation_name;
            } else {
                diag_msg += " shadow=<absent>";
            }
            diag_msg += " owner=" + visual->resolved_owner_id;
            ctx.add_diagnostic(
                {PrepareDiagnosticKind::Resolved, stack.id, "Stack", -1, -1, std::move(diag_msg)});
        }
    };
}

// ── Forest contributor ────────────────────────────────────────────────────
//
// Forests come from terrain cells with low-byte values 9..14.
// Each forest cell produces one tree primitive with determinant variant
// selected via hash(seed, canonical cell.x, canonical cell.y).
//
// Terrain → tree family mapping:
//   9(HU) → HUF, 10(DW) → DWF, 11(HE) → HEF,
//   12(UN) → UNF, 13(NE) → NEF, 14(EL) → ELF
namespace {

std::string_view tree_family_prefix_for_material(int family_id) {
    switch (family_id) {
    case 1:
        return "HUF";
    case 2:
        return "DWF";
    case 3:
        return "HEF";
    case 4:
        return "UNF";
    case 5:
        return "NEF";
    case 6:
        return "ELF";
    default:
        return {};
    }
}

} // namespace

RenderContributor
make_forest_contributor(const TreeAssetCatalog&                                       tree_catalog,
                        const std::vector<d2runtime::AdventureTerrainTileDescriptor>& descriptors) {
    return [&tree_catalog, &descriptors](const d2runtime::AdventureWorldState& world,
                                         PreparationContext&                   ctx) {
        const auto& geo = ctx.geometry();
        const auto  map_seed = world.map_seed;

        std::size_t total_terrain = descriptors.size();
        std::size_t forest_cells = 0;
        std::size_t rendered = 0;
        std::size_t skipped = 0;
        std::size_t fam_counts[6] = {};

        for (std::size_t idx = 0; idx < descriptors.size(); ++idx) {
            const auto& d = descriptors[idx];
            if (!d.is_forest)
                continue;
            ++forest_cells;

            const d2runtime::MapCellCoord cell{static_cast<int>(idx) % world.map_width,
                                               static_cast<int>(idx) / world.map_width};

            const auto family_prefix =
                tree_family_prefix_for_material(static_cast<int>(d.material));

            if (family_prefix.empty()) {
                ++skipped;
                continue;
            }

            const auto* family = tree_catalog.find_family(family_prefix);
            if (family == nullptr || family->empty()) {
                ++skipped;
                continue;
            }

            const auto fam_idx = static_cast<int>(d.material) - 1;
            if (fam_idx >= 0 && fam_idx < 6)
                ++fam_counts[fam_idx];

            const auto variant_index =
                static_cast<std::size_t>(stable_tree_hash(map_seed, cell.x, cell.y)) %
                family->size();
            const auto& logical_sprite = family->logical_sprites[variant_index];

            const auto proj = geo.project_cell(cell);
            const int  tile_center_x = proj.x - geo.min_world_x;
            const int  tile_center_y = proj.y - geo.min_world_y;

            constexpr int kTreeCanvasWidth = 320;
            constexpr int kTreeCanvasHeight = 320;

            PreparedAdventureRenderPrimitive prim;
            prim.stable_id =
                stable_render_id("Tree:" + std::to_string(cell.x) + ":" + std::to_string(cell.y));
            prim.debug_label = "Tree@" + std::to_string(cell.x) + "," + std::to_string(cell.y);
            prim.phase = AdventureRenderPhase::World;
            prim.level = WorldRenderLevel::GroundObject;
            prim.container_path = tree_catalog.container;
            prim.record_name = logical_sprite;
            prim.draw_origin = {tile_center_x - kTreeSpriteAnchor.x,
                                tile_center_y - kTreeSpriteAnchor.y};
            prim.visual_bounds = {prim.draw_origin.x, prim.draw_origin.y,
                                  prim.draw_origin.x + kTreeCanvasWidth,
                                  prim.draw_origin.y + kTreeCanvasHeight};
            prim.footprint.push_back(cell);
            prim.depth_anchor = cell;
            prim.src_width = kTreeCanvasWidth;
            prim.src_height = kTreeCanvasHeight;
            ctx.add_primitive(std::move(prim));
            ++rendered;
        }

        if (forest_cells > 0) {
            PrepareDiagnostic diag;
            diag.kind = PrepareDiagnosticKind::Resolved;
            diag.object_kind = "Forest";
            diag.image = static_cast<int>(forest_cells);
            diag.index = static_cast<int>(rendered);
            diag.message =
                "forests terrain_cells=" + std::to_string(total_terrain) +
                " forest_cells=" + std::to_string(forest_cells) +
                " rendered=" + std::to_string(rendered) + " skipped=" + std::to_string(skipped) +
                " HUF=" + std::to_string(fam_counts[0]) + " DWF=" + std::to_string(fam_counts[1]) +
                " HEF=" + std::to_string(fam_counts[2]) + " UNF=" + std::to_string(fam_counts[3]) +
                " NEF=" + std::to_string(fam_counts[4]) + " ELF=" + std::to_string(fam_counts[5]);
            ctx.add_diagnostic(std::move(diag));
        }
    };
}

} // namespace d2engine::adventure_render
