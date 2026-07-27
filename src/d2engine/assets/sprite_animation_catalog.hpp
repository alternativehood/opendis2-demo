#pragma once

#include "../animation/animation_sequence.hpp"

#include <d2adventure_render/adventure_render_types.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace d2engine {

struct AnimationSpriteMeta {
    int32_t                                         canvas_width = 0;
    int32_t                                         canvas_height = 0;
    bool                                            has_visible_pieces = true;
    d2engine::adventure_render::CanvasContentBounds content_bounds;
};

class ISpriteAnimationCatalog {
public:
    ISpriteAnimationCatalog() = default;
    virtual ~ISpriteAnimationCatalog() = default;
    ISpriteAnimationCatalog(const ISpriteAnimationCatalog&) = delete;
    ISpriteAnimationCatalog& operator=(const ISpriteAnimationCatalog&) = delete;
    ISpriteAnimationCatalog(ISpriteAnimationCatalog&&) = delete;
    ISpriteAnimationCatalog& operator=(ISpriteAnimationCatalog&&) = delete;

    [[nodiscard]] virtual std::vector<std::string>
    animations_in(std::string_view container) const = 0;

    [[nodiscard]] virtual AnimationSequence
    animation_sequence(std::string_view container, std::string_view anim_name) const = 0;

    [[nodiscard]] virtual AnimationSpriteMeta
    sprite_metadata(std::string_view container, std::string_view sprite_name) const = 0;
};

} // namespace d2engine
