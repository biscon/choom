#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/EngineContext.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/SectorEditorLightmapAsyncTypes.h"
#include "sector_editor/SectorEditorMaterialActions.h"
#include "sector_editor/SectorEditorMainMenu.h"
#include "sector_editor/document/SectorEditorDocumentState.h"
#include "sector_editor/inspector/SectorEditorInspectorUiState.h"
#include "sector_editor/services/lights/SectorEditorLightEditingService.h"
#include "sector_editor/services/lights/SectorEditorLightEditingState.h"
#include "sector_editor/services/config_clipboard/SectorEditorConfigClipboardService.h"
#include "sector_editor/services/material_edit/SectorEditorMaterialEditingService.h"
#include "sector_editor/npcs/SectorEditorNpcEditorService.h"
#include "sector_editor/npcs/SectorEditorNpcEditorState.h"
#include "sector_editor/patrols/SectorEditorPatrolEditorService.h"
#include "sector_editor/patrols/SectorEditorPatrolEditorState.h"
#include "sector_editor/materials/SectorEditorMaterialRegistryEditorService.h"
#include "sector_editor/materials/SectorEditorMaterialRegistryEditorState.h"
#include "sector_editor/weapons/SectorEditorWeaponEditorService.h"
#include "sector_editor/weapons/SectorEditorWeaponEditorState.h"
#include "sector_editor/services/runtime_objects/SectorEditorRuntimeObjectEditingService.h"
#include "sector_editor/services/sounds/SectorEditorSoundCatalogState.h"
#include "sector_editor/services/sounds/SectorEditorSoundService.h"
#include "sector_editor/sounds/SectorEditorSoundEditorService.h"
#include "sector_editor/sounds/SectorEditorSoundEditorState.h"
#include "sector_editor/services/static_model_picker/SectorEditorStaticModelPickerService.h"
#include "sector_editor/services/texture_catalog/SectorEditorTextureCatalogService.h"
#include "sector_editor/services/texture_catalog/SectorEditorTextureCatalogState.h"
#include "sector_editor/preview/SectorEditorPreviewState.h"
#include "game/FpsPlayerRuntime.h"
#include "game/PlayerAudio.h"
#include "sector_editor/selection/SectorEditorManipulationService.h"
#include "sector_editor/selection/SectorEditorManipulationState.h"
#include "sector_editor/selection/SectorEditorSelectionService.h"
#include "sector_editor/selection/SectorEditorSelectionState.h"
#include "sector_editor/SectorEditorSelectionTypes.h"
#include "sector_editor/SectorEditorTopologyActions.h"
#include "sector_editor/SectorEditorTypes.h"
#include "sector_editor/services/lightmap_bake/SectorEditorLightmapBakeController.h"
#include "sector_editor/services/fog_volumes/SectorEditorAuthoringFogVolumeEditingService.h"
#include "sector_editor/services/reflection_probes/SectorEditorReflectionProbeEditingService.h"
#include "sector_editor/services/fog_volumes/SectorEditorFogVolumeEditingState.h"
#include "sector_editor/services/footsteps/SectorEditorFootstepService.h"
#include "sector_editor/services/authoring_faces/SectorEditorAuthoringFaceMergeService.h"
#include "sector_editor/services/level_markers/SectorEditorLevelMarkerEditingService.h"
#include "sector_editor/services/level_markers/SectorEditorLevelMarkerEditingState.h"
#include "sector_editor/services/sound_emitters/SectorEditorSoundEmitterEditingService.h"
#include "sector_editor/services/sound_emitters/SectorEditorSoundEmitterEditingState.h"
#include "sector_editor/services/triggers/SectorEditorTriggerEditingService.h"
#include "sector_editor/services/triggers/SectorEditorTriggerEditingState.h"
#include "sector_editor/items/SectorEditorItemEditorService.h"
#include "sector_editor/items/SectorEditorItemEditorState.h"
#include "sector_editor/player/SectorEditorPlayerSettingsService.h"
#include "sector_editor/player/SectorEditorPlayerSettingsState.h"
#include "sector_demo/SectorSceneRuntime.h"
#include "sector_demo/SectorUseInteraction.h"
#include "sector_demo/SectorMaterialRegistry.h"
#include "game/FpsWeaponRegistry.h"
#include "game/items/ItemAssets.h"
#include "game/items/ItemDefinitions.h"

