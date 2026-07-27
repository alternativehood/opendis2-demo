#include "hexdump.hpp"
#include <algorithm>
#include <cstdio>
#include <sstream>

namespace d2res {

static bool is_printable(uint8_t b) {
    return b >= 0x20 && b <= 0x7E;
}

static uint32_t read_u32le(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

void write_hexdump(std::ostream& out, std::span<const uint8_t> data, HexdumpOptions opts) {
    out << "Offset   | 00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F | ASCII            | "
           "u32le[0]   u32le[1]   u32le[2]   u32le[3]\n";
    out << "---------|-------------------------------------------------|------------------|--------"
           "---------------------------------\n";

    const std::size_t limit = std::min(data.size(), opts.limit_bytes);
    char              buf[64];

    for (std::size_t offset = 0; offset < limit; offset += 16) {
        const std::size_t row_len = std::min<std::size_t>(16, limit - offset);
        const uint8_t*    row = data.data() + offset;

        std::snprintf(buf, sizeof(buf), "0x%06llX | ", static_cast<unsigned long long>(offset));
        out << buf;

        // Hex bytes, first group of 8
        for (std::size_t i = 0; i < 8; ++i) {
            if (i < row_len) {
                std::snprintf(buf, sizeof(buf), "%02X ", row[i]);
            } else {
                std::snprintf(buf, sizeof(buf), "   ");
            }
            out << buf;
        }
        out << ' '; // extra gap between groups

        // Hex bytes, second group of 8
        for (std::size_t i = 8; i < 16; ++i) {
            if (i < row_len) {
                std::snprintf(buf, sizeof(buf), "%02X ", row[i]);
            } else {
                std::snprintf(buf, sizeof(buf), "   ");
            }
            out << buf;
        }

        out << "| ";

        // ASCII column (16 chars)
        for (std::size_t i = 0; i < 16; ++i) {
            if (i < row_len) {
                out << (is_printable(row[i]) ? static_cast<char>(row[i]) : '.');
            } else {
                out << ' ';
            }
        }

        out << " | ";

        // Four u32le columns (only emitted when the full 4-byte group is present)
        for (std::size_t g = 0; g < 4; ++g) {
            const std::size_t gi = g * 4;
            if (gi + 4 <= row_len) {
                std::snprintf(buf, sizeof(buf), "0x%08X ", read_u32le(row + gi));
                out << buf;
            } else {
                out << "           "; // pad for alignment
            }
        }

        out << '\n';
    }

    if (data.size() > opts.limit_bytes) {
        std::ostringstream line;
        line << "... [truncated at " << static_cast<unsigned long long>(opts.limit_bytes)
             << " bytes, total " << static_cast<unsigned long long>(data.size()) << " bytes]\n";
        out << line.str();
    }
}

} // namespace d2res
