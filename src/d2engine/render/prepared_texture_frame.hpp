#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace d2engine {

struct PreparedTextureFrame {
    std::string               container_path;
    std::string               image_name;
    std::vector<std::uint8_t> rgba;
    std::uint32_t             width = 0;
    std::uint32_t             height = 0;
    double                    decode_ms = 0.0;
    double                    compose_ms = 0.0;
};

} // namespace d2engine
