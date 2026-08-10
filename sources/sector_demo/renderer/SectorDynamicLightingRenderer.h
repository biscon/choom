#pragma once

#include "sector_demo/SectorDynamicPointLightSelection.h"
#include "sector_demo/SectorMeshTypes.h"

#include <raylib.h>

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace engine {
class AssetManager;
class World;
}

namespace game {

struct SectorTopologyMap;
struct SectorDoorShadowCaster;

struct SectorDynamicSpotLightShadowRenderContext {
    using TextureResolver = const Texture2D* (*)(
            void* userData,
            engine::AssetManager& assets,
            const std::string& textureId);
    using DoorMeshResolver = const Mesh* (*)(
            void* userData,
            const SectorDoorShadowCaster& caster,
            float& outWidth,
            float& outHeight);

    engine::AssetManager* assets = nullptr;
    const std::vector<SectorMeshBatch>* sectorDrawRecords = nullptr;
    const std::vector<SectorDoorShadowCaster>* doorShadowCasters = nullptr;
    void* userData = nullptr;
    void* doorMeshResolverUserData = nullptr;
    TextureResolver textureResolver = nullptr;
    DoorMeshResolver doorMeshResolver = nullptr;
};

struct SectorDynamicLightShaderLocations {
    int dynamicLightCount = -1;
    int dynamicLightPositions = -1;
    int dynamicLightColors = -1;
    int dynamicLightRadii = -1;
    int dynamicLightIntensities = -1;
    int dynamicLightTypes = -1;
    int dynamicLightDirections = -1;
    int dynamicLightInnerConeCos = -1;
    int dynamicLightOuterConeCos = -1;
};

struct SectorDynamicSpotLightShadowShaderLocations {
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

struct SectorDynamicShadowMapTextures {
    const Texture2D* shadowMap0 = nullptr;
    const Texture2D* shadowMap1 = nullptr;
};

struct SectorBillboardDynamicLightContext {
    int dynamicLightCount = 0;
    std::array<int, MaxDynamicLights> dynamicLightIds{};
    std::array<Vector3, MaxDynamicLights> dynamicLightPositions{};
    std::array<Vector3, MaxDynamicLights> dynamicLightColors{};
    std::array<float, MaxDynamicLights> dynamicLightRadii{};
    std::array<float, MaxDynamicLights> dynamicLightIntensities{};
    std::array<int, MaxDynamicLights> dynamicLightTypes{};
    std::array<Vector3, MaxDynamicLights> dynamicLightDirections{};
    std::array<float, MaxDynamicLights> dynamicLightInnerConeCos{};
    std::array<float, MaxDynamicLights> dynamicLightOuterConeCos{};
    SectorPreviewDynamicSpotLightShadowUniforms shadowUniforms{};
    SectorDynamicShadowMapTextures shadowMaps{};
};

void UploadSectorRendererDynamicPointLights(
        Shader shader,
        const SectorDynamicLightShaderLocations& locations,
        bool dynamicLightingEnabled,
        float runtimeSeconds,
        const std::vector<SectorPreviewDynamicPointLightUniform>& lights);

void UploadSectorRendererDynamicPointLights(
        Shader shader,
        const SectorDynamicLightShaderLocations& locations,
        const SectorBillboardDynamicLightContext& context);

void UploadSectorRendererDynamicSpotLightShadowUniforms(
        Shader shader,
        const SectorDynamicSpotLightShadowShaderLocations& locations,
        const SectorPreviewDynamicSpotLightShadowUniforms& uniforms);

class SectorDynamicLightingRenderer {
public:
    void Reset();
    void RebuildSources(const SectorTopologyMap& map, const SectorCollisionWorld* sectorLookupWorld);
    void UpdateSelection(
            const RuntimePortalVisibilityResult& visibility,
            const std::vector<SectorReceiverBounds>& sectorReceiverBounds,
            engine::World* runtimeObjectWorld);
    void SetRuntimePointLight(
            const SectorPreviewDynamicPointLightSource* light);

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
    bool LoadShadowMaterial();
    void UnloadShadowMaterial();
    bool HasShadowMaterial() const { return shadowMaterialLoaded; }
    bool IsShadowRenderReady() const;
    RenderTexture2D* ShadowMap(std::size_t index);
    const RenderTexture2D* ShadowMap(std::size_t index) const;
    const Texture2D* ShadowMapDepthTexture(std::size_t index) const;
    SectorDynamicShadowMapTextures BuildShadowMapTextures() const;
    void RenderShadowMaps(const SectorDynamicSpotLightShadowRenderContext& context);

private:
    void ReserveSelectionBuffers();
    void BuildReceiverBounds(
            const std::vector<SectorReceiverBounds>& sectorReceiverBounds,
            engine::World* runtimeObjectWorld);

    std::vector<SectorPreviewDynamicPointLightSource> sources;
    std::vector<SectorPreviewDynamicPointLightSource> selectionSources;
    SectorPreviewDynamicPointLightSource runtimePointLight;
    bool runtimePointLightActive = false;
    std::vector<SectorPreviewDynamicPointLightSource> candidates;
    std::vector<SectorPreviewDynamicPointLightUniform> selectedLights;
    std::vector<int> selectedLightIds;
    std::vector<SectorReceiverBounds> receiverBounds;
    std::vector<SectorPreviewDynamicSpotLightShadowCaster> shadowCasters;
    std::vector<SectorPreviewDynamicSpotLightShadowMatrix> shadowMatrices;
    std::array<RenderTexture2D, MaxDynamicSpotLightShadowCasters> shadowMaps{};
    Material shadowMaterial = {};
    Texture2D shadowDefaultTexture = {};
    bool shadowMaterialLoaded = false;
    int shadowLightViewProjectionLoc = -1;
    int shadowAlphaTestLoc = -1;
    int shadowAlphaCutoffLoc = -1;
};

} // namespace game
