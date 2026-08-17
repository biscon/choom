#include "sector_demo/renderer/SectorVolumetricAtmosphereMath.h"

#include "engine/render/HdrEffectPolicy.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>

namespace game {
namespace {

bool NearFloat(float left, float right, float epsilon = 0.00001f)
{
    return std::isfinite(left) && std::isfinite(right)
            && std::fabs(left - right) <= epsilon;
}

bool SameAtlas(
        const SectorVolumetricAtlasLayout& left,
        const SectorVolumetricAtlasLayout& right)
{
    return left.grid.x == right.grid.x && left.grid.y == right.grid.y
            && left.grid.z == right.grid.z
            && left.tileColumns == right.tileColumns
            && left.tileRows == right.tileRows
            && left.width == right.width && left.height == right.height;
}

float Halton(std::uint64_t index, int base)
{
    float result = 0.0f;
    float fraction = 1.0f / static_cast<float>(base);
    while (index > 0) {
        result += fraction * static_cast<float>(index % static_cast<std::uint64_t>(base));
        index /= static_cast<std::uint64_t>(base);
        fraction /= static_cast<float>(base);
    }
    return result;
}

Vector4 TransformHomogeneous(Matrix matrix, Vector4 value)
{
    return Vector4{
            matrix.m0 * value.x + matrix.m4 * value.y
                    + matrix.m8 * value.z + matrix.m12 * value.w,
            matrix.m1 * value.x + matrix.m5 * value.y
                    + matrix.m9 * value.z + matrix.m13 * value.w,
            matrix.m2 * value.x + matrix.m6 * value.y
                    + matrix.m10 * value.z + matrix.m14 * value.w,
            matrix.m3 * value.x + matrix.m7 * value.y
                    + matrix.m11 * value.z + matrix.m15 * value.w};
}

float SanitizeHistoryChannel(float value, float maximum)
{
    if (!std::isfinite(value)) return 0.0f;
    return std::clamp(value, 0.0f, maximum);
}

} // namespace

const char* SectorVolumetricHistoryResetReasonName(
        SectorVolumetricHistoryResetReason reason)
{
    switch (reason) {
        case SectorVolumetricHistoryResetReason::None: return "none";
        case SectorVolumetricHistoryResetReason::FirstFrame: return "first frame";
        case SectorVolumetricHistoryResetReason::RendererReset: return "renderer reset";
        case SectorVolumetricHistoryResetReason::BackendSwitch: return "backend switch";
        case SectorVolumetricHistoryResetReason::ResourceUnavailable: return "resource unavailable";
        case SectorVolumetricHistoryResetReason::InactiveFrame: return "inactive frame";
        case SectorVolumetricHistoryResetReason::TargetChanged: return "target changed";
        case SectorVolumetricHistoryResetReason::QualityChanged: return "quality changed";
        case SectorVolumetricHistoryResetReason::AtlasLayoutChanged: return "atlas layout changed";
        case SectorVolumetricHistoryResetReason::FogSettingsChanged: return "fog settings changed";
        case SectorVolumetricHistoryResetReason::SourceRevisionChanged: return "source revision changed";
        case SectorVolumetricHistoryResetReason::ProjectionChanged: return "projection changed";
        case SectorVolumetricHistoryResetReason::CameraTranslation: return "camera translation";
        case SectorVolumetricHistoryResetReason::CameraRotation: return "camera rotation";
        case SectorVolumetricHistoryResetReason::RenderGap: return "render gap";
        case SectorVolumetricHistoryResetReason::FreezeReleased: return "history unfrozen";
        case SectorVolumetricHistoryResetReason::DebugViewChanged: return "debug view changed";
    }
    return "unknown";
}

SectorVolumetricHistoryResetReason EvaluateSectorVolumetricHistoryReset(
        const SectorVolumetricHistoryFrameState& previous,
        const SectorVolumetricHistoryFrameState& current)
{
    if (!previous.valid) return SectorVolumetricHistoryResetReason::FirstFrame;
    if (previous.targetWidth != current.targetWidth
            || previous.targetHeight != current.targetHeight) {
        return SectorVolumetricHistoryResetReason::TargetChanged;
    }
    if (previous.quality != current.quality) {
        return SectorVolumetricHistoryResetReason::QualityChanged;
    }
    if (!SameAtlas(previous.atlas, current.atlas)) {
        return SectorVolumetricHistoryResetReason::AtlasLayoutChanged;
    }
    if (previous.fogSignature != current.fogSignature) {
        return SectorVolumetricHistoryResetReason::FogSettingsChanged;
    }
    if (previous.sourceRevision != current.sourceRevision) {
        return SectorVolumetricHistoryResetReason::SourceRevisionChanged;
    }
    if (previous.projection != current.projection
            || !NearFloat(previous.verticalFovDegrees, current.verticalFovDegrees)
            || !NearFloat(previous.aspectRatio, current.aspectRatio)
            || !NearFloat(previous.nearPlane, current.nearPlane)
            || !NearFloat(previous.farPlane, current.farPlane)) {
        return SectorVolumetricHistoryResetReason::ProjectionChanged;
    }
    if (Vector3Distance(previous.cameraPosition, current.cameraPosition) > 2.0f) {
        return SectorVolumetricHistoryResetReason::CameraTranslation;
    }
    const Vector3 previousForward = Vector3Normalize(previous.cameraForward);
    const Vector3 currentForward = Vector3Normalize(current.cameraForward);
    const Vector3 previousUp = Vector3Normalize(previous.cameraUp);
    const Vector3 currentUp = Vector3Normalize(current.cameraUp);
    const float minimumCosine = std::cos(30.0f * DEG2RAD);
    if (Vector3DotProduct(previousForward, currentForward) < minimumCosine
            || Vector3DotProduct(previousUp, currentUp) < minimumCosine) {
        return SectorVolumetricHistoryResetReason::CameraRotation;
    }
    if (!std::isfinite(previous.renderSeconds)
            || !std::isfinite(current.renderSeconds)
            || current.renderSeconds < previous.renderSeconds
            || current.renderSeconds - previous.renderSeconds > 0.25f) {
        return SectorVolumetricHistoryResetReason::RenderGap;
    }
    return SectorVolumetricHistoryResetReason::None;
}

Vector3 ComputeSectorVolumetricJitter(
        SectorTopologyFogSettings::VolumetricQuality quality,
        std::uint64_t frameIndex)
{
    const SectorVolumetricTemporalPolicy policy =
            GetSectorVolumetricTemporalPolicy(quality);
    if (!policy.enabled || policy.jitterPeriod <= 1) {
        return Vector3{0.0f, 0.0f, 0.5f};
    }
    const std::uint64_t sample = frameIndex
            % static_cast<std::uint64_t>(policy.jitterPeriod) + 1u;
    return Vector3{
            Halton(sample, 2) - 0.5f,
            Halton(sample, 3) - 0.5f,
            Halton(sample, 5)};
}

bool ReprojectSectorVolumetricHistoryUv(
        Vector2 currentUv,
        float currentDepth,
        Matrix inverseCurrentViewProjection,
        Matrix previousViewProjection,
        Vector2& outHistoryUv,
        float& outExpectedPreviousDepth)
{
    outHistoryUv = {};
    outExpectedPreviousDepth = 1.0f;
    if (!std::isfinite(currentUv.x) || !std::isfinite(currentUv.y)
            || !std::isfinite(currentDepth)) {
        return false;
    }
    const Vector4 clip{
            currentUv.x * 2.0f - 1.0f,
            currentUv.y * 2.0f - 1.0f,
            std::clamp(currentDepth, 0.0f, 1.0f) * 2.0f - 1.0f,
            1.0f};
    const Vector4 world = TransformHomogeneous(inverseCurrentViewProjection, clip);
    if (!std::isfinite(world.w) || std::fabs(world.w) <= 0.000001f) return false;
    const Vector4 previousClip = TransformHomogeneous(
            previousViewProjection,
            Vector4{world.x / world.w, world.y / world.w,
                    world.z / world.w, 1.0f});
    if (!std::isfinite(previousClip.w) || previousClip.w <= 0.000001f) return false;
    const Vector3 previousNdc{
            previousClip.x / previousClip.w,
            previousClip.y / previousClip.w,
            previousClip.z / previousClip.w};
    if (!std::isfinite(previousNdc.x) || !std::isfinite(previousNdc.y)
            || !std::isfinite(previousNdc.z)) {
        return false;
    }
    outHistoryUv = Vector2{
            previousNdc.x * 0.5f + 0.5f,
            previousNdc.y * 0.5f + 0.5f};
    outExpectedPreviousDepth = previousNdc.z * 0.5f + 0.5f;
    return outHistoryUv.x >= 0.0f && outHistoryUv.x <= 1.0f
            && outHistoryUv.y >= 0.0f && outHistoryUv.y <= 1.0f
            && outExpectedPreviousDepth >= 0.0f
            && outExpectedPreviousDepth <= 1.0f;
}

bool AcceptSectorVolumetricHistoryDepth(
        float expectedPreviousDepthWorld,
        float sampledPreviousDepthWorld)
{
    if (!std::isfinite(expectedPreviousDepthWorld)
            || !std::isfinite(sampledPreviousDepthWorld)
            || expectedPreviousDepthWorld <= 0.0f
            || sampledPreviousDepthWorld <= 0.0f) {
        return false;
    }
    const float tolerance = std::max(
            0.10f, expectedPreviousDepthWorld * 0.02f);
    return std::fabs(expectedPreviousDepthWorld - sampledPreviousDepthWorld)
            <= tolerance;
}

float ComputeSectorVolumetricBilateralDepthWeight(
        float centerDepthWorld,
        float sampleDepthWorld)
{
    if (!std::isfinite(centerDepthWorld) || !std::isfinite(sampleDepthWorld)
            || centerDepthWorld < 0.0f || sampleDepthWorld < 0.0f) {
        return 0.0f;
    }
    const float scale = std::max(0.05f, centerDepthWorld * 0.02f);
    return std::exp(-std::fabs(sampleDepthWorld - centerDepthWorld) / scale);
}

Vector4 ClampSectorVolumetricHistorySample(
        Vector4 history,
        Vector4 neighborhoodMinimum,
        Vector4 neighborhoodMaximum)
{
    history = Vector4{
            SanitizeHistoryChannel(history.x, 65504.0f),
            SanitizeHistoryChannel(history.y, 65504.0f),
            SanitizeHistoryChannel(history.z, 65504.0f),
            SanitizeHistoryChannel(history.w, 1.0f)};
    const Vector4 minimum{
            SanitizeHistoryChannel(neighborhoodMinimum.x, 65504.0f),
            SanitizeHistoryChannel(neighborhoodMinimum.y, 65504.0f),
            SanitizeHistoryChannel(neighborhoodMinimum.z, 65504.0f),
            SanitizeHistoryChannel(neighborhoodMinimum.w, 1.0f)};
    const Vector4 maximum{
            std::max(minimum.x, SanitizeHistoryChannel(neighborhoodMaximum.x, 65504.0f)),
            std::max(minimum.y, SanitizeHistoryChannel(neighborhoodMaximum.y, 65504.0f)),
            std::max(minimum.z, SanitizeHistoryChannel(neighborhoodMaximum.z, 65504.0f)),
            std::max(minimum.w, SanitizeHistoryChannel(neighborhoodMaximum.w, 1.0f))};
    return Vector4{
            std::clamp(history.x, minimum.x, maximum.x),
            std::clamp(history.y, minimum.y, maximum.y),
            std::clamp(history.z, minimum.z, maximum.z),
            std::clamp(history.w, minimum.w, maximum.w)};
}

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