#include <raylib.h>

#include <functional>
#include <array>
#include <optional>
#include <string>
#include <vector>

namespace game {

struct SectorEditorToolContext;
struct SectorEditorSaveLevelPlan;

class SectorEditor {
public:
    SectorEditor(
            FpsApplicationSettings& sharedApplicationSettings,
            SectorMaterialRegistry& sharedMaterialRegistry,
            ItemRegistry& sharedItemRegistry,
            ItemModelAssetState& sharedItemModelAssets)
        : materialRegistry(sharedMaterialRegistry),
          itemRegistry(sharedItemRegistry),
          itemModelAssets(sharedItemModelAssets),
          applicationSettings(sharedApplicationSettings) {}

    bool Init(engine::EngineContext& context);
    void Shutdown(engine::EngineContext& context);
    void SetPreviewGraphicsQuality(
            bool shadowsEnabled,
            int shadowMapResolution,
            int maxDynamicLights,
            int maxShadowLightUpdatesPerFrame,
            bool depthPrepass,
            float dynamicLightFadeInSeconds)
    {
        sceneRuntime.Renderer().SetGraphicsQuality(
                shadowsEnabled, shadowMapResolution,
                maxDynamicLights, maxShadowLightUpdatesPerFrame, depthPrepass,
                dynamicLightFadeInSeconds);
    }
    void SetPreviewVerticalFovDegrees(float value)
    {
        sceneRuntime.Renderer().SetVerticalFovDegrees(value);
    }

    void Update(engine::EngineContext& context, float dt);
    void SetGameSessionExists(bool exists) { gameSessionExists = exists; }
    bool ConsumeClearGameSessionRequest();
    bool ConsumePlayerAudioSettingsChanged();
    void Render(engine::AssetManager& assets);
    void RenderPreview3DShadowMaps(engine::AssetManager& assets);
    void RenderPreview3DScene(engine::EngineContext& context);
    void RenderPreview3DViewmodel(engine::AssetManager& assets);
    void RenderPreview3DOverlays();
    void RenderPreview3DHud(
            engine::AssetManager& assets,
            engine::FontHandle usePromptFont,
            Rectangle playableViewport) const;
    void ApplyPreview3DWorldAtmosphere(
            engine::RenderTarget& sceneTarget,
            bool collectGpuDiagnostics = false);
    void ApplyPreview3DHdrBloom(engine::RenderTarget& sceneTarget);
    bool CompositePreview3DViewmodel(
            engine::RenderTarget& sceneTarget,
            const engine::RenderTarget& viewmodelTarget);
    const engine::RenderTarget* Preview3DHdrDebugPresentationSource() const
    {
        return sceneRuntime.HdrDebugPresentationSource();
    }
    const SectorAtmosphereDiagnostics& PreviewAtmosphereDiagnostics() const
    {
        return sceneRuntime.Renderer().AtmosphereDiagnostics();
    }
    void RenderUI(
            engine::UIContext& ui,
            const engine::UIConfig& config,
            engine::Input& input,
            engine::AssetManager& assets,
            engine::FontHandle font,
            engine::FontHandle smallFont);
    bool IsPreview3DActive() const;
    float VisibleMainMenuHeight() const;
    bool OpenLevel(
            engine::EngineContext& context,
            const std::string& levelName,
            const std::string& jsonAssetPath);
    void SuspendRuntime(engine::EngineContext& context);
    void RestoreRuntimeObjects(engine::EngineContext& context);
    const SectorTopologyMap& CurrentTopologyMap() const;

