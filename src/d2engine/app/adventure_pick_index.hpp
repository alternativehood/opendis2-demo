#pragma once

#include "../render/adventure_render_state.hpp"

#include <d2adventure_render/adventure_render_types.hpp>
#include <d2adventure_render/map_geometry.hpp>
#include <d2adventure_render/prepared_adventure_map.hpp>

#include <cstddef>
#include <memory>
#include <string>

namespace d2engine {

enum class AdventurePickKind : uint8_t {
    Stack,
};

struct AdventurePickTarget {
    adventure_render::StableRenderId stable_id = 0;
    AdventurePickKind                kind = AdventurePickKind::Stack;
    std::string                      object_id;

    adventure_render::MapCell                                cell;
    adventure_render::ScreenPoint                            cell_foot;
    adventure_render::ScreenPoint                            draw_origin;
    int                                                      src_width = 0;
    int                                                      src_height = 0;
    std::shared_ptr<const adventure_render::InteractionMask> mask;

    std::size_t render_rank = 0;
};

struct AdventurePointerResult {
    const AdventurePickTarget* occupied_cell_hover = nullptr;
    const AdventurePickTarget* interaction_target = nullptr;
};

// picking consumes final screen/world render bounds and does not transform MapCellCoord
// independently
class AdventurePickIndex {
public:
    void build(const adventure_render::PreparedAdventureMap&    map,
               const adventure_render::SelectionCircleGeometry& selection_geometry);

    [[nodiscard]] AdventurePointerResult hit_test(int canvas_x, int canvas_y) const;

    [[nodiscard]] bool        empty() const { return targets_.empty(); }
    [[nodiscard]] std::size_t size() const { return targets_.size(); }

private:
    const adventure_render::AdventureMapGeometry*    geometry_ = nullptr;
    const adventure_render::CellInteractionMetrics*  cell_metrics_ = nullptr;
    const adventure_render::SelectionCircleGeometry* selection_geometry_ = nullptr;
    std::vector<AdventurePickTarget>                 targets_;
};

class AdventureHitTester {
public:
    AdventureHitTester(const AdventurePickIndex& index, const AdventureCamera& camera)
        : index_(&index), camera_(&camera) {}

    [[nodiscard]] AdventurePointerResult hit_test(int logical_screen_x, int logical_screen_y) const;

private:
    const AdventurePickIndex* index_;
    const AdventureCamera*    camera_;
};

} // namespace d2engine
