#include "sector_demo/renderer/SectorLocalFogLighting.h"

#include "sector_demo/SectorLightmap.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>

namespace game {

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
    return SampleSectorLocalFogStaticLighting(map, probes, volume, 0.0f);
}

SectorLocalFogStaticLightingSamples SampleSectorLocalFogStaticLighting(
        const SectorTopologyMap& map,
        const SectorBakedObjectLightProbeRuntimeData& probes,
        const SectorCompiledLocalFogVolume& volume,
        float yawRadians)
{
    SectorLocalFogStaticLightingSamples samples;
    constexpr std::array<Vector2, 4> offsets = {
            Vector2{-1.0f, -1.0f},
            Vector2{1.0f, -1.0f},
            Vector2{-1.0f, 1.0f},
            Vector2{1.0f, 1.0f}};
    const float cosine = std::cos(yawRadians);
    const float sine = std::sin(yawRadians);
    for (std::size_t index = 0; index < offsets.size(); ++index) {
        const float localX = offsets[index].x * volume.radiiWorld.x
                * SectorLocalFogProbeFootprintFraction;
        const float localZ = offsets[index].y * volume.radiiWorld.z
                * SectorLocalFogProbeFootprintFraction;
        const Vector3 position{
                volume.centerWorld.x + cosine * localX + sine * localZ,
                volume.centerWorld.y,
                volume.centerWorld.z - sine * localX + cosine * localZ};
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