    Vector2 MapToScreen(Vector2 map) const;
    Vector2 ScreenToMap(Vector2 screen) const;
    Vector2 SnapMapPoint(Vector2 map) const;

private:
    Vector2 CanvasWorldToScreen(Vector2 canvasWorld) const;
    Vector2 ScreenToCanvasWorld(Vector2 screen) const;
    Rectangle BuildLeftPanelRect() const;
    Rectangle BuildRightPanelRect() const;
    Rectangle BuildBottomPanelRect() const;
    Rectangle BuildCanvasRect() const;
    bool IsMainMenuInteractionEnabled() const;
    void HandleMainMenuCommand(
            SectorEditorMainMenuCommand command,
            engine::UIContext& ui,
            engine::AssetManager& assets);

    bool IsMouseOverCanvas(const engine::Input& input) const;
    void UpdateHoverAndMouse(engine::Input& input);
    void HandleCanvasInput(engine::Input& input, float dt);
    SectorEditorPickTarget CurrentPickSelectionTarget() const;
    std::vector<SectorEditorPickCandidate> BuildSelectPickCandidates(Vector2 screenPoint) const;
    void StartAuthoringVertexDrag(int vertexId, SectorTopologyCoordPoint point);
    void UpdateAuthoringVertexDrag(engine::Input& input);
    void FinishAuthoringVertexDrag();
    void CancelAuthoringVertexDrag(const char* message);
    void StartLightDrag(
            int topologyLightId,
            SpotLightHandle spotHandle = SpotLightHandle::Origin);
    void UpdateLightDrag(engine::Input& input);
    void FinishLightDrag();
    void CancelLightDrag(const char* message);
    void StartRuntimeObjectDrag(int objectId);
    void UpdateRuntimeObjectDrag(engine::Input& input);
    void FinishRuntimeObjectDrag();
    void CancelRuntimeObjectDrag(const char* message);
    void UpdatePreview3D(engine::Input& input, engine::AssetManager& assets, float dt);
    void BeginFpsViewmodel(engine::AssetManager& assets);
    void EndFpsViewmodel(engine::AssetManager& assets);
    void UpdateFpsViewmodel(engine::AssetManager& assets, float dt);
    bool ProcessFpsWeaponFire(engine::Input& input);
    void UpdateFpsViewmodelTransformsAndLight();
    void UpdatePreview3DSelection(engine::Input& input);
    void CancelPendingAuthoringLine(const char* message);
    void CancelPendingAuthoringRectangle(const char* message);
    void CancelPendingAuthoringInsertVertex(const char* message);
    void BeginPendingAuthoringInsertVertex(int lineId);
    bool TryResolveAuthoringInsertVertexPoint(
            int lineId,
            Vector2 mapPoint,
            SectorTopologyCoordPoint& outPoint,
            std::string& error) const;
    void UpdatePendingAuthoringInsertVertex(Vector2 mapPoint);
    SectorPoint CurrentSnappedSectorPoint() const;
    bool ToTopologyCoordPoint(SectorPoint point, SectorTopologyCoordPoint& outPoint, std::string& error) const;
    bool ToCanonicalSectorPoint(SectorPoint point, SectorPoint& outPoint, std::string& error) const;

    void DrawGrid() const;
    void InvalidateTopologyRenderCache();
    void EnsureTopologyRenderCache();
    void DrawTopologyDocument();
    void DrawTopologySelectedLineHighlight() const;
    void DrawTopologySnapCrosshair() const;
    void DrawAuthoringVertexMoveOverlay() const;
    void DrawAuthoringFogVolumes() const;
    void DrawAuthoringReflectionProbes() const;
    void DrawAuthoringFogVolumeMoveOverlay() const;
    void DrawAuthoringReflectionProbeMoveOverlay() const;
    void DrawLightMoveOverlay() const;
    void DrawCanvasOverlay(engine::AssetManager& assets, engine::FontHandle font) const;
    void RenderPreview3D(engine::AssetManager& assets);
    void DrawPreviewSurfaceHighlights() const;
    void DrawPreviewSpotLightOverlay() const;
    void DrawPreviewObjectProbeOverlay() const;
    void DrawPreviewReflectionProbeOverlay() const;
    void RefreshPreviewObjectProbeDebugData();
    bool IsPreviewOverlayMouseInteractive() const;
    Rectangle BuildPreviewOverlayInteractionRect() const;
    void DrawPreviewOverlay(
            engine::UIContext& ui,
            const engine::UIConfig& config,
            engine::Input& input,
            engine::AssetManager& assets,
            engine::FontHandle font,
            engine::FontHandle smallFont);
    void DrawPreviewUvPanel(
            engine::UIContext& ui,
            const engine::UIConfig& config,
            engine::Input& input,
            engine::AssetManager& assets,
            engine::FontHandle font);

