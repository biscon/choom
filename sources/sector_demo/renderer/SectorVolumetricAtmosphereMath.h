#pragma once

#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/renderer/SectorVolumetricAtmosphereContracts.h"

#include <raylib.h>

#include <cstdint>

namespace game {

struct SectorVolumetricMediumSample {
    float extinction = 0.0f;
    Vector3 extinctionWeightedTint = {};
    Vector3 extinctionWeightedStaticRadiance = {};
};

struct SectorVolumetricIntegrationState {
    Vector3 premultipliedRadiance = {};
    float transmittance = 1.0f;
};

enum class SectorVolumetricHistoryResetReason {
    None,
    FirstFrame,
    RendererReset,
    BackendSwitch,
    ResourceUnavailable,
    InactiveFrame,
    TargetChanged,
    QualityChanged,
    AtlasLayoutChanged,
    FogSettingsChanged,
    SourceRevisionChanged,
    ProjectionChanged,
    CameraTranslation,
    CameraRotation,
    RenderGap,
    FreezeReleased,
    DebugViewChanged
};

const char* SectorVolumetricHistoryResetReasonName(
        SectorVolumetricHistoryResetReason reason);

struct SectorVolumetricHistoryFrameState {
    bool valid = false;
    int targetWidth = 0;
    int targetHeight = 0;
    SectorTopologyFogSettings::VolumetricQuality quality =
            SectorTopologyFogSettings::VolumetricQuality::Off;
    SectorVolumetricAtlasLayout atlas;
    Vector3 cameraPosition = {};
    Vector3 cameraForward = {0.0f, 0.0f, 1.0f};
    Vector3 cameraUp = {0.0f, 1.0f, 0.0f};
    float verticalFovDegrees = 0.0f;
    float aspectRatio = 1.0f;
    float nearPlane = 0.0f;
    float farPlane = 0.0f;
    int projection = CAMERA_PERSPECTIVE;
    float renderSeconds = 0.0f;
    std::uint64_t fogSignature = 0;
    std::uint64_t sourceRevision = 0;
};

SectorVolumetricHistoryResetReason EvaluateSectorVolumetricHistoryReset(
        const SectorVolumetricHistoryFrameState& previous,
        const SectorVolumetricHistoryFrameState& current);
Vector3 ComputeSectorVolumetricJitter(
        SectorTopologyFogSettings::VolumetricQuality quality,
        std::uint64_t frameIndex);
bool ReprojectSectorVolumetricHistoryUv(
        Vector2 currentUv,
        float currentDepth,
        Matrix inverseCurrentViewProjection,
        Matrix previousViewProjection,
        Vector2 currentJitterUv,
        Vector2 previousJitterUv,
        Vector2& outHistoryUv,
        float& outExpectedPreviousDepth);
bool AcceptSectorVolumetricHistoryDepth(
        float expectedPreviousDepthWorld,
        float sampledPreviousDepthWorld);
float ComputeSectorVolumetricBilateralDepthWeight(
        float centerDepthWorld,
        float sampleDepthWorld);
Vector4 ClampSectorVolumetricHistorySample(
        Vector4 history,
        Vector4 neighborhoodMinimum,
        Vector4 neighborhoodMaximum);

SectorVolumetricMediumSample CombineSectorVolumetricMediumSamples(
        const SectorVolumetricMediumSample& left,
        const SectorVolumetricMediumSample& right);
float EvaluateSectorHenyeyGreensteinPhase(float cosineTheta, float anisotropy);
void IntegrateSectorVolumetricStep(
        SectorVolumetricIntegrationState& state,
        float opticalDepth,
        Vector3 radiancePerUnitExtinction);
Vector4 FinishSectorVolumetricIntegration(
        const SectorVolumetricIntegrationState& state);
float ResolveSectorVolumetricSceneDistance(
        float depthSample,
        float nearPlane,
        float farPlane,
        float rayForwardDot,
        float maximumDistanceWorld);
float ComputeSectorFogOpticalDepth(
        const SectorTopologyFogSettings& settings,
        Vector3 cameraPosition,
        Vector3 worldPosition,
        float pathDistanceWorld);
float ComputeSectorAnalyticFogTailOpacity(
        const SectorTopologyFogSettings& settings,
        Vector3 cameraPosition,
        Vector3 worldPosition,
        float volumetricMaximumDistanceWorld);

} // namespace game
