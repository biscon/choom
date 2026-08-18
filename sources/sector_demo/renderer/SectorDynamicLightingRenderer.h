#pragma once

#include "sector_demo/SectorDynamicPointLightSelection.h"
#include "sector_demo/SectorMeshTypes.h"

#include <raylib.h>

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace engine {
class AssetManager;
class World;
}

namespace game {

struct SectorTopologyMap;
struct SectorDoorShadowCaster;
struct SectorDoorModelShadowCaster;
struct SectorStaticModelShadowCaster;

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
    const std::vector<SectorDoorModelShadowCaster>* doorModelShadowCasters = nullptr;
    const std::vector<SectorStaticModelShadowCaster>* staticModelShadowCasters = nullptr;
    const std::vector<SectorReceiverBounds>* sectorReceiverBounds = nullptr;
    uint64_t doorShadowCasterRevision = 0;
    uint64_t staticModelShadowCasterRevision = 0;
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
    int dynamicLightSpotShadowRight = -1;
    int dynamicLightSpotShadowProjection = -1;
    int hasPointShadows = -1;
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
    int shadowAtlasTilesPerRow = -1;
};

struct SectorDynamicShadowMapTextures {
    const Texture2D* shadowAtlas = nullptr;
    const Texture2D* shadowMap0 = nullptr;
    const Texture2D* shadowMap1 = nullptr;
};

struct SectorDynamicShadowRenderStats {
    bool enabled = false;
    bool cacheHit = false;
    bool atlasRendered = false;
    std::size_t renderedTiles = 0;
    std::size_t updatedLights = 0;
    std::size_t queuedLights = 0;
    std::size_t validLights = 0;
    std::size_t dirtyLights = 0;
    std::size_t occupiedTiles = 0;
    std::size_t pointLights = 0;
    std::size_t spotLights = 0;
    double cpuMilliseconds = 0.0;
    std::size_t sectorBatchesDrawn = 0;
    std::size_t sectorBatchesCulled = 0;
    std::size_t objectCastersDrawn = 0;
    std::size_t objectCastersCulled = 0;
    uint64_t doorCasterRevision = 0;
    uint64_t staticModelCasterRevision = 0;
};

struct SectorDynamicLightSelectionStats {
    bool reachabilityCacheHit = false;
    bool cameraVisibilityFallback = false;
    int lightingStartSectorId = -1;
    std::size_t dynamicPortalBlockerCount = 0;
    std::size_t reachableSectorCount = 0;
    std::size_t visibleReceiverCount = 0;
    std::size_t visibleReceiverLightReferences = 0;
    std::size_t maxVisibleReceiverLights = 0;
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
    std::array<Vector3, MaxDynamicLights> dynamicLightSpotShadowRight{};
    // x = inverse tan(outer half-angle), y = far / (far - near).
    std::array<Vector2, MaxDynamicLights> dynamicLightSpotShadowProjection{};
    int hasPointShadows = 0;
    SectorPreviewDynamicSpotLightShadowUniforms shadowUniforms{};
    SectorDynamicShadowMapTextures shadowMaps{};
};

