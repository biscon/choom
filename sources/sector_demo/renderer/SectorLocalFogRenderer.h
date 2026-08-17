#pragma once

#include "engine/render/RenderTarget.h"
#include "sector_demo/SectorLightmapTypes.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/renderer/SectorDynamicLightingRenderer.h"
#include "sector_demo/renderer/SectorLocalFogLighting.h"

#include <raylib.h>

#include <array>
#include <cstddef>
#include <string>

namespace game {

class SectorLocalFogRenderer {
public:
    bool Apply(
            RenderTexture2D& sceneTarget,
            RenderTexture2D& sceneScratch,
            const SectorTopologyMap& map,
            SectorTopologyFogSettings::VolumetricQuality quality,
            const Camera3D& camera,
            float runtimeSeconds,
            const SectorBakedObjectLightProbeRuntimeData& objectLightProbes,
            const SectorBillboardDynamicLightContext& dynamicLightContext);
    void Shutdown();

    int EligibleVolumeCount() const { return eligibleVolumeCount; }
    int ActiveVolumeCount() const { return activeVolumeCount; }
    const engine::RenderTarget& AccumulationTarget() const { return fogTarget; }
    const std::string& AccumulationDiagnostic() const { return accumulationDiagnostic; }

private:
    struct AccumulateShaderLocations {
        int sceneDepth = -1;
        int volumeCount = -1;
        int marchSteps = -1;
        int cameraPosition = -1;
        int cameraForward = -1;
        int cameraRight = -1;
        int cameraUp = -1;
        int tanHalfFov = -1;
        int aspectRatio = -1;
        int nearPlane = -1;
        int farPlane = -1;
        int runtimeSeconds = -1;
        int pathLimitSettings = -1;
        int probeFootprintFraction = -1;
        int fogCenters = -1;
        int fogRadii = -1;
        int fogColors = -1;
        int fogParamsA = -1;
        int fogParamsB = -1;
        int fogLightingA = -1;
        int fogLightingB = -1;
        int fogLightingC = -1;
        SectorDynamicLightShaderLocations dynamicLights;
        SectorDynamicSpotLightShadowShaderLocations dynamicShadows;
        int shadowMap0 = -1;
        int shadowMap1 = -1;
    };

    struct CompositeShaderLocations {
        int sceneColor = -1;
        int sceneDepth = -1;
        int fogTexture = -1;
        int fogTexelSize = -1;
        int bilateralUpsample = -1;
    };

    struct StaticLightingCacheEntry {
        bool valid = false;
        int sourceFogVolumeId = -1;
        int topologySectorId = -1;
        Vector3 centerWorld = {};
        Vector3 radiiWorld = {};
        Color sectorAmbientColor = {};
        float sectorAmbientIntensity = 0.0f;
        SectorLocalFogStaticLightingSamples samples;
    };

    bool EnsureShaders();
    bool EnsureTargets(int sceneWidth, int sceneHeight, float scale);
    const SectorLocalFogStaticLightingSamples& StaticLightingForVolume(
            const SectorTopologyMap& map,
            const SectorBakedObjectLightProbeRuntimeData& objectLightProbes,
            const SectorCompiledLocalFogVolume& volume);
    void RefreshStaticLightingCacheIdentity(
            const SectorTopologyMap& map,
            const SectorBakedObjectLightProbeRuntimeData& objectLightProbes);
    void ClearStaticLightingCache();
    void ReleaseTargets();

    Shader accumulateShader = {};
    Shader compositeShader = {};
    AccumulateShaderLocations accumulateLocations;
    CompositeShaderLocations compositeLocations;
    engine::RenderTarget fogTarget;
    std::string accumulationDiagnostic = "not allocated";
    std::array<StaticLightingCacheEntry, 16> staticLightingCache{};
    const SectorBakedObjectLightProbe* cachedProbeData = nullptr;
    std::size_t cachedProbeCount = 0;
    std::size_t cachedProbeSourceHashValue = 0;
    std::size_t cachedMapProbeSourceHashValue = 0;
    int sceneWidth = 0;
    int sceneHeight = 0;
    float targetScale = 0.0f;
    int failedWidth = 0;
    int failedHeight = 0;
    float failedScale = 0.0f;
    bool warnedUnavailable = false;
    bool shaderFailed = false;
    bool warnedInvalidProjection = false;
    int eligibleVolumeCount = 0;
    int activeVolumeCount = 0;
};

} // namespace game
