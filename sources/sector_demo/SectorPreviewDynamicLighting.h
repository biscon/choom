#pragma once

#include "sector_demo/SectorDynamicPointLightSelection.h"
#include "sector_demo/SectorMeshTypes.h"

#include <raylib.h>

#include <array>
#include <cstddef>
#include <vector>

namespace engine {
class World;
}

namespace game {

struct SectorTopologyMap;

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

void UploadSectorPreviewDynamicPointLights(
        Shader shader,
        const SectorPreviewDynamicLightShaderLocations& locations,
        bool dynamicLightingEnabled,
        float runtimeSeconds,
        const std::vector<SectorPreviewDynamicPointLightUniform>& lights);

void UploadSectorPreviewDynamicPointLights(
        Shader shader,
        const SectorPreviewDynamicLightShaderLocations& locations,
        const SectorPreviewBillboardDynamicLightContext& context);

void UploadSectorPreviewDynamicSpotLightShadowUniforms(
        Shader shader,
        const SectorPreviewDynamicSpotLightShadowShaderLocations& locations,
        const SectorPreviewDynamicSpotLightShadowUniforms& uniforms);

class SectorPreviewDynamicLighting {
public:
    void Reset();
    void RebuildSources(const SectorTopologyMap& map, const SectorCollisionWorld* sectorLookupWorld);
    void UpdateSelection(
            const RuntimePortalVisibilityResult& visibility,
            const std::vector<SectorReceiverBounds>& sectorReceiverBounds,
            engine::World* runtimeObjectWorld);

    const std::vector<SectorPreviewDynamicPointLightSource>& Sources() const { return sources; }
    const std::vector<SectorPreviewDynamicPointLightSource>& Candidates() const { return candidates; }
    const std::vector<SectorPreviewDynamicPointLightUniform>& SelectedLights() const { return selectedLights; }
    const std::vector<int>& SelectedLightIds() const { return selectedLightIds; }
    const std::vector<SectorPreviewDynamicSpotLightShadowCaster>& ShadowCasters() const { return shadowCasters; }
    const std::vector<SectorPreviewDynamicSpotLightShadowMatrix>& ShadowMatrices() const { return shadowMatrices; }

    size_t SourceCount() const { return sources.size(); }
    size_t CandidateCount() const { return candidates.size(); }
    SectorPreviewDynamicSpotLightShadowUniforms PackShadowUniforms() const;
    bool EnsureShadowMapResources();
    void UnloadShadowMapResources();
    bool HasShadowMapResources() const;
    RenderTexture2D* ShadowMap(std::size_t index);
    const RenderTexture2D* ShadowMap(std::size_t index) const;
    const Texture2D* ShadowMapDepthTexture(std::size_t index) const;
    SectorPreviewDynamicShadowMapTextures BuildShadowMapTextures() const;

private:
    void ReserveSelectionBuffers();
    void BuildReceiverBounds(
            const std::vector<SectorReceiverBounds>& sectorReceiverBounds,
            engine::World* runtimeObjectWorld);

    std::vector<SectorPreviewDynamicPointLightSource> sources;
    std::vector<SectorPreviewDynamicPointLightSource> candidates;
    std::vector<SectorPreviewDynamicPointLightUniform> selectedLights;
    std::vector<int> selectedLightIds;
    std::vector<SectorReceiverBounds> receiverBounds;
    std::vector<SectorPreviewDynamicSpotLightShadowCaster> shadowCasters;
    std::vector<SectorPreviewDynamicSpotLightShadowMatrix> shadowMatrices;
    std::array<RenderTexture2D, MaxDynamicSpotLightShadowCasters> shadowMaps{};
};

} // namespace game
