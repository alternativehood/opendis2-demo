#pragma once

#include "../battle_renderer.hpp"

#include <string>

namespace d2engine {

inline constexpr const char* kBackgroundRootPath = "/background";
inline constexpr const char* kGroundBackgroundPath = "/background/ground";
inline constexpr const char* kBattleBackgroundPath = "/background/battle";

[[nodiscard]] std::string unit_group_slot_path(BattleSlotCoord coord);
[[nodiscard]] std::string hp_text(int current_hp, int max_hp);
[[nodiscard]] float       missing_hp_ratio(int current_hp, int max_hp);
void apply_text_style(RenderCommand& command, const TreeLayout& tree, const std::string& path,
                      const std::string& font_face);
[[nodiscard]] Rect portrait_damage_basis(const SnapshotEntity& entity, const TreeLayout& tree,
                                         const std::string& portrait_path);
[[nodiscard]] const SnapshotEntity* find_entity_by_unit(const BattleRenderSnapshot& snapshot,
                                                        UnitInstanceId              id);

} // namespace d2engine
