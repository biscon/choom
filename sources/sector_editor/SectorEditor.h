#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/EngineContext.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/SectorEditorLightmapAsyncTypes.h"
#include "sector_editor/SectorEditorMaterialActions.h"
#include "sector_editor/services/lights/SectorEditorLightEditingService.h"
#include "sector_editor/services/material_edit/SectorEditorMaterialEditingService.h"
#include "sector_editor/services/texture_catalog/SectorEditorTextureCatalogService.h"
#include "sector_editor/tools/placed_objects/SectorEditorPlacedObjectActions.h"
#include "sector_editor/tools/placed_objects/SectorEditorPlacedObjectDrag.h"
#include "sector_editor/selection/SectorEditorManipulationService.h"
#include "sector_editor/selection/SectorEditorSelectionService.h"
#include "sector_editor/SectorEditorSelectionTypes.h"
#include "sector_editor/SectorEditorTopologyActions.h"
#include "sector_editor/SectorEditorTypes.h"
#include "sector_editor/services/lightmap_bake/SectorEditorLightmapBakeController.h"
#include "sector_demo/renderer/SectorMeshRenderer.h"

#include <raylib.h>

#include <functional>
#include <string>
#include <vector>

namespace game {

struct SectorEditorToolContext;

class SectorEditor {
public:
    bool Init(engine::EngineContext& context);
    void Shutdown(engine::EngineContext& context);

    void Update(engine::EngineContext& context, float dt);
    void Render(engine::AssetManager& assets);
    void RenderPreview3DShadowMaps(engine::AssetManager& assets);
    void RenderPreview3DScene(engine::EngineContext& context);
    void RenderPreview3DOverlays();
    void ApplyPreview3DBloom(engine::AssetManager& assets, RenderTexture2D& sceneTarget);
    void RenderUI(
            engine::UIContext& ui,
            const engine::UIConfig& config,
            engine::Input& input,
            engine::AssetManager& assets,
            engine::FontHandle font,
            engine::FontHandle smallFont);
    bool IsPreview3DActive() const;

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
    void DrawLightMoveOverlay() const;
    void DrawCanvasOverlay(engine::AssetManager& assets, engine::FontHandle font) const;
    void RenderPreview3D(engine::AssetManager& assets);
    void DrawPreviewSurfaceHighlights() const;
    void DrawPreviewSpotLightOverlay() const;
    void DrawPreviewObjectProbeOverlay() const;
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
    void DrawTexturePickerModal(
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
    void DrawAddMapTextureModal(
            engine::UIContext& ui,
            const engine::UIConfig& config,
            engine::Input& input,
            engine::AssetManager& assets,
            engine::FontHandle font);
    void DrawSpritePickerModal(
            engine::UIContext& ui,
            const engine::UIConfig& config,
            engine::Input& input,
            engine::AssetManager& assets,
            engine::FontHandle font);
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
    bool SaveLevelFromModal(bool overwriteConfirmed = false);
    void RefreshLevelList();
    bool HasDocumentModalOpen() const;
    bool TryEnterPreview3D(engine::EngineContext& context, engine::UIContext& ui);
    void LeavePreview3D();
    SectorViewPose ActivePreviewPose() const;
    void ApplyGameplayPoseToPreview();
    void TogglePreviewControlMode();
    bool StartSpotLightPilot();
    bool ApplySpotLightPilotFromPreviewPose();
    void CancelSpotLightPilotWithPreviewRestore(const char* message);
    bool RebuildSectorCollisionWorld();
    SectorFpsVerticalContext BuildGameplayVerticalContext();
    void RefreshGameplaySectorAndVerticalContext();
    void InitializeGameplayVerticalState();
    void OpenPreviewSettingsModal();
    void ApplyPreviewSettingsModal(engine::AssetManager& assets);
    void OpenDoorTextureSettingsModal();
    void RefreshDefaultTextures();
    void RefreshEditorTextureAssets(engine::AssetManager& assets);
    engine::TextureHandle EditorTextureHandleForId(const std::string& textureId) const;
    void OpenAddMapTextureModal(engine::AssetManager& assets);
    void CloseAddMapTextureModal(engine::AssetManager& assets);
    void RefreshAddMapTextureScan();
    void SelectAddMapTexturePath(int pathIndex);
    void RefreshAddMapTexturePreview(engine::AssetManager& assets);
    bool ValidateAddMapTextureId(std::string& error) const;
    bool AddSelectedMapTexture(engine::AssetManager& assets);
    SectorEditorManipulationServiceContext BuildManipulationServiceContext();
    SectorEditorSelectionServiceContext BuildSelectionServiceContext();
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
    SectorEditorLightEditingService BuildLightEditingService();
    SectorEditorTextureCatalogService BuildTextureCatalogService();
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
    bool OpenDeleteSelectedLightConfirmation();
    SectorEditorToolContext BuildToolContext(engine::Input* input);
    SectorEditorPlacedObjectDragContext BuildRuntimeObjectDragContext();
    SectorEditorPlacedObjectActionContext BuildRuntimeObjectActionContext();
    void AddRuntimeObjectAt(Vector2 mapPoint);
    void AddDoorAtPortal(Vector2 screenPoint);
    bool DeleteSelectedRuntimeObject();
    bool DeleteRuntimeObjectById(int objectId);
    bool MutateSelectedRuntimeObject(
            const char* status,
            const std::function<bool(SectorPlacedRuntimeObject&)>& mutate);
    void RefreshRuntimeObjectsAfterAuthoringEdit();
    bool StartLightmapBake();
    void PollLightmapBakeResult(engine::AssetManager& assets);
    bool InstallLightmapBakeResult(const SectorLightmapBakeAsyncResult& result, engine::AssetManager& assets);
    SectorEditorState state;
    SectorEditorUiState uiState;
    SectorEditorLightmapBakeController lightmapBake;
    Rectangle canvasRect = {};
    std::string statusText;
    SectorMeshRenderer preview;
    engine::EngineContext* engineContext = nullptr;
    bool initialized = false;
};

} // namespace game
