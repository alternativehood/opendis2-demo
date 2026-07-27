#include "random_action_policy.hpp"

#include <d2battle_rules/battle_action.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace d2battle_sweep {

namespace {

constexpr std::uint64_t kFnvOffsetBasis = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

} // namespace

std::uint64_t stable_hash_64(std::uint64_t seed, const std::string& a, const std::string& b) {
    std::uint64_t hash = kFnvOffsetBasis;

    auto fnv_u64 = [&](std::uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            hash ^= static_cast<unsigned char>(v & 0xFF);
            hash *= kFnvPrime;
            v >>= 8;
        }
    };

    auto fnv_str = [&](const std::string& s) {
        for (char c : s) {
            hash ^= static_cast<unsigned char>(c);
            hash *= kFnvPrime;
        }
        hash ^= 0;
        hash *= kFnvPrime;
    };

    fnv_u64(seed);
    fnv_str(a);
    fnv_str(b);

    return hash;
}

RandomActionPolicy::RandomActionPolicy(std::uint64_t seed) : rng_(seed) {}

std::size_t
RandomActionPolicy::choose_index(std::span<const d2battle::BattleActionOutcome> outcomes) {
    if (outcomes.empty())
        return 0;
    std::uniform_int_distribution<std::size_t> dist(0, outcomes.size() - 1);
    return dist(rng_);
}

} // namespace d2battle_sweep
