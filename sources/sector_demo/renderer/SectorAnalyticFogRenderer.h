#pragma once

#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/SectorVolumetricQuality.h"

#include <raylib.h>

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
            const Camera3D& camera);
    void Shutdown();

    int EligibleVolumeCount() const { return eligibleCount; }
    int ActiveVolumeCount() const { return activeCount; }
    float ScissorCoverage() const { return scissorCoverage; }

private:
    struct VisibleVolume {
        const SectorCompiledLocalFogVolume* volume = nullptr;
        float distanceSquared = 0.0f;
    };

    bool EnsureShader();

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
    bool shaderFailed = false;
    std::vector<VisibleVolume> visibleVolumes;
    int eligibleCount = 0;
    int activeCount = 0;
    float scissorCoverage = 0.0f;
};

} // namespace game
