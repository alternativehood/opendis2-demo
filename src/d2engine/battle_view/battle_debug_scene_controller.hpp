#pragma once

#include "battle_scene.hpp"

#include <optional>

namespace d2engine {

class BattleDebugSceneController {
public:
    [[nodiscard]] static bool toggle_pause(BattleScene& scene, VisualEntityId id);
    [[nodiscard]] static bool set_all_paused(BattleScene& scene, bool paused);
    [[nodiscard]] static bool step_frame(BattleScene& scene, VisualEntityId id, int delta);
    [[nodiscard]] static bool move_unit(BattleScene& scene, VisualEntityId id, Vec2 delta);
    [[nodiscard]] static std::optional<Vec2> unit_position_offset(const BattleScene& scene,
                                                                  VisualEntityId     id);
};

} // namespace d2engine
