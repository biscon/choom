#pragma once

#include "sector_demo/renderer/SectorDynamicLightingRenderer.h"
#include "sector_demo/renderer/SectorFog.h"

#include <raylib.h>

#include <cstddef>
#include <string>

namespace engine {
class AssetManager;
class World;
}

namespace game {

class SectorBillboardRenderer {
public:
    bool Load();
    void Shutdown();
    void ResetDebugState();

    void Draw(
            engine::AssetManager& assets,
            engine::World& runtimeObjectWorld,
            const Camera3D& camera,
            const SectorBillboardDynamicLightContext& dynamicLightContext,
            const SectorFogRenderContext& fogContext,
            std::string& renderDebugText);

    bool IsLoaded() const { return shaderLoaded; }
    const std::string& DebugText() const { return renderDebugText; }
    size_t ConsideredCount() const { return consideredCount; }
    size_t DrawnCount() const { return drawnCount; }
    size_t SkippedCount() const { return skippedCount; }

private:
    Shader cutoutShader = {};
    int textureLoc = -1;
    int alphaCutoffLoc = -1;
    int bakedLightingLoc = -1;
    int dynamicLightCountLoc = -1;
    int dynamicLightPositionsLoc = -1;
    int dynamicLightColorsLoc = -1;
    int dynamicLightRadiiLoc = -1;
    int dynamicLightIntensitiesLoc = -1;
    int dynamicLightTypesLoc = -1;
    int dynamicLightDirectionsLoc = -1;
    int dynamicLightInnerConeCosLoc = -1;
    int dynamicLightOuterConeCosLoc = -1;
    int dynamicLightShadowSlotsLoc = -1;
    std::array<int, MaxDynamicSpotLightShadowCasters> shadowLightMatrixLocs = [] {
        std::array<int, MaxDynamicSpotLightShadowCasters> locs{};
        locs.fill(-1);
        return locs;
    }();
    int shadowBiasLoc = -1;
    int shadowStrengthLoc = -1;
    int shadowSoftnessLoc = -1;
    int shadowMap0Loc = -1;
    int shadowMap1Loc = -1;
    SectorFogShaderLocations fogShaderLocations;
    bool shaderLoaded = false;
    bool warningPrinted = false;
    std::string renderDebugText;
    size_t consideredCount = 0;
    size_t drawnCount = 0;
    size_t skippedCount = 0;
};

} // namespace game
