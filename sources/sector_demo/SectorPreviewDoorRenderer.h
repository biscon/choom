#pragma once

#include "sector_demo/SectorDoorRuntime.h"
#include "sector_demo/SectorDynamicPointLightSelection.h"
#include "sector_demo/SectorMeshTypes.h"
#include "sector_demo/SectorPreviewDynamicLighting.h"

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
struct SectorTopologyMap;

enum class SectorDoorLightingDebugMode {
    Normal = 0,
    AlbedoOnly = 1,
    BakedOnly = 2,
    DynamicOnly = 3,
    NormalVisualize = 4,
    FlatColorNoTexture = 5
};

const char* SectorDoorLightingDebugModeName(SectorDoorLightingDebugMode mode);

struct SectorRuntimeDoorLightingContext {
    const SectorBakedObjectLightProbeRuntimeData* objectLightProbes = nullptr;
    const SectorTopologyMap* mapForFallback = nullptr;
};

struct SectorPreviewDoorRenderStats {
    size_t considered = 0;
    size_t drawn = 0;
    size_t skipped = 0;
};

struct SectorPreviewDoorTextureResolver {
    using ResolveFn = const Texture2D* (*)(
            void* userData,
            engine::AssetManager& assets,
            const std::string& textureId);

    void* userData = nullptr;
    ResolveFn resolve = nullptr;
};

struct SectorPreviewDoorMeshResolver {
    using ResolveFn = const Mesh* (*)(
            void* userData,
            const SectorDoorShadowCaster& caster,
            float& outWidth,
            float& outHeight);

    void* userData = nullptr;
    ResolveFn resolve = nullptr;
};

struct SectorPreviewDoorPreparationContext {
    engine::World* runtimeObjectWorld = nullptr;
    std::vector<SectorDoorShadowCaster>* shadowCasters = nullptr;
};

struct SectorPreviewDoorResourceContext {
    Shader* opaqueShader = nullptr;
    Material* opaqueMaterial = nullptr;
    Texture2D* defaultMaterialTexture = nullptr;
    bool* opaqueShaderLoaded = nullptr;
    bool* opaqueMaterialLoaded = nullptr;
};

struct SectorPreviewDoorDynamicLightContext {
    bool enabled = true;
    float runtimeSeconds = 0.0f;
    const std::vector<SectorPreviewDynamicPointLightUniform>* selectedLights = nullptr;
    SectorPreviewDynamicSpotLightShadowUniforms shadowUniforms{};
    SectorPreviewDynamicShadowMapTextures shadowMaps{};
    float lightingClamp = 4.0f;
};

struct SectorPreviewDoorDrawContext {
    engine::AssetManager* assets = nullptr;
    engine::World* runtimeObjectWorld = nullptr;
    SectorRuntimeDoorLightingContext lighting;
    SectorPreviewDoorDynamicLightContext dynamicLighting;
    SectorPreviewDoorTextureResolver textureResolver;
    const Texture2D* defaultMaterialTexture = nullptr;
    std::string* renderDebugText = nullptr;
};

struct SectorPreviewDoorShadowCasterContext {
    const std::vector<SectorDoorShadowCaster>* shadowCasters = nullptr;
    SectorPreviewDoorTextureResolver textureResolver;
    SectorPreviewDoorMeshResolver meshResolver;
};

struct SectorPreviewDoorOpaqueShaderLocations {
    int texture = -1;
    int dynamicLightCount = -1;
    int dynamicLightPositions = -1;
    int dynamicLightColors = -1;
    int dynamicLightRadii = -1;
    int dynamicLightIntensities = -1;
    int dynamicLightTypes = -1;
    int dynamicLightDirections = -1;
    int dynamicLightInnerConeCos = -1;
    int dynamicLightOuterConeCos = -1;
    int dynamicLightShadowSlots = -1;
    std::array<int, MaxDynamicSpotLightShadowCasters> shadowLightMatrices = [] {
        std::array<int, MaxDynamicSpotLightShadowCasters> locs{};
        locs.fill(-1);
        return locs;
    }();
    int shadowBias = -1;
    int shadowStrength = -1;
    int shadowSoftness = -1;
    int dynamicLightingClamp = -1;
    int debugMode = -1;
    int tint = -1;
};

class SectorPreviewDoorRenderer {
public:
    struct DoorMeshCacheEntry {
        Mesh mesh = {};
        SectorDoorSlabMeshData meshData;
        float width = 0.0f;
        float height = 0.0f;
        float thickness = 0.0f;
        SectorDoorFaceUvSet faceUvs;
        std::vector<Color> staticLightingColors;
        bool seenThisFrame = false;
    };

    void ReserveRuntimeDoorCapacity(size_t capacity);
    bool LoadOpaqueResources();
    void ShutdownOpaqueResources();
    void PrepareRuntimeDoorMeshes(engine::World& runtimeObjectWorld);
    void Draw(const SectorPreviewDoorDrawContext& context);
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
    const SectorPreviewDoorOpaqueShaderLocations& OpaqueShaderLocations() const { return opaqueShaderLocations; }
    SectorDoorLightingDebugMode DoorLightingDebugMode() const { return doorLightingDebugMode; }
    void SetDoorLightingDebugMode(SectorDoorLightingDebugMode mode) { doorLightingDebugMode = mode; }
    int DoorLightingDebugModeShaderValue() const { return static_cast<int>(doorLightingDebugMode); }
    const SectorPreviewDoorRenderStats& RenderStats() const { return renderStats; }
    DoorMeshCacheEntry* FindMutableDoorMesh(int placedObjectId);
    const DoorMeshCacheEntry* FindDoorMesh(int placedObjectId) const;
    const std::vector<SectorDoorShadowCaster>& ShadowCasters() const { return runtimeDoorShadowCasters; }

    const Mesh* ResolveDoorShadowCasterMesh(
            const SectorDoorShadowCaster& caster,
            float& outWidth,
            float& outHeight) const;
    static const Mesh* ResolveDoorShadowCasterMesh(
            void* userData,
            const SectorDoorShadowCaster& caster,
            float& outWidth,
            float& outHeight);

private:
    void ResetOpaqueShaderLocations();

    std::unordered_map<int, DoorMeshCacheEntry> doorMeshCache;
    std::vector<SectorDoorShadowCaster> runtimeDoorShadowCasters;
    Shader opaqueShader = {};
    SectorPreviewDoorOpaqueShaderLocations opaqueShaderLocations;
    bool opaqueShaderLoaded = false;
    Material opaqueMaterial = {};
    Texture2D opaqueDefaultMaterialTexture = {};
    bool opaqueMaterialLoaded = false;
    SectorDoorLightingDebugMode doorLightingDebugMode = SectorDoorLightingDebugMode::Normal;
    SectorPreviewDoorRenderStats renderStats;
};

} // namespace game
