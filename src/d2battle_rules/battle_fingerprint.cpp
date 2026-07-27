#include "battle_fingerprint.hpp"
#include "battle_effect.hpp"

#include <algorithm>
#include <cstdint>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

namespace d2battle {

namespace {

constexpr std::uint64_t kFnvOffsetBasis = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

void fnv_write(std::uint64_t& hash, const void* data, std::size_t len) {
    auto* bytes = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < len; ++i) {
        hash ^= bytes[i];
        hash *= kFnvPrime;
    }
}

void fnv_str(std::uint64_t& hash, const std::string& s) {
    fnv_write(hash, s.data(), s.size());
    fnv_write(hash, "\x00", 1);
}

void fnv_u8(std::uint64_t& hash, std::uint8_t v) {
    fnv_write(hash, &v, 1);
}

void fnv_i32(std::uint64_t& hash, int v) {
    fnv_write(hash, &v, sizeof(v));
}

void fnv_u32(std::uint64_t& hash, std::uint32_t v) {
    fnv_write(hash, &v, sizeof(v));
}

void fnv_bool(std::uint64_t& hash, bool v) {
    std::uint8_t b = v ? 1 : 0;
    fnv_write(hash, &b, 1);
}

void fnv_side(std::uint64_t& hash, const BattleSideState& s) {
    fnv_str(hash, s.source_stack_id);
    fnv_str(hash, s.owner);
    fnv_str(hash, s.subrace);
    fnv_str(hash, s.leader_id);
    fnv_u8(hash, s.leader_alive);
    fnv_i32(hash, s.morale);
    fnv_i32(hash, s.battles_won);
    for (const auto& m : s.members)
        fnv_str(hash, m.has_value() ? *m : std::string{});
    for (int i = 0; i < 6; ++i) {
        fnv_i32(hash, s.positions[static_cast<std::size_t>(i)]);
        fnv_i32(hash, s.cell_members[static_cast<std::size_t>(i)]);
    }
    fnv_str(hash, s.banner);
    fnv_str(hash, s.tome);
    fnv_str(hash, s.battle1);
    fnv_str(hash, s.battle2);
    fnv_str(hash, s.artifact1);
    fnv_str(hash, s.artifact2);
    fnv_str(hash, s.boots);
}

void fnv_effect(std::uint64_t& hash, const BattleUnitEffectState& e) {
    fnv_i32(hash, static_cast<int>(e.index()));
    if (auto* p = std::get_if<PetrifiedEffect>(&e)) {
        fnv_str(hash, p->source_actor_id);
        fnv_str(hash, p->source_attack_id);
        fnv_u32(hash, p->remaining_activation_skips);
    }
}

void fnv_unit(std::uint64_t& hash, const BattleUnitState& u) {
    fnv_str(hash, u.id);
    fnv_str(hash, u.type_id);
    fnv_i32(hash, u.serialized_level);
    for (const auto& mid : u.modifier_ids)
        fnv_str(hash, mid);
    fnv_i32(hash, u.creation);
    fnv_str(hash, u.name);
    fnv_u8(hash, u.transformed);
    fnv_i32(hash, u.dynamic_level.has_value() ? static_cast<int>(*u.dynamic_level) : -1);
    fnv_i32(hash, u.current_hp);
    fnv_i32(hash, u.xp);
    fnv_i32(hash, static_cast<int>(u.side));
    fnv_i32(hash, u.member_index);
    fnv_i32(hash, u.formation_cell);
    fnv_bool(hash, u.alive);
    if (!u.effects.empty()) {
        fnv_u8(hash, 0xEF);
        fnv_u32(hash, static_cast<std::uint32_t>(u.effects.size()));
        for (const auto& e : u.effects)
            fnv_effect(hash, e);
    }
}

} // namespace

std::string compute_fingerprint(const BattleState& state) {
    std::uint64_t hash = kFnvOffsetBasis;

    fnv_i32(hash, static_cast<int>(state.status));
    fnv_i32(hash, state.winner.has_value() ? static_cast<int>(*state.winner) : -1);
    fnv_side(hash, state.party1);
    fnv_side(hash, state.party2);
    fnv_u32(hash, state.round_state.round_number);
    fnv_i32(hash, static_cast<int>(state.round_state.current_turn_index));

    for (const auto& u : state.units)
        fnv_unit(hash, u);

    for (const auto& entry : state.round_state.turn_order) {
        fnv_str(hash, entry.unit_id);
        fnv_i32(hash, entry.effective_initiative);
        fnv_i32(hash, entry.tie_break_key);
    }

    std::ostringstream oss;
    oss << std::hex << hash;
    return oss.str();
}

} // namespace d2battle
