#pragma once

#include "sector_demo/SectorLightmapTypes.h"
#include "sector_demo/SectorTopologyMap.h"

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

Vector3 InterpolateSectorLocalFogStaticLighting(
        const SectorLocalFogStaticLightingSamples& samples,
        Vector2 normalizedLocalXZ);

} // namespace game
