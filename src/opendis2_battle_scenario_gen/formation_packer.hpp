#pragma once

#include <d2engine/assets/unit_def.hpp>
#include <d2engine/battle_view/battle_slot.hpp>

#include <algorithm>
#include <array>
#include <random>
#include <stdexcept>
#include <vector>

namespace d2gen {

struct FormationSlotAssignment {
    const d2engine::UnitDef*  def;
    d2engine::BattleSlotCoord coord;
    bool                      is_large;
};

// Pack large+small unit defs into valid battle slots for one side.
// Large units → CENTER slots. Small units → FRONT/BACK of lanes not taken by large units.
// Throws std::invalid_argument if:
//   - any pointer is null
//   - large_defs.size() > 3
//   - large_defs.size()*2 + small_defs.size() > 6
//   - a small UnitDef (size_small==true) appears in large_defs
//   - a large UnitDef (size_small==false) appears in small_defs
inline std::vector<FormationSlotAssignment>
pack_formation(d2engine::BattleSide side, std::vector<const d2engine::UnitDef*> large_defs,
               std::vector<const d2engine::UnitDef*> small_defs, std::mt19937& rng) {
    if (large_defs.size() > 3)
        throw std::invalid_argument("pack_formation: large_defs.size() > 3");
    if (large_defs.size() * 2 + small_defs.size() > 6)
        throw std::invalid_argument("pack_formation: physical footprint exceeds 6 cells");
    for (const auto* p : large_defs) {
        if (p == nullptr)
            throw std::invalid_argument("pack_formation: null UnitDef pointer in large_defs");
        if (p->size_small) {
            throw std::invalid_argument(
                "pack_formation: small UnitDef (size_small==true) in large_defs");
        }
    }
    for (const auto* p : small_defs) {
        if (p == nullptr)
            throw std::invalid_argument("pack_formation: null UnitDef pointer in small_defs");
        if (!p->size_small) {
            throw std::invalid_argument(
                "pack_formation: large UnitDef (size_small==false) in small_defs");
        }
    }

    std::array<int, 3> lanes = {0, 1, 2};
    std::shuffle(lanes.begin(), lanes.end(), rng);

    std::vector<FormationSlotAssignment> out;
    out.reserve(large_defs.size() + small_defs.size());

    std::array<bool, 3> lane_occupied = {false, false, false};

    for (std::size_t i = 0; i < large_defs.size(); ++i) {
        const int lane = lanes[i];
        lane_occupied[static_cast<std::size_t>(lane)] = true;
        out.push_back({large_defs[i],
                       {.side = side, .lane = lane, .depth = d2engine::BattleDepth::Center},
                       true});
    }

    std::size_t src = 0;
    for (auto depth : {d2engine::BattleDepth::Front, d2engine::BattleDepth::Back}) {
        for (int lane = 0; lane < 3 && src < small_defs.size(); ++lane) {
            if (lane_occupied[static_cast<std::size_t>(lane)])
                continue;
            out.push_back({small_defs[src++], {.side = side, .lane = lane, .depth = depth}, false});
        }
    }

    return out;
}

} // namespace d2gen
