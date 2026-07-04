#pragma once

#include "sector_demo/SectorDynamicPointLightSelection.h"

#include <raylib.h>

#include <array>
#include <cstddef>
#include <string>

namespace engine {
class AssetManager;
class World;
}

namespace game {

struct SectorPreviewBillboardDynamicLightContext {
    int dynamicLightCount = 0;
    std::array<Vector3, MaxDynamicLights> dynamicLightPositions{};
    std::array<Vector3, MaxDynamicLights> dynamicLightColors{};
    std::array<float, MaxDynamicLights> dynamicLightRadii{};
    std::array<float, MaxDynamicLights> dynamicLightIntensities{};
    std::array<int, MaxDynamicLights> dynamicLightTypes{};
    std::array<Vector3, MaxDynamicLights> dynamicLightDirections{};
    std::array<float, MaxDynamicLights> dynamicLightInnerConeCos{};
    std::array<float, MaxDynamicLights> dynamicLightOuterConeCos{};
    SectorPreviewDynamicSpotLightShadowUniforms shadowUniforms{};
    const Texture2D* shadowMap0 = nullptr;
    const Texture2D* shadowMap1 = nullptr;
    float dynamicLightingClamp = 4.0f;
};

class SectorPreviewBillboardRenderer {
public:
    bool Load();
    void Shutdown();
    void ResetDebugState();

    void Draw(
            engine::AssetManager& assets,
            engine::World& runtimeObjectWorld,
            const Camera3D& camera,
            const SectorPreviewBillboardDynamicLightContext& dynamicLightContext,
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
    int dynamicLightingClampLoc = -1;
    bool shaderLoaded = false;
    bool warningPrinted = false;
    std::string renderDebugText;
    size_t consideredCount = 0;
    size_t drawnCount = 0;
    size_t skippedCount = 0;
};

} // namespace game
