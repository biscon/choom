#pragma once

#include "sector_demo/SectorPortalVisibility.h"
#include "sector_demo/SectorTopologyTypes.h"

#include <raylib.h>

#include <cstddef>
#include <vector>

namespace game {

class SectorCollisionWorld;
struct SectorReceiverBounds;
struct SectorTopologyDynamicPointLight;
struct SectorTopologyMap;

struct SectorPreviewDynamicPointLightUniform {
    int lightId = 0;
    Vector3 position = {};
    Vector3 color = {};
    float radius = 0.0f;
    // Authored/base intensity used for selection. Upload applies flicker to a local effective value.
    float intensity = 0.0f;
    bool flicker = false;
    float flickerSpeed = DynamicLightFlickerDefaultSpeed;
    float flickerAmount = DynamicLightFlickerDefaultAmount;
};

struct SectorPreviewDynamicPointLightSource {
    int lightId = 0;
    int ownerSectorId = 0;
    SectorPreviewDynamicPointLightUniform light = {};
};

bool MakeSectorPreviewDynamicPointLightUniform(
        const SectorTopologyDynamicPointLight& light,
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

} // namespace game
