#pragma once

#include <raylib.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

namespace game {

struct SectorShadowCasterBoundsRecord {
    uint64_t key = 0;
    BoundingBox bounds{};
    uint64_t contentFingerprint = 0;
    unsigned int part = 0; // Distinguishes pieces of the same placed object.
};

// Output storage is reserved by the renderer during loading.
inline void AppendChangedSectorShadowCasterBounds(
        const std::vector<SectorShadowCasterBoundsRecord>& current,
        const std::vector<SectorShadowCasterBoundsRecord>& previous,
        std::vector<BoundingBox>& outChangedBounds)
{
    for (const SectorShadowCasterBoundsRecord& currentRecord : current) {
        const auto previousIt = std::find_if(
                previous.begin(), previous.end(),
                [&currentRecord](const SectorShadowCasterBoundsRecord& candidate) {
                    return candidate.key == currentRecord.key && candidate.part == currentRecord.part;
                });
        if (previousIt == previous.end()) {
            outChangedBounds.push_back(currentRecord.bounds);
            continue;
        }
        if (std::memcmp(&previousIt->bounds,
                    &currentRecord.bounds, sizeof(BoundingBox)) != 0
                || previousIt->contentFingerprint
                        != currentRecord.contentFingerprint) {
            outChangedBounds.push_back(previousIt->bounds);
            outChangedBounds.push_back(currentRecord.bounds);
        }
    }
    for (const SectorShadowCasterBoundsRecord& previousRecord : previous) {
        const auto currentIt = std::find_if(
                current.begin(), current.end(),
                [&previousRecord](const SectorShadowCasterBoundsRecord& candidate) {
                    return candidate.key == previousRecord.key && candidate.part == previousRecord.part;
                });
        if (currentIt == current.end()) {
            outChangedBounds.push_back(previousRecord.bounds);
        }
    }
}

} // namespace game
