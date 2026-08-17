#pragma once

#include "engine/render/RenderTarget.h"
#include "sector_demo/SectorMeshTypes.h"
#include "sector_demo/SectorPortalVisibility.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/renderer/SectorDynamicLightingRenderer.h"
#include "sector_demo/renderer/SectorLightAtmosphere.h"
#include "sector_demo/renderer/SectorVolumetricAtmosphereClusters.h"
#include "sector_demo/renderer/SectorVolumetricAtmosphereMath.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace game {

class SectorVolumetricAtmosphereRenderer {
public:
    bool Initialize();
    bool Prepare(
            const engine::RenderTarget& sceneTarget,
            const SectorTopologyMap& map,
            SectorTopologyFogSettings::VolumetricQuality quality,
            const Camera3D& camera,
            float runtimeSeconds,
            const SectorBillboardDynamicLightContext& dynamicLights,
            const SectorPreviewDynamicPointLightSource* runtimePointLight,
            const std::vector<SectorLightAtmosphereSource>& lightAtmosphereSources,
            const RuntimePortalVisibilityResult& visibility,
            const std::vector<SectorReceiverBounds>& receiverBounds,
            bool dynamicLightingEnabled,
            std::uint64_t sourceRevision);
    bool Apply(RenderTexture2D& sceneTarget, RenderTexture2D& sceneScratch);
    void ResetPreparedFrame();
    void InvalidateHistory(SectorVolumetricHistoryResetReason reason);
    void Shutdown();

    bool Prepared() const { return prepared; }
    bool ResourcesReady() const { return resourcesReady; }
    bool HasActiveMedia() const { return activeMedia; }
    bool ReadyForAnalyticFogHandoff() const {
        return prepared && resourcesReady && activeMedia && globalFogActive;
    }
    int EligibleLocalVolumeCount() const {
        return clusterBuilder.Diagnostics().eligibleVolumeCount;
    }
    int ActiveLocalVolumeCount() const {
        return clusterBuilder.Diagnostics().retainedVolumeCount;
    }
    int EligibleLightCount() const {
        return clusterBuilder.Diagnostics().eligibleLightCount;
    }
    int ActiveLightCount() const {
        return clusterBuilder.Diagnostics().retainedLightCount;
    }
    const SectorVolumetricClusterBuildDiagnostics& ClusterDiagnostics() const {
        return clusterBuilder.Diagnostics();
    }
    int TargetWidth() const { return resourceLayout.integratedWidth; }
    int TargetHeight() const { return resourceLayout.integratedHeight; }
    int MarchSteps() const { return resourceLayout.grid.z; }
    const SectorVolumetricResourceLayout& ResourceLayout() const {
        return resourceLayout;
    }
    SectorTopologyFogSettings::VolumetricQuality RequestedQuality() const {
        return requestedQuality;
    }
    SectorTopologyFogSettings::VolumetricQuality EffectiveQuality() const {
        return resourceLayout.quality;
    }
    std::uint64_t EstimatedResourceBytes() const { return estimatedResourceBytes; }
    const engine::RenderTarget& AccumulationTarget() const { return integratedTarget; }
    const std::string& ResourceDiagnostic() const { return resourceDiagnostic; }
    bool HistoryEnabled() const { return temporalPolicy.enabled; }
    bool HistoryValid() const { return historyValid; }
    bool HistoryFrozen() const { return historyFrozen; }
    void SetHistoryFrozen(bool frozen);
    std::uint64_t HistoryFrameCount() const { return historyFrameCount; }
    SectorVolumetricHistoryResetReason HistoryResetReason() const {
        return historyResetReason;
    }
    SectorVolumetricDebugView DebugView() const { return debugView; }
    void SetDebugView(SectorVolumetricDebugView view);
    int ShadowedSpotLightCount() const { return shadowedSpotLightCount; }
    const SectorVolumetricTemporalPolicy& TemporalPolicy() const {
        return temporalPolicy;
    }

private:
    struct CommonLocations {
        int cameraPosition = -1;
        int cameraForward = -1;
        int cameraRight = -1;
        int cameraUp = -1;
        int tanHalfFov = -1;
        int aspectRatio = -1;
        int gridSize = -1;
        int clusterGridSize = -1;
        int tileColumns = -1;
        int sliceCount = -1;
        int clusterBandCount = -1;
        int sliceDepths = -1;
        int runtimeSeconds = -1;
        int jitter = -1;
    };

    struct MediumLocations : CommonLocations {
        int volumeData = -1;
        int volumeLists = -1;
        int fogEnabled = -1;
        int fogColor = -1;
        int fogStartDistance = -1;
        int fogDensity = -1;
        int fogMaximumOpacity = -1;
        int fogReferenceHeight = -1;
        int fogHeightFalloff = -1;
    };

