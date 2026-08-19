#pragma once

#include "sector_demo/SectorPortalVisibility.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/SectorVolumetricQuality.h"
#include "sector_demo/renderer/SectorAtmosphereCulling.h"
#include "sector_demo/renderer/SectorDynamicLightingRenderer.h"
#include "sector_demo/renderer/SectorLightAtmosphere.h"

#include <raylib.h>

#include <cstddef>
#include <vector>

namespace game {

struct SectorReceiverBounds;

class SectorAnalyticLightShaftRenderer {
public:
    void Reserve(std::size_t sourceCount);
    bool Apply(
            RenderTexture2D& sceneTarget,
            RenderTexture2D& colorOnlyTarget,
            const SectorTopologyFogSettings& fogSettings,
            SectorVolumetricQuality quality,
            const Camera3D& camera,
            const SectorBillboardDynamicLightContext& dynamicLights,
            const std::vector<SectorLightAtmosphereSource>& sources,
            const RuntimePortalVisibilityResult& visibility,
            const std::vector<SectorReceiverBounds>& receiverBounds);
    void Shutdown();

    int EligibleCount() const { return eligibleCount; }
    int ActiveCount() const { return activeCount; }
    float ScissorCoverage() const { return scissorCoverage; }
    int DrawCallCount() const { return drawCallCount; }

private:
    struct VisibleShaft {
        const SectorLightAtmosphereSource* source = nullptr;
        SectorLightAtmosphereVolume volume;
        SectorAtmosphereScissorRect scissor;
        Vector3 radiance = {};
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
    int coneApexLoc = -1;
    int coneDirectionLoc = -1;
    int coneLengthLoc = -1;
    int coneBaseRadiusLoc = -1;
    int shaftRadianceLoc = -1;
    int shaftParamsLoc = -1;
    int fogParamsALoc = -1;
    int fogParamsBLoc = -1;
    bool shaderFailed = false;
    std::vector<VisibleShaft> visibleShafts;
    int eligibleCount = 0;
    int activeCount = 0;
    float scissorCoverage = 0.0f;
    int drawCallCount = 0;
};

} // namespace game
