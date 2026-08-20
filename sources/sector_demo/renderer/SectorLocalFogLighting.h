#pragma once

#include "sector_demo/SectorLightmapTypes.h"
#include "sector_demo/SectorTopologyMap.h"

#include <raylib.h>

#include <array>

namespace game {

struct SectorLocalFogStaticLightingSamples {
    // -X/-Z, +X/-Z, -X/+Z, +X/+Z.
    std::array<Vector3, 4> corners{};
};

constexpr float SectorLocalFogProbeFootprintFraction = 0.5f;

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

} // namespace game
