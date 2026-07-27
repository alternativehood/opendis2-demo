#pragma once
#include <cstddef>
#include <cstdint>
#include <ostream>
#include <span>

namespace d2res {

struct HexdumpOptions {
    std::size_t limit_bytes = 16384;
};

void write_hexdump(std::ostream& out, std::span<const uint8_t> data, HexdumpOptions opts = {});

} // namespace d2res
