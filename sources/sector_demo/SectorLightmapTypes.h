#pragma once

#include "sector_demo/SectorUnits.h"

#include <raylib.h>

#include <string>
#include <vector>

namespace game {

struct SectorLightmapBakeSettings {
    float ambientOcclusionRadius = SectorWorldToAuthoringDistance(1.25f);
    float ambientOcclusionStrength = 0.55f;
    float indirectBounceRadius = SectorWorldToAuthoringDistance(4.0f);
    float indirectBounceStrength = 0.20f;
    float objectProbeSpacingWorld = 4.0f;
    float objectProbeHeightWorld = 1.2f;
};

struct SectorBakedObjectLightProbe {
    int sectorId = 0;
    Vector3 position = {};
    Vector3 ambientCube[6] = {};
};

struct SectorBakedObjectLightProbePlacementSettings {
    float probeSpacingWorld = 4.0f;
    float probeHeightWorld = 1.2f;
};

struct SectorBakedObjectLightProbeMetadata {
    std::string path;
    int version = 0;
    std::string sourceHash;
    int count = 0;
    float probeSpacingWorld = 4.0f;
    float probeHeightWorld = 1.2f;
    std::string format;
};

struct SectorBakedObjectLightProbeSectorRange {
    int sectorId = 0;
    int begin = 0;
    int count = 0;
};

struct SectorBakedObjectLightProbeRuntimeData {
    std::vector<SectorBakedObjectLightProbe> probes;
    std::vector<SectorBakedObjectLightProbeSectorRange> sectorRanges;
    SectorBakedObjectLightProbeMetadata metadata;
};

struct BakedObjectLightingSample {
    Vector3 ambientCube[6] = {};
    bool valid = false;
};

struct SectorLightmapMetadata {
    std::string path;
    int width = 0;
    int height = 0;
    std::string sourceHash;
    SectorBakedObjectLightProbeMetadata objectProbes;
};

} // namespace game
