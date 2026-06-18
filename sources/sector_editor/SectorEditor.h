#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/SectorEditorTypes.h"
#include "sector_demo/SectorMeshPreview.h"

#include <raylib.h>

#include <string>

namespace game {

class SectorEditor {
public:
    bool Init(engine::AssetManager& assets, const char* mapPath);
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
    Rectangle BuildLeftPanelRect() const;
    Rectangle BuildRightPanelRect() const;
    Rectangle BuildBottomPanelRect() const;
    Rectangle BuildCanvasRect() const;

    bool IsMouseOverCanvas(const engine::Input& input) const;
    void UpdateHoverAndMouse(engine::Input& input);
    void HandleCanvasInput(engine::Input& input, float dt);
    void StartVertexDrag(SectorPoint point, const std::vector<SectorVertexRef>& refs);
    void UpdateVertexDrag(engine::Input& input);
    void FinishVertexDrag();
    void CancelVertexDrag(const char* message);
    void UpdatePreview3D(engine::Input& input, float dt);
    void UpdatePreview3DSelection(engine::Input& input);
    void CancelPendingSector(const char* message);
    void RemoveLastPendingSectorPoint();
    void AddPendingSectorPoint(SectorPoint point);
    void FinalizePendingSector();
    bool CanClosePendingSectorAt(SectorPoint point) const;
    SectorPoint CurrentSnappedSectorPoint() const;

    void DrawGrid() const;
    void DrawSectors() const;
    void DrawSector(const SectorDefinition& sector, int sectorIndex) const;
    void DrawStaticLights() const;
    Vector2 SelectedEdgeInwardNormal(const SectorDefinition& sector, int edgeIndex) const;
    void DrawPendingSector() const;
    void DrawVertexMoveOverlay() const;
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
    void DrawStatusPanel(
            engine::UIContext& ui,
            const engine::UIConfig& config,
            engine::AssetManager& assets,
            engine::FontHandle font);

    bool PointInSectorPolygon(Vector2 mapPoint, const SectorDefinition& sector) const;
    SectorSurfaceHit PickSectorSurface3D(Vector2 mousePosition, Rectangle viewportRect) const;
    int FindSectorAt(Vector2 mapPoint) const;
    int FindLightNearScreenPoint(Vector2 screenPoint) const;
    bool FindEdgeNearScreenPoint(Vector2 screenPoint, SectorEdgeRef& outEdge) const;
    std::vector<SectorEdgeHitCandidate> FindEdgeHitCandidates(Vector2 screenPoint) const;
    bool ResolveEdgeHit(Vector2 screenPoint, Vector2 rawMapPoint, SectorEdgeRef& outEdge) const;
    std::vector<SectorVertexRef> FindVerticesAtPoint(SectorPoint point) const;
    bool FindVertexNearScreenPoint(
            Vector2 screenPoint,
            SectorPoint& outPoint,
            std::vector<SectorVertexRef>& outRefs) const;
    SectorPoint SnapVertexMoveTarget(Vector2 mapPoint) const;
    bool ValidateExistingSectorMap(const SectorMap& map, std::string& error) const;
    bool ValidateMovedVertexGroup(SectorPoint targetPoint, std::string& error) const;
    SectorMap BuildMapWithMovedVertexGroup(SectorPoint targetPoint) const;
    void LoadInitialMap(const char* requestedPath);
    void ReloadMap(engine::AssetManager& assets);
    void SaveMap();
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
    void SyncSelectedSectorIdBuffer();
    void SyncSelectedLightIdBuffer();
    void SelectSector(int sectorIndex);
    void SelectEdge(int sectorIndex, int edgeIndex);
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
    void ClearSelection();
    SectorEdgeOverride* FindMutableEdgeOverride(int sectorIndex, int edgeIndex);
    const SectorEdgeOverride* FindEdgeOverride(int sectorIndex, int edgeIndex) const;
    SectorEdgeOverride& EnsureEdgeOverride(int sectorIndex, int edgeIndex);
    void RemoveEdgeOverrideIfEmpty(int sectorIndex, int edgeIndex);
    void OpenTexturePicker(TexturePickerTargetKind target, int sectorIndex, int edgeIndex);
    void ApplyTexturePickerSelection(engine::AssetManager& assets);
    std::string CurrentTextureForTarget(TexturePickerTargetKind target, int sectorIndex, int edgeIndex) const;
    bool TargetAllowsSectorDefault(TexturePickerTargetKind target) const;
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

    SectorEditorState state;
    SectorEditorUiState uiState;
    LightmapBakeAsyncState lightmapBake;
    Rectangle canvasRect = {};
    std::string mapPath;
    std::string fallbackMapPath;
    std::string statusText;
    SectorMeshPreview preview;
    bool initialized = false;
};

} // namespace game