struct SectorDynamicLightSectorContext {
    int sectorId = -1;
    SectorBillboardDynamicLightContext lighting;
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

void UploadSectorRendererDynamicShadowSlots(
        Shader shader,
        int dynamicLightShadowSlotsLocation,
        const SectorPreviewDynamicSpotLightShadowUniforms& uniforms);

class SectorDynamicLightingRenderer {
public:
    void Reset();
    void RebuildSources(const SectorTopologyMap& map, const SectorCollisionWorld* sectorLookupWorld);
    void ReserveReceiverBoundsCapacity(
            size_t sectorCapacity,
            size_t runtimeObjectCapacity);
    void UpdateSelection(
            const RuntimePortalVisibilityResult& visibility,
            int lightingStartSectorId,
            const std::vector<SectorReceiverBounds>& sectorReceiverBounds,
            engine::World* runtimeObjectWorld,
            const RuntimeSectorVisibilityGraph* visibilityGraph = nullptr,
            const std::vector<RuntimePortalDynamicBlocker>* dynamicPortalBlockers = nullptr);
    void BuildSectorLightContexts(
            const std::vector<SectorReceiverBounds>& sectorBounds,
            bool dynamicLightingEnabled,
            bool shadowMapsEnabled,
            float runtimeSeconds);
    const SectorBillboardDynamicLightContext* FindSectorLightContext(
            int sectorId) const;
    SectorBillboardDynamicLightContext BuildLightContext(
            const SectorReceiverBounds* bounds,
            bool dynamicLightingEnabled,
            bool shadowMapsEnabled,
            float runtimeSeconds) const;
    void SetRuntimePointLight(
            const SectorPreviewDynamicPointLightSource* light);
    void SetMaxDynamicLights(std::size_t count) {
        maxDynamicLights = std::min(count, MaxDynamicLights);
    }
    void SetMaxShadowLightUpdatesPerFrame(std::size_t count) {
        maxShadowLightUpdatesPerFrame = std::min(count, MaxDynamicLights);
    }

    const std::vector<SectorPreviewDynamicPointLightSource>& Sources() const { return sources; }
    const std::vector<SectorPreviewDynamicPointLightSource>& Candidates() const { return candidates; }
    const std::vector<SectorPreviewDynamicPointLightUniform>& SelectedLights() const { return selectedLights; }
    const std::vector<SectorPreviewDynamicLightKey>& SelectedLightKeys() const {
        return selectedLightKeys;
    }
    const SectorDynamicLightSelectionStats& SelectionStats() const {
        return selectionStats;
    }
    const std::vector<SectorPreviewDynamicSpotLightShadowCaster>& ShadowCasters() const { return shadowCasters; }
    const std::vector<SectorPreviewDynamicSpotLightShadowMatrix>& ShadowMatrices() const { return shadowMatrices; }

    size_t SourceCount() const { return sources.size(); }
    size_t CandidateCount() const { return candidates.size(); }
    std::size_t ShadowSlotBudget() const {
        return shadowMapResolution <= 512 ? MaxDynamicSpotLightShadowCasters : 16u;
    }
    SectorPreviewDynamicSpotLightShadowUniforms PackShadowUniforms(
            bool enabled = true) const;
    bool EnsureShadowMapResources();
    void SetShadowMapResolution(int resolution);
    void UnloadShadowMapResources();
    bool HasShadowMapResources() const;
    bool LoadShadowMaterial();
    void UnloadShadowMaterial();
    bool HasShadowMaterial() const { return shadowMaterialLoaded; }
    bool IsShadowRenderReady() const;
    void BeginShadowFrame(bool enabled);
    const SectorDynamicShadowRenderStats& ShadowRenderStats() const {
        return shadowRenderStats;
    }
    RenderTexture2D* ShadowMap(std::size_t index);
    const RenderTexture2D* ShadowMap(std::size_t index) const;
    const Texture2D* ShadowMapDepthTexture(std::size_t index) const;
    SectorDynamicShadowMapTextures BuildShadowMapTextures(
            bool enabled = true) const;
    void RenderShadowMaps(const SectorDynamicSpotLightShadowRenderContext& context);

private:
    struct ShadowAtlasTileState {
        SectorPreviewDynamicSpotLightShadowMatrix matrix{};
        bool assigned = false;
        bool valid = false;
        bool dirty = true;
        uint64_t dirtySerial = 0;
    };

    struct ShadowCasterBoundsRecord {
        uint64_t key = 0;
        BoundingBox bounds{};
    };

