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

class SectorLightProxyRenderer {
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
    int HaloCount() const { return haloCount; }
    float ScissorCoverage() const { return scissorCoverage; }
    int DrawCallCount() const { return drawCallCount; }

private:
    bool EnsureShader();

    struct VisibleHalo {
        const SectorLightAtmosphereSource* source = nullptr;
        SectorAtmosphereScissorRect scissor;
        Vector3 radiance = {};
        float distanceSquared = 0.0f;
    };

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
    int sphereCenterLoc = -1;
    int sphereRadiusLoc = -1;
    int haloRadianceLoc = -1;
    int haloParamsLoc = -1;
    int fogParamsALoc = -1;
    int fogParamsBLoc = -1;
    bool shaderFailed = false;
    std::vector<VisibleHalo> visibleHalos;
    int eligibleCount = 0;
    int haloCount = 0;
    float scissorCoverage = 0.0f;
    int drawCallCount = 0;
};

} // namespace game
