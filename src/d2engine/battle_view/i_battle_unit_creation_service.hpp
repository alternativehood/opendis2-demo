#pragma once

#include "unit_creation_types.hpp"

namespace d2engine {

class IBattleUnitCreationService {
public:
    IBattleUnitCreationService() = default;
    virtual ~IBattleUnitCreationService() = default;

    IBattleUnitCreationService(const IBattleUnitCreationService&) = delete;
    IBattleUnitCreationService& operator=(const IBattleUnitCreationService&) = delete;
    IBattleUnitCreationService(IBattleUnitCreationService&&) = delete;
    IBattleUnitCreationService& operator=(IBattleUnitCreationService&&) = delete;

    virtual UnitCreationData create_unit(const UnitCreationRequest& request) = 0;
};

} // namespace d2engine
