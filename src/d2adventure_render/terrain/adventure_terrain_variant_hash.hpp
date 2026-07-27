#pragma once

#include <cstddef>
#include <cstdint>

namespace d2engine {

[[nodiscard]] std::uint64_t coordinate_key(std::int32_t x, std::int32_t y);

[[nodiscard]] std::uint64_t terrain_variant_hash(std::int32_t x, std::int32_t y);

[[nodiscard]] std::uint32_t terrain_variant_bucket(std::int32_t x, std::int32_t y);

std::size_t select_border_variant_index_from_bucket(std::uint32_t bucket,
                                                    std::size_t   variant_count);

} // namespace d2engine
