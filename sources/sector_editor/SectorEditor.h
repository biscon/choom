#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/SectorEditorMaterialActions.h"
#include "sector_editor/SectorEditorTopologyActions.h"
#include "sector_editor/SectorEditorTypes.h"
#include "sector_demo/SectorMeshPreview.h"

#include <raylib.h>

#include <functional>
#include <string>

namespace game {

class SectorEditor {
public:
    bool Init(engine::AssetManager& assets);
    void Shutdown(engine::AssetManager& assets);

    void Update(engine::Input& input, float dt);
    void Render(engine::AssetManager& assets);
    void RenderPreview3DScene(engine::AssetManager& assets);
    void RenderPreview3DOverlays();
    void ApplyPreview3DBloom(engine::AssetManager& assets, RenderTexture2D& sceneTarget);
    void RenderUI(
            engine::UIContext& ui,
            const engine::UIConfig& config,
            engine::Input& input,
            engine::AssetManager& assets,
            engine::FontHandle font);
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
    void StartAuthoringVertexDrag(int vertexId, SectorTopologyCoordPoint point);
    void UpdateAuthoringVertexDrag(engine::Input& input);
    void FinishAuthoringVertexDrag();
    void CancelAuthoringVertexDrag(const char* message);
    void StartLightDrag(
            int topologyLightId,
            DynamicSpotLightHandle dynamicSpotHandle = DynamicSpotLightHandle::Origin);
    void UpdateLightDrag(engine::Input& input);
    void FinishLightDrag();
    void CancelLightDrag(const char* message);
    void UpdatePreview3D(engine::Input& input, float dt);
    void UpdatePreview3DSelection(engine::Input& input);
    void CancelPendingAuthoringLine(const char* message);
    void CancelPendingAuthoringRectangle(const char* message);
    void CancelPendingAuthoringInsertVertex(const char* message);
    void BeginPendingAuthoringInsertVertex(int lineId);
    void AddAuthoringLinePoint(SectorPoint point);
    void AddAuthoringRectanglePoint(SectorPoint point);
    bool TryResolveAuthoringInsertVertexPoint(
            int lineId,
            Vector2 mapPoint,
            SectorTopologyCoordPoint& outPoint,
            std::string& error) const;
    void UpdatePendingAuthoringInsertVertex(Vector2 mapPoint);
    void CommitAuthoringInsertVertex(Vector2 screenPoint);
    SectorPoint CurrentSnappedSectorPoint() const;
    bool ToTopologyCoordPoint(SectorPoint point, SectorTopologyCoordPoint& outPoint, std::string& error) const;
    bool ToCanonicalSectorPoint(SectorPoint point, SectorPoint& outPoint, std::string& error) const;

