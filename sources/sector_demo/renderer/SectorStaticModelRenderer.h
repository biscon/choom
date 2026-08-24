#pragma once

#include "engine/assets/AssetHandles.h"
#include "engine/assets/ModelAssets.h"
#include "engine/render/HdrEffectPolicy.h"
#include "sector_demo/renderer/SectorDynamicLightingRenderer.h"
#include "sector_demo/renderer/SectorFog.h"
#include "sector_demo/renderer/SectorStaticSpecularLighting.h"
#include "sector_demo/renderer/SectorPbrEnvironment.h"
#include "sector_demo/SectorStaticModelLightmap.h"
#include "sector_demo/SectorStaticModelShadow.h"

#include <raylib.h>

#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>

namespace engine {
class AssetManager;
class World;
struct AnimatedModelInstance;
struct ModelAsset;
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

enum class SectorPbrDiagnosticMode : int {
    Full = 0,
    BaseColor,
    DirectDiffuse,
    DirectSpecular,
    IndirectDiffuse,
    EnvironmentSpecular,
    Emissive,
    MaterialOcclusion,
    MetallicRoughness,
    ShadingNormal,
    TangentNormal,
    Count
};

const char* SectorPbrDiagnosticModeName(SectorPbrDiagnosticMode mode);

enum class SectorPbrLightingPath : int {
    WorldStatic,
    WorldDynamic,
    Viewmodel,
    ViewmodelAttachment
};

const char* SectorPbrLightingPathName(SectorPbrLightingPath path);

enum class SectorPbrIndirectSource : int {
    SectorAmbient,
    ObjectProbe,
    StaticLightmap
};

const char* SectorPbrIndirectSourceName(SectorPbrIndirectSource source);

struct SectorPbrContributionSettings {
    SectorPbrDiagnosticMode diagnosticMode = SectorPbrDiagnosticMode::Full;
    float worldIndirectDiffuseScale = 1.0f;
    float worldEnvironmentSpecularScale = 1.0f;
};

inline SectorPbrContributionSettings NormalizeSectorPbrContributionSettings(
        SectorPbrContributionSettings settings)
{
    const int mode = static_cast<int>(settings.diagnosticMode);
    if (mode < 0 || mode >= static_cast<int>(SectorPbrDiagnosticMode::Count)) {
        settings.diagnosticMode = SectorPbrDiagnosticMode::Full;
    }
    settings.worldIndirectDiffuseScale =
            std::isfinite(settings.worldIndirectDiffuseScale)
            ? std::clamp(settings.worldIndirectDiffuseScale, 0.0f, 1.0f)
            : 1.0f;
    settings.worldEnvironmentSpecularScale =
            std::isfinite(settings.worldEnvironmentSpecularScale)
            ? std::clamp(settings.worldEnvironmentSpecularScale, 0.0f, 1.0f)
            : 1.0f;
    return settings;
}

struct SectorPbrDrawState {
    SectorPbrLightingPath path = SectorPbrLightingPath::WorldStatic;
    SectorPbrIndirectSource indirectSource = SectorPbrIndirectSource::SectorAmbient;
    SectorPbrDiagnosticMode diagnosticMode = SectorPbrDiagnosticMode::Full;
    float indirectDiffuseScale = 1.0f;
    float environmentSpecularScale = 1.0f;
    float environmentExposure = 0.0f;
    float outputBrightnessMultiplier = 1.0f;
    bool useObjectProbe = false;
    bool useVerticalObjectProbe = false;
    bool environmentActive = false;
    bool materialOverrideActive = false;
    bool staticSpecularEligible = false;
};

inline float SanitizeSectorPbrNonnegative(float value, float fallback = 0.0f)
{
    return std::isfinite(value) ? std::max(value, 0.0f) : fallback;
}

inline Vector3 SanitizeSectorPbrNonnegative(Vector3 value)
{
    return Vector3{
            SanitizeSectorPbrNonnegative(value.x),
            SanitizeSectorPbrNonnegative(value.y),
            SanitizeSectorPbrNonnegative(value.z)};
}

inline engine::ModelMaterialAsset NormalizeSectorPbrMaterial(
        engine::ModelMaterialAsset material)
{
    material.baseColorFactor.x = std::isfinite(material.baseColorFactor.x)
            ? std::clamp(material.baseColorFactor.x, 0.0f, 1.0f) : 1.0f;
    material.baseColorFactor.y = std::isfinite(material.baseColorFactor.y)
            ? std::clamp(material.baseColorFactor.y, 0.0f, 1.0f) : 1.0f;
    material.baseColorFactor.z = std::isfinite(material.baseColorFactor.z)
            ? std::clamp(material.baseColorFactor.z, 0.0f, 1.0f) : 1.0f;
    material.baseColorFactor.w = std::isfinite(material.baseColorFactor.w)
            ? std::clamp(material.baseColorFactor.w, 0.0f, 1.0f) : 1.0f;
    material.emissiveFactor = SanitizeSectorPbrNonnegative(
            material.emissiveFactor);
    material.emissiveStrength = std::isfinite(material.emissiveStrength)
            ? std::clamp(material.emissiveStrength, 0.0f,
                    engine::Rgba16fMaximumFinite)
            : 1.0f;
    material.metallicFactor = std::isfinite(material.metallicFactor)
            ? std::clamp(material.metallicFactor, 0.0f, 1.0f) : 0.0f;
    material.roughnessFactor = std::isfinite(material.roughnessFactor)
            ? std::clamp(material.roughnessFactor, 0.045f, 1.0f) : 1.0f;
    material.normalScale = std::isfinite(material.normalScale)
            ? std::max(material.normalScale, 0.0f) : 1.0f;
    material.occlusionStrength = std::isfinite(material.occlusionStrength)
            ? std::clamp(material.occlusionStrength, 0.0f, 1.0f) : 1.0f;
    return material;
}

inline SectorPbrDrawState BuildSectorPbrDrawState(
        SectorPbrLightingPath path,
        bool validObjectProbe,
        bool hasStaticLightmap,
        bool staticSpecularBakeCurrent,
        bool environmentActive,
        float environmentExposure,
        float outputBrightnessMultiplier,
        bool materialOverrideActive,
        SectorPbrContributionSettings settings)
{
    settings = NormalizeSectorPbrContributionSettings(settings);
    const bool worldPath = path == SectorPbrLightingPath::WorldStatic
            || path == SectorPbrLightingPath::WorldDynamic;
    SectorPbrDrawState state;
    state.path = path;
    state.diagnosticMode = settings.diagnosticMode;
    state.indirectDiffuseScale = worldPath
            ? settings.worldIndirectDiffuseScale
            : 1.0f;
    state.environmentSpecularScale = worldPath
            ? settings.worldEnvironmentSpecularScale
            : 1.0f;
    state.environmentExposure = environmentActive
            ? SanitizeSectorPbrNonnegative(environmentExposure)
            : 0.0f;
    state.outputBrightnessMultiplier = worldPath
            ? 1.0f
            : SanitizeSectorPbrNonnegative(outputBrightnessMultiplier, 1.0f);
    state.environmentActive = environmentActive;
    state.materialOverrideActive = !worldPath && materialOverrideActive;
    state.useObjectProbe = validObjectProbe && !hasStaticLightmap;
    state.useVerticalObjectProbe = state.useObjectProbe;
    state.indirectSource = state.useObjectProbe
            ? SectorPbrIndirectSource::ObjectProbe
            : (hasStaticLightmap
                    ? SectorPbrIndirectSource::StaticLightmap
                    : SectorPbrIndirectSource::SectorAmbient);
    state.staticSpecularEligible = staticSpecularBakeCurrent
            && (state.useObjectProbe || hasStaticLightmap);
    return state;
}

struct SectorPbrDrawDiagnostics {
    bool valid = false;
    int placedObjectId = -1;
    int materialIndex = -1;
    engine::ModelHandle model = engine::NullModelHandle();
    SectorPbrDrawState state;
    engine::ModelMaterialAsset material;
    SectorStaticSpecularLightContext staticSpecularLights;
};

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

struct SectorViewmodelLightingContext {
    float environmentExposure = 0.15f;
    float brightnessMultiplier = 1.0f;
    bool materialOverrideEnabled = false;
    float metallicFactor = 0.0f;
    float roughnessFactor = 1.0f;
    bool useMetallicRoughnessTexture = true;
};

inline void ApplySectorViewmodelMaterialOverride(
        const SectorViewmodelLightingContext& lighting,
        float& metallicFactor,
        float& roughnessFactor,
        bool& hasMetallicTexture,
        bool& hasRoughnessTexture)
{
    if (!lighting.materialOverrideEnabled) {
        return;
    }
    metallicFactor = lighting.metallicFactor;
    roughnessFactor = lighting.roughnessFactor;
    if (!lighting.useMetallicRoughnessTexture) {
        hasMetallicTexture = false;
        hasRoughnessTexture = false;
    }
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
    void ReserveShadowCasterCapacity(size_t capacity);
    void PrepareShadowRenderContext(
            SectorDynamicSpotLightShadowRenderContext& context,
            engine::World* runtimeObjectWorld);
    void ClearPreparedShadowCasters();

    void Draw(
            engine::AssetManager& assets,
            engine::World& runtimeObjectWorld,
            const Camera3D& camera,
            const SectorBillboardDynamicLightContext& dynamicLightContext,
            const SectorStaticSpecularLightState& staticSpecularLights,
            bool surfaceLightmapBakeCurrent,
            bool objectProbeBakeCurrent,
            const SectorFogRenderContext& fogContext,
            const RuntimePortalVisibilityResult& visibility,
            const std::vector<engine::TextureHandle>& lightmapTextures,
            const TextureCubemap* environment,
            bool useBakedAmbientOcclusion,
            std::string& renderDebugText,
            bool staticCaptureOnly = false);

    void DrawViewmodel(
            const engine::ModelAsset& asset,
            engine::AnimatedModelInstance& instance,
            const Camera3D& camera,
            Matrix transform,
            const engine::ModelAsset* attachmentAsset,
            Matrix attachmentTransform,
            const SectorBillboardDynamicLightContext& dynamicLightContext,
            const SectorStaticSpecularLightContext& staticSpecularLights,
            bool objectProbeBakeCurrent,
            const TextureCubemap* environment,
            const BakedObjectLightingVerticalSample& ambientLighting,
            const SectorViewmodelLightingContext& lighting,
            const SectorViewmodelLightingContext& attachmentLighting);

    bool IsLoaded() const { return shaderLoaded; }
    void SetPbrContributionSettings(SectorPbrContributionSettings settings)
    {
        contributionSettings = NormalizeSectorPbrContributionSettings(settings);
    }
    SectorPbrContributionSettings PbrContributionSettings() const
    {
        return contributionSettings;
    }
    void SetPbrDiagnosticSelectedObjectId(int objectId)
    {
        diagnosticSelectedObjectId = objectId;
    }
    void SetEnvironmentProjection(SectorPbrEnvironmentSelection selection)
    {
        environmentSelection = selection;
    }
    const SectorPbrDrawDiagnostics& WorldPbrDiagnostics() const
    {
        return worldDiagnostics;
    }
    const SectorPbrDrawDiagnostics& ViewmodelPbrDiagnostics() const
    {
        return viewmodelDiagnostics;
    }

private:
    Shader shader = {};
    struct CachedModel {
        engine::ModelHandle handle = engine::NullModelHandle();
        int lightmapModelIndex = -1;
        std::vector<Mesh> meshes;
    };

    void ClearCachedModels();
    void UploadPbrDrawState(const SectorPbrDrawState& state);
    void UploadPbrMaterialTransferState(
            const engine::ModelMaterialAsset& material);
    void RecordPbrDiagnostics(
            SectorPbrDrawDiagnostics& diagnostics,
            int placedObjectId,
            engine::ModelHandle model,
            int materialIndex,
            const SectorPbrDrawState& state,
            const engine::ModelMaterialAsset& material,
            const SectorStaticSpecularLightContext& staticSpecularLights);
    bool DrawWorldDynamicModel(
            const engine::ModelAsset& modelAsset,
            const Model& model,
            engine::ModelHandle modelHandle,
            Matrix modelTransform,
            int placedObjectId,
            int receiverSectorId,
            const SectorReceiverBounds& receiverBounds,
            Vector3 containingSectorAmbient,
            float environmentExposure,
            const BakedObjectLightingVerticalSample& lighting,
            const SectorBillboardDynamicLightContext& dynamicLightContext,
            const SectorStaticSpecularLightState& staticSpecularLights,
            const RuntimePortalVisibilityResult& visibility,
            bool objectProbeBakeCurrent,
            const TextureCubemap* environment,
            bool allowSkinning,
            const engine::AnimatedModelInstance* animatedInstance = nullptr,
            const std::vector<Matrix>* meshNodeMatrices = nullptr,
            float opacity = 1.0f);
    const CachedModel* FindCachedModel(
            engine::ModelHandle handle,
            int lightmapModelIndex) const;

    SectorStaticModelLightmapData lightmapData;
    std::vector<CachedModel> cachedModels;
    SectorStaticModelShadowCasterCollection shadowCasterCollection;
    int baseColorFactorLoc = -1;
    int emissiveFactorLoc = -1;
    int emissiveStrengthLoc = -1;
    int metallicFactorLoc = -1;
    int roughnessFactorLoc = -1;
    int normalScaleLoc = -1;
    int occlusionStrengthLoc = -1;
    int modelOpacityLoc = -1;
    int hasBaseColorTextureLoc = -1;
    int hasMetallicTextureLoc = -1;
    int hasNormalTextureLoc = -1;
    int hasRoughnessTextureLoc = -1;
    int hasOcclusionTextureLoc = -1;
    int hasEmissiveTextureLoc = -1;
    int cameraPositionLoc = -1;
    int environmentExposureLoc = -1;
    int outputBrightnessMultiplierLoc = -1;
    int hasEnvironmentLoc = -1;
    int environmentTextureLoc = -1;
    int baseColorHardwareSrgbLoc = -1;
    int emissiveHardwareSrgbLoc = -1;
    int diagnosticModeLoc = -1;
    int indirectDiffuseScaleLoc = -1;
    int environmentSpecularScaleLoc = -1;
    int environmentBoxProjectionLoc = -1;
    int environmentCapturePositionLoc = -1;
    int environmentInfluenceCenterLoc = -1;
    int environmentHalfExtentsLoc = -1;
    int environmentYawLoc = -1;
    int environmentMaxLodLoc = -1;
    int environmentIntensityLoc = -1;
    int lightmapScaleBiasLoc = -1;
    int hasStaticLightmapLoc = -1;
    int useBakedAmbientOcclusionLoc = -1;
    int containingSectorAmbientLoc = -1;
    int useObjectProbeLightingLoc = -1;
    std::array<int, 6> objectAmbientCubeLocs = {-1, -1, -1, -1, -1, -1};
    std::array<int, 6> objectAmbientCubeUpperLocs = {-1, -1, -1, -1, -1, -1};
    int objectAmbientCubeLowerHeightLoc = -1;
    int objectAmbientCubeUpperHeightLoc = -1;
    int useVerticalObjectProbeLightingLoc = -1;
    int useSkinningLoc = -1;
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
    int dynamicLightSpotShadowRightLoc = -1;
    int dynamicLightSpotShadowProjectionLoc = -1;
    int hasPointShadowsLoc = -1;
    SectorStaticSpecularShaderLocations staticSpecularLocations;
    int useStaticSpecularLightingLoc = -1;
    int dynamicLightShadowSlotsLoc = -1;
    std::array<int, MaxDynamicSpotLightShadowCasters> shadowLightMatrixLocs = [] {
        std::array<int, MaxDynamicSpotLightShadowCasters> locations{};
        locations.fill(-1);
        return locations;
    }();
    int shadowBiasLoc = -1;
    int shadowStrengthLoc = -1;
    int shadowSoftnessLoc = -1;
    int shadowAtlasTilesPerRowLoc = -1;
    int shadowMap0Loc = -1;
    int shadowMap1Loc = -1;
    SectorFogShaderLocations fogShaderLocations;
    SectorPbrContributionSettings contributionSettings;
    SectorPbrEnvironmentSelection environmentSelection;
    SectorPbrDrawDiagnostics worldDiagnostics;
    SectorPbrDrawDiagnostics viewmodelDiagnostics;
    int diagnosticSelectedObjectId = -1;
    bool shaderLoaded = false;
    bool warningPrinted = false;
};

} // namespace game
