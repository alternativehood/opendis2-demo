#pragma once

#include "game_data_registry.hpp"

#include <d2runtime/AdventureIsoDirection.hpp>
#include <d2runtime/AdventureActorAnimationResolver.hpp>
#include <d2runtime/AdventureStackPresentationResolver.hpp>
#include <d2runtime/AdventureWorldState.hpp>

#include <string>

namespace d2engine {

struct AdventureStackActorVisualRequest {
    d2runtime::AdventureActorPresentation  presentation;
    d2runtime::AdventureActorAnimationRole role = d2runtime::AdventureActorAnimationRole::Idle;
    std::string                            leader_unit_type_id;
    std::string                            race_id;
    d2runtime::AdventureIsoDirection       direction = d2runtime::AdventureIsoDirection::D0;
};

class AdventureStackActorRequestResolver {
public:
    explicit AdventureStackActorRequestResolver(const GameDataRegistry& game_data);

    [[nodiscard]] AdventureStackActorVisualRequest
    resolve(const d2runtime::AdventureWorldState& world,
            const d2runtime::AdventureStack&      stack) const;

    [[nodiscard]] AdventureStackActorVisualRequest
    resolve_for(const d2runtime::AdventureWorldState& world, const d2runtime::AdventureStack& stack,
                d2runtime::MapCellCoord                presentation_cell,
                d2runtime::AdventureIsoDirection       direction,
                d2runtime::AdventureActorAnimationRole role) const;

private:
    const GameDataRegistry* game_data_;
};

} // namespace d2engine
