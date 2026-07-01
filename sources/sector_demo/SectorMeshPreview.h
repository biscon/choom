#pragma once

#include "engine/assets/AssetManager.h"
#include "sector_demo/SectorCollisionWorld.h"
#include "sector_demo/SectorDynamicPointLightSelection.h"
#include "sector_demo/SectorGeneratedGeometry.h"
#include "sector_demo/SectorMeshTypes.h"
#include "sector_demo/SectorPortalVisibility.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorViewPose.h"

#include <raylib.h>

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

namespace engine {
class World;
}

namespace game {

struct SectorTopologyMap;

class SectorMeshPreview {
public:
    bool Rebuild(
            engine::AssetManager& assets,
            const SectorTopologyMap& map,
            const char* scopeName,
            std::string& error);
    bool RebuildRendererResources(
            engine::AssetManager& assets,
            const SectorTopologyMap& map,
            const char* scopeName,
            std::string& error);
    void Shutdown(engine::AssetManager& assets);
    void ShutdownRendererResources(engine::AssetManager& assets);

    void AdvanceRuntime(float dt);
    void Render(
            engine::AssetManager& assets,
            bool useBakedAmbientOcclusion = true,
            engine::World* runtimeObjectWorld = nullptr);
    void RenderDynamicSpotLightShadowMaps(engine::AssetManager& assets);
    void DrawScene(
            engine::AssetManager& assets,
            bool useBakedAmbientOcclusion = true,
            engine::World* runtimeObjectWorld = nullptr);
    void ApplyEmissiveDecalBloom(engine::AssetManager& assets, RenderTexture2D& sceneTarget);
    void ApplyEmissiveDecalBloomToScene(engine::AssetManager& assets, RenderTexture2D& sceneTarget);

    bool IsReady() const { return initialized; }
    bool IsRendererReady() const { return IsReady(); }
    Vector3 Position() const { return position; }
    const Camera3D& Camera() const { return camera; }
    const Camera3D& RenderCamera() const { return Camera(); }
    SectorViewPose Pose() const;
    SectorViewPose RendererPose() const;
    void ApplyPose(const SectorViewPose& pose);
    void ApplyRendererPose(const SectorViewPose& pose);
    void RefreshDynamicLightSources(const SectorTopologyMap& map);
    size_t SectorCount() const { return sectorCount; }
    size_t BatchCount() const { return meshes.sectorDrawRecords.size(); }
    int TriangleCount() const { return meshes.triangleCount; }
    const SectorGeneratedGeometry& GeneratedGeometry() const { return generatedGeometry; }
    const SectorGeneratedGeometry& RenderedGeometry() const { return GeneratedGeometry(); }
    float AssetProgress(engine::AssetManager& assets) const;
    float RendererAssetProgress(engine::AssetManager& assets) const;
    const char* LightmapStatusText() const;
    const char* RendererLightmapStatusText() const;
    void UpdateVisibilityDebug(
            int preferredStartSectorId = 0,
            float visibilitySeedRadiusWorld = 0.0f,
            bool validateEyeY = false);
    const RuntimePortalVisibilityResult& VisibilityResult() const { return visibilityResult; }
    const std::string& PortalVisibilityDebugText() const { return portalVisibilityDebugText; }
    const std::string& VisibilityDebugText() const { return visibilityDebugText; }
    const std::string& RenderDebugText() const { return renderDebugText; }
    bool DynamicLightingEnabled() const { return dynamicLightingEnabled; }
    void SetDynamicLightingEnabled(bool enabled) { dynamicLightingEnabled = enabled; }
    void ToggleDynamicLightingEnabled() { dynamicLightingEnabled = !dynamicLightingEnabled; }

private:
    static std::string ResolveAssetPath(const std::string& path);
    engine::TextureHandle TextureForId(const std::string& textureId) const;
    void UpdateCamera();
    bool EnsureBloomResources(int sceneWidth, int sceneHeight);
    void UnloadBloomResources();
    void RenderBloomSource(engine::AssetManager& assets);
    void UnloadSkyCylinderMesh();
    void DrawSkyCylinder(const Texture2D& texture);
    void DrawRuntimeBillboards(engine::AssetManager& assets, engine::World& runtimeObjectWorld);
    bool EnsureDynamicSpotLightShadowMapResources();
    void UnloadDynamicSpotLightShadowMapResources();

