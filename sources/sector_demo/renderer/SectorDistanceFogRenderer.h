#pragma once

#include "sector_demo/SectorTopologyMap.h"

#include <raylib.h>

namespace game {

class SectorDistanceFogRenderer {
public:
    bool Apply(
            RenderTexture2D& sceneTarget,
            RenderTexture2D& sceneScratch,
            const SectorTopologyFogSettings& settings,
            const Camera3D& camera);
    void Shutdown();

private:
    bool EnsureShader();

    Shader shader = {};
    int sceneColorLoc = -1;
    int sceneDepthLoc = -1;
    int nearPlaneLoc = -1;
    int farPlaneLoc = -1;
    int startDistanceLoc = -1;
    int endDistanceLoc = -1;
    int falloffExponentLoc = -1;
    int maxOpacityLoc = -1;
    int fogColorLoc = -1;
    bool shaderFailed = false;
};

} // namespace game