    void DrawGrid() const;
    void InvalidateTopologyRenderCache();
    void EnsureTopologyRenderCache();
    void DrawTopologyDocument();
    void DrawTopologySelectedLineHighlight() const;
    void DrawTopologySnapCrosshair() const;
    void DrawPendingAuthoringLine() const;
    void DrawPendingAuthoringRectangle() const;
    void DrawPendingAuthoringInsertVertex() const;
    void DrawAuthoringVertexMoveOverlay() const;
    void DrawLightMoveOverlay() const;
    void DrawCanvasOverlay(engine::AssetManager& assets, engine::FontHandle font) const;
    void RenderPreview3D(engine::AssetManager& assets);
    void DrawPreviewSurfaceHighlights() const;
    void DrawPreviewDynamicSpotLightOverlay() const;
    void DrawPreviewOverlay(
            engine::UIContext& ui,
            const engine::UIConfig& config,
            engine::Input& input,
            engine::AssetManager& assets,
            engine::FontHandle font);
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
            engine::FontHandle font);
    bool DrawTopologySideDefInspector(
            engine::UIContext& ui,
            const engine::UIConfig& config,
            engine::Input& input,
            engine::AssetManager& assets,
            engine::FontHandle font,
            engine::UIScrollAreaResult scroll,
            float contentW,
            float rowH,
            float gap);
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
            engine::FontHandle font);

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
    int FindTopologyDynamicLightNearScreenPoint(Vector2 screenPoint) const;
    int FindTopologyDynamicSpotLightNearScreenPoint(Vector2 screenPoint) const;
    bool FindTopologyDynamicSpotLightHandleNearScreenPoint(
            Vector2 screenPoint,
            int& outLightId,
            DynamicSpotLightHandle& outHandle) const;
    bool FindTopologyVertexNearScreenPoint(
            Vector2 screenPoint,
            int& outVertexId,
            SectorTopologyCoordPoint& outPoint) const;
    bool SnapAuthoringVertexMoveTarget(
            Vector2 mapPoint,
            SectorTopologyCoordPoint& outPoint,
            std::string& error) const;
    void ResetToBlankMap(engine::AssetManager& assets);
    bool LoadLevel(engine::AssetManager& assets, const std::string& levelName, const std::string& jsonAssetPath);
    void OpenNewConfirmation(engine::AssetManager& assets);
    void OpenReloadConfirmation(engine::AssetManager& assets);
    void OpenSaveLevelModal();
    void OpenLoadLevelModal();
    void OpenConfirmation(const char* title, const char* message, std::function<void()> onOkay);
    bool SaveLevelFromModal(bool overwriteConfirmed = false);
    void RefreshLevelList();
    bool HasDocumentModalOpen() const;
    bool TryEnterPreview3D(engine::AssetManager& assets, engine::UIContext& ui);
    void LeavePreview3D();
    SectorViewPose ActivePreviewPose() const;
    void ApplyGameplayPoseToPreview();
    void TogglePreviewControlMode();
    bool StartDynamicSpotLightPilot();
    bool ApplyDynamicSpotLightPilot();
    void CancelDynamicSpotLightPilot(const char* message);
    bool RebuildSectorCollisionWorld();
    SectorFpsVerticalContext BuildGameplayVerticalContext();
    void RefreshGameplaySectorAndVerticalContext();
    void InitializeGameplayVerticalState();
    void OpenPreviewSettingsModal();
    void ApplyPreviewSettingsModal(engine::AssetManager& assets);
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
    void SelectTopologyDynamicLight(int topologyLightId);
    void SelectTopologyDynamicSpotLight(int topologyLightId);
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
    const SectorTopologyDecalLayer* DecalForSurface(TopologySurfaceEditTarget target) const;
    SectorTopologyDecalLayer* MutableDecalForSurface(TopologySurfaceEditTarget target);
    const SectorTopologyUvSettings* UvForSurface(TopologySurfaceEditTarget target, TopologyMaterialLayer layer) const;
    SectorTopologyUvSettings* MutableUvForSurface(TopologySurfaceEditTarget target, TopologyMaterialLayer layer);
    bool IsDecalAssigned(TopologySurfaceEditTarget target) const;
    std::string CurrentTextureForSurface(TopologySurfaceEditTarget target, TopologyMaterialLayer layer) const;
    bool CopyTopologyMaterial(TopologySurfaceEditTarget target);
    bool PasteTopologyMaterial(TopologySurfaceEditTarget target, engine::AssetManager& assets);
    bool ApplySurface3DUvValue(TopologySurfaceEditTarget target, TopologyMaterialLayer layer, int component, float value, engine::AssetManager& assets);
    bool ApplySurfaceDecalOpacity(TopologySurfaceEditTarget target, float opacity, engine::AssetManager* assets);
    bool ApplySurfaceDecalEmissive(TopologySurfaceEditTarget target, bool emissive, engine::AssetManager* assets);
    bool ApplySurfaceDecalTint(TopologySurfaceEditTarget target, Vector3 tint, engine::AssetManager* assets);
    bool ApplySurfaceDecalBloomIntensity(TopologySurfaceEditTarget target, float bloomIntensity, engine::AssetManager* assets);
    bool OpenDecalTintModal(TopologySurfaceEditTarget target);
    bool ClearSurfaceDecal(TopologySurfaceEditTarget target, engine::AssetManager* assets);
    bool ClearMiddleTexture(TopologySurfaceEditTarget target, engine::AssetManager* assets);
    bool SetLineDefBlocksPlayer(int lineDefId, bool blocksPlayer);
    bool ResetSurface3DUv(TopologySurfaceEditTarget target, TopologyMaterialLayer layer, engine::AssetManager& assets);
    bool FitSelectedDecal(TopologySurfaceEditTarget target, engine::AssetManager* assets);
    bool FitSelectedFlatDecal(TopologySurfaceEditTarget target, engine::AssetManager* assets);
    bool FitSelectedWallMaterial(TopologySurfaceEditTarget target, TopologyUvFitMode mode, engine::AssetManager* assets, TopologyMaterialLayer layer);
    bool AlignSelectedWallMaterialVertical(TopologySurfaceEditTarget target, engine::AssetManager* assets, TopologyMaterialLayer layer);
    bool AlignSelectedWallMaterialU(TopologySurfaceEditTarget target, TopologyUAlignDirection direction, engine::AssetManager* assets, TopologyMaterialLayer layer);
    bool HasAuthoringGraphData() const;
    bool IsSelectedSurface3DFlatTarget(TopologySurfaceEditTarget target) const;
    bool EnsureSelectedSurface3DAuthoringMappingCurrent();
    bool ApplyAuthoringSideMaterialAction(
            TopologySurfaceEditTarget target,
            engine::AssetManager* assets,
            const std::function<SectorEditorMaterialActionResult(SectorTopologyMap&)>& action);
    bool ApplyAuthoringFaceAnchorFlatMaterialAction(
            TopologySurfaceEditTarget target,
            engine::AssetManager* assets,
            const std::function<SectorEditorMaterialActionResult(SectorTopologyMap&)>& action);
    bool FinishAuthoringSideMaterialActionResult(
            TopologySurfaceEditTarget target,
            const SectorEditorMaterialActionResult& result,
            const SectorTopologyMap& editedTopology,
            engine::AssetManager* assets);
    bool FinishMaterialActionResult(const SectorEditorMaterialActionResult& result, engine::AssetManager* assets);
    bool FinishTopologyMaterialMutation(const char* status, engine::AssetManager* assets);
    bool FinishTopologyActionResult(const SectorEditorTopologyActionResult& result);
    bool RebuildPreviewMeshesPreservingView(engine::AssetManager& assets);
    void ClearTransientTopologyEditStateAfterGeometryChange();
    void ClearTopologySelectionOnly();
    void ClearSelection();
    void OpenTopologyTexturePicker(int sectorId, TopologySectorTextureField field, TopologyMaterialLayer layer);
    void OpenTopologySideDefTexturePicker(int sideDefId, TopologyWallPart wallPart, TopologyMaterialLayer layer);
    void OpenMapSkyTexturePicker();
    void ApplyTexturePickerSelection(engine::AssetManager& assets);
    std::string CurrentTextureForPickerTarget() const;
    bool TryRenameSelectedTopologySector();
    void MarkTopologyDocumentEdited(const char* status);
    bool TryRenameSelectedLight();
    bool DeleteSelectedLight();
    bool DeleteLightById(int topologyLightId);
    void AddStaticLightAt(Vector2 mapPoint);
    bool DeleteDynamicLightById(int topologyLightId);
    void AddDynamicLightAt(Vector2 mapPoint);
    bool DeleteDynamicSpotLightById(int topologyLightId);
    void AddDynamicSpotLightAt(Vector2 mapPoint);
    bool BakeLightmaps();
    bool StartLightmapBake();
    void PollLightmapBakeResult(engine::AssetManager& assets);
    void RequestLightmapBakeCancel();
    void JoinLightmapBakeWorker();
    void ShutdownLightmapBake();
    bool IsLightmapBakeBlocking() const;
    bool ConsumeLightmapBakeResult(const SectorLightmapBakeAsyncResult& result, engine::AssetManager& assets);
    bool InstallLightmapBakeResult(const SectorLightmapBakeAsyncResult& result, engine::AssetManager& assets);
    SectorEditorState state;
    SectorEditorUiState uiState;
    LightmapBakeAsyncState lightmapBake;
    Rectangle canvasRect = {};
    std::string statusText;
    SectorMeshPreview preview;
    bool initialized = false;
};

} // namespace game
