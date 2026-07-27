#include "stack_catalog.hpp"

#include <d2engine/assets/game_data_registry.hpp>
#include <d2runtime/AdventureWorldState.hpp>

#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace d2battle_sweep {

StackCatalogResult build_stack_catalog(const d2runtime::AdventureWorldState& world,
                                       const d2engine::GameDataRegistry&     game_data) {
    StackCatalogResult result;

    for (const auto& stack : world.stacks) {
        if (!d2runtime::is_stack_on_adventure_map(stack))
            continue;

        if (stack.group.members.empty())
            continue;

        bool has_alive_member = false;
        for (const auto& member : stack.group.members) {
            if (!member.has_value())
                continue;
            const auto* unit = world.find_unit(*member);
            if (unit && unit->current_hp > 0) {
                has_alive_member = true;
                break;
            }
        }

        if (!has_alive_member) {
            result.diagnostics.push_back({stack.id, "no alive members"});
            continue;
        }

        StackCatalogEntry entry;
        entry.stack_id = stack.id;
        entry.owner_id = stack.owner;
        entry.leader_id = stack.leader_id;

        for (const auto& m : stack.group.members) {
            if (m.has_value())
                entry.member_unit_ids.push_back(*m);
        }

        for (int i = 0; i < 6; ++i) {
            entry.cell_members[static_cast<std::size_t>(i)] =
                stack.group.cell_members[static_cast<std::size_t>(i)];
        }

        std::string desc;
        for (std::size_t slot = 0; slot < stack.group.members.size(); ++slot) {
            const auto& member = stack.group.members[slot];
            if (!member.has_value())
                continue;
            const auto* unit = world.find_unit(*member);
            if (!unit)
                continue;
            if (!desc.empty())
                desc += "+";

            if (!unit->name.empty()) {
                desc += unit->name;
            } else {
                const auto* udef = game_data.find_unit(unit->type_id);
                if (udef && !udef->name.empty()) {
                    desc += udef->name;
                } else {
                    desc += unit->type_id;
                }
            }
        }
        if (desc.empty())
            desc = "UNKNOWN";
        entry.human_descriptor = desc;
        result.entries.push_back(std::move(entry));
    }

    std::sort(result.entries.begin(), result.entries.end(),
              [](const StackCatalogEntry& a, const StackCatalogEntry& b) {
                  return a.stack_id < b.stack_id;
              });

    return result;
}

} // namespace d2battle_sweep
