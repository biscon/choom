#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/SectorEditorTypes.h"
#include "sector_demo/SectorMeshPreview.h"
#include "sector_demo/SectorTopologyCreation.h"
#include "sector_demo/SectorTopologyEdit.h"

#include <raylib.h>

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
    void StartVertexDrag(int vertexId, SectorTopologyCoordPoint point);
    void UpdateVertexDrag(engine::Input& input);
    void FinishVertexDrag();
    void CancelVertexDrag(const char* message);
    void StartLightDrag(int topologyLightId);
    void UpdateLightDrag(engine::Input& input);
    void FinishLightDrag();
    void CancelLightDrag(const char* message);
    void StartPendingTopologyVertexMerge(int sourceVertexId);
    void CancelPendingTopologyVertexMerge(const char* message);
    void UpdatePendingTopologyVertexMerge(engine::Input& input);
    void CommitPendingTopologyVertexMerge();
    void StartPendingTopologyLineSplitAtPoint();
    void CancelPendingTopologyLineSplitAtPoint(const char* message);
    bool ValidatePendingTopologyLineSplitAtPointTarget(const char* staleMessage);
    void UpdatePendingTopologyLineSplitAtPoint(engine::Input& input);
    void CommitPendingTopologyLineSplitAtPoint();
    void StartPendingTopologySectorCut();
    void CancelPendingTopologySectorCut(const char* message);
    bool ValidatePendingTopologySectorCutTarget(const char* staleMessage);
    void UpdatePendingTopologySectorCut(engine::Input& input);
    void CommitPendingTopologySectorCut();
    void UpdatePreview3D(engine::Input& input, float dt);
    void UpdatePreview3DSelection(engine::Input& input);
    void CancelPendingSector(const char* message);
    void StartInsertSectorInside();
    bool IsPendingInsertParentValid() const;
    void RemoveLastPendingSectorPoint();
    void AddPendingSectorPoint(SectorPoint point);
    void FinalizePendingSector();
    bool CanClosePendingSectorAt(SectorPoint point) const;
    SectorPoint CurrentSnappedSectorPoint() const;
    bool ToTopologyCoordPoint(SectorPoint point, SectorTopologyCoordPoint& outPoint, std::string& error) const;
    bool ToCanonicalSectorPoint(SectorPoint point, SectorPoint& outPoint, std::string& error) const;
    bool BuildPendingTopologyPoints(std::vector<SectorTopologyCoordPoint>& outPoints, std::string& error) const;
    bool ValidatePendingTopologyPoint(SectorPoint point, std::string& error) const;
    SectorTopologyCreatePolygonOptions BuildTopologyCreateOptions() const;

    void DrawGrid() const;
    void DrawTopologyDocument();
    void DrawTopologySectorLoops(const SectorTopologySector& sector, Color fill, Color outline, float outlineThickness = 2.0f);
    void DrawTopologySelectedLineHighlight() const;
    void DrawTopologyLineDefs() const;
    void DrawTopologyVertices() const;
    void DrawTopologySnapCrosshair() const;
    void DrawStaticLights() const;
    void DrawPendingSector() const;
    void DrawVertexMoveOverlay() const;
    void DrawPendingTopologyVertexMerge() const;
    void DrawLightMoveOverlay() const;
    void DrawPendingTopologyLineSplitAtPoint() const;
    void DrawPendingTopologySectorCut() const;
    void DrawCanvasOverlay(engine::AssetManager& assets, engine::FontHandle font) const;
    void RenderPreview3D(engine::AssetManager& assets);
    void DrawPreviewSurfaceHighlights() const;
    void DrawPreviewOverlay(
            const engine::UIConfig& config,
            engine::AssetManager& assets,
            engine::FontHandle font) const;
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
    bool DrawTopologySectorInspector(
            engine::UIContext& ui,
            const engine::UIConfig& config,
            engine::Input& input,
            engine::AssetManager& assets,
            engine::FontHandle font,
            engine::UIScrollAreaResult scroll,
            float contentW,
            float rowH,
            float gap);
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
    int FindTopologyLightNearScreenPoint(Vector2 screenPoint) const;
    bool FindTopologyVertexNearScreenPoint(
            Vector2 screenPoint,
            int& outVertexId,
            SectorTopologyCoordPoint& outPoint) const;
    bool FindSelectedSectorBoundaryCutPointNearScreenPoint(
            Vector2 screenPoint,
            SectorTopologyBoundaryCutPoint& outPoint) const;
    int FindTopologyVertexAtCoordPoint(
            SectorTopologyCoordPoint point,
            int excludedVertexId = -1) const;
    bool SnapTopologyVertexMoveTarget(
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
    void ClearStaleTopologySelection();
    void SyncSelectedSectorIdBuffer();
    void SyncSelectedLightIdBuffer();
    void SelectTopologySector(int sectorId);
    void SelectTopologyVertex(int vertexId);
    void SelectTopologySideDef(int sideDefId, TopologyWallPart wallPart);
    void SelectTopologyLineDef(int lineDefId, SectorTopologySideKind side, TopologyWallPart wallPart);
    void SelectTopologyLight(int topologyLightId);
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
    bool ResetSurface3DUv(TopologySurfaceEditTarget target, TopologyMaterialLayer layer, engine::AssetManager& assets);
    bool FitSelectedDecal(TopologySurfaceEditTarget target, engine::AssetManager* assets);
    bool FitSelectedFlatDecal(TopologySurfaceEditTarget target, engine::AssetManager* assets);
    bool FitSelectedWallMaterial(TopologySurfaceEditTarget target, TopologyUvFitMode mode, engine::AssetManager* assets, TopologyMaterialLayer layer);
    bool AlignSelectedWallMaterialVertical(TopologySurfaceEditTarget target, engine::AssetManager* assets, TopologyMaterialLayer layer);
    bool AlignSelectedWallMaterialU(TopologySurfaceEditTarget target, TopologyUAlignDirection direction, engine::AssetManager* assets, TopologyMaterialLayer layer);
    bool FinishTopologyMaterialMutation(const char* status, engine::AssetManager* assets);
    bool RebuildPreviewMeshesPreservingView(engine::AssetManager& assets);
    bool SplitSelectedTopologyLineDef();
    bool DissolveSelectedTopologyVertex();
    bool JoinSelectedTopologySectors();
    void ClearTransientTopologyEditStateAfterGeometryChange();
    void OpenDeleteSelectedTopologySectorConfirmation();
    void OpenDeleteTopologySectorConfirmation(int sectorId);
    bool DeleteSelectedTopologySectorConfirmed(int sectorId);
    void ClearSelection();
    void OpenTopologyTexturePicker(int sectorId, TopologySectorTextureField field, TopologyMaterialLayer layer);
    void OpenTopologySideDefTexturePicker(int sideDefId, TopologyWallPart wallPart, TopologyMaterialLayer layer);
    void ApplyTexturePickerSelection(engine::AssetManager& assets);
    std::string CurrentTextureForPickerTarget() const;
    bool TryRenameSelectedTopologySector();
    void MarkTopologyDocumentEdited(const char* status);
    bool TryRenameSelectedLight();
    bool DeleteSelectedLight();
    bool DeleteLightById(int topologyLightId);
    void AddStaticLightAt(Vector2 mapPoint);
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
