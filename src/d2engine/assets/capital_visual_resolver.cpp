#include "capital_visual_resolver.hpp"

#include "game_data_registry.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>

namespace d2engine {

namespace {

[[nodiscard]] std::string lower(std::string s) {
    std::ranges::transform(s, s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    });
    return s;
}

[[nodiscard]] bool is_empty_member_id(const std::string& id) {
    return id.empty() || id == "G000000000";
}

} // namespace

CapitalVisualResolver::CapitalVisualResolver(
    const d2engine::adventure_render::CapitalAssetCatalog& catalog,
    const GameDataRegistry&                                game_data)
    : catalog_(catalog), game_data_(game_data) {}

d2engine::adventure_render::ResolvedCapitalVisual
CapitalVisualResolver::resolve(const d2runtime::AdventureWorldState& world,
                               const d2runtime::AdventureCapital&    capital,
                               std::string_view                      race_id) const {
    const auto  canonical_race_id = lower(std::string(race_id));
    const auto* race = game_data_.find_race(canonical_race_id);
    if (race == nullptr) {
        throw std::runtime_error("capital_race_definition_missing race_id=" + std::string(race_id) +
                                 " canonical=" + canonical_race_id);
    }

    const auto guardian_type_id = lower(race->guardian_unit_id);
    if (guardian_type_id.empty() || guardian_type_id == "g000000000") {
        throw std::runtime_error("capital_guardian_definition_missing race_id=" +
                                 canonical_race_id + " guardian_type=" + guardian_type_id);
    }

    d2engine::adventure_render::ResolvedCapitalVisual resolved;
    resolved.guardian_type_id = guardian_type_id;

    for (const auto& member_id : capital.garrison.members) {
        if (!member_id.has_value() || is_empty_member_id(*member_id)) {
            continue;
        }
        const auto* instance = world.find_unit(*member_id);
        if (instance == nullptr) {
            throw std::runtime_error("capital_garrison_unit_missing capital=" + capital.id +
                                     " unit=" + *member_id);
        }

        const auto instance_type_id = lower(instance->type_id);
        if (instance_type_id != guardian_type_id) {
            continue;
        }

        resolved.guardian_instance_id = instance->id;
        resolved.state = instance->current_hp > 0
                             ? d2engine::adventure_render::CapitalVisualState::Active
                             : d2engine::adventure_render::CapitalVisualState::Ruined;
        resolved.visual = &catalog_.resolve(canonical_race_id, resolved.state);
        return resolved;
    }

    resolved.state = d2engine::adventure_render::CapitalVisualState::Ruined;
    resolved.visual = &catalog_.resolve(canonical_race_id, resolved.state);
    return resolved;
}

} // namespace d2engine