    SectorMeshBuildResult meshes;
    SectorGeneratedGeometry generatedGeometry;
    RuntimeSectorVisibilityGraph visibilityGraph;
    RuntimePortalVisibilityResult visibilityResult;
    std::string portalVisibilityDebugText;
    std::string visibilityDebugText;
    std::string renderDebugText;
    SectorCollisionWorld visibilityLookupWorld;
    bool visibilityGraphValid = false;
    bool visibilityLookupWorldValid = false;
    std::unordered_map<std::string, engine::TextureHandle> textureHandlesById;
    engine::TextureHandle lightmapTexture = engine::NullTextureHandle();
    engine::TextureHandle skyTextureHandle = engine::NullTextureHandle();
    engine::AssetScopeHandle assetScope = engine::NullAssetScopeHandle();
    Material material = {};
    Texture2D defaultMaterialTexture = {};
    bool materialLoaded = false;
    Mesh skyCylinderMesh = {};
    Mesh skyTopCapMesh = {};
    Material skyMaterial = {};
    Texture2D skyDefaultMaterialTexture = {};
    float skyYawOffsetDegrees = 0.0f;
    Color skyTopCapColor = Color{95, 165, 235, 255};
    bool skyMaterialLoaded = false;
    int useLightmapLoc = -1;
    int useBakedAmbientOcclusionLoc = -1;
    int hasLightmapLoc = -1;
    int alphaTestLoc = -1;
    int alphaCutoffLoc = -1;
    int hasDecalLoc = -1;
    int decalOpacityLoc = -1;
    int decalEmissiveLoc = -1;
    int decalTintLoc = -1;
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
        std::array<int, MaxDynamicSpotLightShadowCasters> locs{};
        locs.fill(-1);
        return locs;
    }();
    int shadowBiasLoc = -1;
    int shadowStrengthLoc = -1;
    int shadowSoftnessLoc = -1;
    int dynamicLightingClampLoc = -1;
    Shader billboardCutoutShader = {};
    int billboardCutoutTextureLoc = -1;
    int billboardCutoutAlphaCutoffLoc = -1;
    bool billboardCutoutShaderLoaded = false;
    std::vector<SectorPreviewDynamicPointLightSource> dynamicPointLightSources;
    std::vector<SectorPreviewDynamicPointLightSource> dynamicPointLightCandidates;
    std::vector<SectorPreviewDynamicPointLightUniform> dynamicPointLights;
    std::vector<int> selectedDynamicPointLightIds;
    std::vector<SectorPreviewDynamicSpotLightShadowCaster> dynamicSpotLightShadowCasters;
    std::vector<SectorPreviewDynamicSpotLightShadowMatrix> dynamicSpotLightShadowMatrices;
    std::array<RenderTexture2D, MaxDynamicSpotLightShadowCasters> dynamicSpotLightShadowMaps{};
    Material dynamicSpotLightShadowMaterial = {};
    Texture2D dynamicSpotLightShadowDefaultTexture = {};
    bool dynamicSpotLightShadowMaterialLoaded = false;
    int dynamicSpotLightShadowLightViewProjectionLoc = -1;
    int dynamicSpotLightShadowAlphaTestLoc = -1;
    int dynamicSpotLightShadowAlphaCutoffLoc = -1;
    float runtimeSeconds = 0.0f;
    bool dynamicLightingEnabled = true;
    bool billboardRenderWarningPrinted = false;
    Material bloomSourceMaterial = {};
    Texture2D bloomDefaultMaterialTexture = {};
    bool bloomSourceMaterialLoaded = false;
    int bloomHasDecalLoc = -1;
    int bloomDecalOpacityLoc = -1;
    int bloomDecalEmissiveLoc = -1;
    int bloomDecalTintLoc = -1;
    int bloomDecalIntensityLoc = -1;
    Shader blurShader = {};
    Shader compositeShader = {};
    int blurTexelSizeLoc = -1;
    int blurDirectionLoc = -1;
    int compositeStrengthLoc = -1;
    int compositeBloomTextureLoc = -1;
    RenderTexture2D bloomSceneCopy = {};
    RenderTexture2D bloomSource = {};
    RenderTexture2D bloomBlurA = {};
    RenderTexture2D bloomBlurB = {};
    int bloomSceneWidth = 0;
    int bloomSceneHeight = 0;
    int bloomTargetWidth = 0;
    int bloomTargetHeight = 0;
    int lightmapStatus = 0;
    bool initialized = false;
    size_t sectorCount = 0;

    Camera3D camera = {};
    Vector3 position = {};
    float yawRadians = 0.0f;
    float pitchRadians = 0.0f;
};

} // namespace game
