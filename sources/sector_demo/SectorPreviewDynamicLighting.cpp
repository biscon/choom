#include "sector_demo/SectorPreviewDynamicLighting.h"

#include "sector_demo/SectorDoorRuntime.h"
#include "sector_demo/SectorTopologyMap.h"

namespace game {

void SectorPreviewDynamicLighting::Reset()
{
    sources.clear();
    candidates.clear();
    selectedLights.clear();
    selectedLightIds.clear();
    receiverBounds.clear();
    shadowCasters.clear();
    shadowMatrices.clear();
}

void SectorPreviewDynamicLighting::RebuildSources(
        const SectorTopologyMap& map,
        const SectorCollisionWorld* sectorLookupWorld)
{
    BuildSectorPreviewDynamicPointLightSources(map, sectorLookupWorld, sources);
    ReserveSelectionBuffers();
}

void SectorPreviewDynamicLighting::UpdateSelection(
        const RuntimePortalVisibilityResult& visibility,
        const std::vector<SectorReceiverBounds>& sectorReceiverBounds,
        engine::World* runtimeObjectWorld)
{
    BuildReceiverBounds(sectorReceiverBounds, runtimeObjectWorld);
    CollectSectorPreviewDynamicPointLightCandidates(
            sources,
            visibility,
            receiverBounds,
            candidates);
    SelectRankedSectorPreviewDynamicPointLights(
            candidates,
            visibility,
            receiverBounds,
            static_cast<std::size_t>(MaxDynamicLights),
            selectedLights,
            &selectedLightIds,
            &selectedLightIds);
    SelectRankedSectorPreviewDynamicSpotLightShadowCasters(
            selectedLights,
            visibility,
            receiverBounds,
            MaxDynamicSpotLightShadowCasters,
            shadowCasters);
    BuildSectorPreviewDynamicSpotLightShadowMatrices(
            selectedLights,
            shadowCasters,
            shadowMatrices);
}

SectorPreviewDynamicSpotLightShadowUniforms SectorPreviewDynamicLighting::PackShadowUniforms() const
{
    return PackSectorPreviewDynamicSpotLightShadowUniforms(selectedLights, shadowCasters, shadowMatrices);
}

void SectorPreviewDynamicLighting::ReserveSelectionBuffers()
{
    candidates.clear();
    candidates.reserve(sources.size());
    selectedLights.clear();
    selectedLights.reserve(MaxDynamicLights);
    selectedLightIds.clear();
    selectedLightIds.reserve(MaxDynamicLights);
    shadowCasters.clear();
    shadowCasters.reserve(MaxDynamicSpotLightShadowCasters);
    shadowMatrices.clear();
    shadowMatrices.reserve(MaxDynamicSpotLightShadowCasters);
}

void SectorPreviewDynamicLighting::BuildReceiverBounds(
        const std::vector<SectorReceiverBounds>& sectorReceiverBounds,
        engine::World* runtimeObjectWorld)
{
    receiverBounds.clear();
    receiverBounds.reserve(sectorReceiverBounds.size());
    receiverBounds.insert(receiverBounds.end(), sectorReceiverBounds.begin(), sectorReceiverBounds.end());
    if (runtimeObjectWorld != nullptr) {
        CollectSectorDoorReceiverBounds(*runtimeObjectWorld, receiverBounds);
    }
}

} // namespace game
