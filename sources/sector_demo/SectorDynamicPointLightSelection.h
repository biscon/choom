#pragma once

#include "sector_demo/SectorPortalVisibility.h"
#include "sector_demo/SectorTopologyTypes.h"

#include <raylib.h>

#include <cstddef>
#include <vector>

namespace game {

constexpr std::size_t MaxDynamicSpotLightShadowCasters = 2;
constexpr int DynamicSpotLightShadowMapResolution = 1024;

class SectorCollisionWorld;
struct SectorReceiverBounds;
struct SectorTopologyDynamicPointLight;
struct SectorTopologyDynamicSpotLight;
struct SectorTopologyMap;

enum class SectorPreviewDynamicLightKind {
    Point = 0,
    Spot = 1
};

struct SectorPreviewDynamicPointLightUniform {
    int lightId = 0;
    SectorPreviewDynamicLightKind kind = SectorPreviewDynamicLightKind::Point;
    Vector3 position = {};
    Vector3 direction = {0.0f, -1.0f, 0.0f};
    Vector3 color = {};
    float radius = 0.0f;
    float innerConeCos = -1.0f;
    float outerConeCos = -1.0f;
    // Authored/base intensity used for selection. Upload applies flicker to a local effective value.
    float intensity = 0.0f;
    bool flicker = false;
    float flickerSpeed = DynamicLightFlickerDefaultSpeed;
    float flickerAmount = DynamicLightFlickerDefaultAmount;
    bool castsShadow = false;
    int shadowPriority = DynamicSpotLightDefaultShadowPriority;
    float shadowBias = DynamicSpotLightDefaultShadowBias;
    float shadowStrength = DynamicSpotLightDefaultShadowStrength;
};

struct SectorPreviewDynamicPointLightSource {
    int lightId = 0;
    int ownerSectorId = 0;
    SectorPreviewDynamicPointLightUniform light = {};
};

struct SectorPreviewDynamicSpotLightShadowCaster {
    int lightId = 0;
    int dynamicLightIndex = -1;
    int shadowSlot = -1;
    int shadowPriority = DynamicSpotLightDefaultShadowPriority;
    float selectionScore = 0.0f;
    float shadowBias = DynamicSpotLightDefaultShadowBias;
    float shadowStrength = DynamicSpotLightDefaultShadowStrength;
};

struct SectorPreviewDynamicSpotLightShadowMatrix {
    int lightId = 0;
    int dynamicLightIndex = -1;
    int shadowSlot = -1;
    Matrix view = {};
    Matrix projection = {};
    Matrix lightViewProjection = {};
};

bool MakeSectorPreviewDynamicPointLightUniform(
        const SectorTopologyDynamicPointLight& light,
        SectorPreviewDynamicPointLightUniform& outLight);

bool MakeSectorPreviewDynamicSpotLightUniform(
        const SectorTopologyDynamicSpotLight& light,
        SectorPreviewDynamicPointLightUniform& outLight);

float EvaluateDynamicLightFlickerMultiplier(
        int lightId,
        float runtimeSeconds,
        float flickerSpeed,
        float flickerAmount);

float DynamicLightEffectiveUploadIntensity(
        const SectorPreviewDynamicPointLightUniform& light,
        float runtimeSeconds);

void BuildSectorPreviewDynamicPointLightSources(
        const SectorTopologyMap& map,
        const SectorCollisionWorld* sectorLookupWorld,
        std::vector<SectorPreviewDynamicPointLightSource>& outSources);

void CollectSectorPreviewDynamicPointLightCandidates(
        const std::vector<SectorPreviewDynamicPointLightSource>& sources,
        const RuntimePortalVisibilityResult& visibility,
        const std::vector<SectorReceiverBounds>& receiverBounds,
        std::vector<SectorPreviewDynamicPointLightSource>& outCandidates);

void SelectRankedSectorPreviewDynamicPointLights(
        std::vector<SectorPreviewDynamicPointLightSource>& candidates,
        const RuntimePortalVisibilityResult& visibility,
        const std::vector<SectorReceiverBounds>& receiverBounds,
        std::size_t maxLights,
        std::vector<SectorPreviewDynamicPointLightUniform>& outSelectedLights,
        std::vector<int>* outSelectedLightIds = nullptr,
        const std::vector<int>* previousSelectedLightIds = nullptr);

void SelectRankedSectorPreviewDynamicSpotLightShadowCasters(
        const std::vector<SectorPreviewDynamicPointLightUniform>& selectedDynamicLights,
        const RuntimePortalVisibilityResult& visibility,
        const std::vector<SectorReceiverBounds>& receiverBounds,
        std::size_t maxShadowCasters,
        std::vector<SectorPreviewDynamicSpotLightShadowCaster>& outShadowCasters);

bool MakeSectorPreviewDynamicSpotLightShadowMatrix(
        const SectorPreviewDynamicPointLightUniform& light,
        int dynamicLightIndex,
        int shadowSlot,
        SectorPreviewDynamicSpotLightShadowMatrix& outMatrix);

void BuildSectorPreviewDynamicSpotLightShadowMatrices(
        const std::vector<SectorPreviewDynamicPointLightUniform>& selectedDynamicLights,
        const std::vector<SectorPreviewDynamicSpotLightShadowCaster>& shadowCasters,
        std::vector<SectorPreviewDynamicSpotLightShadowMatrix>& outMatrices);

} // namespace game
