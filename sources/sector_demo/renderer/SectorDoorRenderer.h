#pragma once

#include "sector_demo/SectorDoorRuntime.h"
#include "sector_demo/SectorDynamicPointLightSelection.h"
#include "sector_demo/SectorMeshTypes.h"
#include "sector_demo/renderer/SectorDynamicLightingRenderer.h"
#include "sector_demo/renderer/SectorFog.h"
#include "sector_demo/renderer/SectorStaticModelRenderer.h"
#include "sector_demo/renderer/SectorStaticSpecularLighting.h"

#include <raylib.h>

#include <array>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace engine {
class AssetManager;
class World;
}

namespace game {

struct SectorBakedObjectLightProbeRuntimeData;
struct SectorDoorShadowCaster;
struct RuntimePortalVisibilityResult;
struct SectorTopologyMap;

struct SectorRuntimeDoorLightingContext {
    const SectorBakedObjectLightProbeRuntimeData* objectLightProbes = nullptr;
    const SectorTopologyMap* mapForFallback = nullptr;
    uint64_t revision = 0;
};

struct SectorDoorRenderStats {
    size_t considered = 0;
    size_t drawn = 0;
    size_t skipped = 0;
};

struct SectorDoorResolvedMaterial {
    const Texture2D* albedo = nullptr;
    const Texture2D* normal = nullptr;
    float normalStrength = 1.0f;
    float metallicFactor = 0.0f;
    float roughnessFactor = 0.8f;
};

struct SectorDoorMaterialResolver {
    using ResolveFn = SectorDoorResolvedMaterial (*)(
            void* userData,
            engine::AssetManager& assets,
            const std::string& materialId);

    void* userData = nullptr;
    ResolveFn resolve = nullptr;
};

struct SectorDoorDynamicLightContext {
    bool enabled = true;
    float runtimeSeconds = 0.0f;
    const std::vector<SectorPreviewDynamicPointLightUniform>* selectedLights = nullptr;
    SectorPreviewDynamicSpotLightShadowUniforms shadowUniforms{};
    SectorDynamicShadowMapTextures shadowMaps{};
};

struct SectorDoorDrawContext {
    engine::AssetManager* assets = nullptr;
    engine::World* runtimeObjectWorld = nullptr;
    SectorRuntimeDoorLightingContext lighting;
    SectorDoorDynamicLightContext dynamicLighting;
    SectorFogRenderContext fog;
    SectorDoorMaterialResolver materialResolver;
    Camera3D camera = {};
    SectorPbrContributionSettings pbr;
    const SectorStaticSpecularLightState* staticSpecularLights = nullptr;
    const RuntimePortalVisibilityResult* visibility = nullptr;
    const TextureCubemap* environment = nullptr;
    float environmentExposure = 0.15f;
    Vector3 environmentCapturePosition = {};
    Vector3 environmentInfluenceCenter = {};
    Vector3 environmentHalfExtents = {1.0f, 1.0f, 1.0f};
    float environmentYaw = 0.0f;
    float environmentMaxLod = 8.0f;
    bool environmentBoxProjection = false;
    bool staticSpecularEligible = false;
    const Texture2D* defaultMaterialTexture = nullptr;
    std::string* renderDebugText = nullptr;
};

struct SectorDoorOpaqueShaderLocations {
    int texture = -1;
    int normalTexture = -1;
    int hasNormalMap = -1;
    int normalStrength = -1;
    int metallicFactor = -1;
    int roughnessFactor = -1;
    int cameraPosition = -1;
    int hasEnvironment = -1;
    int environmentExposure = -1;
    int indirectDiffuseScale = -1;
    int environmentSpecularScale = -1;
    int environmentBoxProjection = -1;
    int environmentCapturePosition = -1;
    int environmentInfluenceCenter = -1;
    int environmentHalfExtents = -1;
    int environmentYaw = -1;
    int environmentMaxLod = -1;
    int pbrDiagnosticMode = -1;
    int useStaticSpecularLighting = -1;
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
    int dynamicLightProfiles = -1;
    int dynamicLightProfileParameters = -1;
    int flashlightCookie = -1;
    int hasPointShadows = -1;
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
    int tint = -1;
    SectorStaticSpecularShaderLocations staticSpecular;
    SectorFogShaderLocations fog;
};

class SectorDoorRenderer {
public:
    struct DoorMeshCacheEntry {
        Mesh mesh = {};
        SectorDoorSlabMeshData meshData;
        float width = 0.0f;
        float height = 0.0f;
        float thickness = 0.0f;
        SectorDoorFaceUvSet faceUvs;
        std::vector<Vector3> staticLightingValues;
        Matrix staticLightingModel = {};
        int staticLightingSectorId = -1;
        uint64_t staticLightingRevision = 0;
        bool staticLightingValid = false;
        bool seenThisFrame = false;
    };

    void ReserveRuntimeDoorCapacity(size_t capacity);
    bool LoadOpaqueResources();
    void ShutdownOpaqueResources();
    void Draw(const SectorDoorDrawContext& context);
    void PrepareShadowRenderContext(
            SectorDynamicSpotLightShadowRenderContext& context,
            engine::World* runtimeObjectWorld);
    void ClearPreparedShadowCasters();
    void UnloadDoorMeshes();

    bool HasCachedDoorMeshes() const { return !doorMeshCache.empty(); }
    bool HasOpaqueResources() const { return opaqueShaderLoaded || opaqueMaterialLoaded; }
    bool IsOpaqueReady() const
    {
        return opaqueShaderLoaded && opaqueMaterialLoaded && opaqueMaterial.shader.id != 0;
    }
    Material& OpaqueMaterial() { return opaqueMaterial; }
    const Texture2D& OpaqueDefaultMaterialTexture() const { return opaqueDefaultMaterialTexture; }
    const SectorDoorOpaqueShaderLocations& OpaqueShaderLocations() const { return opaqueShaderLocations; }
    const SectorDoorRenderStats& RenderStats() const { return renderStats; }

private:
    void ResetOpaqueShaderLocations();
    void PrepareRuntimeDoorMeshes(
            engine::AssetManager& assets,
            engine::World& runtimeObjectWorld);
    const std::vector<SectorDoorShadowCaster>& ShadowCasters() const { return runtimeDoorShadowCasters; }
    DoorMeshCacheEntry* FindMutableDoorMesh(int placedObjectId);
    const DoorMeshCacheEntry* FindDoorMesh(int placedObjectId) const;
    const Mesh* ResolveDoorShadowCasterMesh(
            const SectorDoorShadowCaster& caster,
            float& outWidth,
            float& outHeight) const;
    static const Mesh* ResolveDoorShadowCasterMesh(
            void* userData,
            const SectorDoorShadowCaster& caster,
            float& outWidth,
            float& outHeight);

    std::unordered_map<int, DoorMeshCacheEntry> doorMeshCache;
    std::vector<SectorDoorShadowCaster> runtimeDoorShadowCasters;
    std::vector<SectorDoorModelShadowCaster> runtimeDoorModelShadowCasters;
    SectorDoorShadowCasterRevisionState shadowCasterRevisionState;
    Shader opaqueShader = {};
    SectorDoorOpaqueShaderLocations opaqueShaderLocations;
    bool opaqueShaderLoaded = false;
    Material opaqueMaterial = {};
    Texture2D opaqueDefaultMaterialTexture = {};
    bool opaqueMaterialLoaded = false;
    SectorDoorRenderStats renderStats;
};

} // namespace game
