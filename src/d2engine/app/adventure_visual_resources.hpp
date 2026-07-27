#pragma once

#include "../assets/image_asset_key.hpp"

#include <d2adventure_render/adventure_render_types.hpp>
#include <d2adventure_render/map_geometry.hpp>

#include "../animation/animation_sequence.hpp"

#include <SDL3/SDL.h>

#include <memory>
#include <cstddef>
#include <string>
#include <string_view>

namespace d2engine {

class RenderAssetRuntime;
class AssetRuntime;

struct SelectVisual {
    ImageAssetKey key;
    int           canvas_foot_x = 0;
    int           canvas_foot_y = 0;
    int           src_width = 0;
    int           src_height = 0;
};

struct AdventureAnimatedVisual {
    std::string                              container_path;
    adventure_render::AdventureAnimationData animation;
    int                                      semantic_anchor_x = 0;
    int                                      semantic_anchor_y = 0;
    int                                      src_width = 0;
    int                                      src_height = 0;
};

struct AdventureRoutePreviewVisualResources {
    AdventureAnimatedVisual normal;
    AdventureAnimatedVisual action_limit;
    AdventureAnimatedVisual destination_highlight;
};

struct AdventureRoutePreviewPlaybackPolicy {
    static constexpr int                                              frame_duration_ms = 100;
    static constexpr bool                                             is_looping = true;
    static constexpr adventure_render::AdventureAnimationTimingSource timing_source =
        adventure_render::AdventureAnimationTimingSource::ProvisionalFallback;
};

struct AdventureRoutePreviewVisualContract {
    std::string_view animation_name;
    std::size_t      expected_frame_count = 0;
    int              expected_width = 0;
    int              expected_height = 0;
    int              semantic_anchor_x = 0;
    int              semantic_anchor_y = 0;
};

[[nodiscard]] AdventureAnimatedVisual
build_adventure_route_preview_visual(const AnimationSequence&                   sequence,
                                     std::string_view                           container_path,
                                     const AdventureRoutePreviewVisualContract& contract);

struct IsoCursorResources {
    SDL_Cursor* default_cursor = nullptr;
    SDL_Cursor* select_unit = nullptr;
};

struct AdventureWorldVisualResources {
    SelectVisual                              select_selected;
    SelectVisual                              select_no;
    SelectVisual                              select_yes;
    adventure_render::SelectionCircleGeometry selection_circle;
    AdventureRoutePreviewVisualResources      route_preview;
};

struct LoadedAdventureVisualResources {
    IsoCursorResources            cursors;
    AdventureWorldVisualResources world;
};

class AdventureVisualResourcesLoader {
public:
    explicit AdventureVisualResourcesLoader(AssetRuntime&       assets,
                                            RenderAssetRuntime& render_assets);

    [[nodiscard]] LoadedAdventureVisualResources load();

private:
    AssetRuntime&       assets_;
    RenderAssetRuntime& render_assets_;
};

} // namespace d2engine
