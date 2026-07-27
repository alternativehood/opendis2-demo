#pragma once

#include "adventure_stack_actor_request_resolver.hpp"
#include "game_data_registry.hpp"
#include "sprite_animation_catalog.hpp"

#include <d2adventure_render/adventure_render_types.hpp>

#include <d2runtime/AdventureActorAnimationResolver.hpp>
#include <d2runtime/AdventureIsoDirection.hpp>
#include <d2runtime/AdventureStackPresentationResolver.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace d2engine {

struct IsoActorVisualFrame {
    std::string                           record_name;
    int                                   canvas_width = 0;
    int                                   canvas_height = 0;
    adventure_render::CanvasContentBounds content_bounds;
};

struct IsoActorVisualLayer {
    std::string                      container_path;
    std::string                      animation_name;
    std::vector<IsoActorVisualFrame> frames;

    int native_canvas_w = 0;
    int native_canvas_h = 0;

    int                                   canvas_foot_x = 0;
    int                                   canvas_foot_y = 0;
    adventure_render::CanvasContentBounds content_bounds;
};

enum class ExactLayerPresence : std::uint8_t {
    Missing,
    AuthoredEmpty,
    Visible,
};

struct ExactLayerResolution {
    ExactLayerPresence                 presence = ExactLayerPresence::Missing;
    std::optional<IsoActorVisualLayer> visual;
};

struct IsoActorVisual {
    d2runtime::AdventureActorPresentationKind presentation_kind =
        d2runtime::AdventureActorPresentationKind::Unit;

    std::string resolved_owner_id;

    ExactLayerPresence shadow_presence = ExactLayerPresence::Missing;

    IsoActorVisualLayer                body;
    std::optional<IsoActorVisualLayer> shadow;
};

class IsoActorVisualResolver {
public:
    IsoActorVisualResolver(const ISpriteAnimationCatalog& catalog,
                           const GameDataRegistry&        game_data);

    [[nodiscard]] std::optional<IsoActorVisual>
    resolve(const AdventureStackActorVisualRequest& request);

    // Classify shadow presence for a resolved body owner.
    // Returns the exact layer presence without building a full visual.
    // Must be called after resolve() has established the body owner.
    [[nodiscard]] ExactLayerPresence
    shadow_presence(std::string_view                 resolved_owner_id,
                    d2runtime::AdventureIsoDirection direction) const;

private:
    [[nodiscard]] ExactLayerResolution
    resolve_exact_layer(const d2runtime::AdventureActorPresentation& presentation,
                        std::string_view owner_id, d2runtime::AdventureIsoDirection direction,
                        d2runtime::AdventureActorAnimationLayer layer,
                        d2runtime::AdventureActorAnimationRole  role) const;

    [[nodiscard]] static std::string cache_key_unit(std::string_view normalized_unit_id,
                                                    d2runtime::AdventureIsoDirection direction,
                                                    d2runtime::AdventureActorAnimationRole role);

    [[nodiscard]] static std::string cache_key_boat(std::string_view                 race_id,
                                                    d2runtime::AdventureIsoDirection direction,
                                                    d2runtime::AdventureActorAnimationRole role);

    const ISpriteAnimationCatalog* catalog_;
    const GameDataRegistry*        game_data_;

    std::unordered_set<std::string>                                animations_;
    std::unordered_map<std::string, std::optional<IsoActorVisual>> cache_;
};

} // namespace d2engine
