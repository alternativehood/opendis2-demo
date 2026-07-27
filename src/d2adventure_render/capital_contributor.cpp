#include "capital_contributor.hpp"

#include "map_preparer.hpp"

#include <d2adventure_render/terrain/capital_asset_catalog.hpp>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace d2engine::adventure_render {

namespace {

[[nodiscard]] std::string capital_error(std::string_view capital_id, std::string_view reason) {
    return std::string(reason) + " capital=" + std::string(capital_id);
}

[[nodiscard]] std::string capital_error(std::string_view capital_id, std::string_view reason,
                                        std::string_view detail_key,
                                        std::string_view detail_value) {
    return std::string(reason) + " capital=" + std::string(capital_id) + " " +
           std::string(detail_key) + "=" + std::string(detail_value);
}

} // namespace

static std::function<void(const d2runtime::AdventureWorldState&, PreparationContext&)>
make_capital_contributor_impl(const CapitalVisualResolveFn& resolve_fn) {
    return [resolve_fn](const d2runtime::AdventureWorldState& world, PreparationContext& ctx) {
        const auto& geo = ctx.geometry();

        for (const auto& capital : world.capitals) {
            if (capital.id.empty()) {
                throw std::runtime_error("capital_missing_id capital=");
            }
            if (capital.footprint.empty()) {
                throw std::runtime_error(capital_error(capital.id, "capital_missing_footprint"));
            }
            if (capital.subrace.empty()) {
                throw std::runtime_error(capital_error(capital.id, "capital_missing_subrace"));
            }

            const auto* subrace = world.find_subrace(capital.subrace);
            if (subrace == nullptr) {
                throw std::runtime_error(capital_error(capital.id, "capital_dangling_subrace",
                                                       "subrace", capital.subrace));
            }
            if (subrace->race_id.empty()) {
                throw std::runtime_error(capital_error(capital.id, "capital_missing_race_id",
                                                       "subrace", capital.subrace));
            }

            const auto resolved = resolve_fn(world, capital, subrace->race_id);
            if (resolved.visual == nullptr) {
                throw std::runtime_error(
                    std::string("capital_missing_visual capital=") + capital.id +
                    " race_id=" + subrace->race_id + " state=" +
                    (resolved.state == CapitalVisualState::Active ? "Active" : "Ruined") +
                    " guardian_type=" + resolved.guardian_type_id + " guardian_instance=" +
                    (resolved.guardian_instance_id.empty() ? "<absent>"
                                                           : resolved.guardian_instance_id));
            }

            const auto  state = resolved.state;
            const auto* visual = resolved.visual;
            const auto  anchor = AdventureMapGeometry::derive_depth_anchor(capital.footprint);
            const auto  foot = geo.cell_foot_anchor(anchor);

            if (visual->animation_data.frames.empty()) {
                throw std::runtime_error(capital_error(capital.id, "capital_visual_zero_frames"));
            }

            int max_w = 0;
            int max_h = 0;
            for (const auto& frame : visual->animation_data.frames) {
                max_w = std::max(max_w, frame.canvas_width);
                max_h = std::max(max_h, frame.canvas_height);
            }

            PreparedAdventureRenderPrimitive prim;
            prim.stable_id = stable_render_id("Capital:" + capital.id);
            prim.debug_label = "Capital:" + capital.id;
            prim.phase = AdventureRenderPhase::World;
            prim.level = WorldRenderLevel::Structure;
            prim.local_suborder = 0;
            prim.container_path = visual->container_path;
            prim.record_name = visual->animation_data.frames.front().record_name;
            prim.animation = visual->animation_data;
            prim.draw_origin = {foot.x - visual->canvas_foot_x, foot.y - visual->canvas_foot_y};
            prim.visual_bounds = {prim.draw_origin.x, prim.draw_origin.y,
                                  prim.draw_origin.x + max_w, prim.draw_origin.y + max_h};
            prim.footprint = capital.footprint;
            prim.depth_anchor = anchor;
            prim.src_width = visual->animation_data.frames.front().canvas_width;
            prim.src_height = visual->animation_data.frames.front().canvas_height;
            ctx.add_primitive(std::move(prim));

            ctx.add_diagnostic(
                {PrepareDiagnosticKind::Resolved, capital.id, "Capital",
                 static_cast<int>(visual->animation_data.frames.size()), 0,
                 "resolved Capital id=" + capital.id + " subrace=" + capital.subrace +
                     " race_id=" + subrace->race_id +
                     " state=" + (state == CapitalVisualState::Active ? "Active" : "Ruined") +
                     " guardian_type=" + resolved.guardian_type_id + " guardian_instance=" +
                     (resolved.guardian_instance_id.empty() ? "<absent>"
                                                            : resolved.guardian_instance_id) +
                     " animation=" + visual->logical_animation_name + " frames=" +
                     std::to_string(visual->animation_data.frames.size()) + " depth_anchor=(" +
                     std::to_string(anchor.x) + "," + std::to_string(anchor.y) + ")"});
        }
    };
}

std::function<void(const d2runtime::AdventureWorldState&, PreparationContext&)>
make_capital_contributor(const CapitalAssetCatalog& catalog, CapitalVisualResolveFn resolve_fn) {
    (void)catalog;
    return make_capital_contributor_impl(resolve_fn);
}

} // namespace d2engine::adventure_render
