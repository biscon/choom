#pragma once

#include "engine/assets/AssetHandles.h"
#include "sector_demo/renderer/SectorDynamicLightingRenderer.h"
#include "sector_demo/renderer/SectorFog.h"
#include "sector_demo/SectorStaticModelLightmap.h"

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

struct RuntimePortalVisibilityResult;

constexpr size_t SectorStaticModelMaterialMapCount = 12;
constexpr int SectorStaticModelLightmapMaterialMap =
        MATERIAL_MAP_HEIGHT;
constexpr int SectorStaticModelEnvironmentMaterialMap =
        MATERIAL_MAP_CUBEMAP;
constexpr int SectorStaticModelShadowMap0MaterialMap =
        MATERIAL_MAP_BRDF;
constexpr int SectorStaticModelShadowMap1MaterialMap =
        11;

inline void ConfigureSectorStaticModelAuxiliaryMaterialMaps(
        std::array<MaterialMap, SectorStaticModelMaterialMapCount>& maps,
        const Texture2D* lightmap,
        bool hasStaticLightmap,
        const TextureCubemap* environment,
        const Texture2D* shadowMap0,
        const Texture2D* shadowMap1)
{
    maps[SectorStaticModelLightmapMaterialMap].texture =
            hasStaticLightmap
                    && lightmap != nullptr
                    && lightmap->id != 0
            ? *lightmap
            : Texture2D{};
    maps[SectorStaticModelEnvironmentMaterialMap].texture =
            environment != nullptr && environment->id != 0
            ? *environment
            : Texture2D{};
    maps[SectorStaticModelShadowMap0MaterialMap].texture =
            shadowMap0 != nullptr && shadowMap0->id != 0
            ? *shadowMap0
            : Texture2D{};
    maps[SectorStaticModelShadowMap1MaterialMap].texture =
            shadowMap1 != nullptr && shadowMap1->id != 0
            ? *shadowMap1
            : Texture2D{};
}

class SectorStaticModelRenderer {
public:
    bool Load();
    void Shutdown();
    void ResetDebugState();
    void SetLightmapData(SectorStaticModelLightmapData data);
    void FinalizeResources(
            engine::AssetManager& assets,
            engine::World& runtimeObjectWorld);

    void Draw(
            engine::AssetManager& assets,
            engine::World& runtimeObjectWorld,
            const Camera3D& camera,
            const SectorBillboardDynamicLightContext& dynamicLightContext,
            const SectorFogRenderContext& fogContext,
            const RuntimePortalVisibilityResult& visibility,
            const std::vector<engine::TextureHandle>& lightmapTextures,
            const TextureCubemap* environment,
            bool useBakedAmbientOcclusion,
            std::string& renderDebugText);

    bool IsLoaded() const { return shaderLoaded; }

private:
    Shader shader = {};
    struct CachedModel {
        engine::ModelHandle handle = engine::NullModelHandle();
        int lightmapModelIndex = -1;
        std::vector<Mesh> meshes;
    };

    void ClearCachedModels();
    const CachedModel* FindCachedModel(
            engine::ModelHandle handle,
            int lightmapModelIndex) const;

    SectorStaticModelLightmapData lightmapData;
    std::vector<CachedModel> cachedModels;
    int baseColorFactorLoc = -1;
    int emissiveFactorLoc = -1;
    int metallicFactorLoc = -1;
    int roughnessFactorLoc = -1;
    int normalScaleLoc = -1;
    int occlusionStrengthLoc = -1;
    int hasBaseColorTextureLoc = -1;
    int hasMetallicTextureLoc = -1;
    int hasNormalTextureLoc = -1;
    int hasRoughnessTextureLoc = -1;
    int hasOcclusionTextureLoc = -1;
    int hasEmissiveTextureLoc = -1;
    int cameraPositionLoc = -1;
    int environmentExposureLoc = -1;
    int hasEnvironmentLoc = -1;
    int environmentTextureLoc = -1;
    int lightmapScaleBiasLoc = -1;
    int hasStaticLightmapLoc = -1;
    int useBakedAmbientOcclusionLoc = -1;
    int containingSectorAmbientLoc = -1;
    int lightmapTextureLoc = -1;
    int dynamicLightCountLoc = -1;
    int dynamicLightPositionsLoc = -1;
    int dynamicLightColorsLoc = -1;
    int dynamicLightRadiiLoc = -1;
    int dynamicLightIntensitiesLoc = -1;
    int dynamicLightTypesLoc = -1;
    int dynamicLightDirectionsLoc = -1;
    int dynamicLightInnerConeCosLoc = -1;
    int dynamicLightOuterConeCosLoc = -1;
    int dynamicLightShadowSlotsLoc = -1;
    std::array<int, MaxDynamicSpotLightShadowCasters> shadowLightMatrixLocs = [] {
        std::array<int, MaxDynamicSpotLightShadowCasters> locations{};
        locations.fill(-1);
        return locations;
    }();
    int shadowBiasLoc = -1;
    int shadowStrengthLoc = -1;
    int shadowSoftnessLoc = -1;
    int shadowMap0Loc = -1;
    int shadowMap1Loc = -1;
    int dynamicLightingClampLoc = -1;
    SectorFogShaderLocations fogShaderLocations;
    bool shaderLoaded = false;
    bool warningPrinted = false;
};

} // namespace game
