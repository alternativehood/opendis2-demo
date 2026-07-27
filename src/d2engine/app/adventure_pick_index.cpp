#include "adventure_pick_index.hpp"

#include <d2adventure_render/adventure_render_types.hpp>
#include <d2adventure_render/map_geometry.hpp>

#include <string_view>
#include <unordered_map>

namespace d2engine {

void AdventurePickIndex::build(
    const adventure_render::PreparedAdventureMap&    map,
    const adventure_render::SelectionCircleGeometry& selection_geometry) {
    targets_.clear();
    geometry_ = &map.geometry;
    cell_metrics_ = &map.cell_interaction_metrics;
    selection_geometry_ = &selection_geometry;

    std::unordered_map<adventure_render::StableRenderId, const adventure_render::PickEntry*>
        pick_map;
    for (const auto& entry : map.pick_entries) {
        pick_map[entry.stable_id] = &entry;
    }

    std::size_t rank = 0;
    for (const auto& prim : map.world_graph.world) {
        if (prim.level != adventure_render::WorldRenderLevel::Actor)
            continue;

        auto it = pick_map.find(prim.stable_id);
        if (it == pick_map.end())
            continue;

        const auto*       entry = it->second;
        AdventurePickKind kind;
        switch (entry->kind) {
        case adventure_render::PickEntryKind::Stack:
            kind = AdventurePickKind::Stack;
            break;
        }

        const adventure_render::MapCell cell = prim.depth_anchor;
        const auto                      foot = geometry_->cell_foot_anchor(cell);

        targets_.push_back(AdventurePickTarget{
            .stable_id = prim.stable_id,
            .kind = kind,
            .object_id = entry->object_id,
            .cell = cell,
            .cell_foot = foot,
            .draw_origin = prim.draw_origin,
            .src_width = prim.src_width,
            .src_height = prim.src_height,
            .mask = prim.interaction_mask,
            .render_rank = rank,
        });
        ++rank;
    }
}

AdventurePointerResult AdventurePickIndex::hit_test(int canvas_x, int canvas_y) const {
    AdventurePointerResult result;

    if (geometry_ == nullptr || cell_metrics_ == nullptr || selection_geometry_ == nullptr)
        return result;

    const AdventurePickTarget* best_coarse = nullptr;
    const AdventurePickTarget* best_ellipse = nullptr;
    const AdventurePickTarget* best_alpha = nullptr;

    std::size_t best_coarse_rank = 0;

    for (const auto& t : targets_) {
        const int dx = canvas_x - t.cell_foot.x;
        const int dy = canvas_y - t.cell_foot.y;
        if (!cell_metrics_->contains(dx, dy))
            continue;

        if (best_coarse == nullptr || t.render_rank > best_coarse_rank) {
            best_coarse = &t;
            best_coarse_rank = t.render_rank;
        }

        if (selection_geometry_->contains(canvas_x, canvas_y, t.cell_foot)) {
            if (best_ellipse == nullptr || t.render_rank > best_ellipse->render_rank)
                best_ellipse = &t;
            continue;
        }

        if (t.mask != nullptr) {
            const int local_x = canvas_x - t.draw_origin.x;
            const int local_y = canvas_y - t.draw_origin.y;
            if (t.mask->opaque(local_x, local_y)) {
                if (best_alpha == nullptr || t.render_rank > best_alpha->render_rank)
                    best_alpha = &t;
            }
        }
    }

    result.occupied_cell_hover = best_coarse;

    if (best_ellipse != nullptr) {
        result.interaction_target = best_ellipse;
    } else if (best_alpha != nullptr) {
        result.interaction_target = best_alpha;
    }

    return result;
}

AdventurePointerResult AdventureHitTester::hit_test(int logical_screen_x,
                                                    int logical_screen_y) const {
    if (!index_ || !camera_)
        return {};
    const int canvas_x = camera_->screen_to_canvas_x(logical_screen_x);
    const int canvas_y = camera_->screen_to_canvas_y(logical_screen_y);
    return index_->hit_test(canvas_x, canvas_y);
}

} // namespace d2engine
