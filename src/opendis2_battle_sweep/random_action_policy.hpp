#pragma once

#include <cstddef>
#include <cstdint>
#include <random>
#include <span>

namespace d2battle {
struct BattleActionOutcome;
} // namespace d2battle

namespace d2battle_sweep {

class RandomActionPolicy {
public:
    explicit RandomActionPolicy(std::uint64_t seed);

    [[nodiscard]] std::size_t choose_index(std::span<const d2battle::BattleActionOutcome> outcomes);

private:
    std::mt19937_64 rng_;
};

[[nodiscard]] std::uint64_t stable_hash_64(std::uint64_t seed, const std::string& a,
                                           const std::string& b);

} // namespace d2battle_sweep
