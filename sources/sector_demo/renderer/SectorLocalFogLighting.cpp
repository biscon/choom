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

SectorLightHazeStaticLightingSamples SampleSectorLightHazeStaticLighting(
        const SectorTopologyMap& map,
        const SectorBakedObjectLightProbeRuntimeData& probes,
        const SectorLightAtmosphereVolume& volume)
{
    SectorLightHazeStaticLightingSamples samples;
    if (volume.source == nullptr) return samples;

    Vector3 right = Vector3CrossProduct(volume.directionWorld, Vector3{0.0f, 1.0f, 0.0f});
    if (Vector3LengthSqr(right) <= 0.000001f) {
        right = Vector3CrossProduct(volume.directionWorld, Vector3{0.0f, 0.0f, 1.0f});
    }
    right = Vector3Normalize(right);
    const Vector3 up = Vector3Normalize(Vector3CrossProduct(right, volume.directionWorld));

    for (int depthIndex = 0; depthIndex < 2; ++depthIndex) {
        for (int yIndex = 0; yIndex < 2; ++yIndex) {
            for (int xIndex = 0; xIndex < 2; ++xIndex) {
                const float xSign = xIndex == 0 ? -0.5f : 0.5f;
                const float ySign = yIndex == 0 ? -0.5f : 0.5f;
                Vector3 position{};
                if (volume.source->shape == SectorLightAtmosphereShape::Sphere) {
                    position = Vector3Add(
                            volume.originWorld,
                            Vector3Add(
                                    Vector3Scale(right, xSign * volume.extentWorld),
                                    Vector3Add(
                                            Vector3Scale(up, ySign * volume.extentWorld),
                                            Vector3Scale(
                                                    volume.directionWorld,
                                                    (depthIndex == 0 ? -0.5f : 0.5f)
                                                            * volume.extentWorld))));
                } else {
                    const float axialFraction = depthIndex == 0 ? 0.25f : 0.75f;
                    const float axial = volume.extentWorld * axialFraction;
                    const float crossRadius = volume.coneRadiusWorld * axialFraction;
                    position = Vector3Add(
                            volume.originWorld,
                            Vector3Add(
                                    Vector3Scale(volume.directionWorld, axial),
                                    Vector3Add(
                                            Vector3Scale(right, xSign * crossRadius),
                                            Vector3Scale(up, ySign * crossRadius))));
                }
                const std::size_t index = static_cast<std::size_t>(
                        xIndex + yIndex * 2 + depthIndex * 4);
                samples.corners[index] = EvaluateSectorLocalFogProbeLighting(
                        SampleBakedObjectLighting(
                                probes,
                                position,
                                volume.source->ownerSectorId,
                                &map));
            }
        }
    }
    return samples;
}

Vector3 InterpolateSectorLightHazeStaticLighting(
        const SectorLightHazeStaticLightingSamples& samples,
        Vector3 normalizedGridPosition)
{
    const float x = std::clamp(normalizedGridPosition.x, 0.0f, 1.0f);
    const float y = std::clamp(normalizedGridPosition.y, 0.0f, 1.0f);
    const float z = std::clamp(normalizedGridPosition.z, 0.0f, 1.0f);
    const Vector3 z0y0 = Vector3Lerp(samples.corners[0], samples.corners[1], x);
    const Vector3 z0y1 = Vector3Lerp(samples.corners[2], samples.corners[3], x);
    const Vector3 z1y0 = Vector3Lerp(samples.corners[4], samples.corners[5], x);
    const Vector3 z1y1 = Vector3Lerp(samples.corners[6], samples.corners[7], x);
    return Vector3Lerp(Vector3Lerp(z0y0, z0y1, y), Vector3Lerp(z1y0, z1y1, y), z);
}

} // namespace game
