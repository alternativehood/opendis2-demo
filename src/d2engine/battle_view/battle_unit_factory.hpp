#pragma once

#include "i_battle_unit_creation_service.hpp"
#include "unit_attack_visual_intent.hpp"
#include "unit_creation_types.hpp"

namespace d2engine {

class IAnimationCatalog;
class GameDataRegistry;

class BattleUnitFactory : public IBattleUnitCreationService {
public:
    BattleUnitFactory(const GameDataRegistry& registry, const IAnimationCatalog& catalog,
                      const UnitAttackVisualIntentMap& intent_map)
        : registry_(registry), catalog_(catalog), intent_map_(intent_map) {}

    UnitCreationData create_unit(const UnitCreationRequest& request) override;

private:
    const GameDataRegistry&          registry_;
    const IAnimationCatalog&         catalog_;
    const UnitAttackVisualIntentMap& intent_map_;
};

} // namespace d2engine
