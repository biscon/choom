#pragma once

#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/SectorVolumetricQuality.h"
#include "sector_demo/renderer/SectorAtmosphereCulling.h"
#include "sector_demo/renderer/SectorLocalFogLighting.h"

#include <raylib.h>

#include <array>
#include <cstddef>
#include <vector>

namespace game {

class SectorAnalyticFogRenderer {
public:
    void Reserve(std::size_t volumeCount);
    bool Apply(
            RenderTexture2D& sceneTarget,
            RenderTexture2D& colorOnlyTarget,
            const SectorTopologyMap& map,
            SectorVolumetricQuality quality,
            const Camera3D& camera,
            float runtimeSeconds,
            const SectorBakedObjectLightProbeRuntimeData& objectLightProbes);
    void Shutdown();

    int EligibleVolumeCount() const { return eligibleCount; }
    int ActiveVolumeCount() const { return activeCount; }
    float ScissorCoverage() const { return scissorCoverage; }

private:
    struct VisibleVolume {
        const SectorCompiledLocalFogVolume* volume = nullptr;
        SectorAtmosphereScissorRect scissor;
        float distanceSquared = 0.0f;
    };

    struct StaticLightingCacheEntry {
        bool valid = false;
        int sourceFogVolumeId = -1;
        int topologySectorId = -1;
        Vector3 centerWorld = {};
        Vector3 radiiWorld = {};
        float yawRadians = 0.0f;
        Color sectorAmbientColor = {};
        float sectorAmbientIntensity = 0.0f;
        SectorLocalFogStaticLightingSamples samples;
    };

    bool EnsureShader();
    const SectorLocalFogStaticLightingSamples& StaticLightingForVolume(
            const SectorTopologyMap& map,
            const SectorBakedObjectLightProbeRuntimeData& objectLightProbes,
            const SectorCompiledLocalFogVolume& volume);
    void RefreshStaticLightingCacheIdentity(
            const SectorTopologyMap& map,
            const SectorBakedObjectLightProbeRuntimeData& objectLightProbes);
    void ClearStaticLightingCache();

    Shader shader = {};
    int sceneDepthLoc = -1;
    int viewportSizeLoc = -1;
    int cameraPositionLoc = -1;
    int cameraForwardLoc = -1;
    int cameraRightLoc = -1;
    int cameraUpLoc = -1;
    int tanHalfFovLoc = -1;
    int aspectRatioLoc = -1;
    int nearPlaneLoc = -1;
    int farPlaneLoc = -1;
    int centerLoc = -1;
    int radiiLoc = -1;
    int colorLoc = -1;
    int fogParamsLoc = -1;
    int fogShapeLoc = -1;
    int fogBoxStyleLoc = -1;
    int fogYawLoc = -1;
    int fogNoiseParamsLoc = -1;
    int fogFlowLoc = -1;
    std::array<int, 4> fogLightingLocs{};
    bool shaderFailed = false;
    std::vector<VisibleVolume> visibleVolumes;
    std::array<StaticLightingCacheEntry, 16> staticLightingCache{};
    const SectorBakedObjectLightProbe* cachedProbeData = nullptr;
    std::size_t cachedProbeCount = 0;
    std::size_t cachedProbeSourceHashValue = 0;
    std::size_t cachedMapProbeSourceHashValue = 0;
    int eligibleCount = 0;
    int activeCount = 0;
    float scissorCoverage = 0.0f;
};

} // namespace game
