#pragma once

#include "engine/assets/AssetManager.h"
#include "sector_demo/SectorCollisionWorld.h"
#include "sector_demo/SectorDynamicPointLightSelection.h"
#include "sector_demo/SectorGeneratedGeometry.h"
#include "sector_demo/SectorMeshTypes.h"
#include "sector_demo/SectorPortalVisibility.h"
#include "sector_demo/renderer/SectorBillboardRenderer.h"
#include "sector_demo/renderer/SectorAtmosphereGpuProfiler.h"
#include "sector_demo/renderer/SectorBloomRenderer.h"
#include "sector_demo/renderer/SectorDoorRenderer.h"
#include "sector_demo/renderer/SectorDynamicLightingRenderer.h"
#include "sector_demo/renderer/SectorDynamicModelShadowRenderer.h"
#include "sector_demo/renderer/SectorFog.h"
#include "sector_demo/renderer/SectorLocalFogRenderer.h"
#include "sector_demo/renderer/SectorLightAtmosphere.h"
#include "sector_demo/renderer/SectorLightDustRenderer.h"
#include "sector_demo/renderer/SectorLightHazeRenderer.h"
#include "sector_demo/renderer/SectorVolumetricAtmosphereRenderer.h"
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
struct AnimatedModelInstance;
struct ModelAsset;
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
            SectorRuntimeDoorLightingContext doorLighting = {},
            const SectorTopologyFogSettings& fogSettings = SectorTopologyFogSettings{});
    void RenderDynamicSpotLightShadowMaps(
            engine::AssetManager& assets,
            engine::World* runtimeObjectWorld = nullptr);
    void DrawScene(
            engine::AssetManager& assets,
            bool useBakedAmbientOcclusion = true,
            engine::World* runtimeObjectWorld = nullptr,
            SectorRuntimeDoorLightingContext doorLighting = {},
            const SectorTopologyFogSettings& fogSettings = SectorTopologyFogSettings{});
    bool ApplyWorldAtmosphere(
            engine::RenderTarget& sceneTarget,
            const SectorTopologyMap& map,
            const SectorBakedObjectLightProbeRuntimeData& objectLightProbes);
    bool PrepareWorldAtmosphere(
            const engine::RenderTarget& sceneTarget,
            const SectorTopologyMap& map,
            const SectorBakedObjectLightProbeRuntimeData& objectLightProbes);
    bool ApplyHdrBloom(
            engine::RenderTarget& sceneTarget,
            const engine::HdrBloomSettings& settings,
            bool presentFromScratch = false);
    bool CompositeViewmodel(
            engine::RenderTarget& sceneTarget,
            const engine::RenderTarget& viewmodelTarget);
    const engine::RenderTarget* HdrDebugPresentationSource() const
    {
        const engine::RenderTarget* debugSource = bloomRenderer.DebugSource();
        return debugSource != nullptr ? debugSource : hdrPresentationSource;
    }
    void DrawViewmodel(
            engine::AssetManager& assets,
            const engine::ModelAsset& asset,
            engine::AnimatedModelInstance& instance,
            const Camera3D& viewmodelCamera,
            Matrix transform,
            const engine::ModelAsset* attachmentAsset,
            Matrix attachmentTransform,
            int receiverSectorId,
            bool objectProbeRuntimeAvailable,
            const BakedObjectLightingVerticalSample& ambientLighting,
            const SectorViewmodelLightingContext& lighting,
            const SectorViewmodelLightingContext& attachmentLighting);

    bool IsReady() const { return initialized; }
    bool IsRendererReady() const { return IsReady(); }
    Vector3 Position() const { return position; }
    const Camera3D& Camera() const { return camera; }
    const Camera3D& RenderCamera() const { return Camera(); }
    SectorViewPose Pose() const;
    SectorViewPose RendererPose() const;
    void ApplyPose(const SectorViewPose& pose);
    void ApplyRendererPose(
            const SectorViewPose& pose,
            bool refreshVisibility = true);
    void SetVerticalFovDegrees(float value);
    void RefreshDynamicLightSources(const SectorTopologyMap& map);
    void SetRuntimePointLight(
            const SectorPreviewDynamicPointLightSource* light)
    {
        dynamicLightState.SetRuntimePointLight(light);
    }
    size_t SectorCount() const { return sectorCount; }
    size_t BatchCount() const { return meshes.sectorDrawRecords.size(); }
    int TriangleCount() const { return meshes.triangleCount; }
    const SectorGeneratedGeometry& GeneratedGeometry() const { return generatedGeometry; }
    const SectorGeneratedGeometry& RenderedGeometry() const { return GeneratedGeometry(); }
    float AssetProgress(engine::AssetManager& assets) const;
    float RendererAssetProgress(engine::AssetManager& assets) const;
    engine::AssetScopeHandle RendererAssetScope() const { return assetScope; }
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
    void SetGraphicsQuality(
            SectorTopologyFogSettings::VolumetricQuality volumetricCap,
            bool shadowsEnabled,
            int shadowMapResolution = DynamicSpotLightShadowMapResolution,
            float projectedShadowIntervalSeconds = 0.0f,
            int projectedShadowResolution = DynamicModelProjectedShadowResolution)
    {
        volumetricQualityCap = volumetricCap;
        shadowMapsEnabled = shadowsEnabled;
        if (shadowsEnabled) {
            dynamicLightState.SetShadowMapResolution(shadowMapResolution);
        }
        dynamicModelShadowIntervalSeconds = std::max(
                projectedShadowIntervalSeconds, 0.0f);
        if (shadowsEnabled) {
            dynamicModelShadowRenderer.SetProjectedShadowResolution(
                    projectedShadowResolution);
        }
    }
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
    SectorPbrContributionSettings PbrContributionSettings() const
    {
        return staticModelRenderer.PbrContributionSettings();
    }
    void SetPbrContributionSettings(SectorPbrContributionSettings settings)
    {
        staticModelRenderer.SetPbrContributionSettings(settings);
    }
    void SetPbrDiagnosticSelectedObjectId(int objectId)
    {
        staticModelRenderer.SetPbrDiagnosticSelectedObjectId(objectId);
    }
    const SectorPbrDrawDiagnostics& WorldPbrDiagnostics() const
    {
        return staticModelRenderer.WorldPbrDiagnostics();
    }
    const SectorPbrDrawDiagnostics& ViewmodelPbrDiagnostics() const
    {
        return staticModelRenderer.ViewmodelPbrDiagnostics();
    }
    bool PbrEnvironmentActive() const { return pbrEnvironment.active; }
    bool PbrEnvironmentUsesSky() const { return pbrEnvironment.usedSky; }
    SectorBloomDebugView BloomDebugView() const { return bloomRenderer.DebugView(); }
    void SetBloomDebugView(SectorBloomDebugView view) { bloomRenderer.SetDebugView(view); }
    const SectorBloomDiagnostics& BloomDiagnostics() const
    {
        return bloomRenderer.Diagnostics();
    }
    const engine::RenderTarget& LocalFogAccumulationTarget() const
    {
        return localFogRenderer.AccumulationTarget();
    }
    const engine::RenderTarget& HazeAccumulationTarget() const
    {
        return lightHazeRenderer.AccumulationTarget();
    }
    const std::string& LocalFogAccumulationDiagnostic() const
    {
        return localFogRenderer.AccumulationDiagnostic();
    }
    const std::string& HazeAccumulationDiagnostic() const
    {
        return lightHazeRenderer.AccumulationDiagnostic();
    }
    const std::string& DustResourceDiagnostic() const
    {
        return lightDustRenderer.ResourceDiagnostic();
    }
    const std::string& HdrSceneScratchDiagnostic() const
    {
        return hdrSceneScratchDiagnostic;
    }
    const SectorAtmosphereDiagnostics& AtmosphereDiagnostics() const
    {
        return atmosphereDiagnostics;
    }
    const SectorAtmosphereCapture& AtmosphereCapture() const
    {
        return atmosphereCapture;
    }
    void StartAtmosphereCapture();
    void CancelAtmosphereCapture();
    SectorAtmosphereBackend AtmosphereBackend() const { return atmosphereBackend; }
    void SetAtmosphereBackend(SectorAtmosphereBackend backend);

