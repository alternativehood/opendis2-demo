#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace d2runtime {
struct AdventureWorldState;
} // namespace d2runtime

namespace d2engine {
class GameDataRegistry;
} // namespace d2engine

namespace d2battle_sweep {

struct StackCatalogEntry {
    std::string stack_id;
    std::string owner_id;
    std::string leader_id;

    std::vector<std::string> member_unit_ids;
    std::array<int, 6>       cell_members{};

    std::string human_descriptor;
};

struct StackCatalogDiagnostic {
    std::string stack_id;
    std::string reason;
};

struct StackCatalogResult {
    std::vector<StackCatalogEntry>      entries;
    std::vector<StackCatalogDiagnostic> diagnostics;
};

[[nodiscard]] StackCatalogResult build_stack_catalog(const d2runtime::AdventureWorldState& world,
                                                     const d2engine::GameDataRegistry& game_data);

} // namespace d2battle_sweep
