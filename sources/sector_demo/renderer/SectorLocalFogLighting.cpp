#include "sector_demo/renderer/SectorLocalFogLighting.h"

#include "sector_demo/SectorLightmap.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>

namespace game {

float ComputeSectorLocalFogEffectivePathLength(
        float geometricPathLength,
        float volumeHeight,
        const SectorLocalFogPathLimitSettings& settings)
{
    if (!std::isfinite(geometricPathLength) || geometricPathLength <= 0.0f) {
        return 0.0f;
    }

    const float safeHeight = std::isfinite(volumeHeight) ? std::max(volumeHeight, 0.0f) : 0.0f;
    const float minimumPath = std::isfinite(settings.minimumPathWorld)
            ? std::max(settings.minimumPathWorld, 0.0001f)
            : 0.5f;
    const float heightMultiplier = std::isfinite(settings.heightMultiplier)
            ? std::max(settings.heightMultiplier, 0.0f)
            : 3.0f;
    const float saturationPower = std::isfinite(settings.saturationPower)
            ? std::max(settings.saturationPower, 1.0f)
            : 4.0f;
    const float pathLimit = std::max(minimumPath, safeHeight * heightMultiplier);
    const float ratio = geometricPathLength / pathLimit;
    const float denominator = std::pow(1.0f + std::pow(ratio, saturationPower), 1.0f / saturationPower);
    return std::min(geometricPathLength / std::max(denominator, 1.0f), pathLimit);
}

float ComputeSectorLocalFogEffectiveStepLength(
        float geometricPathLength,
        float volumeHeight,
        int marchSteps,
        const SectorLocalFogPathLimitSettings& settings)
{
    return ComputeSectorLocalFogEffectivePathLength(geometricPathLength, volumeHeight, settings)
            / static_cast<float>(std::max(marchSteps, 1));
}

Vector3 EvaluateSectorLocalFogProbeLighting(const BakedObjectLightingSample& sample)
{
    // Match the stable upper-hemisphere reduction used by billboards. The -Y
    // face is omitted because floor-hugging mist is primarily lit from above
    // and the sides, while sector ambient is already present on every face.
    return Vector3Scale(
            Vector3Add(
                    Vector3Add(
                            Vector3Add(sample.ambientCube[0], sample.ambientCube[1]),
                            sample.ambientCube[2]),
                    Vector3Add(sample.ambientCube[4], sample.ambientCube[5])),
            1.0f / 5.0f);
}

SectorLocalFogStaticLightingSamples SampleSectorLocalFogStaticLighting(
        const SectorTopologyMap& map,
        const SectorBakedObjectLightProbeRuntimeData& probes,
        const SectorCompiledLocalFogVolume& volume)
{
    SectorLocalFogStaticLightingSamples samples;
    constexpr std::array<Vector2, 4> offsets = {
            Vector2{-1.0f, -1.0f},
            Vector2{1.0f, -1.0f},
            Vector2{-1.0f, 1.0f},
            Vector2{1.0f, 1.0f}};
    for (std::size_t index = 0; index < offsets.size(); ++index) {
        const Vector3 position{
                volume.centerWorld.x
                        + offsets[index].x * volume.radiiWorld.x * SectorLocalFogProbeFootprintFraction,
                volume.centerWorld.y,
                volume.centerWorld.z
                        + offsets[index].y * volume.radiiWorld.z * SectorLocalFogProbeFootprintFraction};
        samples.corners[index] = EvaluateSectorLocalFogProbeLighting(
                SampleBakedObjectLighting(probes, position, volume.topologySectorId, &map));
    }
    return samples;
}

Vector3 InterpolateSectorLocalFogStaticLighting(
        const SectorLocalFogStaticLightingSamples& samples,
        Vector2 normalizedLocalXZ)
{
    const float denominator = std::max(2.0f * SectorLocalFogProbeFootprintFraction, 0.0001f);
    const float u = std::clamp(
            normalizedLocalXZ.x / denominator + 0.5f,
            0.0f,
            1.0f);
    const float v = std::clamp(
            normalizedLocalXZ.y / denominator + 0.5f,
            0.0f,
            1.0f);
    const Vector3 negativeZ = Vector3Lerp(samples.corners[0], samples.corners[1], u);
    const Vector3 positiveZ = Vector3Lerp(samples.corners[2], samples.corners[3], u);
    return Vector3Lerp(negativeZ, positiveZ, v);
}

} // namespace game
