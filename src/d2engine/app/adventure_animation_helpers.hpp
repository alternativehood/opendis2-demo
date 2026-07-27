#pragma once

#include "../animation/animation_sequence.hpp"
#include "../animation/animation_player.hpp"

#include <d2adventure_render/adventure_render_types.hpp>
#include <d2adventure_render/render_graph.hpp>

#include <string_view>
#include <unordered_map>

namespace d2engine {

[[nodiscard]] AnimationSequence
adventure_animation_data_to_sequence(std::string_view                                container_path,
                                     const adventure_render::AdventureAnimationData& anim_data);

using AdventureAnimationPlayerMap =
    std::unordered_map<adventure_render::StableRenderId, AnimationPlayer>;

[[nodiscard]] AdventureAnimationPlayerMap
build_adventure_animation_players(const adventure_render::PreparedAdventureRenderGraph& graph);

[[nodiscard]] adventure_render::StableRenderId
adventure_animation_player_id(const adventure_render::PreparedAdventureRenderPrimitive& primitive);

} // namespace d2engine