    void DrawToolsPanel(
            engine::UIContext& ui,
            const engine::UIConfig& config,
            engine::Input& input,
            engine::AssetManager& assets,
            engine::FontHandle font);
    void DrawSectorsPanel(
            engine::UIContext& ui,
            const engine::UIConfig& config,
            engine::Input& input,
            engine::AssetManager& assets,
            engine::FontHandle font,
            engine::FontHandle smallFont);
    void DrawSetAllModal(
            engine::UIContext& ui,
            const engine::UIConfig& config,
            engine::Input& input,
            engine::AssetManager& assets,
            engine::FontHandle font);
    void DrawTexturePickerModal(
            engine::UIContext& ui,
            const engine::UIConfig& config,
            engine::Input& input,
            engine::AssetManager& assets,
            engine::FontHandle font,
            engine::FontHandle smallFont);
    void DrawFootstepPickerModal(
            engine::UIContext& ui,
            const engine::UIConfig& config,
            engine::Input& input,
            engine::AssetManager& assets,
            engine::FontHandle font);
    void DrawLightmapBakeModal(
            engine::UIContext& ui,
            const engine::UIConfig& config,
            engine::Input& input,
            engine::AssetManager& assets,
            engine::FontHandle font);
    void DrawLightmapBakeSetupModal(
            engine::UIContext& ui,
            const engine::UIConfig& config,
            engine::Input& input,
            engine::AssetManager& assets,
            engine::FontHandle font);
    void DrawSoundEditor(
            engine::UIContext& ui,
            const engine::UIConfig& config,
            engine::Input& input,
            engine::AssetManager& assets,
            engine::FontHandle font,
            engine::FontHandle smallFont);
    void DrawPatrolEditor(
            engine::UIContext& ui,
            const engine::UIConfig& config,
            engine::Input& input,
            engine::AssetManager& assets,
            engine::FontHandle font,
            engine::FontHandle smallFont);
    void DrawSoundPickerModal(
            engine::UIContext& ui,
            const engine::UIConfig& config,
            engine::Input& input,
            engine::FontHandle font);
    void DrawSpritePickerModal(
            engine::UIContext& ui,
            const engine::UIConfig& config,
            engine::Input& input,
            engine::AssetManager& assets,
            engine::FontHandle font);
    void DrawStaticModelPickerModal(
            engine::UIContext& ui,
            const engine::UIConfig& config,
            engine::Input& input,
            engine::AssetManager& assets,
            engine::FontHandle font);
    void DrawNpcEditorModal(
            engine::UIContext& ui,
            const engine::UIConfig& config,
            engine::Input& input,
            engine::AssetManager& assets,
            engine::FontHandle font,
            engine::FontHandle smallFont);
    void DrawWeaponEditor(
            engine::UIContext& ui,
            const engine::UIConfig& config,
            engine::Input& input,
            engine::AssetManager& assets,
            engine::FontHandle font,
            engine::FontHandle smallFont);
    void DrawMaterialRegistryEditor(
            engine::UIContext& ui,
            const engine::UIConfig& config,
            engine::Input& input,
            engine::AssetManager& assets,
            engine::FontHandle font,
            engine::FontHandle smallFont);
    void DrawSaveLevelModal(
            engine::UIContext& ui,
            const engine::UIConfig& config,
            engine::Input& input,
            engine::AssetManager& assets,
            engine::FontHandle font);
    void DrawLoadLevelModal(
            engine::UIContext& ui,
            const engine::UIConfig& config,
            engine::Input& input,
            engine::AssetManager& assets,
            engine::FontHandle font);
    void DrawConfirmationModal(
            engine::UIContext& ui,
            const engine::UIConfig& config,
            engine::Input& input,
            engine::AssetManager& assets,
            engine::FontHandle font);
    void DrawDecalTintModal(
            engine::UIContext& ui,
            const engine::UIConfig& config,
            engine::Input& input,
            engine::AssetManager& assets,
            engine::FontHandle font);
    void DrawDoorTextureSettingsModal(
            engine::UIContext& ui,
            const engine::UIConfig& config,
            engine::Input& input,
            engine::AssetManager& assets,
            engine::FontHandle font,
            engine::FontHandle smallFont);
    void DrawPreviewSettingsModal(
            engine::UIContext& ui,
            const engine::UIConfig& config,
            engine::Input& input,
            engine::AssetManager& assets,
            engine::FontHandle font);
    void DrawPlayerSettingsModal(
            engine::UIContext& ui,
            const engine::UIConfig& config,
            engine::Input& input,
            engine::AssetManager& assets,
            engine::FontHandle font,
            engine::FontHandle smallFont);
    void DrawStatusPanel(
            engine::UIContext& ui,
            const engine::UIConfig& config,
            engine::AssetManager& assets,
            engine::FontHandle smallFont);

