#pragma once

#include "sector_demo/SectorPortalVisibility.h"
#include "sector_demo/SectorTopologyMap.h"
#include "sector_demo/SectorVolumetricQuality.h"
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
    int ShaftCount() const { return shaftCount; }
    int DrawCallCount() const { return drawCallCount; }

private:
    bool EnsureResources();
    bool EnsureCapacity(std::size_t quadCapacity);

    Shader shader = {};
    Material material = {};
    Mesh mesh = {};
    int viewportSizeLoc = -1;
    int cameraPositionLoc = -1;
    int cameraForwardLoc = -1;
    int nearPlaneLoc = -1;
    int farPlaneLoc = -1;
    int fogParamsALoc = -1;
    int fogParamsBLoc = -1;
    bool shaderFailed = false;
    std::size_t quadCapacity = 0;
    std::vector<float> vertices;
    std::vector<float> texcoords;
    std::vector<float> normals;
    std::vector<unsigned char> colors;
    int eligibleCount = 0;
    int haloCount = 0;
    int shaftCount = 0;
    int drawCallCount = 0;
};

} // namespace game
