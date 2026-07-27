#pragma once

#include "battle_scene.hpp"
#include "battle_visual_event.hpp"
#include "battle_visual_profile_registry.hpp"

#include <vector>

namespace d2engine {

class VisualEffectResolver {
public:
    VisualEffectResolver(const BattleScene& scene, const UnitVisualProfileRegistry* profiles);
    [[nodiscard]] std::vector<BattleVisualEvent> expand(const BattleVisualEvent& event) const;

private:
    const BattleScene&               scene_;
    const UnitVisualProfileRegistry* profiles_;
};

} // namespace d2engine
