#include "adventure_stack_actor_request_resolver.hpp"

#include <d2runtime/AdventureStackPresentationResolver.hpp>
#include <d2runtime/AdventureGroundClassifier.hpp>
#include <d2runtime/AdventureTerrainDecoder.hpp>
#include <d2runtime/AdventureTerrain.hpp>
#include <d2runtime/MovementCapabilities.hpp>

#include <stdexcept>
#include <string>

namespace d2engine {

AdventureStackActorRequestResolver::AdventureStackActorRequestResolver(
    const GameDataRegistry& game_data)
    : game_data_(&game_data) {}

AdventureStackActorVisualRequest
AdventureStackActorRequestResolver::resolve(const d2runtime::AdventureWorldState& world,
                                            const d2runtime::AdventureStack&      stack) const {
    return resolve_for(world, stack, stack.position, stack.facing,
                       d2runtime::AdventureActorAnimationRole::Idle);
}

AdventureStackActorVisualRequest AdventureStackActorRequestResolver::resolve_for(
    const d2runtime::AdventureWorldState& world, const d2runtime::AdventureStack& stack,
    d2runtime::MapCellCoord presentation_cell, d2runtime::AdventureIsoDirection direction,
    d2runtime::AdventureActorAnimationRole role) const {
    if (!d2runtime::is_stack_on_adventure_map(stack)) {
        std::string msg = "stack_not_map_visible id=";
        msg += stack.id;
        msg += " pos=(" + std::to_string(presentation_cell.x) + "," +
               std::to_string(presentation_cell.y) + ")";
        throw std::runtime_error(msg);
    }

    // 1. Read terrain at stack position
    const auto* raw_tile = world.terrain.tile_at(presentation_cell.x, presentation_cell.y);
    if (raw_tile == nullptr) {
        std::string msg = "stack_position_out_of_bounds id=";
        msg += stack.id;
        msg += " pos=(" + std::to_string(presentation_cell.x) + "," +
               std::to_string(presentation_cell.y) + ")";
        msg += " terrain=" + std::to_string(world.terrain.width) + "x" +
               std::to_string(world.terrain.height);
        throw std::runtime_error(msg);
    }

    const d2runtime::AdventureTerrainDecoder tile_decoder;
    const auto descriptor = tile_decoder.decode_tile(raw_tile->raw_value);

    const auto ground = d2runtime::classify_adventure_ground(descriptor);
    if (ground == d2runtime::AdventureGroundType::Unknown) {
        std::string msg = "unknown_terrain_material id=";
        msg += stack.id;
        msg += " pos=(" + std::to_string(presentation_cell.x) + "," +
               std::to_string(presentation_cell.y) + ")";
        msg += " raw_value=" + std::to_string(descriptor.raw_value);
        msg += " material=" + std::to_string(static_cast<int>(descriptor.material));
        throw std::runtime_error(msg);
    }

    // 2. Resolve leader
    const auto* leader = world.find_unit(stack.leader_id);
    if (leader == nullptr) {
        std::string msg = "missing_leader_instance id=";
        msg += stack.id;
        msg += " pos=(" + std::to_string(presentation_cell.x) + "," +
               std::to_string(presentation_cell.y) + ")";
        msg += " leader_id=" + stack.leader_id;
        throw std::runtime_error(msg);
    }

    const auto* leader_def = game_data_->find_unit(leader->type_id);
    if (leader_def == nullptr) {
        std::string msg = "missing_leader_unit_def id=";
        msg += stack.id;
        msg += " pos=(" + std::to_string(presentation_cell.x) + "," +
               std::to_string(presentation_cell.y) + ")";
        msg += " leader_id=" + stack.leader_id;
        msg += " leader_type=" + leader->type_id;
        throw std::runtime_error(msg);
    }

    const auto leader_movement =
        d2runtime::MovementCapabilities::from_native_ability_ids(leader_def->native_ability_ids);
    const bool leader_water_only = leader_def->water_only;

    // 3. Resolve presentation
    d2runtime::AdventureStackPresentationInput pres_input;
    pres_input.ground = ground;
    pres_input.leader_movement = leader_movement;
    pres_input.leader_water_only = leader_water_only;

    d2runtime::AdventureStackPresentationResolver pres_resolver;
    const auto                                    presentation = pres_resolver.resolve(pres_input);

    // 4. Build request
    AdventureStackActorVisualRequest request;
    request.presentation = presentation;
    request.role = role;
    request.leader_unit_type_id = leader->type_id;
    request.direction = direction;

    if (presentation.kind == d2runtime::AdventureActorPresentationKind::Boat) {
        // Resolve boat race from stack subrace
        if (stack.subrace.empty()) {
            std::string msg = "boat_presentation_missing_subrace id=";
            msg += stack.id;
            msg += " pos=(" + std::to_string(presentation_cell.x) + "," +
                   std::to_string(presentation_cell.y) + ")";
            msg += " owner=" + stack.owner;
            msg += " leader_type=" + leader->type_id;
            msg += " subrace=<empty>";
            throw std::runtime_error(msg);
        }

        const auto* subrace_ref = world.find_subrace(stack.subrace);
        if (subrace_ref == nullptr) {
            std::string msg = "boat_presentation_dangling_subrace id=";
            msg += stack.id;
            msg += " pos=(" + std::to_string(presentation_cell.x) + "," +
                   std::to_string(presentation_cell.y) + ")";
            msg += " owner=" + stack.owner;
            msg += " leader_type=" + leader->type_id;
            msg += " subrace=" + stack.subrace;
            throw std::runtime_error(msg);
        }

        if (subrace_ref->race_id.empty()) {
            std::string msg = "boat_presentation_empty_race_id id=";
            msg += stack.id;
            msg += " pos=(" + std::to_string(presentation_cell.x) + "," +
                   std::to_string(presentation_cell.y) + ")";
            msg += " owner=" + stack.owner;
            msg += " leader_type=" + leader->type_id;
            msg += " subrace=" + stack.subrace;
            msg += " player_id=" + subrace_ref->player_id;
            throw std::runtime_error(msg);
        }

        request.race_id = subrace_ref->race_id;
    }

    request.direction = direction;
    return request;
}

} // namespace d2engine
