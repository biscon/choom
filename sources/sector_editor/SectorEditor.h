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
    void RenderUI(
            engine::UIContext& ui,
            const engine::UIConfig& config,
            engine::Input& input,
            engine::AssetManager& assets,
            engine::FontHandle font);

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
    void StartLightDrag(int lightIndex);
    void UpdateLightDrag(engine::Input& input);
    void FinishLightDrag();
    void CancelLightDrag(const char* message);
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
    void DrawSectors() const;
    void DrawSector(const SectorDefinition& sector, int sectorIndex) const;
    void DrawStaticLights() const;
    Vector2 SelectedEdgeInwardNormal(const SectorDefinition& sector, int edgeIndex) const;
    void DrawPendingSector() const;
    void DrawVertexMoveOverlay() const;
    void DrawLightMoveOverlay() const;
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
    void DrawStatusPanel(
            engine::UIContext& ui,
            const engine::UIConfig& config,
            engine::AssetManager& assets,
            engine::FontHandle font);

    bool PointInSectorPolygon(Vector2 mapPoint, const SectorDefinition& sector) const;
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
    int FindSectorAt(Vector2 mapPoint) const;
    int FindLightNearScreenPoint(Vector2 screenPoint) const;
    bool FindEdgeNearScreenPoint(Vector2 screenPoint, SectorEdgeRef& outEdge) const;
    std::vector<SectorEdgeHitCandidate> FindEdgeHitCandidates(Vector2 screenPoint) const;
    bool ResolveEdgeHit(Vector2 screenPoint, Vector2 rawMapPoint, SectorEdgeRef& outEdge) const;
    bool FindTopologyVertexNearScreenPoint(
            Vector2 screenPoint,
            int& outVertexId,
            SectorTopologyCoordPoint& outPoint) const;
    bool SnapTopologyVertexMoveTarget(
            Vector2 mapPoint,
            SectorTopologyCoordPoint& outPoint,
            std::string& error) const;
    bool ValidateExistingSectorMap(const SectorMap& map, std::string& error) const;
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
    std::string GenerateUniqueSectorId() const;
    SectorTopologySector* SelectedTopologySector();
    const SectorTopologySector* SelectedTopologySector() const;
    SectorTopologySideDef* SelectedTopologySideDef();
    const SectorTopologySideDef* SelectedTopologySideDef() const;
    SectorTopologyLineDef* SelectedTopologyLineDef();
    const SectorTopologyLineDef* SelectedTopologyLineDef() const;
    void ClearStaleTopologySelection();
    void SyncSelectedSectorIdBuffer();
    void SyncSelectedLightIdBuffer();
    void SelectTopologySector(int sectorId);
    void SelectTopologySideDef(int sideDefId, TopologyWallPart wallPart);
    void SelectTopologyLineDef(int lineDefId, SectorTopologySideKind side, TopologyWallPart wallPart);
    void SelectSector(int sectorIndex);
    void SelectEdge(
            int sectorIndex,
            int edgeIndex,
            SectorBoundaryRingKind ringKind = SectorBoundaryRingKind::Outer,
            int holeIndex = -1);
    void SelectLight(int lightIndex);
    void SelectSurface3D(SectorSurfaceRef surface);
    bool IsValidSurfaceRef(SectorSurfaceRef surface) const;
    bool SameSurfaceRef(SectorSurfaceRef a, SectorSurfaceRef b) const;
    void ResetSurface3DUiState();
    Rectangle BuildPreviewUvPanelRect() const;
    std::string CurrentTextureForSurface(SectorSurfaceRef surface) const;
    TexturePickerTargetKind TexturePickerTargetForSurface(SectorSurfaceRef surface) const;
    bool ApplySurface3DUvValue(SectorSurfaceRef surface, int component, float value, engine::AssetManager& assets);
    bool ResetSurface3DUv(SectorSurfaceRef surface, engine::AssetManager& assets);
    bool RebuildPreviewMeshesPreservingView(engine::AssetManager& assets);
    bool SplitSelectedEdge(engine::AssetManager& assets);
    void ClearSelection();
    SectorEdgeOverride* FindMutableEdgeOverride(SectorBoundaryEdgeRef edge);
    const SectorEdgeOverride* FindEdgeOverride(SectorBoundaryEdgeRef edge) const;
    SectorEdgeOverride& EnsureEdgeOverride(SectorBoundaryEdgeRef edge);
    void RemoveEdgeOverrideIfEmpty(SectorBoundaryEdgeRef edge);
    void OpenTopologyTexturePicker(int sectorId, TopologySectorTextureField field);
    void OpenTopologySideDefTexturePicker(int sideDefId, TopologyWallPart wallPart);
    void ApplyTexturePickerSelection(engine::AssetManager& assets);
    std::string CurrentTextureForPickerTarget() const;
    bool TryRenameSelectedTopologySector();
    void MarkTopologyDocumentEdited(const char* status);
    bool TryRenameSelectedSector();
    bool TryRenameSelectedLight();
    bool DeleteSelectedSector();
    bool DeleteSectorAt(int sectorIndex);
    bool DeleteSelectedLight();
    bool DeleteLightAt(int lightIndex);
    void AddStaticLightAt(Vector2 mapPoint);
    std::string GenerateUniqueLightId() const;
    bool BakeLightmaps();
    bool StartLightmapBake();
    void PollLightmapBakeResult(engine::AssetManager& assets);
    void RequestLightmapBakeCancel();
    void JoinLightmapBakeWorker();
    void ShutdownLightmapBake();
    bool IsLightmapBakeBlocking() const;
    bool ConsumeLightmapBakeResult(const SectorLightmapBakeAsyncResult& result, engine::AssetManager& assets);
    bool InstallLightmapBakeResult(const SectorLightmapBakeAsyncResult& result, engine::AssetManager& assets);
    bool ValidatePendingPoint(SectorPoint point, std::string& error) const;
    bool ValidateSectorPolygon(const std::vector<SectorPoint>& points, std::string& error) const;
    bool ValidateInsertSectorPolygon(
            int parentSectorIndex,
            const std::vector<SectorPoint>& points,
            std::string& error) const;

    SectorEditorState state;
    SectorEditorUiState uiState;
    LightmapBakeAsyncState lightmapBake;
    Rectangle canvasRect = {};
    std::string statusText;
    SectorMeshPreview preview;
    bool initialized = false;
};

} // namespace game
