#pragma once

#include "AdventureTerrain.hpp"
#include "AdventureWorldState.hpp"

#include <vector>

namespace d2runtime {

class AdventureTerrainDecoder {
public:
    [[nodiscard]] AdventureTerrainTileDescriptor
    decode_tile(uint32_t raw_value, const AdventureTerrainDecodeOptions& options = {}) const;
};

class AdventureTerrainMapDecoder {
public:
    explicit AdventureTerrainMapDecoder(const AdventureTerrainDecoder& tile_decoder);

    [[nodiscard]] std::vector<AdventureTerrainTileDescriptor>
    decode_grid(const AdventureTerrainGrid&          grid,
                const AdventureTerrainDecodeOptions& options = {}) const;

private:
    const AdventureTerrainDecoder& tile_decoder_;
};

} // namespace d2runtime
