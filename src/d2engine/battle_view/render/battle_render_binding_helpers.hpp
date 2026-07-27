#pragma once

#include "../battle_renderer.hpp"

#include <map>
#include <optional>
#include <string>

namespace d2engine {

[[nodiscard]] bool        is_unit_attached_marker(TrackKind kind);
[[nodiscard]] BindingRole unit_animation_role(const SnapshotTrack& track) noexcept;
[[nodiscard]] const char* entity_side(const SnapshotEntity& entity) noexcept;
[[nodiscard]] const char* effect_lookup_side(const SnapshotTrack&  track,
                                             const SnapshotEntity& entity) noexcept;
[[nodiscard]] VisualPlacementValue
compose_placements(const std::map<std::string, VisualPlacementValue>& placements,
                   const ConfigBinding& common_binding, const char* side);
[[nodiscard]] std::optional<ConfigBinding> role_binding_for(const SnapshotTrack&  track,
                                                            const SnapshotEntity& entity);
[[nodiscard]] std::optional<ConfigBinding> layer_default_binding_for(const SnapshotTrack& track);
[[nodiscard]] std::optional<ConfigBinding> effect_default_binding_for(const SnapshotTrack& track);
[[nodiscard]] std::optional<ConfigBinding> sprite_binding_for(const SnapshotTrack& track);
[[nodiscard]] std::optional<ConfigBinding> layer_binding_for(const SnapshotTrack&  track,
                                                             const SnapshotEntity& entity);
[[nodiscard]] std::optional<ConfigBinding> layer_side_binding_for(const SnapshotTrack&  track,
                                                                  const SnapshotEntity& entity);
[[nodiscard]] std::optional<ConfigBinding> binding_for(const SnapshotTrack&  track,
                                                       const SnapshotEntity& entity);
[[nodiscard]] DebugRenderableItem make_tree_command_tunable_item(std::string stable_id,
                                                                 std::string tree_path,
                                                                 std::string kind, const Rect& rect,
                                                                 BindingRole role);

} // namespace d2engine
