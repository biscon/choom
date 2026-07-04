#pragma once

#include "sector_demo/SectorDynamicPointLightSelection.h"

#include <raylib.h>

#include <array>

namespace game {

struct SectorPreviewDynamicLightShaderLocations {
    int dynamicLightCount = -1;
    int dynamicLightPositions = -1;
    int dynamicLightColors = -1;
    int dynamicLightRadii = -1;
    int dynamicLightIntensities = -1;
    int dynamicLightTypes = -1;
    int dynamicLightDirections = -1;
    int dynamicLightInnerConeCos = -1;
    int dynamicLightOuterConeCos = -1;
    int dynamicLightingClamp = -1;
};

struct SectorPreviewDynamicSpotLightShadowShaderLocations {
    int dynamicLightShadowSlots = -1;
    std::array<int, MaxDynamicSpotLightShadowCasters> shadowLightMatrices = [] {
        std::array<int, MaxDynamicSpotLightShadowCasters> locs{};
        locs.fill(-1);
        return locs;
    }();
    int shadowBias = -1;
    int shadowStrength = -1;
    int shadowSoftness = -1;
};

struct SectorPreviewDynamicShadowMapTextures {
    const Texture2D* shadowMap0 = nullptr;
    const Texture2D* shadowMap1 = nullptr;
};

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
    SectorPreviewDynamicShadowMapTextures shadowMaps{};
    float dynamicLightingClamp = 4.0f;
};

} // namespace game
