#pragma once

#include "sector_demo/SectorLightmapTypes.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/renderer/SectorLightAtmosphere.h"

#include <raylib.h>

#include <array>

namespace game {

struct SectorLocalFogPathLimitSettings {
    float minimumPathWorld = 0.5f;
    float heightMultiplier = 3.0f;
    float saturationPower = 4.0f;
};

struct SectorLocalFogStaticLightingSamples {
    // -X/-Z, +X/-Z, -X/+Z, +X/+Z.
    std::array<Vector3, 4> corners{};
};

struct SectorLightHazeStaticLightingSamples {
    // x changes fastest, then local cross-section y, then axial/depth.
    std::array<Vector3, 8> corners{};
};

constexpr float SectorLocalFogProbeFootprintFraction = 0.5f;

float ComputeSectorLocalFogEffectivePathLength(
        float geometricPathLength,
        float volumeHeight,
        const SectorLocalFogPathLimitSettings& settings = {});

float ComputeSectorLocalFogEffectiveStepLength(
        float geometricPathLength,
        float volumeHeight,
        int marchSteps,
        const SectorLocalFogPathLimitSettings& settings = {});

Vector3 EvaluateSectorLocalFogProbeLighting(const BakedObjectLightingSample& sample);

SectorLocalFogStaticLightingSamples SampleSectorLocalFogStaticLighting(
        const SectorTopologyMap& map,
        const SectorBakedObjectLightProbeRuntimeData& probes,
        const SectorCompiledLocalFogVolume& volume);

SectorLocalFogStaticLightingSamples SampleSectorLocalFogStaticLighting(
        const SectorTopologyMap& map,
        const SectorBakedObjectLightProbeRuntimeData& probes,
        const SectorCompiledLocalFogVolume& volume,
        float yawRadians);

Vector3 InterpolateSectorLocalFogStaticLighting(
        const SectorLocalFogStaticLightingSamples& samples,
        Vector2 normalizedLocalXZ);

SectorLightHazeStaticLightingSamples SampleSectorLightHazeStaticLighting(
        const SectorTopologyMap& map,
        const SectorBakedObjectLightProbeRuntimeData& probes,
        const SectorLightAtmosphereVolume& volume);

Vector3 InterpolateSectorLightHazeStaticLighting(
        const SectorLightHazeStaticLightingSamples& samples,
        Vector3 normalizedGridPosition);

} // namespace game
