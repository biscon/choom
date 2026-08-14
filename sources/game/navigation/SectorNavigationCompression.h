#pragma once

#include <cstdint>
#include <vector>

namespace game {

int SectorNavigationMaximumCompressedLayerSize(int uncompressedSize);

bool CompressSectorNavigationLayer(
        const uint8_t* uncompressed,
        int uncompressedSize,
        uint8_t* compressed,
        int compressedCapacity,
        int& compressedSize);

bool DecompressSectorNavigationLayer(
        const uint8_t* compressed,
        int compressedSize,
        uint8_t* uncompressed,
        int uncompressedCapacity,
        int& uncompressedSize);

} // namespace game
