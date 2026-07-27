#include "contained_stack_presentation_contributor.hpp"

#include <d2adventure_render/map_geometry.hpp>

#include <d2log/log.hpp>

#include <stdexcept>
#include <string>
#include <string_view>

namespace d2engine::adventure_render {

namespace {

[[nodiscard]] bool is_empty_settlement_stack_id(std::string_view stack_id) {
    return stack_id.empty() || stack_id == "G000000000";
}

template <typename Settlement>
void add_settlement_presentation(const d2runtime::AdventureWorldState&    world,
                                 PreparationContext&                      ctx,
                                 const ContainedStackPresentationBuilder& builder,
                                 const Settlement& settlement, std::string_view stack_id,
                                 std::string_view settlement_kind) {
    if (is_empty_settlement_stack_id(stack_id)) {
        return;
    }

    const auto* stack = world.find_stack(std::string(stack_id));
    if (stack == nullptr) {
        throw std::runtime_error("contained_stack_shield_missing_stack settlement=" +
                                 std::string(settlement.id) + " stack_id=" + std::string(stack_id));
    }
    if (d2runtime::is_stack_on_adventure_map(*stack)) {
        throw std::runtime_error("contained_stack_shield_stack_is_map_visible settlement=" +
                                 std::string(settlement.id) + " stack_id=" + stack->id);
    }
    if (stack->inside != settlement.id) {
        throw std::runtime_error(
            "contained_stack_shield_inside_mismatch settlement=" + std::string(settlement.id) +
            " stack_id=" + stack->id + " inside=" + stack->inside);
    }

    const auto location = world.find_contained_stack_location(*stack);
    if (!location.has_value() || location->settlement_id != settlement.id) {
        throw std::runtime_error("contained_stack_shield_location_mismatch settlement=" +
                                 std::string(settlement.id) + " stack_id=" + stack->id);
    }
    if (stack->subrace.empty()) {
        throw std::runtime_error("contained_stack_shield_missing_subrace settlement=" +
                                 std::string(settlement.id) + " stack_id=" + stack->id);
    }
    const auto* subrace = world.find_subrace(stack->subrace);
    if (subrace == nullptr) {
        throw std::runtime_error(
            "contained_stack_shield_dangling_subrace settlement=" + std::string(settlement.id) +
            " stack_id=" + stack->id + " subrace=" + stack->subrace);
    }
    if (subrace->race_id.empty()) {
        throw std::runtime_error("contained_stack_shield_missing_race_id settlement=" +
                                 std::string(settlement.id) + " stack_id=" + stack->id);
    }
    if (settlement.footprint.empty()) {
        throw std::runtime_error("contained_stack_shield_missing_footprint settlement=" +
                                 std::string(settlement.id) + " stack_id=" + stack->id);
    }

    const auto presentation = builder.build(world, *stack, *location, *subrace);
    ctx.add_primitive(presentation.shield);
    ctx.add_pick_entry(presentation.pick_entry);

    d2log::get("d2.adventure")
        ->debug("contained_stack_presentation stack={} settlement={} kind={} subrace={} race={} "
                "shield_logical={} shield_sprite={}",
                stack->id, settlement.id, settlement_kind, stack->subrace, subrace->race_id,
                presentation.shield_outer_logical_name, presentation.shield_sprite_name);
}

} // namespace

RenderContributor make_contained_stack_presentation_contributor(
    const ContainedStackShieldAssetCatalog& shield_catalog) {
    return [shield_catalog](const d2runtime::AdventureWorldState& world, PreparationContext& ctx) {
        const auto                              geometry = ctx.geometry();
        const ContainedStackPresentationBuilder builder(shield_catalog, geometry);
        for (const auto& city : world.cities) {
            add_settlement_presentation(world, ctx, builder, city, city.stack_id, "Village");
        }
        for (const auto& capital : world.capitals) {
            add_settlement_presentation(world, ctx, builder, capital, capital.visiting_stack_id,
                                        "Capital");
        }
    };
}

} // namespace d2engine::adventure_render