private:
    bool EnsureHdrSceneScratch(const engine::RenderTarget& sceneTarget);
    bool EnsureHdrSceneColorView(const engine::RenderTarget& sceneTarget);
    void UnloadHdrSceneColorView();
    bool EnsureHdrCompositeShader();
    bool CommitHdrScratch(engine::RenderTarget& sceneTarget);
    SectorAtmosphereCaptureMetadata BuildAtmosphereCaptureMetadata() const;
    void UpdateAtmosphereDiagnostics(
            const engine::RenderTarget& sceneTarget,
            SectorTopologyFogSettings::VolumetricQuality quality,
            bool targetSupported,
            bool pipelineFailed,
            bool localFogApplied,
            bool lightHazeApplied,
            bool unifiedApplied,
            bool lightDustApplied);
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
    std::vector<engine::TextureHandle> lightmapTextures;
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
    int decalEmissiveStrengthLoc = -1;
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
    SectorFogShaderLocations fogShaderLocations;
    SectorLocalFogRenderer localFogRenderer;
    SectorLightHazeRenderer lightHazeRenderer;
    SectorVolumetricAtmosphereRenderer volumetricAtmosphereRenderer;
    SectorLightDustRenderer lightDustRenderer;
    SectorAtmosphereGpuProfiler atmosphereGpuProfiler;
    SectorAtmosphereDiagnostics atmosphereDiagnostics;
    SectorAtmosphereCapture atmosphereCapture;
    SectorAtmosphereBackend atmosphereBackend = SectorAtmosphereBackend::Legacy;
    std::vector<SectorLightAtmosphereSource> lightAtmosphereSources;
    SectorSkyRenderer skyRenderer;
    SectorPbrEnvironment pbrEnvironment;
    SectorBloomRenderer bloomRenderer;
    engine::RenderTarget hdrSceneScratch;
    RenderTexture2D hdrSceneColorView = {};
    const engine::RenderTarget* hdrPresentationSource = nullptr;
    Shader hdrCompositeShader = {};
    int hdrCompositeSceneLoc = -1;
    int hdrCompositeSourceLoc = -1;
    int hdrCompositeModeLoc = -1;
    bool hdrCompositeShaderFailed = false;
    std::string hdrSceneScratchError;
    std::string hdrSceneScratchDiagnostic = "not allocated";
    int hdrSceneScratchFailedWidth = 0;
    int hdrSceneScratchFailedHeight = 0;
    SectorBillboardRenderer billboardRenderer;
    SectorStaticModelRenderer staticModelRenderer;
    SectorStaticSpecularLightState staticSpecularLightState;
    SectorDoorRenderer doorRenderer;
    SectorDynamicLightingRenderer dynamicLightState;
    SectorDynamicModelShadowRenderer dynamicModelShadowRenderer;
    float runtimeSeconds = 0.0f;
    bool dynamicLightingEnabled = true;
    SectorTopologyFogSettings::VolumetricQuality volumetricQualityCap =
            SectorTopologyFogSettings::VolumetricQuality::High;
    bool shadowMapsEnabled = true;
    float dynamicModelShadowIntervalSeconds = 0.0f;
    float lastDynamicModelShadowRenderSeconds = -1000.0f;
    int lightmapStatus = 0;
    bool surfaceLightmapBakeCurrent = false;
    bool objectProbeBakeCurrent = false;
    bool initialized = false;
    size_t sectorCount = 0;

    Camera3D camera = {};
    Vector3 position = {};
    float yawRadians = 0.0f;
    float pitchRadians = 0.0f;
    float rollRadians = 0.0f;
    float verticalFovDegrees = 75.0f;
};

} // namespace game
