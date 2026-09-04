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
#include "sector_demo/renderer/SectorDynamicModelShadowRenderer.h"
#include "sector_demo/renderer/SectorFog.h"
#include "sector_demo/renderer/SectorDistanceFogRenderer.h"
#include "sector_demo/renderer/SectorAnalyticFogRenderer.h"
#include "sector_demo/renderer/SectorAnalyticLightShaftRenderer.h"
#include "sector_demo/renderer/SectorLightAtmosphere.h"
#include "sector_demo/renderer/SectorLightDustRenderer.h"
#include "sector_demo/renderer/SectorLightProxyRenderer.h"
#include "sector_demo/renderer/SectorLiquidRenderer.h"
#include "sector_demo/renderer/SectorSkyRenderer.h"
#include "sector_demo/renderer/SectorPbrEnvironment.h"
#include "sector_demo/renderer/SectorStaticModelRenderer.h"
#include "sector_demo/renderer/SectorUnderwaterRenderer.h"
#include "sector_demo/renderer/SectorWindowRenderer.h"
#include "sector_demo/renderer/SectorDuctCoverRenderer.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorViewPose.h"
#include "sector_demo/SectorUseInteraction.h"

#include <raylib.h>

#include <array>
#include <cstdint>
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

struct SectorAtmosphereDiagnostics {
    double causticsGpuMilliseconds = 0.0;
    double distanceFogGpuMilliseconds = 0.0;
    double analyticFogGpuMilliseconds = 0.0;
    double analyticShaftGpuMilliseconds = 0.0;
    double lightHaloGpuMilliseconds = 0.0;
    double dustGpuMilliseconds = 0.0;
    double underwaterParticlesGpuMilliseconds = 0.0;
    int dynamicLightCount = 0;
    int analyticFogEligibleCount = 0;
    int analyticFogActiveCount = 0;
    float analyticFogScissorCoverage = 0.0f;
    int analyticShaftEligibleCount = 0;
    int analyticShaftActiveCount = 0;
    float analyticShaftScissorCoverage = 0.0f;
    int analyticShaftDrawCallCount = 0;
    int lightHaloEligibleCount = 0;
    int lightHaloCount = 0;
    float lightHaloScissorCoverage = 0.0f;
    int lightHaloDrawCallCount = 0;
    int dustEligibleEmitterCount = 0;
    int dustActiveEmitterCount = 0;
    int dustVisibleParticleCount = 0;
    int underwaterVisibleParticleCount = 0;
};

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
    bool RefreshSurfaceMaterials(
            engine::AssetManager& assets,
            const SectorTopologyMap& map,
            std::string& error);
    bool RefreshSurfaceGeometry(
            engine::AssetManager& assets,
            const SectorTopologyMap& map,
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
            const SectorTopologyFogSettings& fogSettings = SectorTopologyFogSettings{},
            bool staticCaptureOnly = false,
            SectorUseHighlight useHighlight = {});
    bool CaptureReflectionProbe(
            engine::AssetManager& assets,
            Vector3 capturePosition,
            int resolution,
            engine::World* runtimeObjectWorld,
            SectorRuntimeDoorLightingContext doorLighting,
            std::vector<Vector4>& outFacePixels,
            std::string& error);
    bool ApplyWorldAtmosphere(
            engine::RenderTarget& sceneTarget,
            const SectorTopologyMap& map,
            const SectorBakedObjectLightProbeRuntimeData& objectLightProbes,
            const SectorUnderwaterRenderContext& underwater,
            bool collectGpuDiagnostics = false);
    bool ApplyTransparentSurfaces(
            engine::RenderTarget& sceneTarget,
            engine::AssetManager& assets,
            engine::World* runtimeObjectWorld,
            SectorRuntimeDoorLightingContext doorLighting,
            const SectorTopologyFogSettings& fogSettings,
            const SectorUnderwaterRenderContext& underwater,
            bool collectGpuDiagnostics = false);
    bool ApplyHdrBloom(
            engine::RenderTarget& sceneTarget,
            const engine::HdrBloomSettings& settings,
            bool presentFromScratch = false);
    bool PreparePostBloomWorldOverlays(
            engine::RenderTarget& sceneTarget,
            bool overlayRequested);
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
    float RuntimeSeconds() const { return runtimeSeconds; }
    const Camera3D& Camera() const { return camera; }
    const Camera3D& RenderCamera() const { return Camera(); }
    SectorViewPose Pose() const;
    SectorViewPose RendererPose() const;
    void ApplyPose(const SectorViewPose& pose);
    void ApplyRendererPose(
            const SectorViewPose& pose,
            bool refreshVisibility = true);
    void SetVerticalFovDegrees(float value);
    void BeginStaticObjectAdjustmentBakedDataStale();
    void FinishStaticObjectAdjustmentBakedData(bool restore);
    void RefreshDynamicLightSources(const SectorTopologyMap& map);
    void SetRuntimePointLight(
            const SectorPreviewDynamicPointLightSource* light)
    {
        dynamicLightState.SetRuntimePointLight(light);
    }
    const SectorPreviewDynamicPointLightSource* RuntimePointLight() const
    {
        return dynamicLightState.RuntimePointLight();
    }
    void SetPlayerFlashlight(
            const SectorPreviewDynamicPointLightSource* light);
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
    bool DepthPrepassEnabled() const { return depthPrepassEnabled; }
    void SetDynamicLightingEnabled(bool enabled) { dynamicLightingEnabled = enabled; }
    void ToggleDynamicLightingEnabled() { dynamicLightingEnabled = !dynamicLightingEnabled; }
    void SetGraphicsQuality(
            bool shadowsEnabled,
            int shadowMapResolution = DynamicSpotLightShadowMapResolution,
            int maxDynamicLights = static_cast<int>(MaxDynamicLights),
            int maxShadowLightUpdatesPerFrame = 2,
            bool depthPrepass = false,
            float dynamicLightFadeInSeconds = DynamicLightDefaultFadeInSeconds)
    {
        shadowMapsEnabled = shadowsEnabled;
        if (shadowsEnabled) {
            dynamicLightState.SetShadowMapResolution(shadowMapResolution);
        }
        dynamicLightState.SetMaxDynamicLights(
                static_cast<std::size_t>(std::max(maxDynamicLights, 0)));
        dynamicLightState.SetMaxShadowLightUpdatesPerFrame(
                static_cast<std::size_t>(std::max(
                        maxShadowLightUpdatesPerFrame, 0)));
        dynamicLightState.SetSelectionFadeInSeconds(
                dynamicLightFadeInSeconds);
        depthPrepassEnabled = depthPrepass;
    }
    const std::vector<SectorPreviewDynamicPointLightUniform>& SelectedDynamicLights() const
    {
        return dynamicLightState.SelectedLights();
    }
    const std::vector<SectorPreviewDynamicLightKey>& SelectedDynamicLightKeys() const {
        return dynamicLightState.SelectedLightKeys();
    }
    const SectorDynamicLightSelectionStats& DynamicLightSelectionStats() const {
        return dynamicLightState.SelectionStats();
    }
    size_t DynamicLightCandidateCount() const { return dynamicLightState.CandidateCount(); }
    size_t DynamicLightSourceCount() const { return dynamicLightState.SourceCount(); }
    const SectorDynamicShadowRenderStats& DynamicShadowRenderStats() const {
        return dynamicLightState.ShadowRenderStats();
    }
    size_t DynamicModelShadowCasterCount() const {
        return dynamicModelShadowRenderer.DynamicCasterCount();
    }
    size_t DoorConsideredCount() const { return doorRenderer.RenderStats().considered; }
    size_t DoorDrawnCount() const { return doorRenderer.RenderStats().drawn; }
    size_t DoorSkippedCount() const { return doorRenderer.RenderStats().skipped; }
    size_t WindowConsideredCount() const { return windowRenderer.ConsideredCount(); }
    size_t WindowDrawnCount() const { return windowRenderer.DrawnCount(); }
    size_t LiquidSurfaceCount() const { return liquidRenderer.SurfaceCount(); }
    size_t LiquidDrawnCount() const { return liquidRenderer.DrawnCount(); }
    SectorPbrContributionSettings PbrContributionSettings() const
    {
        return pbrContributionSettings;
    }
    void SetPbrContributionSettings(SectorPbrContributionSettings settings)
    {
        pbrContributionSettings = NormalizeSectorPbrContributionSettings(settings);
        staticModelRenderer.SetPbrContributionSettings(pbrContributionSettings);
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

private:
    static constexpr std::size_t AtmosphereGpuPassCount = 7;
    static constexpr std::size_t AtmosphereGpuQueryLatency = 4;

    bool EnsureHdrSceneScratch(const engine::RenderTarget& sceneTarget);
    bool EnsureHdrSceneColorView(const engine::RenderTarget& sceneTarget);
    void UnloadHdrSceneColorView();
    bool EnsureHdrCompositeShader();
    bool CommitHdrScratch(engine::RenderTarget& sceneTarget);
    bool EnsureAtmosphereGpuQueries();
    void ShutdownAtmosphereGpuQueries();
    void BeginAtmosphereGpuFrame(bool enabled);
    void BeginAtmosphereGpuPass(std::size_t pass);
    void EndAtmosphereGpuPass(std::size_t pass);
    unsigned int AtmosphereGpuQuery(
            std::size_t pass,
            std::size_t slot,
            bool end) const;
    void RefreshAtmosphereDiagnostics(
            const SectorBillboardDynamicLightContext& dynamicLights);
    void EnsureSurfaceMaterialResources(
            engine::AssetManager& assets,
            const SectorTopologyMap& map,
            const SectorGeneratedGeometry& geometry);
    bool RefreshSurfaceGeometryInternal(
            engine::AssetManager& assets,
            const SectorTopologyMap& map,
            bool refreshVisibilityData,
            std::string& error);
    void RefreshBakedDataStatus(const SectorTopologyMap& map);
    void RefreshBakedDataStatus(
            const SectorTopologyMap& map,
            const std::string& currentSourceHash);
    engine::TextureHandle TextureForId(const std::string& materialId) const;
    engine::TextureHandle NormalTextureForId(const std::string& materialId) const;
    void UpdateCamera();
    SectorBillboardDynamicLightContext BuildBillboardDynamicLightContext() const;
    void DrawDepthPrepass(engine::AssetManager& assets, engine::World* runtimeObjectWorld);
    static const Texture2D* ResolveShadowCasterTexture(
            void* userData,
            engine::AssetManager& assets,
            const std::string& materialId);
    static SectorDoorResolvedMaterial ResolveDoorMaterial(
            void* userData,
            engine::AssetManager& assets,
            const std::string& materialId);

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
    std::unordered_map<std::string, float> normalStrengthById;
    std::unordered_map<std::string, float> metallicFactorById;
    std::unordered_map<std::string, float> roughnessFactorById;
    std::vector<engine::TextureHandle> lightmapTextures;
    std::vector<engine::TextureHandle> directionalLightmapTextures;
    engine::AssetScopeHandle assetScope = engine::NullAssetScopeHandle();
    engine::TextureHandle flashlightCookieTexture =
            engine::NullTextureHandle();
    Material material = {};
    Texture2D defaultMaterialTexture = {};
    bool materialLoaded = false;
    Material depthPrepassMaterial = {};
    bool depthPrepassMaterialLoaded = false;
    int useLightmapLoc = -1;
    int useBakedAmbientOcclusionLoc = -1;
    int hasLightmapLoc = -1;
    int hasDirectionalLightmapLoc = -1;
    int hasNormalMapLoc = -1;
    int normalStrengthLoc = -1;
    int metallicFactorLoc = -1;
    int roughnessFactorLoc = -1;
    int cameraPositionLoc = -1;
    int hasEnvironmentLoc = -1;
    int environmentExposureLoc = -1;
    int indirectDiffuseScaleLoc = -1;
    int environmentSpecularScaleLoc = -1;
    int environmentBoxProjectionLoc = -1;
    int environmentCapturePositionLoc = -1;
    int environmentInfluenceCenterLoc = -1;
    int environmentHalfExtentsLoc = -1;
    int environmentYawLoc = -1;
    int environmentMaxLodLoc = -1;
    int environmentIntensityLoc = -1;
    int pbrDiagnosticModeLoc = -1;
    int useStaticSpecularLightingLoc = -1;
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
    int dynamicLightSpotShadowRightLoc = -1;
    int dynamicLightSpotShadowProjectionLoc = -1;
    int dynamicLightProfilesLoc = -1;
    int dynamicLightProfileParametersLoc = -1;
    int flashlightCookieLoc = -1;
    int hasPointShadowsLoc = -1;
    int dynamicLightShadowSlotsLoc = -1;
    std::array<int, MaxDynamicSpotLightShadowCasters> shadowLightMatrixLocs = [] {
        std::array<int, MaxDynamicSpotLightShadowCasters> locs{};
        locs.fill(-1);
        return locs;
    }();
    int shadowBiasLoc = -1;
    int shadowStrengthLoc = -1;
    int shadowSoftnessLoc = -1;
    int shadowAtlasTilesPerRowLoc = -1;
    bool depthPrepassEnabled = false;
    bool liquidRefractionFallbackLogged = false;
    bool atmosphereGpuFramePrepared = false;
    bool preGlassLightEffectsRendered = false;
    bool preGlassShaftApplied = false;
    bool preGlassHaloApplied = false;
    SectorFogShaderLocations fogShaderLocations;
    SectorDistanceFogRenderer distanceFogRenderer;
    SectorAnalyticFogRenderer analyticFogRenderer;
    SectorAnalyticLightShaftRenderer analyticLightShaftRenderer;
    SectorLightProxyRenderer lightProxyRenderer;
    SectorLightDustRenderer lightDustRenderer;
    SectorUnderwaterRenderer underwaterRenderer;
    std::vector<SectorLightAtmosphereSource> lightAtmosphereSources;
    SectorSkyRenderer skyRenderer;
    SectorPbrEnvironment pbrEnvironment;
    bool localReflectionProbesCurrent = true;
    std::string localReflectionProbeSurfaceHash;
    bool staticObjectAdjustmentBakedDataActive = false;
    int staticObjectAdjustmentOriginalLightmapStatus = 0;
    bool staticObjectAdjustmentOriginalSurfaceLightmapCurrent = false;
    bool staticObjectAdjustmentOriginalObjectProbeCurrent = false;
    bool staticObjectAdjustmentOriginalLocalReflectionProbesCurrent = true;
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
    SectorAtmosphereDiagnostics atmosphereDiagnostics;
    std::array<unsigned int,
            AtmosphereGpuPassCount * AtmosphereGpuQueryLatency * 2>
            atmosphereGpuQueries{};
    std::array<std::uint8_t, AtmosphereGpuQueryLatency>
            atmosphereGpuIssuedMasks{};
    std::size_t atmosphereGpuFrameIndex = 0;
    std::size_t atmosphereGpuSlot = 0;
    bool atmosphereGpuQueriesInitialized = false;
    bool atmosphereGpuActive = false;
    SectorBillboardRenderer billboardRenderer;
    SectorStaticModelRenderer staticModelRenderer;
    SectorStaticSpecularLightState staticSpecularLightState;
    SectorStaticSpecularShaderLocations staticSpecularLocations;
    SectorPbrContributionSettings pbrContributionSettings;
    SectorDoorRenderer doorRenderer;
    SectorWindowRenderer windowRenderer;
    SectorDuctCoverRenderer ductCoverRenderer;
    SectorLiquidRenderer liquidRenderer;
    SectorDynamicLightingRenderer dynamicLightState;
    SectorDynamicModelShadowRenderer dynamicModelShadowRenderer;
    float runtimeSeconds = 0.0f;
    bool dynamicLightingEnabled = true;
    bool shadowMapsEnabled = true;
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
