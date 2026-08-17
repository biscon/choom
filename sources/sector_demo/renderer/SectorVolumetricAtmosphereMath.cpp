#include "sector_demo/renderer/SectorVolumetricAtmosphereMath.h"

#include "engine/render/HdrEffectPolicy.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>

namespace game {

SectorVolumetricMediumSample CombineSectorVolumetricMediumSamples(
        const SectorVolumetricMediumSample& left,
        const SectorVolumetricMediumSample& right)
{
    return SectorVolumetricMediumSample{
            std::max(left.extinction, 0.0f) + std::max(right.extinction, 0.0f),
            Vector3Add(left.extinctionWeightedTint, right.extinctionWeightedTint),
            Vector3Add(
                    left.extinctionWeightedStaticRadiance,
                    right.extinctionWeightedStaticRadiance)};
}

float EvaluateSectorHenyeyGreensteinPhase(float cosineTheta, float anisotropy)
{
    const float g = std::clamp(
            std::isfinite(anisotropy) ? anisotropy : 0.0f,
            -0.90f,
            0.90f);
    const float cosine = std::clamp(
            std::isfinite(cosineTheta) ? cosineTheta : 0.0f,
            -1.0f,
            1.0f);
    const float denominator = std::max(
            1.0f + g * g - 2.0f * g * cosine,
            0.000001f);
    return (1.0f - g * g)
            / (4.0f * PI * denominator * std::sqrt(denominator));
}

void IntegrateSectorVolumetricStep(
        SectorVolumetricIntegrationState& state,
        float opticalDepth,
        Vector3 radiancePerUnitExtinction)
{
    if (!std::isfinite(opticalDepth) || opticalDepth <= 0.0f
            || !std::isfinite(state.transmittance)
            || state.transmittance <= 0.0f) {
        return;
    }
    const float stepTransmittance = std::exp(-opticalDepth);
    const float stepOpacity = 1.0f - stepTransmittance;
    const Vector3 contribution = Vector3Scale(
            engine::SanitizeLinearHdrForRgba16f(radiancePerUnitExtinction),
            state.transmittance * stepOpacity);
    state.premultipliedRadiance = engine::SanitizeLinearHdrForRgba16f(
            Vector3Add(state.premultipliedRadiance, contribution));
    state.transmittance = std::clamp(
            state.transmittance * stepTransmittance,
            0.0f,
            1.0f);
}

Vector4 FinishSectorVolumetricIntegration(
        const SectorVolumetricIntegrationState& state)
{
    const Vector3 radiance = engine::SanitizeLinearHdrForRgba16f(
            state.premultipliedRadiance);
    const float opacity = engine::SanitizeBoundedAlpha(
            1.0f - state.transmittance);
    return Vector4{radiance.x, radiance.y, radiance.z, opacity};
}

float ResolveSectorVolumetricSceneDistance(
        float depthSample,
        float nearPlane,
        float farPlane,
        float rayForwardDot,
        float maximumDistanceWorld)
{
    if (!std::isfinite(nearPlane) || !std::isfinite(farPlane)
            || nearPlane <= 0.0f || farPlane <= nearPlane
            || !std::isfinite(maximumDistanceWorld)
            || maximumDistanceWorld <= 0.0f) {
        return 0.0f;
    }
    const float maximumDistance = std::min(maximumDistanceWorld, farPlane);
    if (!std::isfinite(depthSample) || depthSample >= 0.999999f) {
        return maximumDistance;
    }
    const float depth = std::clamp(depthSample, 0.0f, 1.0f);
    const float zNdc = depth * 2.0f - 1.0f;
    const float denominator = farPlane + nearPlane
            - zNdc * (farPlane - nearPlane);
    if (!std::isfinite(denominator) || denominator <= 0.00001f) {
        return maximumDistance;
    }
    const float forwardDistance = 2.0f * nearPlane * farPlane / denominator;
    const float safeForwardDot = std::max(
            std::isfinite(rayForwardDot) ? rayForwardDot : 0.0f,
            0.0001f);
    return std::clamp(forwardDistance / safeForwardDot, 0.0f, maximumDistance);
}

float ComputeSectorFogOpticalDepth(
        const SectorTopologyFogSettings& unnormalizedSettings,
        Vector3 cameraPosition,
        Vector3 worldPosition,
        float pathDistanceWorld)
{
    const SectorTopologyFogSettings settings =
            NormalizeSectorTopologyFogSettings(unnormalizedSettings);
    if (!settings.enabled || settings.density <= 0.0f
            || settings.maxOpacity <= 0.0f
            || !std::isfinite(pathDistanceWorld)
            || pathDistanceWorld <= settings.startDistanceWorld) {
        return 0.0f;
    }
    const float fogDistance = pathDistanceWorld - settings.startDistanceWorld;
    const float midpointHeight = (cameraPosition.y + worldPosition.y) * 0.5f;
    const float heightAboveReference = std::max(
            midpointHeight - settings.referenceHeightWorld,
            0.0f);
    const float heightMultiplier = std::exp(
            -heightAboveReference * settings.heightFalloff);
    const float capOpticalDepth = -std::log(std::max(
            1.0f - std::clamp(settings.maxOpacity, 0.0f, 0.9999f),
            0.0001f));
    return std::min(settings.density * fogDistance * heightMultiplier,
            capOpticalDepth);
}

float ComputeSectorAnalyticFogTailOpacity(
        const SectorTopologyFogSettings& settings,
        Vector3 cameraPosition,
        Vector3 worldPosition,
        float volumetricMaximumDistanceWorld)
{
    const Vector3 offset = Vector3Subtract(worldPosition, cameraPosition);
    const float fullDistance = Vector3Length(offset);
    const float fullOpticalDepth = ComputeSectorFogOpticalDepth(
            settings, cameraPosition, worldPosition, fullDistance);
    if (fullOpticalDepth <= 0.0f) return 0.0f;
    const float prefixDistance = std::clamp(
            volumetricMaximumDistanceWorld, 0.0f, fullDistance);
    const Vector3 prefixPosition = fullDistance > 0.0001f
            ? Vector3Add(cameraPosition, Vector3Scale(offset, prefixDistance / fullDistance))
            : cameraPosition;
    const float prefixOpticalDepth = ComputeSectorFogOpticalDepth(
            settings, cameraPosition, prefixPosition, prefixDistance);
    return 1.0f - std::exp(-std::max(
            fullOpticalDepth - prefixOpticalDepth,
            0.0f));
}

} // namespace game
