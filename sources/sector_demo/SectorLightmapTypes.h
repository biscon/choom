#pragma once

#include "sector_demo/SectorUnits.h"

#include <string>

namespace game {

struct SectorLightmapBakeSettings {
    float ambientOcclusionRadius = SectorWorldToAuthoringDistance(1.25f);
    float ambientOcclusionStrength = 0.55f;
    float indirectBounceRadius = SectorWorldToAuthoringDistance(4.0f);
    float indirectBounceStrength = 0.20f;
};

struct SectorLightmapMetadata {
    std::string path;
    int width = 0;
    int height = 0;
    std::string sourceHash;
};

} // namespace game