    bool PointInTopologyLoop(Vector2 mapPoint, const SectorTopologyLoop& loop) const;
    bool PointInTopologySector(Vector2 mapPoint, const SectorTopologySector& sector) const;
    SectorSurfaceHit PickSectorSurface3D(Vector2 mousePosition, Rectangle viewportRect) const;
    int FindTopologySectorAt(Vector2 mapPoint, bool* outMultipleMatches = nullptr) const;
    bool FindTopologyLineNearScreenPoint(
            Vector2 screenPoint,
            Vector2 mapPoint,
            int& outLineDefId,
            int& outSideDefId,
            SectorTopologySideKind& outSide,
            bool& outPreferredMissing) const;
    int FindAuthoringLineNearScreenPoint(Vector2 screenPoint) const;
    bool FindAuthoringVertexNearScreenPoint(
            Vector2 screenPoint,
            int& outVertexId,
            SectorTopologyCoordPoint& outPoint) const;
    bool FindAuthoringSelectionNearScreenPoint(
            Vector2 screenPoint,
            SectorAuthoringSelectionTarget& outTarget,
            SectorTopologyCoordPoint& outVertexPoint) const;
    int FindTopologyLightNearScreenPoint(Vector2 screenPoint) const;
    int FindTopologyStaticSpotLightNearScreenPoint(Vector2 screenPoint) const;
    int FindTopologyDynamicLightNearScreenPoint(Vector2 screenPoint) const;
    int FindTopologyDynamicSpotLightNearScreenPoint(Vector2 screenPoint) const;
    int FindRuntimeObjectNearScreenPoint(Vector2 screenPoint) const;
    bool FindTopologyStaticSpotLightHandleNearScreenPoint(
            Vector2 screenPoint,
            int& outLightId,
            SpotLightHandle& outHandle) const;
    bool FindTopologyDynamicSpotLightHandleNearScreenPoint(
            Vector2 screenPoint,
            int& outLightId,
            SpotLightHandle& outHandle) const;
    bool FindTopologyVertexNearScreenPoint(
            Vector2 screenPoint,
            int& outVertexId,
            SectorTopologyCoordPoint& outPoint) const;
    bool SnapAuthoringVertexMoveTarget(
            Vector2 mapPoint,
            SectorTopologyCoordPoint& outPoint,
            std::string& error) const;
    void ResetToBlankMap(engine::EngineContext& context);
    bool LoadLevel(engine::EngineContext& context, const std::string& levelName, const std::string& jsonAssetPath);
    void OpenNewConfirmation(engine::AssetManager& assets);
    void OpenReloadConfirmation(engine::AssetManager& assets);
    void OpenSaveLevelModal();
    void OpenLoadLevelModal();
    void OpenConfirmation(const char* title, const char* message, std::function<void()> onOkay);
    bool SaveCurrentLevel();
    bool SaveLevelFromModal(bool overwriteConfirmed = false);
    bool SaveLevelWithPlan(
            const std::string& name,
            const SectorEditorSaveLevelPlan& savePlan,
            std::string& errorMessage);
    void RefreshLevelList();
    bool HasDocumentModalOpen() const;
    bool TryEnterPreview3D(engine::EngineContext& context, engine::UIContext& ui);
    void LeavePreview3D();
    SectorViewPose ActivePreviewPose() const;
    void ApplyGameplayPoseToPreview();
    void TogglePreviewControlMode();
    bool StartLightPilot();
    bool ApplyLightPilotFromPreviewPose();
    void CancelLightPilotWithPreviewRestore(const char* message);
    bool StartLightProxyPlacement(LightProxyPlacementKind proxyKind);
    bool PreviewLightProxyPlacementOffset(Vector3 offsetWorld);
    bool ApplyLightProxyPlacement();
    void CancelLightProxyPlacement(const char* message);
    bool RebuildSectorCollisionWorld();
    SectorFpsVerticalContext BuildGameplayVerticalContext();
    void RefreshGameplaySectorAndVerticalContext();
    void InitializeGameplayVerticalState();
    void OpenPreviewSettingsModal();
    void ApplyPreviewSettingsModal(engine::AssetManager& assets);
    void OpenDoorTextureSettingsModal();
    SectorEditorManipulationServiceContext BuildManipulationServiceContext();
    SectorEditorSelectionServiceContext BuildSelectionServiceContext();
    SectorAuthoringGraph& AuthoringGraph();
    const SectorAuthoringGraph& AuthoringGraph() const;
    SectorTopologySector* SelectedTopologySector();
    const SectorTopologySector* SelectedTopologySector() const;
    SectorTopologyVertex* SelectedTopologyVertex();
    const SectorTopologyVertex* SelectedTopologyVertex() const;
    SectorTopologySideDef* SelectedTopologySideDef();
    const SectorTopologySideDef* SelectedTopologySideDef() const;
    SectorTopologyLineDef* SelectedTopologyLineDef();
    const SectorTopologyLineDef* SelectedTopologyLineDef() const;
    SectorTopologyStaticPointLight* SelectedTopologyLight();
    const SectorTopologyStaticPointLight* SelectedTopologyLight() const;
    SectorTopologyStaticSpotLight* SelectedTopologyStaticSpotLight();
    const SectorTopologyStaticSpotLight* SelectedTopologyStaticSpotLight() const;
    SectorTopologyDynamicPointLight* SelectedTopologyDynamicLight();
    const SectorTopologyDynamicPointLight* SelectedTopologyDynamicLight() const;
    SectorTopologyDynamicSpotLight* SelectedTopologyDynamicSpotLight();
    const SectorTopologyDynamicSpotLight* SelectedTopologyDynamicSpotLight() const;
    void ClearStaleTopologySelection();
    void SyncSelectedSectorIdBuffer();
    void SyncSelectedLightIdBuffer();
    void SelectTopologySector(int sectorId);
    void SelectTopologyVertex(int vertexId);
    void SelectTopologySideDef(int sideDefId, TopologyWallPart wallPart);
    void SelectTopologyLineDef(int lineDefId, SectorTopologySideKind side, TopologyWallPart wallPart);
    void SelectTopologyLight(int topologyLightId);
    void SelectTopologyStaticSpotLight(int topologyLightId);
    void SelectTopologyDynamicLight(int topologyLightId);
    void SelectTopologyDynamicSpotLight(int topologyLightId);
    void SelectRuntimeObject(int objectId);
    SectorPlacedRuntimeObject* SelectedRuntimeObject();
    const SectorPlacedRuntimeObject* SelectedRuntimeObject() const;
    void SelectAuthoringLine(int lineId);
    bool DeleteSelectedAuthoringLine();
    void SelectAuthoringVertex(int vertexId);
    bool DeleteSelectedAuthoringVertex();
    void SelectAuthoringFaceAnchor(int faceAnchorId);
    void SelectSurface3D(SectorSurfaceRef surface);
    bool IsValidSurfaceRef(SectorSurfaceRef surface) const;
    bool SameSurfaceRef(SectorSurfaceRef a, SectorSurfaceRef b) const;
    TopologySurfaceEditTarget TopologyEditTargetForSurface(SectorSurfaceRef surface) const;
    bool IsValidTopologySurfaceEditTarget(TopologySurfaceEditTarget target) const;
    void ResetSurface3DUiState();
    Rectangle BuildPreviewUvPanelRect() const;
    bool SetAuthoringLineDefBlocksPlayer(int lineDefId, bool blocksPlayer);
    SectorEditorMaterialEditingService BuildMaterialEditingService();
    bool CopySelectedConfig(engine::AssetManager& assets);
    bool PasteSelectedConfig(engine::AssetManager& assets);
    SectorEditorFootstepService BuildFootstepService();
    SectorEditorLightEditingService BuildLightEditingService();
    SectorEditorRuntimeObjectEditingService BuildRuntimeObjectEditingService(
            SectorEditorSelectionServiceContext* selectionService = nullptr);
    SectorEditorSoundService BuildSoundService(
            SectorEditorRuntimeObjectEditingService* runtimeObjectEditing = nullptr,
            SectorEditorSoundEmitterEditingService* soundEmitterEditing = nullptr);
    SectorEditorSoundEditorService BuildSoundEditorService();
    SectorEditorPatrolEditorService BuildPatrolEditorService();
    SectorEditorTextureCatalogService MakeTextureCatalogService();
    SectorEditorNpcEditorService BuildNpcEditorService();
    SectorEditorWeaponEditorService BuildWeaponEditorService();
    SectorEditorItemEditorService BuildItemEditorService();
    SectorEditorMaterialRegistryEditorService BuildMaterialRegistryEditorService();
    SectorEditorPlayerSettingsService BuildPlayerSettingsService();
    void OpenWeaponEditor(bool fromPreview3D);
    void DrawItemEditor(
            engine::UIContext& ui,
            const engine::UIConfig& config,
            engine::Input& input,
            engine::AssetManager& assets,
            engine::FontHandle font,
            engine::FontHandle smallFont);
    SectorEditorDocumentLifecycleAccess Lifecycle();
    SectorEditorConstDocumentLifecycleAccess Lifecycle() const;
    SectorTopologyMap& TopologyMap();
    const SectorTopologyMap& TopologyMap() const;
    bool HasAuthoringGraphData() const;
    bool EnsureSelectedSurface3DAuthoringMappingCurrent();
    bool FinishTopologyActionResult(const SectorEditorTopologyActionResult& result);
    bool RebuildPreviewMeshesPreservingView(engine::EngineContext& context);
    void ClearTransientTopologyEditStateAfterGeometryChange();
    void ClearTopologySelectionOnly();
    void ClearSelection();
    void OpenMapSkyTexturePicker();
    void OpenSelectedDoorTexturePicker();
    void OpenSelectedBillboardSpritePicker();
    void ApplySelectedBillboardSpritePickerSelection();
    void ApplyTexturePickerSelection(engine::AssetManager& assets);
    std::string CurrentTextureForPickerTarget() const;
    bool TryRenameSelectedDerivedSectorAuthoringName();
    void MarkTopologyDocumentEdited(const char* status);
    void RefreshResolvedMaterials();
    bool OpenDeleteSelectedLightConfirmation();
    bool ConvertSelectedLight();
    SectorEditorToolContext BuildToolContext(engine::Input* input);
    void AddRuntimeObjectAt(Vector2 mapPoint);
    void AddStaticModelAt(Vector2 mapPoint);
    void AddDynamicModelAt(Vector2 mapPoint);
    void AddItemAt(Vector2 mapPoint);
    void AddNpcAt(Vector2 mapPoint);
    void AddDoorAtPortal(Vector2 screenPoint);
    bool DeleteSelectedRuntimeObject();
    bool DeleteRuntimeObjectById(int objectId);
    bool MutateSelectedRuntimeObject(
            const char* status,
            const std::function<bool(SectorPlacedRuntimeObject&)>& mutate);
    void RefreshRuntimeObjectsAfterAuthoringEdit();
    bool OpenLightmapBakeSetup();
    bool StartLightmapBake(SectorLightmapBakeQualityPreset qualityPreset);
    void PollLightmapBakeResult(engine::AssetManager& assets);
    bool InstallLightmapBakeResult(const SectorLightmapBakeAsyncResult& result, engine::AssetManager& assets);
    void ProcessPendingReflectionProbeBake(engine::EngineContext& context);
    bool BakeReflectionProbes(engine::EngineContext& context, int selectedProbeId);
    SectorEditorState state;
    SectorEditorDocumentState documentState;
    SectorEditorPreviewState previewState;
    SelectionState selectionState;
    ManipulationState manipulationState;
    LightEditingState lightEditingState;
    SectorEditorUiState uiState;
    RuntimeObjectEditingState runtimeObjectEditingState;
    RuntimeObjectEditingUiState runtimeObjectEditingUiState;
    SectorEditorNpcEditorState npcEditorState;
    SectorEditorNpcEditorSessionState npcEditorSessionState;
    SectorEditorWeaponEditorState weaponEditorState;
    SectorEditorWeaponEditorSessionState weaponEditorSessionState;
    SectorEditorItemEditorState itemEditorState;
    SectorEditorItemEditorSessionState itemEditorSessionState;
    SectorEditorPlayerSettingsState playerSettingsState;
    SectorEditorMaterialRegistryEditorState materialRegistryEditorState;
    SectorEditorSoundEditorState soundEditorState;
    SectorEditorPatrolEditorState patrolEditorState;
    SectorEditorAudioAssetPickerSessionState audioAssetPickerSessionState;
    InspectorIdUiState inspectorIdUiState;
    TextureCatalogState textureCatalogState;
    SectorEditorSoundCatalogState soundCatalogState;
    SectorEditorConfigClipboardState configClipboardState;
    MaterialEditingUiState materialEditingUiState;
    FogVolumeEditingUiState fogVolumeEditingUiState;
    ReflectionProbeEditingUiState reflectionProbeEditingUiState;
    std::optional<SectorEditorAuthoringFogVolumeEditingService> fogVolumeEditingService;
    std::optional<SectorEditorReflectionProbeEditingService> reflectionProbeEditingService;
    LevelMarkerEditingState levelMarkerEditingState;
    LevelMarkerEditingUiState levelMarkerEditingUiState;
    SectorEditorAuthoringFaceMergeState authoringFaceMergeState;
    std::optional<SectorEditorAuthoringFaceMergeService> authoringFaceMergeService;
    std::optional<SectorEditorLevelMarkerEditingService> levelMarkerEditingService;
    SoundEmitterEditingState soundEmitterEditingState;
    SoundEmitterEditingUiState soundEmitterEditingUiState;
    std::optional<SectorEditorSoundEmitterEditingService> soundEmitterEditingService;
    TriggerEditingState triggerEditingState;
    TriggerEditingUiState triggerEditingUiState;
    std::optional<SectorEditorTriggerEditingService> triggerEditingService;
    SectorEditorLightmapBakeController lightmapBake;
    bool reflectionProbeBakePending = false;
    int reflectionProbeBakeSelectedId = -1;
    Rectangle canvasRect = {};
    std::string statusText;
    SectorSceneRuntime sceneRuntime;
    FpsPlayerRuntime fpsPlayer;
    FpsWeaponRegistry weaponRegistry;
    SectorMaterialRegistry& materialRegistry;
    ItemRegistry& itemRegistry;
    ItemModelAssetState& itemModelAssets;
    FpsApplicationSettings& applicationSettings;
    PlayerAudioRuntime playerAudio;
    std::string applicationSettingsPath;
    std::string weaponRegistryPath;
    std::string itemRegistryPath;
    std::string weaponRegistryError;
    SectorUseTarget previewUseTarget;
    std::array<char, 128> previewUsePromptTitle{};
    engine::EngineContext* engineContext = nullptr;
    bool initialized = false;
    bool gameSessionExists = false;
    bool clearGameSessionRequested = false;
    bool playerAudioSettingsChanged = false;
};

} // namespace game
