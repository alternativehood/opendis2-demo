#include "battle_debug_scene_controller.hpp"

namespace d2engine {

bool BattleDebugSceneController::toggle_pause(BattleScene& scene, VisualEntityId id) {
    return scene.toggle_pause(id);
}

bool BattleDebugSceneController::set_all_paused(BattleScene& scene, bool paused) {
    scene.set_all_paused(paused);
    return true;
}

bool BattleDebugSceneController::step_frame(BattleScene& scene, VisualEntityId id, int delta) {
    return scene.step_frame(id, delta);
}

bool BattleDebugSceneController::move_unit(BattleScene& scene, VisualEntityId id, Vec2 delta) {
    return scene.move_unit(id, delta.x, delta.y);
}

std::optional<Vec2> BattleDebugSceneController::unit_position_offset(const BattleScene& scene,
                                                                     VisualEntityId     id) {
    return scene.unit_position_offset(id);
}

} // namespace d2engine
