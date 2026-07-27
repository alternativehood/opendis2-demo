#pragma once

#include <cstdint>

namespace d2engine {
namespace adventure_render {

// Stable deterministic hash of (seed, canonical cell.x, canonical cell.y) into a uint32_t.
[[nodiscard]] constexpr std::uint32_t stable_tree_hash(int seed, int canonical_x,
                                                       int canonical_y) noexcept {
    // SplitMix64-inspired mixing with 32-bit output
    auto h = static_cast<std::uint64_t>(static_cast<std::uint32_t>(seed));
    auto x = static_cast<std::uint64_t>(static_cast<std::uint32_t>(canonical_x));
    auto y = static_cast<std::uint64_t>(static_cast<std::uint32_t>(canonical_y));

    // Combine
    h ^= x * 0x9E3779B97F4A7C15ULL;
    h = (h ^ (h >> 30)) * 0xBF58476D1CE4E5B9ULL;
    h ^= y * 0x9E3779B97F4A7C15ULL;
    h = (h ^ (h >> 27)) * 0x94D049BB133111EBULL;
    h = h ^ (h >> 31);

    return static_cast<std::uint32_t>(h);
}

} // namespace adventure_render
} // namespace d2engine
