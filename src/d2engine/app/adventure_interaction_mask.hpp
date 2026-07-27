#pragma once

#include "../assets/asset_runtime.hpp"
#include "../assets/image_asset_key.hpp"

#include <d2adventure_render/prepared_adventure_map.hpp>
#include <d2res/rgba_buffer.hpp>

#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace d2engine {

[[nodiscard]] ImageAssetKey
adventure_world_asset_key(const adventure_render::PreparedAdventureRenderPrimitive& primitive);

[[nodiscard]] std::shared_ptr<const adventure_render::InteractionMask>
build_interaction_mask(const d2res::RgbaBuffer& rgba);

[[nodiscard]] std::shared_ptr<const adventure_render::InteractionMask>
build_union_interaction_mask(const std::vector<const d2res::RgbaBuffer*>& rgba_frames);

[[nodiscard]] std::shared_ptr<const adventure_render::InteractionMask>
build_actor_layer_interaction_mask(const adventure_render::AdventureActorVisualLayer& layer,
                                   std::span<const PreparedImageResult> decoded_images);

[[nodiscard]] std::vector<ImageAssetKey>
collect_stack_mask_asset_keys(const adventure_render::PreparedAdventureMap& map);

[[nodiscard]] std::size_t
attach_stack_interaction_masks(adventure_render::PreparedAdventureMap& map,
                               std::span<const PreparedImageResult>    decoded_world_images);

} // namespace d2engine