    void ReserveSelectionBuffers();
    void UpdateLightingReachability(
            const RuntimePortalVisibilityResult& visibility,
            int lightingStartSectorId,
            const RuntimeSectorVisibilityGraph* visibilityGraph,
            const std::vector<RuntimePortalDynamicBlocker>* dynamicPortalBlockers);
    void UpdateSelectionStats(const RuntimePortalVisibilityResult& cameraVisibility);
    void RefreshShadowTileRequirements();
    void BuildReceiverBounds(
            const std::vector<SectorReceiverBounds>& sectorReceiverBounds,
            engine::World* runtimeObjectWorld);

    std::vector<SectorPreviewDynamicPointLightSource> sources;
    std::vector<SectorPreviewDynamicPointLightSource> selectionSources;
    SectorPreviewDynamicPointLightSource runtimePointLight;
    bool runtimePointLightActive = false;
    std::vector<SectorPreviewDynamicPointLightSource> candidates;
    std::vector<SectorPreviewDynamicPointLightUniform> selectedLights;
    std::vector<SectorPreviewDynamicLightKey> selectedLightKeys;
    RuntimePortalVisibilityResult lightingVisibility;
    std::vector<int> cachedLightingStartSectorIds;
    std::vector<RuntimePortalDynamicBlocker> cachedLightingPortalBlockers;
    const RuntimeSectorVisibilityGraph* cachedLightingVisibilityGraph = nullptr;
    bool lightingVisibilityCacheValid = false;
    SectorDynamicLightSelectionStats selectionStats;
    std::vector<SectorReceiverBounds> receiverBounds;
    std::vector<SectorDynamicLightSectorContext> sectorLightContexts;
    std::vector<SectorPreviewDynamicSpotLightShadowCaster> shadowCasters;
    std::vector<SectorPreviewDynamicSpotLightShadowMatrix> shadowMatrices;
    std::array<ShadowAtlasTileState, MaxDynamicSpotLightShadowCasters>
            shadowAtlasTileStates{};
    std::array<SectorDynamicShadowSlotOwner, MaxDynamicSpotLightShadowCasters>
            shadowAtlasSlotOwners{};
    std::vector<SectorDynamicShadowUpdateRequest> pendingShadowLightUpdates;
    std::vector<ShadowCasterBoundsRecord> previousDoorShadowCasterBounds;
    std::vector<ShadowCasterBoundsRecord> currentDoorShadowCasterBounds;
    std::vector<ShadowCasterBoundsRecord> previousStaticShadowCasterBounds;
    std::vector<ShadowCasterBoundsRecord> currentStaticShadowCasterBounds;
    std::vector<BoundingBox> changedShadowCasterBounds;
    uint64_t nextShadowDirtySerial = 1;
    bool shadowAtlasNeedsFullClear = true;
    bool doorShadowCasterBoundsInitialized = false;
    bool staticShadowCasterBoundsInitialized = false;
    uint64_t cachedDoorShadowCasterRevision = 0;
    uint64_t cachedStaticModelShadowCasterRevision = 0;
    SectorDynamicShadowRenderStats shadowRenderStats;
    RenderTexture2D shadowAtlas{};
    Material shadowMaterial = {};
    Material spotShadowCutoutMaterial = {};
    Material pointShadowMaterial = {};
    Texture2D shadowDefaultTexture = {};
    Texture2D spotShadowCutoutDefaultTexture = {};
    Texture2D pointShadowDefaultTexture = {};
    bool shadowMaterialLoaded = false;
    int shadowMapResolution = DynamicSpotLightShadowMapResolution;
    std::size_t maxDynamicLights = MaxDynamicLights;
    std::size_t maxShadowLightUpdatesPerFrame = 2;
    int shadowLightViewProjectionLoc = -1;
    int spotShadowCutoutLightViewProjectionLoc = -1;
    int shadowAlphaTestLoc = -1;
    int shadowAlphaCutoffLoc = -1;
    int pointShadowLightPositionLoc = -1;
    int pointShadowLightRadiusLoc = -1;
    int pointShadowHemisphereLoc = -1;
    int pointShadowAlphaTestLoc = -1;
    int pointShadowAlphaCutoffLoc = -1;
};

} // namespace game
