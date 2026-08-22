#pragma once

#include "sector_demo/SectorUnits.h"

#include <raylib.h>

#include <cstdint>
#include <string>
#include <vector>

namespace game {

enum class SectorLightmapBakeQualityPreset : unsigned int {
    Draft = 0,
    Standard = 1,
    High = 2
};

inline SectorLightmapBakeQualityPreset NormalizeSectorLightmapBakeQualityPreset(
        SectorLightmapBakeQualityPreset preset)
{
    switch (preset) {
        case SectorLightmapBakeQualityPreset::Draft:
        case SectorLightmapBakeQualityPreset::Standard:
        case SectorLightmapBakeQualityPreset::High:
            return preset;
    }
    return SectorLightmapBakeQualityPreset::Standard;
}

struct SectorLightmapBakeQualityParameters {
    float texelsPerWorldUnit = 8.0f;
    int directSoftShadowSampleCount = 8;
    int ambientOcclusionSampleCount = 12;
    int indirectBounceSampleCount = 8;
};

struct SectorIlluminationStatistics {
    Vector3 rgbMin = {};
    Vector3 rgbMax = {};
    float auxiliaryMin = 0.0f;
    float auxiliaryMax = 0.0f;
    uint64_t sampleCount = 0;
    uint64_t rgbChannelsAboveOne = 0;
};

struct SectorLightmapBakeSettings {
    SectorLightmapBakeQualityPreset qualityPreset =
            SectorLightmapBakeQualityPreset::Standard;
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
    SectorIlluminationStatistics storedStatistics;
};

struct SectorBakedObjectLightProbeSectorRange {
    int sectorId = 0;
    int begin = 0;
    int count = 0;
    SectorBakedObjectLightProbeLayer layer =
            SectorBakedObjectLightProbeLayer::Lower;
};

struct SectorBakedObjectLightProbePortal {
    int adjacentSectorId = 0;
    Vector2 startWorld = {};
    Vector2 endWorld = {};
};

struct SectorBakedObjectLightProbePortalRange {
    int sectorId = 0;
    int begin = 0;
    int count = 0;
};

struct SectorBakedObjectLightProbeRuntimeData {
    std::vector<SectorBakedObjectLightProbe> probes;
    std::vector<SectorBakedObjectLightProbeSectorRange> sectorRanges;
    std::vector<SectorBakedObjectLightProbePortal> portals;
    std::vector<SectorBakedObjectLightProbePortalRange> portalRanges;
    SectorBakedObjectLightProbeMetadata metadata;
    bool portalAdjacencyPrepared = false;
};

struct SectorLightmapArtifactData {
    int width = 0;
    int height = 0;
    std::string sourceHash;
    // Host-order IEEE 754 binary16 bit patterns, interleaved RGBA.
    std::vector<uint16_t> rgba16;
    // Linear UNORM dominant-light direction RGB and direct-light fraction A.
    std::vector<unsigned char> directionalRgba8;
    SectorIlluminationStatistics storedStatistics;
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
    SectorIlluminationStatistics storedStatistics;
};

struct SectorLightmapMetadata {
    std::string path;
    int width = 0;
    int height = 0;
    int version = 0;
    std::string format;
    std::string sourceHash;
    SectorIlluminationStatistics storedStatistics;
    std::vector<SectorLightmapAtlasMetadata> additionalAtlases;
    SectorBakedObjectLightProbeMetadata objectProbes;
    SectorBakedStaticModelLightmapMetadata staticModels;
};

} // namespace game
