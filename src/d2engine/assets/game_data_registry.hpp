#pragma once

#include "attack_def.hpp"
#include "race_def.hpp"
#include "unit_def.hpp"

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace d2engine {

class GameDataRegistry {
public:
    explicit GameDataRegistry(const std::filesystem::path& globals_dir);

    GameDataRegistry(const GameDataRegistry&) = delete;
    GameDataRegistry& operator=(const GameDataRegistry&) = delete;
    GameDataRegistry(GameDataRegistry&&) = default;
    GameDataRegistry& operator=(GameDataRegistry&&) = default;

    ~GameDataRegistry() = default;

    // Lookup by lowercase unit ID (e.g. "g000uu0001")
    [[nodiscard]] const UnitDef* find_unit(std::string_view id) const;

    [[nodiscard]] const RaceDef* find_race(std::string_view race_id) const;
    [[nodiscard]] const RaceDef* find_race_by_type(int race_type) const;

    // Resolve a text ID from Tglobal.dbf — returns empty string_view if not found

    [[nodiscard]] const std::vector<UnitDef>& all_units() const { return units_; }

    // Map an L-name from LDthAnim (e.g. "L_HUMAN") to a Battle.ff animation entry.
    // L_ELF branches on size_small: true → DEATH_ELF_S15, false → DEATH_ELF_L15.
    static std::string death_battle_ff_name(std::string_view l_xxx, bool size_small);

    // Returns base sprite name for the permanent bones that remain after death.
    // Pass to corpse_sequence() — naming convention depends on whether the name has a .PNG
    // extension.
    static std::string bones_sprite_base(std::string_view l_xxx, bool size_small);

    // Visual unit resolution: follow BASE_UNIT chain to find a unit with assets.
    // Calls `has_visual` on each candidate. The first concrete unit with assets wins.
    // Returns the concrete unit_id if no BASE_UNIT has visual assets.
    // Uses cycle detection. Does NOT follow PREV_ID.
    using VisualCheckFn = std::function<bool(const std::string& unit_id)>;
    [[nodiscard]] std::string resolve_visual_type(std::string_view     concrete_unit_id,
                                                  const VisualCheckFn& has_visual) const;

private:
    std::vector<UnitDef>       units_;
    std::vector<AttackDef>     attacks_;
    std::vector<DynUpgradeDef> dyn_upgrades_;
    std::vector<RaceDef>       races_;

    std::unordered_map<std::string, std::size_t> unit_by_id_;
    std::unordered_map<std::string, std::size_t> attack_by_id_;
    std::unordered_map<std::string, std::size_t> upgrade_by_id_;
    std::unordered_map<std::string, std::size_t> race_by_id_;
    std::unordered_map<int, std::size_t>         race_by_type_;

    std::unordered_map<std::string, std::string> text_map_;
};

} // namespace d2engine