    struct LightLocations : CommonLocations {
        int mediumAtlas = -1;
        int lightData = -1;
        int lightLists = -1;
        int anisotropy = -1;
        std::array<int, MaxDynamicSpotLightShadowCasters> shadowLightMatrices = [] {
            std::array<int, MaxDynamicSpotLightShadowCasters> locations{};
            locations.fill(-1);
            return locations;
        }();
        int shadowBias = -1;
        int shadowStrength = -1;
        int shadowSoftness = -1;
        int shadowMap0 = -1;
        int shadowMap1 = -1;
    };

    struct IntegrationLocations : CommonLocations {
        int lightingAtlas = -1;
        int sceneDepth = -1;
        int nearPlane = -1;
        int farPlane = -1;
        int fogEnabled = -1;
        int fogStartDistance = -1;
        int fogDensity = -1;
        int fogMaximumOpacity = -1;
        int fogReferenceHeight = -1;
        int fogHeightFalloff = -1;
    };

    struct TemporalLocations {
        int currentAtmosphere = -1;
        int currentDepth = -1;
        int historyAtmosphere = -1;
        int historyDepth = -1;
        int inverseCurrentViewProjection = -1;
        int previousViewProjection = -1;
        int nearPlane = -1;
        int farPlane = -1;
        int historyValid = -1;
        int baseCurrentWeight = -1;
        int responsiveCurrentWeight = -1;
        int outputHistoryWeight = -1;
    };

    struct CompositeLocations {
        int sceneColor = -1;
        int sceneDepth = -1;
        int atmosphereTexture = -1;
        int atmosphereTexelSize = -1;
        int debugView = -1;
    };

    bool EnsureResources(const SectorVolumetricResourceLayout& layout);
    bool AllocateDataTexture(
            int width,
            int height,
            int pixelFormat,
            Texture2D& texture,
            const char* debugName);
    bool BuildAndUploadDataTextures();
    void ReleaseResources();
    void CaptureCommonLocations(Shader shader, CommonLocations& locations);
    void UploadCommon(Shader shader, const CommonLocations& locations) const;

    Shader mediumShader = {};
    Shader lightShader = {};
    Shader integrationShader = {};
    Shader temporalShader = {};
    Shader compositeShader = {};
    MediumLocations mediumLocations;
    LightLocations lightLocations;
    IntegrationLocations integrationLocations;
    TemporalLocations temporalLocations;
    CompositeLocations compositeLocations;
    engine::RenderTarget mediumAtlas;
    engine::RenderTarget lightingAtlas;
    engine::RenderTarget integratedTarget;
    std::array<engine::RenderTarget, 2> historyTargets;
    Texture2D lightDataTexture = {};
    Texture2D volumeDataTexture = {};
    Texture2D lightListTexture = {};
    Texture2D volumeListTexture = {};
    std::array<float, SectorVolumetricMaximumViewLights
            * SectorVolumetricLightRecordTexels * 4> lightDataStaging{};
    std::array<float, SectorVolumetricMaximumViewVolumes
            * SectorVolumetricVolumeRecordTexels * 4> volumeDataStaging{};
    SectorVolumetricClusterBuilder clusterBuilder;
    SectorVolumetricResourceLayout resourceLayout;
    SectorVolumetricDepthSliceLayout depthLayout;
    SectorTopologyFogSettings fogSettings;
    Camera3D preparedCamera = {};
    SectorBillboardDynamicLightContext preparedDynamicLights;
    float preparedRuntimeSeconds = 0.0f;
    float nearPlane = 0.0f;
    float farPlane = 0.0f;
    float aspectRatio = 1.0f;
    Matrix currentViewProjection = {};
    Matrix inverseCurrentViewProjection = {};
    Matrix historyViewProjection = {};
    Vector3 currentJitter = {};
    SectorVolumetricTemporalPolicy temporalPolicy;
    SectorVolumetricHistoryFrameState currentFrameState;
    SectorVolumetricHistoryFrameState previousFrameState;
    int maximumTextureSize = 0;
    int failedSceneWidth = 0;
    int failedSceneHeight = 0;
    SectorTopologyFogSettings::VolumetricQuality failedQuality =
            SectorTopologyFogSettings::VolumetricQuality::Off;
    SectorTopologyFogSettings::VolumetricQuality requestedQuality =
            SectorTopologyFogSettings::VolumetricQuality::Off;
    std::uint64_t estimatedResourceBytes = 0;
    bool shaderFailed = false;
    bool warnedUnavailable = false;
    bool prepared = false;
    bool resourcesReady = false;
    bool activeMedia = false;
    bool globalFogActive = false;
    bool historyValid = false;
    bool historyFrozen = false;
    int historyReadIndex = 0;
    std::uint64_t temporalFrameIndex = 0;
    std::uint64_t historyFrameCount = 0;
    SectorVolumetricHistoryResetReason historyResetReason =
            SectorVolumetricHistoryResetReason::FirstFrame;
    SectorVolumetricDebugView debugView = SectorVolumetricDebugView::Composite;
    int shadowedSpotLightCount = 0;
    std::string resourceDiagnostic = "not initialized";
};

} // namespace game
