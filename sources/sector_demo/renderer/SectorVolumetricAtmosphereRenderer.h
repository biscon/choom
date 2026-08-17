#pragma once

#include "engine/render/RenderTarget.h"
#include "sector_demo/SectorLightmapTypes.h"
#include "sector_demo/SectorMeshTypes.h"
#include "sector_demo/SectorPortalVisibility.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/renderer/LegacyHazeComparisonAdapter.h"
#include "sector_demo/renderer/SectorDynamicLightingRenderer.h"
#include "sector_demo/renderer/SectorLightAtmosphere.h"

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace game {

class SectorVolumetricAtmosphereRenderer {
public:
    static constexpr std::size_t MaximumLocalVolumes = 16;

    bool Initialize();
    bool Prepare(
            const engine::RenderTarget& sceneTarget,
            const SectorTopologyMap& map,
            SectorTopologyFogSettings::LocalVolumeQuality quality,
            const Camera3D& camera,
            float runtimeSeconds,
            const SectorBakedObjectLightProbeRuntimeData& objectLightProbes,
            const SectorBillboardDynamicLightContext& dynamicLights,
            const std::vector<SectorLightAtmosphereSource>& lightAtmosphereSources,
            const RuntimePortalVisibilityResult& visibility,
            const std::vector<SectorReceiverBounds>& receiverBounds);
    bool Apply(RenderTexture2D& sceneTarget, RenderTexture2D& sceneScratch);
    void ResetPreparedFrame();
    void Shutdown();

    bool Prepared() const { return prepared; }
    bool ResourcesReady() const { return resourcesReady; }
    bool HasActiveMedia() const { return activeMedia; }
    bool ReadyForAnalyticFogHandoff() const {
        return prepared && resourcesReady && activeMedia && globalFogActive;
    }
    int EligibleLocalVolumeCount() const { return eligibleLocalVolumeCount; }
    int ActiveLocalVolumeCount() const { return activeLocalVolumeCount; }
    int EligibleHazeVolumeCount() const { return hazeAdapter.EligibleCount(); }
    int ActiveHazeVolumeCount() const { return hazeAdapter.ActiveCount(); }
    int ActiveDynamicLightCount() const { return dynamicLightContext.dynamicLightCount; }
    int TargetWidth() const { return target.native.texture.width; }
    int TargetHeight() const { return target.native.texture.height; }
    int MarchSteps() const { return marchSteps; }
    const engine::RenderTarget& AccumulationTarget() const { return target; }
    const std::string& ResourceDiagnostic() const { return resourceDiagnostic; }

private:
    struct ShaderLocations {
        int sceneDepth = -1;
        int marchSteps = -1;
        int cameraPosition = -1;
        int cameraForward = -1;
        int cameraRight = -1;
        int cameraUp = -1;
        int tanHalfFov = -1;
        int aspectRatio = -1;
        int nearPlane = -1;
        int farPlane = -1;
        int maximumDistance = -1;
        int anisotropy = -1;
        int runtimeSeconds = -1;
        int fogEnabled = -1;
        int fogColor = -1;
        int fogStartDistance = -1;
        int fogDensity = -1;
        int fogMaximumOpacity = -1;
        int fogReferenceHeight = -1;
        int fogHeightFalloff = -1;
        int localVolumeCount = -1;
        int localCenters = -1;
        int localRadii = -1;
        int localColors = -1;
        int localParamsA = -1;
        int localParamsB = -1;
        int localLighting = -1;
        int hazeVolumeCount = -1;
        int hazeCenters = -1;
        int hazeDirections = -1;
        int hazeShapes = -1;
        int hazeExtents = -1;
        int hazeConeRadii = -1;
        int hazeColors = -1;
        int hazeParamsA = -1;
        int hazeParamsB = -1;
        int hazeLighting = -1;
        SectorDynamicLightShaderLocations dynamicLights;
        SectorDynamicSpotLightShadowShaderLocations dynamicShadows;
        int shadowMap0 = -1;
        int shadowMap1 = -1;
    };

    struct CompositeLocations {
        int sceneColor = -1;
        int sceneDepth = -1;
        int atmosphereTexture = -1;
        int atmosphereTexelSize = -1;
    };

    struct LocalLightingCacheEntry {
        bool valid = false;
        int volumeId = -1;
        int topologySectorId = -1;
        Vector3 center = {};
        Vector3 radii = {};
        Color ambientColor = {};
        float ambientIntensity = 0.0f;
        SectorLocalFogStaticLightingSamples lighting;
    };

    bool EnsureTargets(int width, int height);
    void ReleaseTargets();
    void RefreshLocalLightingCacheIdentity(
            const SectorTopologyMap& map,
            const SectorBakedObjectLightProbeRuntimeData& probes);
    const SectorLocalFogStaticLightingSamples& LocalLightingFor(
            const SectorTopologyMap& map,
            const SectorBakedObjectLightProbeRuntimeData& probes,
            const SectorCompiledLocalFogVolume& volume);
    void BuildLocalVolumes(
            const SectorTopologyMap& map,
            const SectorBakedObjectLightProbeRuntimeData& probes,
            SectorTopologyFogSettings::LocalVolumeQuality quality,
            const Camera3D& camera);

    Shader shader = {};
    Shader compositeShader = {};
    ShaderLocations locations;
    CompositeLocations compositeLocations;
    engine::RenderTarget target;
    std::array<SectorVolumetricComparisonVolume, MaximumLocalVolumes> localVolumes{};
    std::array<SectorVolumetricComparisonVolume,
            LegacyHazeComparisonAdapter::MaximumVolumes> hazeVolumes{};
    std::array<LocalLightingCacheEntry, MaximumLocalVolumes> localLightingCache{};
    LegacyHazeComparisonAdapter hazeAdapter;
    const SectorBakedObjectLightProbe* cachedProbeData = nullptr;
    std::size_t cachedProbeCount = 0;
    std::size_t cachedProbeHash = 0;
    std::size_t cachedMapProbeHash = 0;
    SectorTopologyFogSettings fogSettings;
    SectorBillboardDynamicLightContext dynamicLightContext;
    Camera3D preparedCamera = {};
    float preparedRuntimeSeconds = 0.0f;
    float nearPlane = 0.0f;
    float farPlane = 0.0f;
    float aspectRatio = 1.0f;
    int marchSteps = 0;
    int eligibleLocalVolumeCount = 0;
    int activeLocalVolumeCount = 0;
    int failedWidth = 0;
    int failedHeight = 0;
    bool shaderFailed = false;
    bool warnedUnavailable = false;
    bool prepared = false;
    bool resourcesReady = false;
    bool activeMedia = false;
    bool globalFogActive = false;
    std::string resourceDiagnostic = "not initialized";
};

} // namespace game
