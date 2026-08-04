#pragma once

#include "engine/assets/AssetManager.h"
#include "sector_demo/SectorCollisionWorld.h"
#include "sector_demo/SectorDynamicPointLightSelection.h"
#include "sector_demo/SectorGeneratedGeometry.h"
#include "sector_demo/SectorMeshTypes.h"
#include "sector_demo/SectorPortalVisibility.h"
#include "sector_demo/renderer/SectorBillboardRenderer.h"
#include "sector_demo/renderer/SectorBloomRenderer.h"
#include "sector_demo/renderer/SectorDoorRenderer.h"
#include "sector_demo/renderer/SectorDynamicLightingRenderer.h"
#include "sector_demo/renderer/SectorSkyRenderer.h"
#include "sector_demo/renderer/SectorPbrEnvironment.h"
#include "sector_demo/renderer/SectorStaticModelRenderer.h"
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
struct SectorBakedObjectLightProbeRuntimeData;

class SectorMeshRenderer {
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
    void FinalizeRuntimeObjectResources(
            engine::AssetManager& assets,
            engine::World& runtimeObjectWorld);
    void Render(
            engine::AssetManager& assets,
            bool useBakedAmbientOcclusion = true,
            engine::World* runtimeObjectWorld = nullptr,
            SectorRuntimeDoorLightingContext doorLighting = {});
    void RenderDynamicSpotLightShadowMaps(
            engine::AssetManager& assets,
            engine::World* runtimeObjectWorld = nullptr);
    void DrawScene(
            engine::AssetManager& assets,
            bool useBakedAmbientOcclusion = true,
            engine::World* runtimeObjectWorld = nullptr,
            SectorRuntimeDoorLightingContext doorLighting = {});
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
            bool validateEyeY = false,
            const std::vector<RuntimePortalDynamicBlocker>* dynamicPortalBlockers = nullptr,
            engine::World* runtimeObjectWorld = nullptr);
    const RuntimePortalVisibilityResult& VisibilityResult() const { return visibilityResult; }
    const std::string& PortalVisibilityDebugText() const { return portalVisibilityDebugText; }
    const std::string& VisibilityDebugText() const { return visibilityDebugText; }
    const std::string& RenderDebugText() const { return renderDebugText; }
    bool DynamicLightingEnabled() const { return dynamicLightingEnabled; }
    void SetDynamicLightingEnabled(bool enabled) { dynamicLightingEnabled = enabled; }
    void ToggleDynamicLightingEnabled() { dynamicLightingEnabled = !dynamicLightingEnabled; }
    SectorDoorLightingDebugMode DoorLightingDebugMode() const { return doorRenderer.DoorLightingDebugMode(); }
    void SetDoorLightingDebugMode(SectorDoorLightingDebugMode mode) { doorRenderer.SetDoorLightingDebugMode(mode); }
    const std::vector<SectorPreviewDynamicPointLightUniform>& SelectedDynamicLights() const
    {
        return dynamicLightState.SelectedLights();
    }
    const std::vector<int>& SelectedDynamicLightIds() const { return dynamicLightState.SelectedLightIds(); }
    size_t DynamicLightCandidateCount() const { return dynamicLightState.CandidateCount(); }
    size_t DynamicLightSourceCount() const { return dynamicLightState.SourceCount(); }
    size_t DoorConsideredCount() const { return doorRenderer.RenderStats().considered; }
    size_t DoorDrawnCount() const { return doorRenderer.RenderStats().drawn; }
    size_t DoorSkippedCount() const { return doorRenderer.RenderStats().skipped; }

private:
    engine::TextureHandle TextureForId(const std::string& textureId) const;
    engine::TextureHandle NormalTextureForId(const std::string& textureId) const;
    void UpdateCamera();
    SectorBillboardDynamicLightContext BuildBillboardDynamicLightContext() const;
    static const Texture2D* ResolveShadowCasterTexture(
            void* userData,
            engine::AssetManager& assets,
            const std::string& textureId);

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
    std::unordered_map<std::string, engine::TextureHandle> normalTextureHandlesById;
    engine::TextureHandle lightmapTexture = engine::NullTextureHandle();
    engine::AssetScopeHandle assetScope = engine::NullAssetScopeHandle();
    Material material = {};
    Texture2D defaultMaterialTexture = {};
    bool materialLoaded = false;
    int useLightmapLoc = -1;
    int useBakedAmbientOcclusionLoc = -1;
    int hasLightmapLoc = -1;
    int hasNormalMapLoc = -1;
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
    SectorSkyRenderer skyRenderer;
    SectorPbrEnvironment pbrEnvironment;
    SectorBloomRenderer bloomRenderer;
    SectorBillboardRenderer billboardRenderer;
    SectorStaticModelRenderer staticModelRenderer;
    SectorDoorRenderer doorRenderer;
    SectorDynamicLightingRenderer dynamicLightState;
    float runtimeSeconds = 0.0f;
    bool dynamicLightingEnabled = true;
    int lightmapStatus = 0;
    bool initialized = false;
    size_t sectorCount = 0;

    Camera3D camera = {};
    Vector3 position = {};
    float yawRadians = 0.0f;
    float pitchRadians = 0.0f;
};

} // namespace game
