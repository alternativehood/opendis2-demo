#include "adventure_terrain_variant_hash.hpp"

#include <stdexcept>

namespace d2engine {

std::uint64_t coordinate_key(std::int32_t x, std::int32_t y) {
    const auto ux = static_cast<std::uint32_t>(x);
    const auto uy = static_cast<std::uint32_t>(y);
    return (static_cast<std::uint64_t>(ux) << 32U) | static_cast<std::uint64_t>(uy);
}

std::uint64_t terrain_variant_hash(std::int32_t x, std::int32_t y) {
    std::uint64_t z = coordinate_key(x, y);

    z += 0x9E3779B97F4A7C15ULL;
    z = (z ^ (z >> 30U)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27U)) * 0x94D049BB133111EBULL;
    z ^= z >> 31U;

    return z;
}

std::uint32_t terrain_variant_bucket(std::int32_t x, std::int32_t y) {
    return static_cast<std::uint32_t>(terrain_variant_hash(x, y) >> 62U);
}

std::size_t select_border_variant_index_from_bucket(std::uint32_t bucket,
                                                    std::size_t   variant_count) {
    if (variant_count == 0) {
        throw std::invalid_argument("border variant count cannot be zero");
    }
    if (variant_count > 4) {
        throw std::logic_error("unsupported border variant count greater than 4");
    }
    if (variant_count == 2) {
        return bucket == 3U ? 1U : 0U;
    }
    if (variant_count == 3) {
        if (bucket < 2U) {
            return 0U;
        }
        return static_cast<std::size_t>(bucket - 1U);
    }
    if (variant_count == 4) {
        return static_cast<std::size_t>(bucket);
    }
    return 0;
}

} // namespace d2engine
