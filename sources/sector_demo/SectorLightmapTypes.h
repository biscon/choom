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
    float objectProbeLowerHeightWorld = 0.6f;
    float objectProbeUpperHeightWorld = 1.5f;
};

enum class SectorBakedObjectLightProbeLayer : unsigned int {
    Lower = 0,
    Upper = 1
};

struct SectorBakedObjectLightProbe {
    int sectorId = 0;
    SectorBakedObjectLightProbeLayer layer =
            SectorBakedObjectLightProbeLayer::Lower;
    Vector3 position = {};
    Vector3 ambientCube[6] = {};
};

struct SectorBakedObjectLightProbePlacementSettings {
    float probeSpacingWorld = 4.0f;
    float lowerHeightWorld = 0.6f;
    float upperHeightWorld = 1.5f;
};

struct SectorBakedObjectLightProbeMetadata {
    std::string path;
    int version = 0;
    std::string sourceHash;
    int count = 0;
    float probeSpacingWorld = 4.0f;
    float probeLowerHeightWorld = 0.6f;
    float probeUpperHeightWorld = 1.5f;
    std::string format;
};

struct SectorBakedObjectLightProbeSectorRange {
    int sectorId = 0;
    int begin = 0;
    int count = 0;
    SectorBakedObjectLightProbeLayer layer =
            SectorBakedObjectLightProbeLayer::Lower;
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

struct BakedObjectLightingVerticalSample {
    BakedObjectLightingSample lower;
    BakedObjectLightingSample upper;
    float lowerHeightWorld = 0.0f;
    float upperHeightWorld = 0.0f;
};

struct SectorBakedStaticModelLightmapMetadata {
    std::string path;
    int version = 0;
    std::string sourceHash;
    int modelCount = 0;
    int objectCount = 0;
    std::string format;
};

struct SectorLightmapAtlasMetadata {
    std::string path;
    int width = 0;
    int height = 0;
};

struct SectorLightmapMetadata {
    std::string path;
    int width = 0;
    int height = 0;
    std::string sourceHash;
    std::vector<SectorLightmapAtlasMetadata> additionalAtlases;
    SectorBakedObjectLightProbeMetadata objectProbes;
    SectorBakedStaticModelLightmapMetadata staticModels;
};

} // namespace game
