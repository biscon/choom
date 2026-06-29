#pragma once

#include "sector_demo/SectorPortalVisibility.h"

#include <raylib.h>

#include <cstddef>
#include <vector>

namespace game {

class SectorCollisionWorld;
struct SectorReceiverBounds;
struct SectorTopologyDynamicPointLight;
struct SectorTopologyMap;

struct SectorPreviewDynamicPointLightUniform {
    Vector3 position = {};
    Vector3 color = {};
    float radius = 0.0f;
    float intensity = 0.0f;
};

struct SectorPreviewDynamicPointLightSource {
    int lightId = 0;
    int ownerSectorId = 0;
    SectorPreviewDynamicPointLightUniform light = {};
};

bool MakeSectorPreviewDynamicPointLightUniform(
        const SectorTopologyDynamicPointLight& light,
        SectorPreviewDynamicPointLightUniform& outLight);

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
        std::vector<SectorPreviewDynamicPointLightUniform>& outSelectedLights);

} // namespace game
