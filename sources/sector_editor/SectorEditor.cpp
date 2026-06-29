#include "sector_editor/SectorEditor.h"

#include "engine/assets/TextureLoadFlags.h"
#include "engine/input/InputEvents.h"
#include "sector_editor/SectorEditorAuthoringState.h"
#include "sector_editor/SectorEditorDocumentActions.h"
#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/SectorEditorLightInspector.h"
#include "sector_editor/SectorEditorLightmapModal.h"
#include "sector_editor/SectorEditorMaterialActions.h"
#include "sector_editor/SectorEditorPreviewActions.h"
#include "sector_editor/SectorEditorPreviewSettingsModal.h"
#include "sector_editor/SectorEditorSectorInspector.h"
#include "sector_editor/SectorEditorTextureModals.h"
#include "sector_editor/SectorEditorTopologyActions.h"
#include "sector_editor/SectorEditorTopologyRenderCache.h"
#include "sector_editor/SectorEditorUiHelpers.h"
#include "sector_editor/SectorEditorVertexInspector.h"
#include "sector_demo/SectorFpsController.h"
#include "sector_demo/SectorFreeflyController.h"
#include "sector_demo/SectorGeneratedGeometry.h"
#include "sector_demo/SectorLightmap.h"
#include "sector_demo/SectorTextureTypes.h"
#include "sector_demo/SectorTopologyGeometry.h"
#include "sector_demo/SectorTopologySerialization.h"
#include "sector_demo/SectorTopologyUnits.h"
#include "sector_demo/SectorUnits.h"

#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <sstream>
#include <system_error>
#include <utility>
#include <vector>

namespace game {

namespace {

bool IsFlatTopologySurfaceTarget(TopologySurfaceEditTarget target)
{
    return target.kind == TopologySurfaceEditTargetKind::SectorFloor
            || target.kind == TopologySurfaceEditTargetKind::SectorCeiling;
}

int64_t PositiveModulo(int64_t value, int64_t divisor)
{
    if (divisor <= 0) {
        return 0;
    }
    const int64_t result = value % divisor;
    return result < 0 ? result + divisor : result;
}

bool CoordAlignedToStep(SectorCoord value, int64_t step)
{
    return step <= 1 || PositiveModulo(value, step) == 0;
}

int64_t LcmClamped(int64_t a, int64_t b)
{
    if (a <= 0 || b <= 0) {
        return 1;
    }
    const int64_t divisor = std::gcd(a, b);
    if (divisor <= 0) {
        return 1;
    }
    constexpr int64_t maxReasonablePeriod = 1 << 20;
    const int64_t divided = a / divisor;
    if (divided > maxReasonablePeriod / b) {
        return maxReasonablePeriod;
    }
    return std::max<int64_t>(1, divided * b);
}

int64_t CoordinateSequencePeriod(SectorCoord stepDelta, int64_t snapStep)
{
    if (snapStep <= 1) {
        return 1;
    }
    const int64_t divisor = std::gcd<int64_t>(
            std::llabs(static_cast<int64_t>(stepDelta)),
            snapStep);
    return std::max<int64_t>(1, snapStep / std::max<int64_t>(1, divisor));
}

const char* InsertVertexFailureStatus(SectorAuthoringInsertVertexStatus status)
{
    switch (status) {
        case SectorAuthoringInsertVertexStatus::Inserted:
            return "Inserted vertex on authoring line";
        case SectorAuthoringInsertVertexStatus::InvalidLine:
            return "Insert Vertex: select or click an authoring line";
        case SectorAuthoringInsertVertexStatus::InvalidEndpoint:
            return "Insert Vertex unavailable: selected authoring line is invalid";
        case SectorAuthoringInsertVertexStatus::OffLine:
            return "Insert point must lie on the selected line";
        case SectorAuthoringInsertVertexStatus::Endpoint:
            return "Insert point is too close to an endpoint";
        case SectorAuthoringInsertVertexStatus::IdAllocationFailed:
            return "Insert Vertex failed: could not allocate authoring IDs";
    }
    return "Insert Vertex failed";
}

float AuthoringInspectorTextureRowTotalHeight(float gap)
{
    return SectorEditorInspectorTextureRowHeight() + gap;
}

float AuthoringInspectorAssignedDecalControlsHeight(bool emissive, float rowH, float gap, bool includeTintAndFit)
{
    float height = 0.0f;
    height += rowH + gap;
    height += 36.0f + gap;
    if (emissive) {
        height += rowH + gap;
    }
    if (includeTintAndFit) {
        height += rowH + gap;
        height += 36.0f + gap;
    }
    return height;
}

float AuthoringInspectorDecalBlockHeight(
        const SectorTopologyDecalLayer& decal,
        float rowH,
        float gap,
        bool includeTintAndFit)
{
    float height = AuthoringInspectorTextureRowTotalHeight(gap);
    if (!decal.textureId.empty()) {
        height += AuthoringInspectorAssignedDecalControlsHeight(
                decal.emissive,
                rowH,
                gap,
                includeTintAndFit);
    }
    return height;
}

float AuthoringLineInspectorContentHeight(
        const SectorAuthoringLine& line,
        const SectorAuthoringGraph& graph,
        float rowH,
        float gap)
{
    float height = 0.0f;
    height += 38.0f;
    height += 32.0f;
    height += 36.0f + gap;

    const auto addSideSection = [&](SectorTopologySideKind sideKind) {
        height += 18.0f;
        height += 30.0f;
        height += AuthoringInspectorTextureRowTotalHeight(gap) * 4.0f;

        const SectorAuthoringLineSide* side =
                FindSectorAuthoringLineSide(graph, SectorAuthoringSideId{line.id, sideKind});
        const SectorTopologyDecalLayer emptyDecal;
        const SectorTopologyDecalLayer& wallDecal =
                side != nullptr ? side->wall.decal : emptyDecal;
        const SectorTopologyDecalLayer& lowerDecal =
                side != nullptr ? side->lower.decal : emptyDecal;
        const SectorTopologyDecalLayer& upperDecal =
                side != nullptr ? side->upper.decal : emptyDecal;
        height += AuthoringInspectorDecalBlockHeight(wallDecal, rowH, gap, true);
        height += AuthoringInspectorDecalBlockHeight(lowerDecal, rowH, gap, true);
        height += AuthoringInspectorDecalBlockHeight(upperDecal, rowH, gap, true);
    };

    addSideSection(SectorTopologySideKind::Front);
    addSideSection(SectorTopologySideKind::Back);
    height += rowH + gap;
    height += rowH + gap;
    return height;
}

float AuthoringFaceInspectorContentHeight(
        const SectorAuthoringFaceAnchor& anchor,
        float rowH,
        float gap)
{
    float height = 0.0f;
    height += 38.0f;
    height += 32.0f;
    height += (rowH + gap) * 2.0f;
    height += rowH + gap;

    height += 18.0f;
    height += 30.0f;
    height += rowH + gap;
    height += (rowH + gap) * 3.0f;

    height += 18.0f;
    height += 30.0f;
    height += AuthoringInspectorTextureRowTotalHeight(gap) * 5.0f;
    height += AuthoringInspectorDecalBlockHeight(anchor.floorDecal, rowH, gap, true);
    height += AuthoringInspectorDecalBlockHeight(anchor.ceilingDecal, rowH, gap, true);
    height += AuthoringInspectorDecalBlockHeight(anchor.defaultWall.decal, rowH, gap, false);
    height += AuthoringInspectorDecalBlockHeight(anchor.defaultLower.decal, rowH, gap, false);
    height += AuthoringInspectorDecalBlockHeight(anchor.defaultUpper.decal, rowH, gap, false);
    height += rowH + gap;
    return height;
}

} // namespace

bool SectorEditor::Init(engine::AssetManager& assets)
{
    Shutdown(assets);
    ResetToBlankMap(assets);
    return true;
}

void SectorEditor::Shutdown(engine::AssetManager& assets)
{
    ShutdownLightmapBake();
    preview.ShutdownRendererResources(assets);
    if (!engine::IsNull(state.editorTextureScope)) {
        assets.UnloadScope(state.editorTextureScope);
    }
    if (!engine::IsNull(state.addMapTexture.previewScope)) {
        assets.UnloadScope(state.addMapTexture.previewScope);
    }
    state = SectorEditorState{};
    uiState = SectorEditorUiState{};
    canvasRect = {};
    statusText.clear();
    initialized = false;
}

void SectorEditor::Update(engine::Input& input, float dt)
{
    if (IsLightmapBakeBlocking()) {
        CancelAuthoringVertexDrag(nullptr);
        CancelLightDrag(nullptr);
        CancelPendingAuthoringLine(nullptr);
        CancelPendingAuthoringRectangle(nullptr);
        return;
    }

    if (state.mode == SectorEditorMode::Preview3D) {
        if (state.texturePicker.open || HasDocumentModalOpen()) {
            return;
        }
        UpdatePreview3D(input, dt);
        return;
    }

    canvasRect = BuildCanvasRect();
    if (state.texturePicker.open || state.addMapTexture.open || HasDocumentModalOpen()) {
        return;
    }
    UpdateHoverAndMouse(input);
    HandleCanvasInput(input, dt);
}

void SectorEditor::Render(engine::AssetManager& assets)
{
    if (state.mode == SectorEditorMode::Preview3D) {
        RenderPreview3D(assets);
        return;
    }

    canvasRect = BuildCanvasRect();
    DrawRectangleRec(canvasRect, Color{12, 15, 20, 255});

    BeginScissorMode(
            static_cast<int>(std::round(canvasRect.x)),
            static_cast<int>(std::round(canvasRect.y)),
            static_cast<int>(std::round(canvasRect.width)),
            static_cast<int>(std::round(canvasRect.height))
    );

    if (state.showGrid) {
        DrawGrid();
    }
    DrawTopologyDocument();
    EndScissorMode();

    DrawRectangleLinesEx(canvasRect, 2.0f, Color{67, 76, 93, 255});
}

void SectorEditor::RenderUI(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    PollLightmapBakeResult(assets);

    if (state.mode == SectorEditorMode::Preview3D) {
        engine::BeginUI(ui, input);
        if (IsLightmapBakeBlocking()) {
            DrawLightmapBakeModal(ui, config, input, assets, font);
            uiState.keyboardCaptured = true;
            engine::EndUI(ui, config, input, assets);
            return;
        }
        if (state.previewUiHidden) {
            ui.hotId = 0;
            ui.activeId = 0;
            ui.focusedId = 0;
            ui.openOptionId = 0;
        } else {
            DrawPreviewOverlay(ui, config, input, assets, font);
        }
        if (!state.previewUiHidden
                && !state.texturePicker.open
                && !state.decalTintModal.open
                && !state.previewSettingsModal.open) {
            DrawPreviewUvPanel(ui, config, input, assets, font);
        }
        if (state.decalTintModal.open) {
            DrawDecalTintModal(ui, config, input, assets, font);
        }
        if (state.previewSettingsModal.open) {
            DrawPreviewSettingsModal(ui, config, input, assets, font);
        }
        DrawTexturePickerModal(ui, config, input, assets, font);
        uiState.keyboardCaptured = ui.focusedId != 0;
        if (state.texturePicker.open || state.decalTintModal.open || state.previewSettingsModal.open) {
            uiState.keyboardCaptured = true;
        }
        engine::EndUI(ui, config, input, assets);
        return;
    }

    engine::BeginUI(ui, input);
    if (IsLightmapBakeBlocking()) {
        DrawLightmapBakeModal(ui, config, input, assets, font);
        uiState.keyboardCaptured = true;
        engine::EndUI(ui, config, input, assets);
        return;
    }
    if (state.confirmationModal.open) {
        DrawConfirmationModal(ui, config, input, assets, font);
        uiState.keyboardCaptured = true;
        engine::EndUI(ui, config, input, assets);
        return;
    }
    if (state.saveLevelModal.open) {
        DrawSaveLevelModal(ui, config, input, assets, font);
        uiState.keyboardCaptured = true;
        engine::EndUI(ui, config, input, assets);
        return;
    }
    if (state.loadLevelModal.open) {
        DrawLoadLevelModal(ui, config, input, assets, font);
        uiState.keyboardCaptured = true;
        engine::EndUI(ui, config, input, assets);
        return;
    }
    if (state.decalTintModal.open) {
        DrawDecalTintModal(ui, config, input, assets, font);
        uiState.keyboardCaptured = true;
        engine::EndUI(ui, config, input, assets);
        return;
    }
    if (state.previewSettingsModal.open) {
        DrawPreviewSettingsModal(ui, config, input, assets, font);
        DrawTexturePickerModal(ui, config, input, assets, font);
        uiState.keyboardCaptured = true;
        engine::EndUI(ui, config, input, assets);
        return;
    }
    if (state.addMapTexture.open) {
        DrawAddMapTextureModal(ui, config, input, assets, font);
        uiState.keyboardCaptured = true;
        engine::EndUI(ui, config, input, assets);
        return;
    }
    if (state.texturePicker.open) {
        DrawTexturePickerModal(ui, config, input, assets, font);
        uiState.keyboardCaptured = true;
        engine::EndUI(ui, config, input, assets);
        return;
    }

    DrawToolsPanel(ui, config, input, assets, font);
    DrawSectorsPanel(ui, config, input, assets, font);
    DrawStatusPanel(ui, config, assets, font);
    DrawAddMapTextureModal(ui, config, input, assets, font);
    DrawTexturePickerModal(ui, config, input, assets, font);
    uiState.keyboardCaptured = ui.focusedId != 0;
    if (state.texturePicker.open || state.addMapTexture.open || HasDocumentModalOpen()) {
        uiState.keyboardCaptured = true;
    }
    engine::EndUI(ui, config, input, assets);
}

bool SectorEditor::IsPreview3DActive() const
{
    return state.mode == SectorEditorMode::Preview3D;
}

Vector2 SectorEditor::MapToScreen(Vector2 map) const
{
    return CanvasWorldToScreen(SectorAuthoringToWorldPosition(map));
}

Vector2 SectorEditor::ScreenToMap(Vector2 screen) const
{
    return SectorWorldToAuthoringPosition(ScreenToCanvasWorld(screen));
}

Vector2 SectorEditor::CanvasWorldToScreen(Vector2 canvasWorld) const
{
    return Vector2{
            canvasRect.x + canvasRect.width * 0.5f + (canvasWorld.x - state.viewCenter.x) * state.viewZoom,
            canvasRect.y + canvasRect.height * 0.5f + (canvasWorld.y - state.viewCenter.y) * state.viewZoom
    };
}

Vector2 SectorEditor::ScreenToCanvasWorld(Vector2 screen) const
{
    return Vector2{
            state.viewCenter.x + (screen.x - (canvasRect.x + canvasRect.width * 0.5f)) / state.viewZoom,
            state.viewCenter.y + (screen.y - (canvasRect.y + canvasRect.height * 0.5f)) / state.viewZoom
    };
}

Vector2 SectorEditor::SnapMapPoint(Vector2 map) const
{
    const float grid = static_cast<float>(std::max(1, state.gridSize));
    Vector2 snapped{
            std::round(map.x / grid) * grid,
            std::round(map.y / grid) * grid
    };

    if (state.currentTool != SectorEditorTool::AuthoringLine
            && state.currentTool != SectorEditorTool::AuthoringRectangle) {
        return snapped;
    }

    const float threshold = std::max(
            SectorWorldToAuthoringDistance(ScreenVertexSnapPixels / std::max(1.0f, state.viewZoom)),
            grid * 0.20f
    );
    float bestDistance2 = threshold * threshold;
    bool found = false;
    Vector2 best = snapped;
    if (state.currentTool == SectorEditorTool::AuthoringLine
            || state.currentTool == SectorEditorTool::AuthoringRectangle) {
        for (const SectorAuthoringVertex& authoringVertex : state.authoringGraph.vertices) {
            const Vector2 vertex{
                    SectorCoordToVisibleAuthoring(authoringVertex.x),
                    SectorCoordToVisibleAuthoring(authoringVertex.y)};
            const float dx = vertex.x - map.x;
            const float dy = vertex.y - map.y;
            const float distance2 = dx * dx + dy * dy;
            if (distance2 <= bestDistance2) {
                bestDistance2 = distance2;
                best = vertex;
                found = true;
            }
        }
    } else {
        for (const SectorTopologyVertex& topologyVertex : state.topologyMap.vertices) {
            const Vector2 vertex = SectorTopologyVertexToMap(topologyVertex);
            const float dx = vertex.x - map.x;
            const float dy = vertex.y - map.y;
            const float distance2 = dx * dx + dy * dy;
            if (distance2 <= bestDistance2) {
                bestDistance2 = distance2;
                best = vertex;
                found = true;
            }
        }
    }

    return found ? best : snapped;
}

Rectangle SectorEditor::BuildLeftPanelRect() const
{
    return Rectangle{0.0f, 0.0f, LeftPanelWidth, EditorHeight - BottomPanelHeight};
}

Rectangle SectorEditor::BuildRightPanelRect() const
{
    return Rectangle{EditorWidth - RightPanelWidth, 0.0f, RightPanelWidth, EditorHeight - BottomPanelHeight};
}

Rectangle SectorEditor::BuildBottomPanelRect() const
{
    return Rectangle{0.0f, EditorHeight - BottomPanelHeight, EditorWidth, BottomPanelHeight};
}

Rectangle SectorEditor::BuildCanvasRect() const
{
    return Rectangle{
            LeftPanelWidth + PanelGap,
            PanelGap,
            EditorWidth - LeftPanelWidth - RightPanelWidth - PanelGap * 2.0f,
            EditorHeight - BottomPanelHeight - PanelGap * 2.0f
    };
}

bool SectorEditor::IsMouseOverCanvas(const engine::Input& input) const
{
    return Contains(canvasRect, input.MousePosition());
}

void SectorEditor::UpdateHoverAndMouse(engine::Input& input)
{
    state.rawMouseMap = ScreenToMap(input.MousePosition());
    state.snappedMouseMap = SnapMapPoint(state.rawMouseMap);
    if (state.pendingAuthoringRectangle.active) {
        std::string error;
        SectorTopologyCoordPoint currentCorner;
        if (ToTopologyCoordPoint(CurrentSnappedSectorPoint(), currentCorner, error)) {
            state.pendingAuthoringRectangle.currentCorner = currentCorner;
        }
    }
    if (state.currentTool == SectorEditorTool::AuthoringInsertVertex
            || state.pendingAuthoringInsertVertex.active) {
        UpdatePendingAuthoringInsertVertex(state.rawMouseMap);
    }
    state.hasHoveredVertex = false;
    state.hoveredTopologyLightId = -1;
    state.hoveredTopologyDynamicLightId = -1;
    state.hoveredTopologyVertexId = -1;
    state.hoveredTopologyVertexPoint = SectorTopologyCoordPoint{};
    ClearSectorEditorAuthoringHover(state);

    if (!initialized || !IsMouseOverCanvas(input)) {
        return;
    }

    if (state.currentTool == SectorEditorTool::Select) {
        SectorAuthoringSelectionTarget target;
        SectorTopologyCoordPoint authoringVertexPoint{};
        if (FindAuthoringSelectionNearScreenPoint(
                    input.MousePosition(),
                    target,
                    authoringVertexPoint)) {
            if (target.kind == SectorAuthoringSelectionKind::Vertex) {
                SetHoveredSectorEditorAuthoringVertex(state, target.vertexId);
            } else if (target.kind == SectorAuthoringSelectionKind::Line) {
                SetHoveredSectorEditorAuthoringLine(state, target.lineId);
            }
        }
        state.inspectedTopologyVertexId = -1;
        return;
    }

    if (state.currentTool == SectorEditorTool::AuthoringMove) {
        int authoringVertexId = -1;
        SectorTopologyCoordPoint authoringVertexPoint{};
        if (FindAuthoringVertexNearScreenPoint(
                    input.MousePosition(),
                    authoringVertexId,
                    authoringVertexPoint)) {
            SetHoveredSectorEditorAuthoringVertex(state, authoringVertexId);
        }
        state.inspectedTopologyVertexId = -1;
        return;
    }

    if (state.currentTool == SectorEditorTool::AuthoringInsertVertex) {
        if (state.pendingAuthoringInsertVertex.lineId >= 0) {
            SetHoveredSectorEditorAuthoringLine(state, state.pendingAuthoringInsertVertex.lineId);
        }
        state.inspectedTopologyVertexId = -1;
        return;
    }

    if (state.currentTool == SectorEditorTool::StaticLight
            || state.currentTool == SectorEditorTool::Move) {
        const int lightId = FindTopologyLightNearScreenPoint(input.MousePosition());
        if (lightId >= 0) {
            state.hoveredTopologyLightId = lightId;
            state.inspectedTopologyVertexId = -1;
        } else if (state.currentTool == SectorEditorTool::Move
                && !IsSectorEditorGraphAuthoritativeMode()) {
            int vertexId = -1;
            SectorTopologyCoordPoint point;
            if (FindTopologyVertexNearScreenPoint(input.MousePosition(), vertexId, point)) {
                state.hasHoveredVertex = true;
                state.hoveredTopologyVertexId = vertexId;
                state.hoveredTopologyVertexPoint = point;
                state.inspectedTopologyVertexId = vertexId;
            } else {
                state.inspectedTopologyVertexId = -1;
            }
        }
    }

    if (state.currentTool == SectorEditorTool::DynamicLight) {
        const int lightId = FindTopologyDynamicLightNearScreenPoint(input.MousePosition());
        if (lightId >= 0) {
            state.hoveredTopologyDynamicLightId = lightId;
            state.inspectedTopologyVertexId = -1;
        }
    }
}

void SectorEditor::HandleCanvasInput(engine::Input& input, float dt)
{
    if (!initialized) {
        return;
    }

    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [this](engine::InputEvent& event) {
                if (event.key.key == KEY_ESCAPE) {
                    if (state.authoringVertexDrag.active) {
                        CancelAuthoringVertexDrag("Cancelled authoring vertex move");
                    } else if (state.lightDrag.active) {
                        CancelLightDrag("Cancelled light move");
                    } else if (state.pendingAuthoringLine.active) {
                        CancelPendingAuthoringLine("Line chain stopped");
                    } else if (state.pendingAuthoringRectangle.active) {
                        CancelPendingAuthoringRectangle("Rectangle cancelled");
                    } else if (state.pendingAuthoringInsertVertex.active
                            || state.currentTool == SectorEditorTool::AuthoringInsertVertex) {
                        CancelPendingAuthoringInsertVertex("Insert Vertex cancelled");
                    } else if (state.selectedTopologyLightId >= 0
                            || state.selectedTopologyDynamicLightId >= 0
                            || state.topologySelectionKind != TopologySelectionKind::None
                            || state.selectedAuthoring.kind != SectorAuthoringSelectionKind::None) {
                        ClearSelection();
                    } else {
                        state.currentTool = SectorEditorTool::Select;
                    }
                    engine::ConsumeEvent(event);
                    return;
                }

                if (event.key.key == KEY_DELETE) {
                    if (IsGraphAuthoringTool(state.currentTool)) {
                        if (state.selectedAuthoring.kind == SectorAuthoringSelectionKind::Line) {
                            DeleteSelectedAuthoringLine();
                        } else if (state.selectedAuthoring.kind == SectorAuthoringSelectionKind::Vertex) {
                            DeleteSelectedAuthoringVertex();
                        } else {
                            statusText = "Select an authoring line or isolated authoring vertex to delete.";
                        }
                    } else if (state.selectedAuthoring.kind == SectorAuthoringSelectionKind::Line) {
                        DeleteSelectedAuthoringLine();
                    } else if (state.selectedAuthoring.kind == SectorAuthoringSelectionKind::Vertex) {
                        DeleteSelectedAuthoringVertex();
                    } else if (state.topologySelectionKind == TopologySelectionKind::StaticLight
                            && state.selectedTopologyLightId >= 0) {
                        DeleteSelectedLight();
                    } else if (state.topologySelectionKind == TopologySelectionKind::DynamicLight
                            && state.selectedTopologyDynamicLightId >= 0) {
                        DeleteSelectedLight();
                    } else if (state.topologySelectionKind == TopologySelectionKind::Sector
                            && state.selectedTopologySectorId >= 0) {
                        statusText = LegacyTopologyMutationUnavailableMessage();
                    } else if (state.topologySelectionKind == TopologySelectionKind::Vertex
                            && state.selectedTopologyVertexId >= 0) {
                        statusText = "Standalone vertex deletion is not available; use Dissolve Vertex for simple degree-2 vertices.";
                    } else if (state.topologySelectionKind == TopologySelectionKind::SideDef
                            || state.topologySelectionKind == TopologySelectionKind::LineDef) {
                        statusText = "Direct linedef/sidedef deletion is not available yet.";
                    } else {
                        statusText = "Select a topology sector to delete.";
                    }
                    engine::ConsumeEvent(event);
                    return;
                }
            }
    );

    if (state.authoringVertexDrag.active || state.lightDrag.active) {
        if (state.authoringVertexDrag.active) {
            UpdateAuthoringVertexDrag(input);
        }
        if (state.lightDrag.active) {
            UpdateLightDrag(input);
        }

        input.ForEachEvent(
                engine::InputEventType::MouseButtonPressed,
                true,
                [this](engine::InputEvent& event) {
                    if (event.mouseButton.button == MOUSE_RIGHT_BUTTON) {
                        if (state.authoringVertexDrag.active) {
                            CancelAuthoringVertexDrag("Cancelled authoring vertex move");
                        }
                        if (state.lightDrag.active) {
                            CancelLightDrag("Cancelled light move");
                        }
                        engine::ConsumeEvent(event);
                    }
                }
        );

        input.ForEachEvent(
                engine::InputEventType::MouseButtonReleased,
                true,
                [this](engine::InputEvent& event) {
                    if (event.mouseButton.button == MOUSE_LEFT_BUTTON) {
                        if (state.authoringVertexDrag.active) {
                            FinishAuthoringVertexDrag();
                        }
                        if (state.lightDrag.active) {
                            FinishLightDrag();
                        }
                        engine::ConsumeEvent(event);
                    }
                }
        );

        input.ForEachEvent(
                engine::InputEventType::MouseWheel,
                true,
                [](engine::InputEvent& event) {
                    engine::ConsumeEvent(event);
                }
        );
        return;
    }

    if (!uiState.keyboardCaptured) {
        Vector2 pan{};
        if (input.IsKeyDown(KEY_A)) {
            pan.x -= 1.0f;
        }
        if (input.IsKeyDown(KEY_D)) {
            pan.x += 1.0f;
        }
        if (input.IsKeyDown(KEY_W)) {
            pan.y -= 1.0f;
        }
        if (input.IsKeyDown(KEY_S)) {
            pan.y += 1.0f;
        }

        if (pan.x != 0.0f || pan.y != 0.0f) {
            const float length = std::sqrt(pan.x * pan.x + pan.y * pan.y);
            pan.x /= length;
            pan.y /= length;
            const float mapUnits = (PanPixelsPerSecond * dt) / std::max(1.0f, state.viewZoom);
            state.viewCenter.x += pan.x * mapUnits;
            state.viewCenter.y += pan.y * mapUnits;
        }
    }

    if (!IsMouseOverCanvas(input)) {
        return;
    }

    input.ForEachEvent(
            engine::InputEventType::MouseButtonPressed,
            true,
            [this](engine::InputEvent& event) {
                if (state.currentTool == SectorEditorTool::AuthoringMove
                        && event.mouseButton.button == MOUSE_LEFT_BUTTON
                        && Contains(canvasRect, event.mouseButton.position)) {
                    int authoringVertexId = -1;
                    SectorTopologyCoordPoint point{};
                    if (FindAuthoringVertexNearScreenPoint(
                                event.mouseButton.position,
                                authoringVertexId,
                                point)) {
                        StartAuthoringVertexDrag(authoringVertexId, point);
                        engine::ConsumeEvent(event);
                    } else {
                        statusText = "Move Vertex: click an authoring vertex";
                        engine::ConsumeEvent(event);
                    }
                    return;
                }

                if (state.currentTool == SectorEditorTool::StaticLight
                        && event.mouseButton.button == MOUSE_LEFT_BUTTON
                        && Contains(canvasRect, event.mouseButton.position)) {
                    const int lightId = FindTopologyLightNearScreenPoint(event.mouseButton.position);
                    if (lightId >= 0) {
                        StartLightDrag(lightId);
                        engine::ConsumeEvent(event);
                    }
                    return;
                }

                if (state.currentTool == SectorEditorTool::DynamicLight
                        && event.mouseButton.button == MOUSE_LEFT_BUTTON
                        && Contains(canvasRect, event.mouseButton.position)) {
                    const int lightId = FindTopologyDynamicLightNearScreenPoint(event.mouseButton.position);
                    if (lightId >= 0) {
                        StartLightDrag(lightId);
                        engine::ConsumeEvent(event);
                    }
                    return;
                }

                if (state.currentTool != SectorEditorTool::Move
                        || event.mouseButton.button != MOUSE_LEFT_BUTTON
                        || !Contains(canvasRect, event.mouseButton.position)) {
                    return;
                }

                const int lightId = FindTopologyLightNearScreenPoint(event.mouseButton.position);
                if (lightId >= 0) {
                    StartLightDrag(lightId);
                } else {
                    if (IsSectorEditorGraphAuthoritativeMode()) {
                        statusText = "Move: drag an existing light, or use Move Vertex for authoring vertices.";
                        engine::ConsumeEvent(event);
                        return;
                    }
                    int vertexId = -1;
                    SectorTopologyCoordPoint point;
                    statusText = FindTopologyVertexNearScreenPoint(event.mouseButton.position, vertexId, point)
                            ? LegacyTopologyMutationUnavailableMessage()
                            : "Move: click a topology light";
                }
                engine::ConsumeEvent(event);
            }
    );

    input.ForEachEvent(
            engine::InputEventType::MouseWheel,
            true,
            [this, &input](engine::InputEvent& event) {
                const Vector2 mouseBefore = ScreenToCanvasWorld(input.MousePosition());
                const float zoomFactor = event.wheel.value > 0.0f ? 1.12f : 1.0f / 1.12f;
                state.viewZoom = std::clamp(state.viewZoom * zoomFactor, MinZoom, MaxZoom);
                const Vector2 mouseAfter = ScreenToCanvasWorld(input.MousePosition());
                state.viewCenter.x += mouseBefore.x - mouseAfter.x;
                state.viewCenter.y += mouseBefore.y - mouseAfter.y;
                engine::ConsumeEvent(event);
            }
    );

    input.ForEachEvent(
            engine::InputEventType::MouseClick,
            true,
            [this](engine::InputEvent& event) {
                if (!Contains(canvasRect, event.mouseClick.releasePosition)) {
                    return;
                }

                if (event.mouseClick.button == MOUSE_RIGHT_BUTTON) {
                    if (state.pendingAuthoringLine.active) {
                        CancelPendingAuthoringLine("Line chain stopped");
                        engine::ConsumeEvent(event);
                        return;
                    }
                    if (state.pendingAuthoringRectangle.active) {
                        CancelPendingAuthoringRectangle("Rectangle cancelled");
                        engine::ConsumeEvent(event);
                        return;
                    }
                    if (state.pendingAuthoringInsertVertex.active
                            || state.currentTool == SectorEditorTool::AuthoringInsertVertex) {
                        CancelPendingAuthoringInsertVertex("Insert Vertex cancelled");
                        engine::ConsumeEvent(event);
                        return;
                    }
                }

                if (event.mouseClick.button != MOUSE_LEFT_BUTTON) {
                    return;
                }

                if (state.currentTool == SectorEditorTool::Select) {
                    SectorAuthoringSelectionTarget target;
                    SectorTopologyCoordPoint authoringVertexPoint{};
                    if (FindAuthoringSelectionNearScreenPoint(
                                event.mouseClick.releasePosition,
                                target,
                                authoringVertexPoint)) {
                        if (target.kind == SectorAuthoringSelectionKind::Vertex) {
                            SelectAuthoringVertex(target.vertexId);
                            statusText = TextFormat("Selected authoring vertex %d", target.vertexId);
                        } else if (target.kind == SectorAuthoringSelectionKind::Line) {
                            SelectAuthoringLine(target.lineId);
                            statusText = TextFormat("Selected authoring line %d", target.lineId);
                        } else if (target.kind == SectorAuthoringSelectionKind::FaceAnchor) {
                            SelectAuthoringFaceAnchor(target.faceAnchorId);
                            statusText = TextFormat("Selected authoring face anchor %d", target.faceAnchorId);
                        }
                        engine::ConsumeEvent(event);
                        return;
                    }

                    ClearSelection();
                    statusText = "Selected authoring: none";
                    engine::ConsumeEvent(event);
                    return;
                }

                if (state.currentTool == SectorEditorTool::AuthoringLine) {
                    AddAuthoringLinePoint(CurrentSnappedSectorPoint());
                    engine::ConsumeEvent(event);
                    return;
                }

                if (state.currentTool == SectorEditorTool::AuthoringRectangle) {
                    AddAuthoringRectanglePoint(CurrentSnappedSectorPoint());
                    engine::ConsumeEvent(event);
                    return;
                }

                if (state.currentTool == SectorEditorTool::AuthoringInsertVertex) {
                    CommitAuthoringInsertVertex(event.mouseClick.releasePosition);
                    engine::ConsumeEvent(event);
                    return;
                }

                if (state.currentTool == SectorEditorTool::StaticLight) {
                    AddStaticLightAt(SnapMapPoint(ScreenToMap(event.mouseClick.releasePosition)));
                    engine::ConsumeEvent(event);
                    return;
                }

                if (state.currentTool == SectorEditorTool::DynamicLight) {
                    AddDynamicLightAt(SnapMapPoint(ScreenToMap(event.mouseClick.releasePosition)));
                    engine::ConsumeEvent(event);
                    return;
                }

                if (state.currentTool == SectorEditorTool::Move) {
                    statusText = "Move: click a topology light";
                    engine::ConsumeEvent(event);
                    return;
                }

                if (state.currentTool == SectorEditorTool::AuthoringMove) {
                    statusText = "Move Vertex: click an authoring vertex";
                    engine::ConsumeEvent(event);
                }
            }
    );
}

void SectorEditor::StartAuthoringVertexDrag(int vertexId, SectorTopologyCoordPoint point)
{
    if (!IsValidSectorAuthoringId(vertexId)
            || FindSectorAuthoringVertex(state.authoringGraph, vertexId) == nullptr) {
        return;
    }

    SelectAuthoringVertex(vertexId);
    ClearTopologySelectionOnly();
    state.authoringVertexDrag.active = true;
    state.authoringVertexDrag.vertexId = vertexId;
    state.authoringVertexDrag.originalPoint = point;
    state.authoringVertexDrag.previewPoint = point;
    state.authoringVertexDrag.hasPreviewPoint = true;
    state.authoringVertexDrag.errorMessage.clear();

    size_t connectedCount = 0;
    for (const SectorAuthoringLine& line : state.authoringGraph.lines) {
        if (line.startVertexId == vertexId || line.endVertexId == vertexId) {
            ++connectedCount;
        }
    }
    statusText = connectedCount > 0
            ? TextFormat("Moving authoring vertex %d (%zu connected lines)", vertexId, connectedCount)
            : TextFormat("Moving authoring vertex %d", vertexId);
}

void SectorEditor::UpdateAuthoringVertexDrag(engine::Input& input)
{
    if (!state.authoringVertexDrag.active) {
        return;
    }

    std::string error;
    SectorTopologyCoordPoint snappedPoint;
    if (!SnapAuthoringVertexMoveTarget(ScreenToMap(input.MousePosition()), snappedPoint, error)) {
        state.authoringVertexDrag.errorMessage = error;
        state.authoringVertexDrag.hasPreviewPoint = false;
        statusText = TextFormat("Authoring move rejected: %s", error.c_str());
        return;
    }

    state.authoringVertexDrag.previewPoint = snappedPoint;
    state.authoringVertexDrag.hasPreviewPoint = true;
    state.authoringVertexDrag.errorMessage.clear();
    if (SameTopologyPoint(snappedPoint, state.authoringVertexDrag.originalPoint)) {
        statusText = "Moving authoring vertex: original point";
    } else {
        statusText = TextFormat("Moving authoring vertex %d", state.authoringVertexDrag.vertexId);
    }
}

void SectorEditor::FinishAuthoringVertexDrag()
{
    if (!state.authoringVertexDrag.active) {
        return;
    }

    const int vertexId = state.authoringVertexDrag.vertexId;
    const SectorTopologyCoordPoint original = state.authoringVertexDrag.originalPoint;
    const SectorTopologyCoordPoint target = state.authoringVertexDrag.previewPoint;
    if (!state.authoringVertexDrag.hasPreviewPoint) {
        const std::string error = state.authoringVertexDrag.errorMessage.empty()
                ? "Move target is outside authoring coordinate range"
                : state.authoringVertexDrag.errorMessage;
        state.authoringVertexDrag = AuthoringVertexDragState{};
        statusText = TextFormat("Authoring move rejected: %s", error.c_str());
        return;
    }

    if (SameTopologyPoint(target, original)) {
        state.authoringVertexDrag = AuthoringVertexDragState{};
        statusText = "Authoring vertex unchanged";
        return;
    }

    if (!MoveSectorEditorAuthoringVertex(state, vertexId, target)) {
        state.authoringVertexDrag = AuthoringVertexDragState{};
        statusText = "Authoring vertex move rejected";
        return;
    }

    SelectAuthoringVertex(vertexId);
    state.authoringVertexDrag = AuthoringVertexDragState{};
    statusText = TextFormat("Moved authoring vertex %d", vertexId);
}

void SectorEditor::CancelAuthoringVertexDrag(const char* message)
{
    state.authoringVertexDrag = AuthoringVertexDragState{};
    if (message != nullptr && message[0] != '\0') {
        statusText = message;
    }
}

void SectorEditor::StartLightDrag(int topologyLightId)
{
    if (state.currentTool == SectorEditorTool::DynamicLight) {
        const SectorTopologyDynamicPointLight* light = FindSectorTopologyDynamicLight(
                state.topologyMap,
                topologyLightId);
        if (light == nullptr) {
            return;
        }

        SelectTopologyDynamicLight(topologyLightId);
        state.lightDrag.active = true;
        state.lightDrag.topologyLightId = topologyLightId;
        state.lightDrag.originalPosition = light->position;
        state.lightDrag.snappedPosition = light->position;
        statusText = TextFormat("Moving dynamic light %d", light->id);
        return;
    }

    const SectorTopologyStaticPointLight* light = FindSectorTopologyStaticLight(
            state.topologyMap,
            topologyLightId);
    if (light == nullptr) {
        return;
    }
    SelectTopologyLight(topologyLightId);
    state.lightDrag.active = true;
    state.lightDrag.topologyLightId = topologyLightId;
    state.lightDrag.originalPosition = light->position;
    state.lightDrag.snappedPosition = light->position;
    statusText = TextFormat("Moving static light %d", light->id);
}

void SectorEditor::UpdateLightDrag(engine::Input& input)
{
    if (!state.lightDrag.active) {
        return;
    }

    const Vector2 snapped = SnapMapPoint(ScreenToMap(input.MousePosition()));
    state.lightDrag.snappedPosition = Vector3{
            snapped.x,
            state.lightDrag.originalPosition.y,
            snapped.y};

    if (state.topologySelectionKind == TopologySelectionKind::DynamicLight) {
        SectorTopologyDynamicPointLight* light = FindSectorTopologyDynamicLight(
                state.topologyMap,
                state.lightDrag.topologyLightId);
        if (light == nullptr) {
            return;
        }
        light->position.x = state.lightDrag.snappedPosition.x;
        light->position.y = state.lightDrag.originalPosition.y;
        light->position.z = state.lightDrag.snappedPosition.z;
        statusText = TextFormat("Moving dynamic light %d", light->id);
        return;
    }

    SectorTopologyStaticPointLight* light = FindSectorTopologyStaticLight(
            state.topologyMap,
            state.lightDrag.topologyLightId);
    if (light == nullptr) {
        return;
    }
    light->position.x = state.lightDrag.snappedPosition.x;
    light->position.y = state.lightDrag.originalPosition.y;
    light->position.z = state.lightDrag.snappedPosition.z;
    statusText = TextFormat("Moving static light %d", light->id);
}

void SectorEditor::FinishLightDrag()
{
    if (!state.lightDrag.active) {
        return;
    }

    const int lightId = state.lightDrag.topologyLightId;
    const Vector3 original = state.lightDrag.originalPosition;
    const TopologySelectionKind selectionKind = state.topologySelectionKind;
    state.lightDrag = LightDragState{};

    if (selectionKind == TopologySelectionKind::DynamicLight) {
        SectorTopologyDynamicPointLight* light = FindSectorTopologyDynamicLight(state.topologyMap, lightId);
        if (light == nullptr) {
            return;
        }

        SelectTopologyDynamicLight(lightId);
        FinishTopologyActionResult(FinishMoveDynamicLight(state.topologyMap, lightId, original));
        return;
    }

    SectorTopologyStaticPointLight* light = FindSectorTopologyStaticLight(state.topologyMap, lightId);
    if (light == nullptr) {
        return;
    }
    SelectTopologyLight(lightId);
    FinishTopologyActionResult(FinishMoveStaticLight(state.topologyMap, lightId, original));
}

void SectorEditor::CancelLightDrag(const char* message)
{
    if (state.lightDrag.active) {
        if (state.topologySelectionKind == TopologySelectionKind::DynamicLight) {
            SectorTopologyDynamicPointLight* light = FindSectorTopologyDynamicLight(
                    state.topologyMap,
                    state.lightDrag.topologyLightId);
            if (light != nullptr) {
                light->position = state.lightDrag.originalPosition;
                SelectTopologyDynamicLight(light->id);
            }
        } else {
            SectorTopologyStaticPointLight* light = FindSectorTopologyStaticLight(
                    state.topologyMap,
                    state.lightDrag.topologyLightId);
            if (light != nullptr) {
                light->position = state.lightDrag.originalPosition;
                SelectTopologyLight(light->id);
            }
        }
    }

    state.lightDrag = LightDragState{};
    if (message != nullptr && message[0] != '\0') {
        statusText = message;
    }
}

void SectorEditor::UpdatePreview3D(engine::Input& input, float dt)
{
    bool controlModeToggled = false;
    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [this, &controlModeToggled](engine::InputEvent& event) {
                if (event.key.key == KEY_F1) {
                    state.useBakedAmbientOcclusion = !state.useBakedAmbientOcclusion;
                    statusText = state.useBakedAmbientOcclusion
                            ? "Baked AO enabled"
                            : "Baked AO disabled";
                    engine::ConsumeEvent(event);
                    return;
                }

                if (event.key.key == KEY_F2) {
                    state.previewUiHidden = !state.previewUiHidden;
                    if (state.previewUiHidden) {
                        state.hoveredSurface3D = SectorSurfaceHit{};
                    }
                    statusText = state.previewUiHidden
                            ? "3D UI hidden"
                            : "3D UI shown";
                    engine::ConsumeEvent(event);
                    return;
                }

                if (event.key.key == KEY_F3) {
                    TogglePreviewControlMode();
                    controlModeToggled = true;
                    engine::ConsumeEvent(event);
                    return;
                }

                if (event.key.key == KEY_TAB || event.key.key == KEY_ESCAPE) {
                    LeavePreview3D();
                    engine::ConsumeEvent(event);
                }
            }
    );

    if (controlModeToggled) {
        UpdatePreview3DSelection(input);
        return;
    }

    if (state.mode == SectorEditorMode::Preview3D) {
        if (state.previewControlMode == SectorPreviewControlMode::FreeFly) {
            UpdateSectorFreeflyController(state.freeflyController, input, dt);
            preview.ApplyRendererPose(state.freeflyController.pose);
        } else {
            const float previousVisualEyeY = preview.RendererPose().position.y;
            input.ForEachEvent(
                    engine::InputEventType::KeyPressed,
                    true,
                    [this](engine::InputEvent& event) {
                        if (event.key.key != KEY_F11) {
                            return;
                        }

                        SetSectorFreeflyMouseLookEnabled(
                                state.freeflyController,
                                !state.freeflyController.mouseLookEnabled);
                        engine::ConsumeEvent(event);
                    }
            );

            SectorFpsControllerInput controllerInput;
            controllerInput.moveForward = input.IsKeyDown(KEY_W);
            controllerInput.moveBackward = input.IsKeyDown(KEY_S);
            controllerInput.strafeLeft = input.IsKeyDown(KEY_A);
            controllerInput.strafeRight = input.IsKeyDown(KEY_D);
            controllerInput.run = input.IsKeyDown(KEY_LEFT_SHIFT) || input.IsKeyDown(KEY_RIGHT_SHIFT);
            controllerInput.mouseLookEnabled = state.freeflyController.mouseLookEnabled;
            controllerInput.mouseDelta = input.MouseDelta();
            const bool canConsumeGameplayJump =
                    state.mode == SectorEditorMode::Preview3D
                    && state.previewControlMode == SectorPreviewControlMode::Gameplay
                    && !uiState.keyboardCaptured
                    && !state.texturePicker.open
                    && !state.decalTintModal.open
                    && !state.previewSettingsModal.open;
            if (canConsumeGameplayJump) {
                input.ForEachEvent(
                        engine::InputEventType::KeyPressed,
                        true,
                        [&controllerInput](engine::InputEvent& event) {
                            if (event.key.key != KEY_SPACE) {
                                return;
                            }
                            controllerInput.jumpPressed = true;
                            engine::ConsumeEvent(event);
                        }
                );
            }
            UpdateSectorEditorGameplayPreview(state, controllerInput, previousVisualEyeY, dt);
            ApplyGameplayPoseToPreview();
            preview.UpdateVisibilityDebug(state.fpsControllerState.currentSectorId);
            state.freeflyController.pose = preview.RendererPose();
        }
        UpdatePreview3DSelection(input);
    }
}

void SectorEditor::UpdatePreview3DSelection(engine::Input& input)
{
    if (!initialized
            || !preview.IsRendererReady()
            || state.freeflyController.mouseLookEnabled
            || state.previewUiHidden
            || state.texturePicker.open) {
        state.hoveredSurface3D = SectorSurfaceHit{};
        return;
    }

    const Rectangle viewport{0.0f, 0.0f, EditorWidth, EditorHeight};
    const Vector2 mouse = input.MousePosition();
    const bool overPanel = IsValidTopologySurfaceEditTarget(state.selectedTopologySurface3D)
            && Contains(BuildPreviewUvPanelRect(), mouse);
    state.hoveredSurface3D = overPanel
            ? SectorSurfaceHit{}
            : PickSectorSurface3D(mouse, viewport);

    input.ForEachEvent(
            engine::InputEventType::MouseClick,
            true,
            [this, overPanel](engine::InputEvent& event) {
                if (event.mouseClick.button != MOUSE_LEFT_BUTTON) {
                    return;
                }
                if (overPanel) {
                    engine::ConsumeEvent(event);
                    return;
                }
                if (state.hoveredSurface3D.hit) {
                    SelectSurface3D(state.hoveredSurface3D.surface);
                    statusText = TextFormat("Selected 3D %s", SurfaceKindName(state.hoveredSurface3D.surface.kind));
                    engine::ConsumeEvent(event);
                }
            }
    );
}

void SectorEditor::CancelPendingAuthoringLine(const char* message)
{
    CancelSectorEditorAuthoringLineToolChain(state);
    if (message != nullptr && message[0] != '\0') {
        statusText = message;
    }
}

void SectorEditor::CancelPendingAuthoringRectangle(const char* message)
{
    state.pendingAuthoringRectangle = PendingAuthoringRectangleDraw{};
    if (message != nullptr && message[0] != '\0') {
        statusText = message;
    }
}

void SectorEditor::CancelPendingAuthoringInsertVertex(const char* message)
{
    state.pendingAuthoringInsertVertex = PendingAuthoringInsertVertex{};
    if (message != nullptr && message[0] != '\0') {
        statusText = message;
    }
}

void SectorEditor::BeginPendingAuthoringInsertVertex(int lineId)
{
    if (FindSectorAuthoringLine(state.authoringGraph, lineId) == nullptr) {
        statusText = "Insert Vertex: select or click an authoring line";
        return;
    }

    if (state.pendingAuthoringLine.active) {
        CancelPendingAuthoringLine("Cancelled authoring line");
    }
    if (state.pendingAuthoringRectangle.active) {
        CancelPendingAuthoringRectangle("Rectangle cancelled");
    }
    if (state.authoringVertexDrag.active) {
        CancelAuthoringVertexDrag("Cancelled authoring vertex move");
    }
    if (state.lightDrag.active) {
        CancelLightDrag("Cancelled light move");
    }

    ClearTopologySelectionOnly();
    SelectSectorEditorAuthoringLine(state, lineId);
    state.currentTool = SectorEditorTool::AuthoringInsertVertex;
    state.pendingAuthoringInsertVertex = PendingAuthoringInsertVertex{};
    state.pendingAuthoringInsertVertex.active = true;
    state.pendingAuthoringInsertVertex.lineId = lineId;
    statusText = "Insert Vertex: click point on selected line, Esc/right click cancels";
}

bool SectorEditor::TryResolveAuthoringInsertVertexPoint(
        int lineId,
        Vector2 mapPoint,
        SectorTopologyCoordPoint& outPoint,
        std::string& error) const
{
    outPoint = SectorTopologyCoordPoint{};
    error.clear();

    const SectorAuthoringLine* line = FindSectorAuthoringLine(state.authoringGraph, lineId);
    if (line == nullptr) {
        error = "Insert Vertex: select or click an authoring line";
        return false;
    }

    const SectorAuthoringVertex* start =
            FindSectorAuthoringVertex(state.authoringGraph, line->startVertexId);
    const SectorAuthoringVertex* end =
            FindSectorAuthoringVertex(state.authoringGraph, line->endVertexId);
    if (start == nullptr || end == nullptr
            || (start->x == end->x && start->y == end->y)) {
        error = "Insert Vertex unavailable: selected authoring line is invalid";
        return false;
    }

    const int64_t dx = static_cast<int64_t>(end->x) - start->x;
    const int64_t dy = static_cast<int64_t>(end->y) - start->y;
    const int64_t latticeCount = std::gcd(std::llabs(dx), std::llabs(dy));
    if (latticeCount <= 1) {
        error = "Insert point is too close to an endpoint";
        return false;
    }

    const double mouseX = static_cast<double>(mapPoint.x)
            * static_cast<double>(SectorCoordSubdivisions);
    const double mouseY = static_cast<double>(mapPoint.y)
            * static_cast<double>(SectorCoordSubdivisions);
    const double apX = mouseX - static_cast<double>(start->x);
    const double apY = mouseY - static_cast<double>(start->y);
    const double lengthSquared = static_cast<double>(dx) * static_cast<double>(dx)
            + static_cast<double>(dy) * static_cast<double>(dy);
    if (lengthSquared <= 0.0) {
        error = "Insert Vertex unavailable: selected authoring line is invalid";
        return false;
    }
    const double t = std::clamp(
            (apX * static_cast<double>(dx) + apY * static_cast<double>(dy)) / lengthSquared,
            0.0,
            1.0);
    const int64_t nearestLattice = std::clamp<int64_t>(
            static_cast<int64_t>(std::llround(t * static_cast<double>(latticeCount))),
            1,
            latticeCount - 1);

    const SectorCoord stepX = static_cast<SectorCoord>(dx / latticeCount);
    const SectorCoord stepY = static_cast<SectorCoord>(dy / latticeCount);
    const auto pointAt = [&](int64_t index) {
        return SectorTopologyCoordPoint{
                static_cast<SectorCoord>(static_cast<int64_t>(start->x) + static_cast<int64_t>(stepX) * index),
                static_cast<SectorCoord>(static_cast<int64_t>(start->y) + static_cast<int64_t>(stepY) * index)};
    };

    const int64_t snapStep = std::max<int64_t>(
            1,
            static_cast<int64_t>(std::max(1, state.gridSize)) * SectorCoordSubdivisions);
    const auto alignedToSnapStep = [&](int64_t index) {
        const SectorTopologyCoordPoint point = pointAt(index);
        return CoordAlignedToStep(point.x, snapStep)
                && CoordAlignedToStep(point.y, snapStep);
    };

    const int64_t xPeriod = CoordinateSequencePeriod(stepX, snapStep);
    const int64_t yPeriod = CoordinateSequencePeriod(stepY, snapStep);
    const int64_t searchPeriod = LcmClamped(xPeriod, yPeriod);
    bool foundSnapCandidate = false;
    int64_t bestSnapIndex = nearestLattice;
    for (int64_t offset = 0; offset <= searchPeriod; ++offset) {
        const int64_t candidates[2] = {
                nearestLattice - offset,
                nearestLattice + offset};
        for (int64_t candidate : candidates) {
            if (candidate <= 0 || candidate >= latticeCount) {
                continue;
            }
            if (!alignedToSnapStep(candidate)) {
                continue;
            }
            bestSnapIndex = candidate;
            foundSnapCandidate = true;
            break;
        }
        if (foundSnapCandidate) {
            break;
        }
    }

    const int64_t chosenIndex = foundSnapCandidate ? bestSnapIndex : nearestLattice;
    if (chosenIndex <= 0 || chosenIndex >= latticeCount) {
        error = "Insert point is too close to an endpoint";
        return false;
    }

    outPoint = pointAt(chosenIndex);
    return true;
}

void SectorEditor::UpdatePendingAuthoringInsertVertex(Vector2 mapPoint)
{
    PendingAuthoringInsertVertex& pending = state.pendingAuthoringInsertVertex;
    pending.hasPreviewPoint = false;
    pending.errorMessage.clear();

    int lineId = pending.active ? pending.lineId : -1;
    if (!pending.active) {
        lineId = FindAuthoringLineNearScreenPoint(MapToScreen(mapPoint));
    }

    if (FindSectorAuthoringLine(state.authoringGraph, lineId) == nullptr) {
        pending.lineId = -1;
        pending.errorMessage = "Insert Vertex: select or click an authoring line";
        return;
    }

    pending.lineId = lineId;
    SetHoveredSectorEditorAuthoringLine(state, lineId);
    SectorTopologyCoordPoint point;
    std::string error;
    if (!TryResolveAuthoringInsertVertexPoint(lineId, mapPoint, point, error)) {
        pending.errorMessage = error;
        return;
    }

    pending.previewPoint = point;
    pending.hasPreviewPoint = true;
}

void SectorEditor::CommitAuthoringInsertVertex(Vector2 screenPoint)
{
    const int lineId = state.pendingAuthoringInsertVertex.active
            ? state.pendingAuthoringInsertVertex.lineId
            : FindAuthoringLineNearScreenPoint(screenPoint);
    if (FindSectorAuthoringLine(state.authoringGraph, lineId) == nullptr) {
        statusText = "Insert Vertex: select or click an authoring line";
        return;
    }

    SectorTopologyCoordPoint point;
    std::string error;
    if (!TryResolveAuthoringInsertVertexPoint(lineId, ScreenToMap(screenPoint), point, error)) {
        state.pendingAuthoringInsertVertex.errorMessage = error;
        statusText = error;
        return;
    }

    SectorAuthoringInsertVertexResult result;
    if (!InsertSectorEditorAuthoringVertexOnLine(state, lineId, point, &result)) {
        statusText = InsertVertexFailureStatus(result.status);
        state.pendingAuthoringInsertVertex.errorMessage = statusText;
        return;
    }

    state.pendingAuthoringInsertVertex = PendingAuthoringInsertVertex{};
    statusText = "Inserted vertex on authoring line";
}

void SectorEditor::AddAuthoringLinePoint(SectorPoint point)
{
    ClearTopologySelectionOnly();

    std::string error;
    SectorTopologyCoordPoint topologyPoint;
    if (!ToTopologyCoordPoint(point, topologyPoint, error)) {
        state.pendingAuthoringLine.errorMessage = error;
        statusText = error;
        return;
    }

    const SectorEditorAuthoringLineToolClickResult result =
            ClickSectorEditorAuthoringLineTool(state, topologyPoint);
    switch (result.status) {
        case SectorEditorAuthoringLineToolClickStatus::StartedChain:
            statusText = "Line: click next point, Esc/right click stops chain";
            return;
        case SectorEditorAuthoringLineToolClickStatus::CreatedSegment:
            ClearSelection();
            SelectSectorEditorAuthoringLine(state, result.segment.lineId);
            statusText = "Created authoring line segment";
            return;
        case SectorEditorAuthoringLineToolClickStatus::ZeroLength:
            statusText = state.pendingAuthoringLine.errorMessage;
            return;
        case SectorEditorAuthoringLineToolClickStatus::Rejected:
            statusText = state.pendingAuthoringLine.errorMessage.empty()
                    ? "Authoring line segment rejected"
                    : state.pendingAuthoringLine.errorMessage;
            return;
    }
}

void SectorEditor::AddAuthoringRectanglePoint(SectorPoint point)
{
    ClearTopologySelectionOnly();

    std::string error;
    SectorTopologyCoordPoint topologyPoint;
    if (!ToTopologyCoordPoint(point, topologyPoint, error)) {
        state.pendingAuthoringRectangle.errorMessage = error;
        statusText = error;
        return;
    }

    if (!state.pendingAuthoringRectangle.active) {
        state.pendingAuthoringRectangle.active = true;
        state.pendingAuthoringRectangle.firstCorner = topologyPoint;
        state.pendingAuthoringRectangle.currentCorner = topologyPoint;
        state.pendingAuthoringRectangle.errorMessage.clear();
        statusText = "Rectangle: click opposite corner, right click/Esc cancels";
        return;
    }

    SectorEditorAuthoringRectangleResult result;
    if (!AddSectorEditorAuthoringRectangle(
                state,
                state.pendingAuthoringRectangle.firstCorner,
                topologyPoint,
                &result)) {
        state.pendingAuthoringRectangle.currentCorner = topologyPoint;
        state.pendingAuthoringRectangle.errorMessage = "Rectangle needs non-zero width and height";
        statusText = state.pendingAuthoringRectangle.errorMessage;
        return;
    }

    state.pendingAuthoringRectangle = PendingAuthoringRectangleDraw{};
    ClearSelection();
    SelectSectorEditorAuthoringLine(state, result.lineIds[0]);
    statusText = "Created authoring rectangle";
}

SectorPoint SectorEditor::CurrentSnappedSectorPoint() const
{
    const SectorPoint point = Vector2ToSectorPoint(state.snappedMouseMap);
    SectorPoint canonical;
    std::string error;
    return ToCanonicalSectorPoint(point, canonical, error) ? canonical : point;
}

bool SectorEditor::ToTopologyCoordPoint(
        SectorPoint point,
        SectorTopologyCoordPoint& outPoint,
        std::string& error) const
{
    SectorCoord x = 0;
    SectorCoord y = 0;
    if (!VisibleAuthoringToSectorCoord(point.x, x)
            || !VisibleAuthoringToSectorCoord(point.y, y)) {
        error = "Point is outside topology coordinate range";
        return false;
    }
    outPoint = SectorTopologyCoordPoint{x, y};
    error.clear();
    return true;
}

bool SectorEditor::ToCanonicalSectorPoint(
        SectorPoint point,
        SectorPoint& outPoint,
        std::string& error) const
{
    SectorTopologyCoordPoint topologyPoint;
    if (!ToTopologyCoordPoint(point, topologyPoint, error)) {
        return false;
    }
    outPoint = SectorTopologyCoordPointToSectorPoint(topologyPoint);
    return true;
}

SectorTopologySector* SectorEditor::SelectedTopologySector()
{
    if (state.topologySelectionKind != TopologySelectionKind::Sector) {
        return nullptr;
    }
    return FindSectorTopologySector(state.topologyMap, state.selectedTopologySectorId);
}

const SectorTopologySector* SectorEditor::SelectedTopologySector() const
{
    if (state.topologySelectionKind != TopologySelectionKind::Sector) {
        return nullptr;
    }
    return FindSectorTopologySector(state.topologyMap, state.selectedTopologySectorId);
}

SectorTopologyVertex* SectorEditor::SelectedTopologyVertex()
{
    if (state.topologySelectionKind != TopologySelectionKind::Vertex) {
        return nullptr;
    }
    return FindSectorTopologyVertex(state.topologyMap, state.selectedTopologyVertexId);
}

const SectorTopologyVertex* SectorEditor::SelectedTopologyVertex() const
{
    if (state.topologySelectionKind != TopologySelectionKind::Vertex) {
        return nullptr;
    }
    return FindSectorTopologyVertex(state.topologyMap, state.selectedTopologyVertexId);
}

SectorTopologySideDef* SectorEditor::SelectedTopologySideDef()
{
    if (state.topologySelectionKind != TopologySelectionKind::SideDef) {
        return nullptr;
    }
    return FindSectorTopologySideDef(state.topologyMap, state.selectedTopologySideDefId);
}

const SectorTopologySideDef* SectorEditor::SelectedTopologySideDef() const
{
    if (state.topologySelectionKind != TopologySelectionKind::SideDef) {
        return nullptr;
    }
    return FindSectorTopologySideDef(state.topologyMap, state.selectedTopologySideDefId);
}

SectorTopologyLineDef* SectorEditor::SelectedTopologyLineDef()
{
    if (state.topologySelectionKind != TopologySelectionKind::LineDef
            && state.topologySelectionKind != TopologySelectionKind::SideDef) {
        return nullptr;
    }
    return FindSectorTopologyLineDef(state.topologyMap, state.selectedTopologyLineDefId);
}

const SectorTopologyLineDef* SectorEditor::SelectedTopologyLineDef() const
{
    if (state.topologySelectionKind != TopologySelectionKind::LineDef
            && state.topologySelectionKind != TopologySelectionKind::SideDef) {
        return nullptr;
    }
    return FindSectorTopologyLineDef(state.topologyMap, state.selectedTopologyLineDefId);
}

SectorTopologyStaticPointLight* SectorEditor::SelectedTopologyLight()
{
    if (state.topologySelectionKind != TopologySelectionKind::StaticLight) {
        return nullptr;
    }
    return FindSectorTopologyStaticLight(state.topologyMap, state.selectedTopologyLightId);
}

const SectorTopologyStaticPointLight* SectorEditor::SelectedTopologyLight() const
{
    if (state.topologySelectionKind != TopologySelectionKind::StaticLight) {
        return nullptr;
    }
    return FindSectorTopologyStaticLight(state.topologyMap, state.selectedTopologyLightId);
}

SectorTopologyDynamicPointLight* SectorEditor::SelectedTopologyDynamicLight()
{
    if (state.topologySelectionKind != TopologySelectionKind::DynamicLight) {
        return nullptr;
    }
    return FindSectorTopologyDynamicLight(state.topologyMap, state.selectedTopologyDynamicLightId);
}

const SectorTopologyDynamicPointLight* SectorEditor::SelectedTopologyDynamicLight() const
{
    if (state.topologySelectionKind != TopologySelectionKind::DynamicLight) {
        return nullptr;
    }
    return FindSectorTopologyDynamicLight(state.topologyMap, state.selectedTopologyDynamicLightId);
}

void SectorEditor::ClearStaleTopologySelection()
{
    bool stale = false;
    if (state.topologySelectionKind == TopologySelectionKind::Sector) {
        stale = state.selectedTopologySectorId < 0
                || FindSectorTopologySector(state.topologyMap, state.selectedTopologySectorId) == nullptr;
    } else if (state.topologySelectionKind == TopologySelectionKind::Vertex) {
        stale = state.selectedTopologyVertexId < 0
                || FindSectorTopologyVertex(state.topologyMap, state.selectedTopologyVertexId) == nullptr;
    } else if (state.topologySelectionKind == TopologySelectionKind::SideDef) {
        const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(
                state.topologyMap,
                state.selectedTopologySideDefId);
        const SectorTopologyLineDef* lineDef = FindSectorTopologyLineDef(
                state.topologyMap,
                state.selectedTopologyLineDefId);
        stale = sideDef == nullptr
                || lineDef == nullptr
                || sideDef->lineDefId != lineDef->id;
        if (!stale) {
            state.selectedTopologyWallPart = ValidTopologyWallPartForSideDef(
                    state.topologyMap,
                    sideDef,
                    state.selectedTopologyWallPart);
        }
    } else if (state.topologySelectionKind == TopologySelectionKind::LineDef) {
        stale = state.selectedTopologyLineDefId < 0
                || FindSectorTopologyLineDef(state.topologyMap, state.selectedTopologyLineDefId) == nullptr;
        if (!stale && state.selectedTopologyWallPart == TopologyWallPart::Middle) {
            state.selectedTopologyWallPart = TopologyWallPart::Wall;
        }
    } else if (state.topologySelectionKind == TopologySelectionKind::StaticLight) {
        stale = state.selectedTopologyLightId < 0
                || FindSectorTopologyStaticLight(state.topologyMap, state.selectedTopologyLightId) == nullptr;
    } else if (state.topologySelectionKind == TopologySelectionKind::DynamicLight) {
        stale = state.selectedTopologyDynamicLightId < 0
                || FindSectorTopologyDynamicLight(state.topologyMap, state.selectedTopologyDynamicLightId) == nullptr;
    }

    if (stale) {
        state.topologySelectionKind = TopologySelectionKind::None;
        state.selectedTopologySectorId = -1;
        state.selectedTopologyVertexId = -1;
        state.selectedTopologySideDefId = -1;
        state.selectedTopologyLineDefId = -1;
        state.selectedTopologyLightId = -1;
        state.selectedTopologyDynamicLightId = -1;
        state.selectedTopologySideKind = SectorTopologySideKind::Front;
        uiState.idBufferSectorIndex = -1;
        uiState.idBufferLightIndex = -1;
        SyncSelectedSectorIdBuffer();
        SyncSelectedLightIdBuffer();
    }
}

void SectorEditor::MarkTopologyDocumentEdited(const char* status)
{
    state.topologyDocumentDirty = true;
    state.hasUnsavedChanges = true;
    InvalidateTopologyRenderCache();
    if (status != nullptr && status[0] != '\0') {
        statusText = status;
    }
}

bool SectorEditor::FinishTopologyMaterialMutation(const char* status, engine::AssetManager* assets)
{
    state.topologyRenderWarning.clear();
    MarkTopologyDocumentEdited(status);
    if (assets != nullptr && state.mode == SectorEditorMode::Preview3D && preview.IsRendererReady()) {
        return RebuildPreviewMeshesPreservingView(*assets);
    }
    return true;
}

bool SectorEditor::FinishMaterialActionResult(
        const SectorEditorMaterialActionResult& result,
        engine::AssetManager* assets)
{
    if (!result.changed) {
        if (!result.status.empty()) {
            statusText = result.status;
        }
        return false;
    }

    if (result.resetSurface3DUi) {
        ResetSurface3DUiState();
    }
    if (result.resetSectorUvInputs) {
        for (engine::UIFloatInputState& inputState : uiState.topologySectorUvInputs) {
            inputState = engine::UIFloatInputState{};
        }
    }
    if (result.resetSideDefUvInputs) {
        for (engine::UIFloatInputState& inputState : uiState.topologySideDefUvInputs) {
            inputState = engine::UIFloatInputState{};
        }
    }
    if (result.resetDecalInputs) {
        for (engine::UIFloatInputState& inputState : uiState.topologySectorDecalOpacityInputs) {
            inputState = engine::UIFloatInputState{};
        }
        for (engine::UIFloatInputState& inputState : uiState.topologySectorDecalBloomIntensityInputs) {
            inputState = engine::UIFloatInputState{};
        }
        uiState.topologySideDefDecalOpacityInput = engine::UIFloatInputState{};
        uiState.topologySideDefDecalBloomIntensityInput = engine::UIFloatInputState{};
        uiState.surface3DDecalOpacityInput = engine::UIFloatInputState{};
        uiState.surface3DDecalBloomIntensityInput = engine::UIFloatInputState{};
    }
    if (result.closeDecalTintModal) {
        state.decalTintModal = DecalTintModalState{};
    }

    return FinishTopologyMaterialMutation(result.status.c_str(), assets);
}

bool SectorEditor::FinishTopologyActionResult(const SectorEditorTopologyActionResult& result)
{
    if (!result.changed) {
        if (!result.status.empty()) {
            statusText = result.status;
        }
        return false;
    }

    MarkTopologyDocumentEdited(result.status.c_str());
    return true;
}

bool SectorEditor::SetLineDefBlocksPlayer(int lineDefId, bool blocksPlayer)
{
    const SectorTopologyLineDef* lineDef = FindSectorTopologyLineDef(state.topologyMap, lineDefId);
    if (lineDef == nullptr) {
        statusText = "Selected linedef is no longer valid.";
        return false;
    }
    if (lineDef->frontSideDefId == -1 || lineDef->backSideDefId == -1) {
        statusText = "Blocks Player is only editable on two-sided portals.";
        return false;
    }

    const int authoringLineId =
            FindSectorEditorAuthoringLineIdForTopologyLineDef(state, lineDefId);
    const SectorAuthoringLine* authoringLine =
            FindSectorAuthoringLine(state.authoringGraph, authoringLineId);
    if (authoringLine == nullptr) {
        statusText = "Blocks Player unavailable: selected linedef has no authoring line mapping.";
        return false;
    }
    if (authoringLine->flags.blocksPlayer == blocksPlayer) {
        return true;
    }

    const bool changed = MutateSectorEditorAuthoringLineForTopologyLineDef(
            state,
            lineDefId,
            blocksPlayer
                    ? "Enabled player blocking on authoring portal."
                    : "Disabled player blocking on authoring portal.",
            [blocksPlayer](SectorAuthoringLine& line) {
                line.flags.blocksPlayer = blocksPlayer;
                return true;
            });
    if (changed) {
        RebuildSectorCollisionWorld();
    }
    return changed;
}

void SectorEditor::ClearTransientTopologyEditStateAfterGeometryChange()
{
    ClearStaleTopologySelection();
    state.topologyRenderWarning.clear();
    state.hoveredSurface3D = SectorSurfaceHit{};
    state.selectedSurface3D = SectorSurfaceRef{};
    state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ResetSurface3DUiState();
}

void SectorEditor::SyncSelectedSectorIdBuffer()
{
    const SectorTopologySector* sector = SelectedTopologySector();
    if (sector == nullptr) {
        uiState.selectedSectorIdBuffer[0] = '\0';
        uiState.idBufferSectorIndex = -1;
        uiState.idEditError.clear();
        return;
    }

    if (uiState.idBufferSectorIndex == state.selectedTopologySectorId) {
        return;
    }

    std::snprintf(
            uiState.selectedSectorIdBuffer,
            sizeof(uiState.selectedSectorIdBuffer),
            "%s",
            sector->name.c_str());
    uiState.idBufferSectorIndex = state.selectedTopologySectorId;
    uiState.idEditError.clear();
}

void SectorEditor::SyncSelectedLightIdBuffer()
{
    const SectorTopologyStaticPointLight* light = SelectedTopologyLight();
    const SectorTopologyDynamicPointLight* dynamicLight = SelectedTopologyDynamicLight();
    if (light == nullptr && dynamicLight == nullptr) {
        uiState.selectedLightIdBuffer[0] = '\0';
        uiState.idBufferLightIndex = -1;
        if (state.topologySelectionKind == TopologySelectionKind::None) {
            uiState.idEditError.clear();
        }
        return;
    }

    const int lightId = light != nullptr ? light->id : dynamicLight->id;
    if (uiState.idBufferLightIndex == lightId) {
        return;
    }

    std::snprintf(uiState.selectedLightIdBuffer, sizeof(uiState.selectedLightIdBuffer), "%d", lightId);
    uiState.idBufferLightIndex = lightId;
    uiState.idEditError.clear();
}

bool SectorEditor::TryRenameSelectedTopologySector()
{
    SectorTopologySector* sector = SelectedTopologySector();
    if (sector == nullptr) {
        uiState.idEditError = "No topology sector selected";
        statusText = uiState.idEditError;
        return false;
    }

    const std::string newName = uiState.selectedSectorIdBuffer;
    if (newName == sector->name) {
        uiState.idEditError.clear();
        return true;
    }

    const bool hasAuthoringGraph =
            !state.authoringGraph.vertices.empty()
            || !state.authoringGraph.lines.empty()
            || !state.authoringGraph.lineSides.empty()
            || !state.authoringGraph.faceAnchors.empty();
    if (hasAuthoringGraph
            && (state.authoringDerivationState != SectorEditorAuthoringDerivationState::ValidCurrent
                    || state.authoringDerivedTopologyStale
                    || !state.authoringDerivation.success)) {
        uiState.idEditError = "Sector name edit unavailable: derived topology is not current";
        statusText = uiState.idEditError;
        return true;
    }

    const bool hasFaceAnchorMapping =
            FindSectorEditorAuthoringFaceAnchorIdForTopologySector(state, sector->id) >= 0;
    if (hasFaceAnchorMapping) {
        MutateSectorEditorAuthoringFaceAnchorForTopologySector(
                state,
                sector->id,
                TextFormat("Renamed authoring face anchor %d", sector->id),
                [&newName](SectorAuthoringFaceAnchor& anchor) {
                    if (anchor.name == newName) {
                        return false;
                    }
                    anchor.name = newName;
                    return true;
                });
        uiState.idEditError.clear();
        return true;
    }
    if (hasAuthoringGraph) {
        uiState.idEditError = "Sector name edit unavailable: selected sector has no face anchor mapping";
        statusText = uiState.idEditError;
        return true;
    }

    sector->name = newName;
    uiState.idEditError.clear();
    MarkTopologyDocumentEdited(TextFormat("Renamed topology sector %d", sector->id));
    return true;
}

bool SectorEditor::TryRenameSelectedLight()
{
    if (SelectedTopologyLight() == nullptr && SelectedTopologyDynamicLight() == nullptr) {
        uiState.idEditError = "No light selected";
        statusText = uiState.idEditError;
        return false;
    }

    uiState.idEditError = "Topology light IDs are stable";
    statusText = uiState.idEditError;
    return false;
}

bool SectorEditor::DeleteSelectedLight()
{
    const SectorTopologyStaticPointLight* light = SelectedTopologyLight();
    const SectorTopologyDynamicPointLight* dynamicLight = SelectedTopologyDynamicLight();
    if (light == nullptr && dynamicLight == nullptr) {
        return false;
    }

    const int lightId = light != nullptr ? light->id : dynamicLight->id;
    OpenConfirmation(
            "Delete Light",
            light != nullptr
                    ? TextFormat("Delete static light %d?", lightId)
                    : TextFormat("Delete dynamic light %d?", lightId),
            [this, lightId, isDynamic = dynamicLight != nullptr]() {
                if (isDynamic) {
                    DeleteDynamicLightById(lightId);
                } else {
                    DeleteLightById(lightId);
                }
            });
    return true;
}

bool SectorEditor::DeleteLightById(int topologyLightId)
{
    const bool hadLight = FindSectorTopologyStaticLight(state.topologyMap, topologyLightId) != nullptr;
    const SectorEditorTopologyActionResult result = DeleteStaticLight(state.topologyMap, topologyLightId);
    if (!result.changed) {
        if (!hadLight) {
            ClearStaleTopologySelection();
        }
        FinishTopologyActionResult(result);
        return false;
    }

    if (!hadLight) {
        ClearStaleTopologySelection();
        return false;
    }

    if (state.selectedTopologyLightId == topologyLightId) {
        ClearSelection();
    }
    if (state.hoveredTopologyLightId == topologyLightId) {
        state.hoveredTopologyLightId = -1;
    }
    if (state.lightDrag.topologyLightId == topologyLightId) {
        state.lightDrag = LightDragState{};
    }
    return FinishTopologyActionResult(result);
}

void SectorEditor::AddStaticLightAt(Vector2 mapPoint)
{
    const int sectorId = FindTopologySectorAt(mapPoint);
    const SectorEditorAddStaticLightResult result = AddStaticLightToSector(
            state.topologyMap,
            sectorId,
            mapPoint);
    if (!result.changed) {
        if (!result.status.empty()) {
            statusText = result.status;
        }
        return;
    }

    SelectTopologyLight(result.lightId);
    SectorEditorTopologyActionResult finish;
    finish.changed = true;
    finish.status = result.status;
    FinishTopologyActionResult(finish);
}

bool SectorEditor::DeleteDynamicLightById(int topologyLightId)
{
    const bool hadLight = FindSectorTopologyDynamicLight(state.topologyMap, topologyLightId) != nullptr;
    const SectorEditorTopologyActionResult result = DeleteDynamicLight(state.topologyMap, topologyLightId);
    if (!result.changed) {
        if (!hadLight) {
            ClearStaleTopologySelection();
        }
        FinishTopologyActionResult(result);
        return false;
    }

    if (!hadLight) {
        ClearStaleTopologySelection();
        return false;
    }

    if (state.selectedTopologyDynamicLightId == topologyLightId) {
        ClearSelection();
    }
    if (state.hoveredTopologyDynamicLightId == topologyLightId) {
        state.hoveredTopologyDynamicLightId = -1;
    }
    if (state.lightDrag.topologyLightId == topologyLightId) {
        state.lightDrag = LightDragState{};
    }
    return FinishTopologyActionResult(result);
}

void SectorEditor::AddDynamicLightAt(Vector2 mapPoint)
{
    const int sectorId = FindTopologySectorAt(mapPoint);
    const SectorEditorAddDynamicLightResult result = AddDynamicLightToSector(
            state.topologyMap,
            sectorId,
            mapPoint);
    if (!result.changed) {
        if (!result.status.empty()) {
            statusText = result.status;
        }
        return;
    }

    SelectTopologyDynamicLight(result.lightId);
    SectorEditorTopologyActionResult finish;
    finish.changed = true;
    finish.status = result.status;
    FinishTopologyActionResult(finish);
}

bool SectorEditor::BakeLightmaps()
{
    return StartLightmapBake();
}

bool SectorEditor::StartLightmapBake()
{
    if (lightmapBake.progress.running.load() || lightmapBake.worker.joinable() || lightmapBake.modalOpen) {
        statusText = "Lightmap bake already running";
        return false;
    }

    if (!state.hasCurrentLevelPath) {
        statusText = "Save the level before baking lightmaps";
        return false;
    }

    std::string gateMessage;
    if (!CanUseCurrentAuthoringDerivedTopologyForLightmapBake(state, &gateMessage)) {
        statusText = gateMessage.empty() ? "Bake failed: derived topology is not current" : gateMessage;
        return false;
    }

    if (state.topologyMap.sectors.empty()) {
        statusText = "Bake failed: no sectors";
        return false;
    }

    LevelPaths levelPaths;
    std::string pathError;
    if (!BuildLevelPaths(state.currentLevelName, levelPaths, pathError)) {
        statusText = TextFormat("Bake failed: %s", pathError.c_str());
        return false;
    }
    const std::string finalOutputPath = levelPaths.lightmapFilePath.string();
    const std::string temporaryOutputPath = MakeTemporaryLightmapPath(finalOutputPath);

    SectorTopologyLightmapBakeInput input;
    input.mapSnapshot = state.topologyMap;
    input.expectedSourceHash = ComputeSectorLightmapSourceHash(state.topologyMap);
    input.finalOutputPath = finalOutputPath;
    input.temporaryOutputPath = temporaryOutputPath;

    DeleteFileIfExists(temporaryOutputPath);

    lightmapBake.progress.phase.store(SectorLightmapBakePhase::Preparing);
    lightmapBake.progress.completedWork.store(0);
    lightmapBake.progress.totalWork.store(1);
    lightmapBake.progress.cancelRequested.store(false);
    lightmapBake.progress.running.store(true);
    lightmapBake.modalOpen = true;
    lightmapBake.awaitingAcknowledgement = false;
    lightmapBake.cancelButtonPressed = false;
    lightmapBake.terminalMessage.clear();
    lightmapBake.terminalSuccess = false;
    lightmapBake.terminalCancelled = false;
    lightmapBake.temporaryOutputPath = temporaryOutputPath;
    lightmapBake.startTimeSeconds = GetTime();
    lightmapBake.completedTimeSeconds = 0.0;
    {
        std::lock_guard<std::mutex> lock(lightmapBake.resultMutex);
        lightmapBake.pendingResult.reset();
    }

    LightmapBakeProgress* progress = &lightmapBake.progress;
    std::mutex* resultMutex = &lightmapBake.resultMutex;
    std::optional<SectorLightmapBakeAsyncResult>* pendingResult = &lightmapBake.pendingResult;

    lightmapBake.worker = std::thread([input = std::move(input), progress, resultMutex, pendingResult]() mutable {
        SectorLightmapBakeAsyncResult asyncResult;
        asyncResult.expectedSourceHash = input.expectedSourceHash;
        asyncResult.sourceMapRevision = input.editorMapRevision;
        asyncResult.finalOutputPath = input.finalOutputPath;
        asyncResult.temporaryOutputPath = input.temporaryOutputPath;

        SectorLightmapBakeCallbacks callbacks;
        callbacks.onProgress = [progress](SectorLightmapBakePhase phase, uint32_t completedWork, uint32_t totalWork) {
            progress->phase.store(phase);
            progress->completedWork.store(completedWork);
            progress->totalWork.store(totalWork);
        };
        callbacks.isCancellationRequested = [progress]() {
            return progress->cancelRequested.load();
        };

        std::string error;
        const bool succeeded = BakeSectorLightmap(input, callbacks, asyncResult.bakeResult, error);
        asyncResult.cancelled = !succeeded && progress->cancelRequested.load();
        asyncResult.succeeded = succeeded && !asyncResult.cancelled;
        asyncResult.errorMessage = error.empty()
                ? (asyncResult.cancelled ? "Bake cancelled" : "Bake failed")
                : error;
        if (asyncResult.succeeded) {
            asyncResult.bakeReportText = FormatSectorLightmapBakeReport(asyncResult.bakeResult);
        }

        {
            std::lock_guard<std::mutex> lock(*resultMutex);
            *pendingResult = std::move(asyncResult);
        }

        if (progress->cancelRequested.load()) {
            progress->phase.store(SectorLightmapBakePhase::Cancelled);
        } else if (succeeded) {
            progress->phase.store(SectorLightmapBakePhase::Completed);
        } else {
            progress->phase.store(SectorLightmapBakePhase::Failed);
        }
        progress->completedWork.store(1);
        progress->totalWork.store(1);
        progress->running.store(false);
    });

    statusText = "Baking lightmap...";
    return true;
}

void SectorEditor::PollLightmapBakeResult(engine::AssetManager& assets)
{
    std::optional<SectorLightmapBakeAsyncResult> pending;
    {
        std::lock_guard<std::mutex> lock(lightmapBake.resultMutex);
        if (lightmapBake.pendingResult.has_value()) {
            pending = std::move(lightmapBake.pendingResult);
            lightmapBake.pendingResult.reset();
        }
    }

    if (!pending.has_value()) {
        return;
    }

    JoinLightmapBakeWorker();
    lightmapBake.completedTimeSeconds = GetTime();
    ConsumeLightmapBakeResult(*pending, assets);
}

void SectorEditor::RequestLightmapBakeCancel()
{
    if (!lightmapBake.progress.running.load()) {
        return;
    }
    lightmapBake.progress.cancelRequested.store(true);
    lightmapBake.cancelButtonPressed = true;
    statusText = "Cancelling bake...";
}

void SectorEditor::JoinLightmapBakeWorker()
{
    if (lightmapBake.worker.joinable()) {
        lightmapBake.worker.join();
    }
}

void SectorEditor::ShutdownLightmapBake()
{
    if (lightmapBake.progress.running.load()) {
        lightmapBake.progress.cancelRequested.store(true);
    }
    JoinLightmapBakeWorker();
    DeleteFileIfExists(lightmapBake.temporaryOutputPath);
    lightmapBake.temporaryOutputPath.clear();
    {
        std::lock_guard<std::mutex> lock(lightmapBake.resultMutex);
        if (lightmapBake.pendingResult.has_value()) {
            DeleteFileIfExists(lightmapBake.pendingResult->temporaryOutputPath);
            lightmapBake.pendingResult.reset();
        }
    }
    lightmapBake.modalOpen = false;
    lightmapBake.awaitingAcknowledgement = false;
    lightmapBake.progress.running.store(false);
    lightmapBake.progress.cancelRequested.store(false);
    lightmapBake.progress.phase.store(SectorLightmapBakePhase::Idle);
}

bool SectorEditor::IsLightmapBakeBlocking() const
{
    return lightmapBake.modalOpen || lightmapBake.progress.running.load();
}

bool SectorEditor::ConsumeLightmapBakeResult(const SectorLightmapBakeAsyncResult& result, engine::AssetManager& assets)
{
    lightmapBake.progress.phase.store(result.cancelled
            ? SectorLightmapBakePhase::Cancelled
            : (result.succeeded ? SectorLightmapBakePhase::InstallingResult : SectorLightmapBakePhase::Failed));

    if (result.cancelled) {
        DeleteFileIfExists(result.temporaryOutputPath);
        lightmapBake.terminalMessage = "Lightmap bake cancelled";
        lightmapBake.terminalCancelled = true;
        lightmapBake.awaitingAcknowledgement = true;
        statusText = lightmapBake.terminalMessage;
        return false;
    }

    if (!result.succeeded) {
        DeleteFileIfExists(result.temporaryOutputPath);
        lightmapBake.terminalMessage = result.errorMessage.empty() ? "Bake failed" : result.errorMessage;
        lightmapBake.terminalSuccess = false;
        lightmapBake.awaitingAcknowledgement = true;
        statusText = lightmapBake.terminalMessage;
        TraceLog(LOG_WARNING, "%s", lightmapBake.terminalMessage.c_str());
        return false;
    }

    const bool installed = InstallLightmapBakeResult(result, assets);
    lightmapBake.modalOpen = false;
    lightmapBake.awaitingAcknowledgement = false;
    lightmapBake.cancelButtonPressed = false;
    lightmapBake.terminalSuccess = installed;
    lightmapBake.terminalCancelled = false;
    lightmapBake.temporaryOutputPath.clear();
    lightmapBake.progress.phase.store(installed ? SectorLightmapBakePhase::Completed : SectorLightmapBakePhase::Failed);
    return installed;
}

bool SectorEditor::InstallLightmapBakeResult(const SectorLightmapBakeAsyncResult& result, engine::AssetManager& assets)
{
    if (ComputeSectorLightmapSourceHash(state.topologyMap) != result.expectedSourceHash) {
        DeleteFileIfExists(result.temporaryOutputPath);
        statusText = "Bake discarded: document changed during bake";
        return false;
    }

    std::error_code ec;
    if (!std::filesystem::exists(result.temporaryOutputPath, ec) || ec) {
        DeleteFileIfExists(result.temporaryOutputPath);
        statusText = "Bake failed: temporary lightmap output missing";
        return false;
    }

    const std::filesystem::path finalPath(result.finalOutputPath);
    if (!finalPath.parent_path().empty()) {
        std::filesystem::create_directories(finalPath.parent_path(), ec);
        if (ec) {
            DeleteFileIfExists(result.temporaryOutputPath);
            statusText = TextFormat("Bake failed: could not create output directory: %s", ec.message().c_str());
            return false;
        }
    }

    std::filesystem::copy_file(
            result.temporaryOutputPath,
            result.finalOutputPath,
            std::filesystem::copy_options::overwrite_existing,
            ec
    );
    if (ec) {
        DeleteFileIfExists(result.temporaryOutputPath);
        statusText = TextFormat("Bake failed: could not install lightmap: %s", ec.message().c_str());
        return false;
    }
    DeleteFileIfExists(result.temporaryOutputPath);

    LevelPaths levelPaths;
    std::string pathError;
    if (!BuildLevelPaths(state.currentLevelName, levelPaths, pathError)) {
        DeleteFileIfExists(result.temporaryOutputPath);
        statusText = TextFormat("Bake failed: %s", pathError.c_str());
        return false;
    }
    state.topologyMap.bakedLightmap.path = levelPaths.lightmapAssetPath;
    state.topologyMap.bakedLightmap.width = result.bakeResult.width;
    state.topologyMap.bakedLightmap.height = result.bakeResult.height;
    state.topologyMap.bakedLightmap.sourceHash = result.bakeResult.sourceHash;
    state.hasUnsavedChanges = true;
    state.topologyDocumentDirty = true;

    std::istringstream report(result.bakeReportText);
    std::string line;
    while (std::getline(report, line)) {
        TraceLog(LOG_INFO, "%s", line.c_str());
    }
    TraceLog(LOG_INFO, "INFO: Lightmap bake completed asynchronously in %.2fs", result.bakeResult.totalBakeSeconds);

    if (state.mode == SectorEditorMode::Preview3D && preview.IsRendererReady()) {
        RebuildPreviewMeshesPreservingView(assets);
    }

    statusText = TextFormat("Baked lightmap in %.1fs", result.bakeResult.totalBakeSeconds);
    return true;
}

bool SectorEditor::FindTopologyVertexNearScreenPoint(
        Vector2 screenPoint,
        int& outVertexId,
        SectorTopologyCoordPoint& outPoint) const
{
    float bestDistance2 = ScreenVertexSnapPixels * ScreenVertexSnapPixels;
    int bestVertexId = -1;
    SectorTopologyCoordPoint bestPoint{};

    for (const SectorTopologyVertex& vertex : state.topologyMap.vertices) {
        const Vector2 screenVertex = MapToScreen(SectorTopologyVertexToMap(vertex));
        const float dx = screenVertex.x - screenPoint.x;
        const float dy = screenVertex.y - screenPoint.y;
        const float distance2 = dx * dx + dy * dy;
        if (distance2 > bestDistance2) {
            continue;
        }
        if (bestVertexId >= 0
                && std::fabs(distance2 - bestDistance2) <= 0.001f
                && vertex.id >= bestVertexId) {
            continue;
        }
        bestDistance2 = distance2;
        bestVertexId = vertex.id;
        bestPoint = SectorTopologyCoordPoint{vertex.x, vertex.y};
    }

    if (bestVertexId < 0) {
        outVertexId = -1;
        outPoint = SectorTopologyCoordPoint{};
        return false;
    }

    outVertexId = bestVertexId;
    outPoint = bestPoint;
    return true;
}

bool SectorEditor::SnapAuthoringVertexMoveTarget(
        Vector2 mapPoint,
        SectorTopologyCoordPoint& outPoint,
        std::string& error) const
{
    const float grid = static_cast<float>(std::max(1, state.gridSize));
    const Vector2 snapped{
            std::round(mapPoint.x / grid) * grid,
            std::round(mapPoint.y / grid) * grid
    };

    SectorCoord x = 0;
    SectorCoord y = 0;
    if (!VisibleAuthoringToSectorCoord(snapped.x, x)
            || !VisibleAuthoringToSectorCoord(snapped.y, y)) {
        error = "Move target is outside authoring coordinate range";
        outPoint = SectorTopologyCoordPoint{};
        return false;
    }

    outPoint = SectorTopologyCoordPoint{x, y};
    error.clear();
    return true;
}

void SectorEditor::RenderPreview3D(engine::AssetManager& assets)
{
    RenderPreview3DScene(assets);
    RenderPreview3DOverlays();
}

void SectorEditor::RenderPreview3DScene(engine::AssetManager& assets)
{
    preview.DrawScene(assets, state.useBakedAmbientOcclusion);
}

void SectorEditor::ApplyPreview3DBloom(engine::AssetManager& assets, RenderTexture2D& sceneTarget)
{
    if (state.mode != SectorEditorMode::Preview3D) {
        return;
    }
    preview.ApplyEmissiveDecalBloomToScene(assets, sceneTarget);
}

void SectorEditor::RenderPreview3DOverlays()
{
    if (!state.previewUiHidden) {
        DrawPreviewSurfaceHighlights();
    }
}

SectorSurfaceHit SectorEditor::PickSectorSurface3D(Vector2 mousePosition, Rectangle viewportRect) const
{
    SectorSurfaceHit best;
    if (!preview.IsRendererReady()) {
        return best;
    }

    const Vector2 localMouse{
            mousePosition.x - viewportRect.x,
            mousePosition.y - viewportRect.y
    };
    const Ray ray = GetScreenToWorldRayEx(
            localMouse,
            preview.RenderCamera(),
            static_cast<int>(std::round(viewportRect.width)),
            static_cast<int>(std::round(viewportRect.height))
    );

    const SectorGeneratedSurfaceHit hit = PickSectorGeneratedGeometry(
            preview.RenderedGeometry(),
            ray,
            preview.VisibilityResult(),
            GeometryEpsilon);
    if (!hit.hit) {
        return best;
    }

    best.hit = true;
    best.surface = ToEditorSurfaceRef(hit.ref);
    best.worldPosition = hit.worldPosition;
    best.distance = hit.distance;
    return best;
}

void SectorEditor::DrawPreviewSurfaceHighlights() const
{
    if (!preview.IsRendererReady() || state.freeflyController.mouseLookEnabled) {
        return;
    }

    auto drawSurface = [this](SectorSurfaceRef surface, Color color, float thickness) {
        if (!IsValidSurfaceRef(surface)) {
            return;
        }
        const float lift = IsWallSurface(surface.kind) ? PreviewHighlightLift : PreviewHighlightLift * 2.0f;
        for (const SectorGeneratedSurface& generated : preview.RenderedGeometry().surfaces) {
            if (!ShouldIncludeSectorGeneratedSurfaceForVisibility(generated, preview.VisibilityResult())) {
                continue;
            }
            const SectorSurfaceRef generatedRef = ToEditorSurfaceRef(generated.ref);
            if (!SameSurfaceRef(surface, generatedRef)) {
                continue;
            }
            const Vector3 offset = Vector3Scale(generated.normal, lift);
            for (size_t i = 0; i + 2 < generated.vertices.size(); i += 3) {
                const Vector3 a = Vector3Add(generated.vertices[i + 0].position, offset);
                const Vector3 b = Vector3Add(generated.vertices[i + 1].position, offset);
                const Vector3 c = Vector3Add(generated.vertices[i + 2].position, offset);
                DrawLine3D(a, b, color);
                DrawLine3D(b, c, color);
                DrawLine3D(c, a, color);
            }
        }
        (void)thickness;
    };

    BeginMode3D(preview.RenderCamera());
    if (state.hoveredSurface3D.hit
            && !SameSurfaceRef(state.hoveredSurface3D.surface, state.selectedSurface3D)) {
        drawSurface(state.hoveredSurface3D.surface, Color{248, 238, 124, 235}, 2.0f);
    }
    if (state.selectedSurface3D.kind != SectorSurfaceKind::None) {
        drawSurface(state.selectedSurface3D, Color{84, 204, 255, 255}, 3.0f);
    }
    EndMode3D();
}

void SectorEditor::DrawPreviewOverlay(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    const Rectangle panel{32.0f, 32.0f, 980.0f, 232.0f};
    DrawRectangleRec(panel, Color{12, 15, 20, 205});
    DrawRectangleLinesEx(panel, config.borderThickness, config.borderColor);

    const SectorViewPose pose = ActivePreviewPose();
    const Vector3 position = pose.position;
    engine::Text(
            config,
            assets,
            Rectangle{panel.x + 18.0f, panel.y + 14.0f, panel.width - 36.0f, 34.0f},
            font,
            TextFormat("3D Mode | %s", PreviewControlModeName(state.previewControlMode)),
            engine::UITextJustify::Left
    );
    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_preview_settings",
                Rectangle{panel.x + panel.width - 152.0f, panel.y + 12.0f, 130.0f, 36.0f},
                font,
                "Settings")) {
        OpenPreviewSettingsModal();
    }
    const char* interactionText = state.freeflyController.mouseLookEnabled
            ? (state.previewControlMode == SectorPreviewControlMode::Gameplay
                    ? "WASD move | Space jump | Shift run | Mouse look | F3 FreeFly/Gameplay | F1 AO | F2 hide UI | F11 cursor | Tab/Escape return"
                    : "WASD move | Mouse look | Space/Ctrl up/down | F3 FreeFly/Gameplay | F1 AO | F2 hide UI | F11 cursor | Tab/Escape return")
            : "F1 AO | F2 hide UI | F3 FreeFly/Gameplay | F11 cursor | click surface to select | Tab/Escape return";
    engine::Text(
            config,
            assets,
            Rectangle{panel.x + 18.0f, panel.y + 54.0f, panel.width - 36.0f, 30.0f},
            font,
            interactionText,
            engine::UITextJustify::Left,
            config.mutedTextColor
    );
    std::string collisionStatus;
    if (state.previewControlMode == SectorPreviewControlMode::Gameplay) {
        if (state.sectorCollisionWorldValid) {
            if (state.previewCollisionNoclipFallback) {
                collisionStatus = "Gameplay collision: no sector | noclip";
            } else if (state.fpsControllerState.currentSectorId == 0
                    || !state.previewVerticalResult.hasSector) {
                collisionStatus = "Gameplay collision: on | no sector";
            } else {
                std::string blockText;
                if (state.previewMoveResult.hitWall) {
                    blockText += "wall ";
                }
                if (state.previewMoveResult.blockedByStep) {
                    blockText += "step ";
                }
                if (state.previewMoveResult.blockedByCeiling) {
                    blockText += "ceiling ";
                }
                if (blockText.empty()) {
                    blockText = "clear";
                }
                const char* verticalState = state.previewVerticalResult.cannotFit
                        ? "cannot fit"
                        : (state.fpsControllerState.grounded
                                ? "grounded"
                                : (state.fpsControllerState.verticalVelocity > 0.0f ? "jumping" : "falling"));
                collisionStatus = TextFormat(
                        "Gameplay collision: sector %d | %s%s | vertical %s | block %s | r %.2f step %.2f jump %.2f | floor %.2f feet %.2f vel %.2f",
                        state.fpsControllerState.currentSectorId,
                        verticalState,
                        state.previewVerticalResult.cannotFit ? " grounded" : "",
                        VerticalTransitionName(state.previewVerticalResult.transition),
                        blockText.c_str(),
                        state.fpsControllerConfig.playerRadius,
                        state.fpsControllerConfig.stepHeight,
                        state.fpsControllerConfig.jumpHeight,
                        state.previewVerticalResult.floorZ,
                        state.fpsControllerState.feetPosition.y,
                        state.fpsControllerState.verticalVelocity);
            }
        } else {
            collisionStatus = "Gameplay collision: unavailable";
        }
    }
    engine::Text(
            config,
            assets,
            Rectangle{panel.x + 18.0f, panel.y + 92.0f, panel.width - 36.0f, 30.0f},
            font,
            TextFormat(
                    "pos %.2f %.2f %.2f | sectors %zu | batches %zu | triangles %d",
                    position.x,
                    position.y,
                    position.z,
                    preview.SectorCount(),
                    preview.BatchCount(),
                    preview.TriangleCount()
            ),
            engine::UITextJustify::Left,
            config.mutedTextColor
    );
    engine::Text(
            config,
            assets,
            Rectangle{panel.x + 18.0f, panel.y + 122.0f, panel.width - 36.0f, 30.0f},
            font,
            collisionStatus.empty() ? "3D collision: FreeFly noclip" : collisionStatus.c_str(),
            engine::UITextJustify::Left,
            config.mutedTextColor
    );
    engine::Text(
            config,
            assets,
            Rectangle{panel.x + 18.0f, panel.y + 152.0f, panel.width - 36.0f, 30.0f},
            font,
            preview.VisibilityDebugText().c_str(),
            engine::UITextJustify::Left,
            config.mutedTextColor
    );
    const char* previewStatusText = statusText.empty() ? "Ready" : statusText.c_str();
    const char* collisionWarningText = state.sectorCollisionWorldWarning.empty()
            ? previewStatusText
            : state.sectorCollisionWorldWarning.c_str();
    engine::Text(
            config,
            assets,
            Rectangle{panel.x + 18.0f, panel.y + 190.0f, panel.width - 36.0f, 30.0f},
            font,
            state.previewControlMode == SectorPreviewControlMode::Gameplay
                    ? TextFormat(
                            "assets %.0f%% | Lightmap: %s | AO: %s | walk %.1f | run %.1f | eye %.1f | height %.1f | gravity %.1f | jump %.1f | %s%s",
                            preview.RendererAssetProgress(assets) * 100.0f,
                            preview.RendererLightmapStatusText(),
                            state.useBakedAmbientOcclusion ? "on" : "off",
                            state.fpsControllerConfig.walkSpeed,
                            state.fpsControllerConfig.runSpeed,
                            state.fpsControllerConfig.eyeHeight,
                            state.fpsControllerConfig.playerHeight,
                            state.fpsControllerConfig.gravity,
                            state.fpsControllerConfig.jumpHeight,
                            collisionWarningText,
                            state.topologyDocumentDirty ? " | unsaved changes" : "")
                    : TextFormat(
                            "assets %.0f%% | Lightmap: %s | AO: %s | %s%s",
                            preview.RendererAssetProgress(assets) * 100.0f,
                            preview.RendererLightmapStatusText(),
                            state.useBakedAmbientOcclusion ? "on" : "off",
                            collisionWarningText,
                            state.topologyDocumentDirty ? " | unsaved changes" : ""),
            engine::UITextJustify::Left,
            state.topologyDocumentDirty ? Color{236, 196, 92, 255} : config.mutedTextColor
    );
}

Rectangle SectorEditor::BuildPreviewUvPanelRect() const
{
    return Rectangle{330.0f, EditorHeight - 252.0f, 1260.0f, 220.0f};
}

void SectorEditor::DrawPreviewUvPanel(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    if (state.freeflyController.mouseLookEnabled) {
        return;
    }

    if (!IsValidSurfaceRef(state.selectedSurface3D)
            || !IsValidTopologySurfaceEditTarget(state.selectedTopologySurface3D)) {
        state.selectedSurface3D = SectorSurfaceRef{};
        state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
        return;
    }
    if (!EnsureSelectedSurface3DAuthoringMappingCurrent()) {
        return;
    }

    const TopologySurfaceEditTarget target = state.selectedTopologySurface3D;
    const bool targetIsMiddle = IsMiddleTopologyEditTarget(target.kind);
    if (targetIsMiddle && state.activeTopologyMaterialLayer != TopologyMaterialLayer::Base) {
        state.activeTopologyMaterialLayer = TopologyMaterialLayer::Base;
        ResetSurface3DUiState();
    }
    const TopologyMaterialLayer layer = EffectiveTopologyMaterialLayer(
            target.kind,
            state.activeTopologyMaterialLayer);
    const Rectangle panel = BuildPreviewUvPanelRect();
    DrawRectangleRec(panel, Color{12, 15, 20, 230});
    DrawRectangleLinesEx(panel, config.borderThickness, config.borderColor);

    std::string targetLabel;
    int portalLineDefId = -1;
    bool portalBlocksPlayer = false;

    if (target.kind == TopologySurfaceEditTargetKind::SectorFloor
            || target.kind == TopologySurfaceEditTargetKind::SectorCeiling) {
        const SectorTopologySector* sector = FindSectorTopologySector(state.topologyMap, target.sectorId);
        if (sector == nullptr) {
            state.selectedSurface3D = SectorSurfaceRef{};
            state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
            return;
        }
        targetLabel = TextFormat(
                "%s | sector %d",
                target.kind == TopologySurfaceEditTargetKind::SectorFloor ? "Floor" : "Ceiling",
                sector->id);
    } else {
        const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(state.topologyMap, target.sideDefId);
        if (sideDef == nullptr) {
            state.selectedSurface3D = SectorSurfaceRef{};
            state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
            return;
        }
        const TopologyWallPart wallPart = TopologyEditTargetWallPart(target.kind);
        (void)wallPart;
        const SectorTopologyLineDef* lineDef = FindSectorTopologyLineDef(
                state.topologyMap,
                sideDef->lineDefId);
        if (lineDef != nullptr
                && lineDef->frontSideDefId != -1
                && lineDef->backSideDefId != -1) {
            portalLineDefId = lineDef->id;
            portalBlocksPlayer = lineDef->flags.blocksPlayer;
        }
        targetLabel = TextFormat(
                "%s | sideDef %d line %d",
                SurfaceKindName(state.selectedSurface3D.kind),
                sideDef->id,
                sideDef->lineDefId);
    }
    targetLabel = BuildSectorEditorSurface3DTargetLabel(state, state.selectedSurface3D, target);

    const float margin = 18.0f;
    const float top = panel.y + margin;
    const float inputTop = panel.y + 104.0f;
    const float colW = 132.0f;
    const float gap = 14.0f;
    const float startX = panel.x + 390.0f;

    engine::Text(
            ui,
            config,
            assets,
            Rectangle{panel.x + margin, top, 350.0f, 34.0f},
            font,
            targetLabel.c_str(),
            engine::UITextJustify::Left,
            config.textColor
    );
    if (!targetIsMiddle) {
        const float layerLabelW = 68.0f;
        const float layerButtonW = 78.0f;
        engine::Text(
                ui,
                config,
                assets,
                Rectangle{panel.x + margin, top + 36.0f, layerLabelW, 30.0f},
                font,
                "Layer:",
                engine::UITextJustify::Left,
                config.mutedTextColor);
        if (engine::ToolButton(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_3d_layer_base",
                    Rectangle{panel.x + margin + layerLabelW, top + 34.0f, layerButtonW, 32.0f},
                    font,
                    "Base",
                    layer == TopologyMaterialLayer::Base)) {
            state.activeTopologyMaterialLayer = TopologyMaterialLayer::Base;
            ResetSurface3DUiState();
        }
        if (engine::ToolButton(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_3d_layer_decal",
                    Rectangle{panel.x + margin + layerLabelW + layerButtonW + 8.0f, top + 34.0f, layerButtonW, 32.0f},
                    font,
                    "Decal",
                    layer == TopologyMaterialLayer::Decal)) {
            state.activeTopologyMaterialLayer = TopologyMaterialLayer::Decal;
            ResetSurface3DUiState();
        }
    }

    const std::string currentTexture = CurrentTextureForSurface(target, layer);
    const bool missingTexture = !currentTexture.empty()
            && FindSectorTopologyTexture(state.topologyMap, currentTexture) == nullptr;
    engine::Text(
            ui,
            config,
            assets,
            Rectangle{panel.x + margin, top + 72.0f, 350.0f, 30.0f},
            font,
            targetIsMiddle
                    ? TextFormat("Middle texture %s", currentTexture.empty() ? "<none>" : currentTexture.c_str())
                    : TextFormat("%s texture %s", TopologyMaterialLayerName(layer), currentTexture.empty() ? "<none>" : currentTexture.c_str()),
            engine::UITextJustify::Left,
            missingTexture ? config.invalidColor : config.mutedTextColor
    );

    const bool decalAssigned = targetIsMiddle
            ? !currentTexture.empty()
            : (layer != TopologyMaterialLayer::Decal || IsDecalAssigned(target));
    const SectorTopologyUvSettings* uv = UvForSurface(target, layer);
    Vector2 uvScale = uv == nullptr ? Vector2{1.0f, 1.0f} : uv->scale;
    Vector2 uvOffset = uv == nullptr ? Vector2{0.0f, 0.0f} : uv->offset;
    if (!decalAssigned) {
        engine::Text(
                ui,
                config,
                assets,
                Rectangle{startX, inputTop, 390.0f, 34.0f},
                font,
                targetIsMiddle ? "No middle texture assigned" : "No decal assigned",
                engine::UITextJustify::Left,
                config.mutedTextColor);
    }

    auto drawFloat = [&](const char* id, const char* label, float value, engine::UIFloatInputState& inputState, int component, float minValue, float maxValue, float x) {
        const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
                ui,
                config,
                input,
                assets,
                font,
                id,
                label,
                Rectangle{x, inputTop - 28.0f, colW, 24.0f},
                Rectangle{x, inputTop, colW, 38.0f},
                engine::UITextJustify::Left,
                value,
                inputState,
                minValue,
                maxValue,
                3);
        if (result.changed && result.value != value && result.finite) {
            ApplySurface3DUvValue(target, layer, component, result.value, assets);
        }
    };

    if (decalAssigned) {
        drawFloat("sector_editor_3d_uv_scale_u", "Scale U", uvScale.x, uiState.surface3DUvScaleUInput, 0, TopologyUvScaleMin, TopologyUvScaleMax, startX);
        drawFloat("sector_editor_3d_uv_scale_v", "Scale V", uvScale.y, uiState.surface3DUvScaleVInput, 1, TopologyUvScaleMin, TopologyUvScaleMax, startX + (colW + gap));
        drawFloat("sector_editor_3d_uv_offset_u", "Offset U", uvOffset.x, uiState.surface3DUvOffsetUInput, 2, -1024.0f, 1024.0f, startX + (colW + gap) * 2.0f);
        drawFloat("sector_editor_3d_uv_offset_v", "Offset V", uvOffset.y, uiState.surface3DUvOffsetVInput, 3, -1024.0f, 1024.0f, startX + (colW + gap) * 3.0f);
    }

    const float actionTop = inputTop + 52.0f;
    const float actionH = 34.0f;
    const float smallActionW = 96.0f;
    float actionX = startX;
    auto openTexturePicker = [&]() {
        if (target.kind == TopologySurfaceEditTargetKind::SectorFloor
                || target.kind == TopologySurfaceEditTargetKind::SectorCeiling) {
            OpenTopologyTexturePicker(target.sectorId, TopologyEditTargetSectorTextureField(target.kind), layer);
            if (state.texturePicker.open && HasAuthoringGraphData()) {
                state.texturePicker.authoringSurface3DFlatTarget = true;
            }
        } else {
            OpenTopologySideDefTexturePicker(target.sideDefId, TopologyEditTargetWallPart(target.kind), layer);
        }
        if (state.texturePicker.open) {
            state.texturePicker.rebuildPreviewOnApply = true;
        }
    };
    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_3d_texture",
                Rectangle{actionX, actionTop, smallActionW, actionH},
                font,
                "Texture")) {
        openTexturePicker();
    }
    actionX += smallActionW + gap;

    if (portalLineDefId != -1) {
        bool blocksPlayer = portalBlocksPlayer;
        if (engine::Checkbox(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_3d_linedef_blocks_player",
                    Rectangle{actionX, actionTop, 146.0f, actionH},
                    font,
                    "Blocks Player",
                    blocksPlayer)) {
            SetLineDefBlocksPlayer(portalLineDefId, blocksPlayer);
        }
        actionX += 146.0f + gap;
    }

    if (decalAssigned) {
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_3d_reset_uv",
                    Rectangle{actionX, actionTop, smallActionW, actionH},
                    font,
                    "Reset UV")) {
            ResetSurface3DUv(target, layer, assets);
        }
        actionX += smallActionW + gap;
    }

    if (targetIsMiddle && decalAssigned) {
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_3d_clear_middle",
                    Rectangle{actionX, actionTop, 118.0f, actionH},
                    font,
                    "Clear Middle")) {
            ClearMiddleTexture(target, &assets);
        }
        actionX += 118.0f + gap;
    }

    if (!targetIsMiddle && layer == TopologyMaterialLayer::Decal && decalAssigned) {
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_3d_fit_decal",
                    Rectangle{actionX, actionTop, smallActionW, actionH},
                    font,
                    "Fit Decal")) {
            FitSelectedDecal(target, &assets);
        }
        actionX += smallActionW + gap;
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_3d_clear_decal",
                    Rectangle{actionX, actionTop, 104.0f, actionH},
                    font,
                    "Clear Decal")) {
            ClearSurfaceDecal(target, &assets);
        }
        actionX += 104.0f + gap;
    }

    if (IsWallTopologyEditTarget(target.kind) && decalAssigned && layer == TopologyMaterialLayer::Base) {
        const float fitButtonW = 118.0f;
        const float fitTop = panel.y + 194.0f;
        const float fitButtonH = 26.0f;
        const float alignStartX = startX + (fitButtonW + gap) * 3.0f;
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_3d_fit_width",
                    Rectangle{startX, fitTop, fitButtonW, fitButtonH},
                    font,
                    "Fit Width")) {
            FitSelectedWallMaterial(target, TopologyUvFitMode::Width, &assets, layer);
        }
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_3d_fit_height",
                    Rectangle{startX + fitButtonW + gap, fitTop, fitButtonW, fitButtonH},
                    font,
                    "Fit Height")) {
            FitSelectedWallMaterial(target, TopologyUvFitMode::Height, &assets, layer);
        }
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_3d_fit_both",
                    Rectangle{startX + (fitButtonW + gap) * 2.0f, fitTop, fitButtonW, fitButtonH},
                    font,
                    "Fit Both")) {
            FitSelectedWallMaterial(target, TopologyUvFitMode::Both, &assets, layer);
        }
        if (!targetIsMiddle) {
            if (engine::Button(
                        ui,
                        config,
                        input,
                        assets,
                        "sector_editor_3d_align_vertical",
                        Rectangle{alignStartX, fitTop, fitButtonW, fitButtonH},
                        font,
                        "Align Vertical")) {
                AlignSelectedWallMaterialVertical(target, &assets, layer);
            }
            if (engine::Button(
                        ui,
                        config,
                        input,
                        assets,
                        "sector_editor_3d_align_u_prev",
                        Rectangle{alignStartX + fitButtonW + gap, fitTop, fitButtonW, fitButtonH},
                        font,
                        "Align U Prev")) {
                AlignSelectedWallMaterialU(target, TopologyUAlignDirection::Previous, &assets, layer);
            }
            if (engine::Button(
                        ui,
                        config,
                        input,
                        assets,
                        "sector_editor_3d_align_u_next",
                        Rectangle{alignStartX + (fitButtonW + gap) * 2.0f, fitTop, fitButtonW, fitButtonH},
                        font,
                        "Align U Next")) {
                AlignSelectedWallMaterialU(target, TopologyUAlignDirection::Next, &assets, layer);
            }
        }
    }

    if (!targetIsMiddle && layer == TopologyMaterialLayer::Decal && decalAssigned) {
        const SectorTopologyDecalLayer* decal = DecalForSurface(target);
        if (decal != nullptr) {
            const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
                    ui,
                    config,
                    input,
                    assets,
                    font,
                    "sector_editor_3d_decal_opacity",
                    "Opacity",
                    Rectangle{startX + (colW + gap) * 4.0f, inputTop - 28.0f, colW, 24.0f},
                    Rectangle{startX + (colW + gap) * 4.0f, inputTop, colW, 38.0f},
                    engine::UITextJustify::Left,
                    decal->opacity,
                    uiState.surface3DDecalOpacityInput,
                    0.0f,
                    1.0f,
                    3);
            if (result.changed && result.value != decal->opacity && result.finite) {
                ApplySurfaceDecalOpacity(target, result.value, &assets);
            }
            if (decal->emissive) {
                const SectorEditorFloatInputResult bloomResult = DrawLabeledFloatInput(
                        ui,
                        config,
                        input,
                        assets,
                        font,
                        "sector_editor_3d_decal_bloom_intensity",
                        "Bloom",
                        Rectangle{startX + (colW + gap) * 5.0f, inputTop - 28.0f, colW, 24.0f},
                        Rectangle{startX + (colW + gap) * 5.0f, inputTop, colW, 38.0f},
                        engine::UITextJustify::Left,
                        decal->bloomIntensity,
                        uiState.surface3DDecalBloomIntensityInput,
                        0.0f,
                        10.0f,
                        3);
                if (bloomResult.changed && bloomResult.value != decal->bloomIntensity) {
                    ApplySurfaceDecalBloomIntensity(target, bloomResult.value, &assets);
                }
            }
            bool emissive = decal->emissive;
            if (engine::Checkbox(
                        ui,
                        config,
                        input,
                        assets,
                        "sector_editor_3d_decal_emissive",
                        Rectangle{actionX, actionTop, 112.0f, actionH},
                        font,
                        "Emissive",
                        emissive)) {
                ApplySurfaceDecalEmissive(target, emissive, &assets);
            }
            actionX += 112.0f + gap;
            const Rectangle label{actionX, actionTop, 36.0f, actionH};
            engine::Text(ui, config, assets, label, font, "Tint:", engine::UITextJustify::Left, config.mutedTextColor);
            const Rectangle swatch{label.x + label.width + 8.0f, label.y + 1.0f, 48.0f, actionH - 2.0f};
            if (engine::Button(
                        ui,
                        config,
                        input,
                        assets,
                        "sector_editor_3d_decal_tint",
                        swatch,
                        font,
                        "")) {
                OpenDecalTintModal(target);
            }
            DrawColorSwatch(config, swatch, DecalTintPreviewColor(decal->tint), config.borderThickness);
        }
    } else if (!targetIsMiddle && layer == TopologyMaterialLayer::Base) {
        if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_3d_copy_material",
                Rectangle{actionX, actionTop, 112.0f, actionH},
                font,
                "Copy Material")) {
            CopyTopologyMaterial(target);
        }
        actionX += 112.0f + gap;

        if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_3d_paste_material",
                Rectangle{actionX, actionTop, 112.0f, actionH},
                font,
                "Paste Material")) {
            PasteTopologyMaterial(target, assets);
        }
    }

    input.ForEachEvent(
            engine::InputEventType::MouseClick,
            true,
            [panel](engine::InputEvent& event) {
                if (Contains(panel, event.mouseClick.releasePosition)
                        || Contains(panel, event.mouseClick.pressPosition)) {
                    engine::ConsumeEvent(event);
                }
            }
    );
}

void SectorEditor::DrawGrid() const
{
    const int grid = std::max(1, state.gridSize);
    const Vector2 minMap = ScreenToMap(Vector2{canvasRect.x, canvasRect.y});
    const Vector2 maxMap = ScreenToMap(Vector2{canvasRect.x + canvasRect.width, canvasRect.y + canvasRect.height});
    const int startX = static_cast<int>(std::floor(minMap.x / static_cast<float>(grid))) * grid;
    const int endX = static_cast<int>(std::ceil(maxMap.x / static_cast<float>(grid))) * grid;
    const int startY = static_cast<int>(std::floor(minMap.y / static_cast<float>(grid))) * grid;
    const int endY = static_cast<int>(std::ceil(maxMap.y / static_cast<float>(grid))) * grid;

    const Color gridColor{44, 50, 62, 155};
    const Color majorColor{62, 70, 86, 185};
    const Color axisColor{112, 148, 148, 230};

    for (int x = startX; x <= endX; x += grid) {
        const Vector2 a = MapToScreen(Vector2{static_cast<float>(x), minMap.y});
        const Vector2 b = MapToScreen(Vector2{static_cast<float>(x), maxMap.y});
        const bool axis = x == 0;
        const bool major = grid < 8 && x % 8 == 0;
        DrawLineEx(a, b, axis && state.showAxes ? 2.0f : 1.0f, axis && state.showAxes ? axisColor : (major ? majorColor : gridColor));
    }

    for (int y = startY; y <= endY; y += grid) {
        const Vector2 a = MapToScreen(Vector2{minMap.x, static_cast<float>(y)});
        const Vector2 b = MapToScreen(Vector2{maxMap.x, static_cast<float>(y)});
        const bool axis = y == 0;
        const bool major = grid < 8 && y % 8 == 0;
        DrawLineEx(a, b, axis && state.showAxes ? 2.0f : 1.0f, axis && state.showAxes ? axisColor : (major ? majorColor : gridColor));
    }

    if (state.showAxes) {
        const Vector2 origin = MapToScreen(Vector2{0.0f, 0.0f});
        DrawCircleV(origin, 5.0f, Color{180, 210, 190, 255});
    }
}

void SectorEditor::InvalidateTopologyRenderCache()
{
    ++state.topologyRenderRevision;
    state.topologyRenderCache.valid = false;
}

void SectorEditor::EnsureTopologyRenderCache()
{
    if (!state.topologyRenderCache.valid
            || state.topologyRenderCache.revision != state.topologyRenderRevision) {
        state.topologyRenderCache = BuildSectorEditorTopologyRenderCache(
                state.topologyMap,
                state.authoringGraph,
                state.authoringDerivation,
                state.topologyRenderRevision);
        state.topologyRenderWarning = state.topologyRenderCache.warning;
    }
}

void SectorEditor::DrawTopologyDocument()
{
    if (!initialized) {
        DrawText("Topology map failed to load", static_cast<int>(canvasRect.x + 24.0f), static_cast<int>(canvasRect.y + 24.0f), 28, RED);
        return;
    }

    EnsureTopologyRenderCache();

    ClearStaleTopologySelection();
    const bool hasAuthoringGraph = HasAuthoringGraphData();
    const bool drawLegacyTopologySelection =
            ShouldDrawLegacyTopologySelectionHighlight(hasAuthoringGraph, state.topologySelectionKind);
    const SectorEditorTopologyDrawContext drawContext{
            canvasRect,
            state.viewCenter,
            state.viewZoom,
            state.showSectorIds,
            state.authoringDerivedTopologyStale,
            state.currentTool,
            drawLegacyTopologySelection ? state.topologySelectionKind : TopologySelectionKind::None,
            drawLegacyTopologySelection ? state.selectedTopologySectorId : -1,
            drawLegacyTopologySelection ? state.selectedTopologyVertexId : -1,
            drawLegacyTopologySelection ? state.selectedTopologyLightId : -1,
            drawLegacyTopologySelection ? state.selectedTopologyDynamicLightId : -1,
            state.hasHoveredVertex,
            state.hoveredTopologyVertexId,
            state.hoveredTopologyLightId,
            state.hoveredTopologyDynamicLightId,
            state.selectedAuthoring,
            state.hoveredAuthoring
    };
    DrawCachedTopologySectors(state.topologyRenderCache, drawContext);

    if (drawLegacyTopologySelection) {
        DrawTopologySelectedLineHighlight();
    }
    DrawCachedTopologyLineDefs(state.topologyRenderCache, drawContext);
    DrawCachedTopologyVertices(state.topologyRenderCache, drawContext);
    DrawCachedAuthoringGraphOverlay(state.topologyRenderCache, drawContext);
    DrawCachedAuthoringDiagnostics(state.topologyRenderCache, drawContext);
    DrawAuthoringVertexMoveOverlay();
    DrawCachedTopologyStaticLights(state.topologyRenderCache, drawContext);
    DrawCachedTopologyDynamicLights(state.topologyRenderCache, drawContext);
    DrawLightMoveOverlay();
    DrawPendingAuthoringLine();
    DrawPendingAuthoringRectangle();
    DrawPendingAuthoringInsertVertex();
    DrawTopologySnapCrosshair();

    if (!state.topologyRenderWarning.empty()) {
        DrawText(
                state.topologyRenderWarning.c_str(),
                static_cast<int>(canvasRect.x + 16.0f),
                static_cast<int>(canvasRect.y + 14.0f),
                18,
                Color{236, 196, 92, 255}
        );
    }
    if (state.authoringDerivedTopologyStale) {
        DrawText(
                "Authoring graph changed; derived sector fills are stale",
                static_cast<int>(canvasRect.x + 16.0f),
                static_cast<int>(canvasRect.y + 64.0f),
                18,
                Color{236, 196, 92, 255}
        );
    }
}

void SectorEditor::DrawTopologySelectedLineHighlight() const
{
    if (state.topologySelectionKind != TopologySelectionKind::SideDef
            && state.topologySelectionKind != TopologySelectionKind::LineDef) {
        return;
    }

    const SectorTopologyLineDef* lineDef = SelectedTopologyLineDef();
    if (lineDef == nullptr) {
        return;
    }

    const SectorTopologyVertex* start = nullptr;
    const SectorTopologyVertex* end = nullptr;
    if (!GetSectorTopologyLineVertices(state.topologyMap, *lineDef, start, end)) {
        return;
    }

    Vector2 a = MapToScreen(SectorTopologyVertexToMap(*start));
    Vector2 b = MapToScreen(SectorTopologyVertexToMap(*end));
    Vector2 dir{b.x - a.x, b.y - a.y};
    const float length = std::sqrt(dir.x * dir.x + dir.y * dir.y);
    if (length <= GeometryEpsilon) {
        return;
    }

    dir.x /= length;
    dir.y /= length;
    Vector2 normal{-dir.y, dir.x};
    Color color{72, 210, 246, 138};
    if (state.topologySelectionKind == TopologySelectionKind::LineDef) {
        normal = Vector2{0.0f, 0.0f};
        color = Color{210, 214, 224, 125};
    } else if (state.selectedTopologySideKind == SectorTopologySideKind::Back) {
        normal.x = -normal.x;
        normal.y = -normal.y;
        color = Color{94, 238, 186, 132};
    }

    const float offset = state.topologySelectionKind == TopologySelectionKind::LineDef ? 0.0f : 7.0f;
    a.x += normal.x * offset;
    a.y += normal.y * offset;
    b.x += normal.x * offset;
    b.y += normal.y * offset;
    DrawLineEx(a, b, 10.0f, color);
}

void SectorEditor::DrawTopologySnapCrosshair() const
{
    if (!Contains(canvasRect, GetMousePosition())) {
        return;
    }

    if ((state.currentTool == SectorEditorTool::AuthoringInsertVertex
                || state.pendingAuthoringInsertVertex.active)
            && state.pendingAuthoringInsertVertex.hasPreviewPoint) {
        const Vector2 snap = MapToScreen(Vector2{
                SectorCoordToVisibleAuthoring(state.pendingAuthoringInsertVertex.previewPoint.x),
                SectorCoordToVisibleAuthoring(state.pendingAuthoringInsertVertex.previewPoint.y)});
        DrawLineEx(Vector2{snap.x - 9.0f, snap.y}, Vector2{snap.x + 9.0f, snap.y}, 2.0f, Color{235, 224, 130, 255});
        DrawLineEx(Vector2{snap.x, snap.y - 9.0f}, Vector2{snap.x, snap.y + 9.0f}, 2.0f, Color{235, 224, 130, 255});
        return;
    }

    const bool useCanonicalSectorPoint = state.currentTool == SectorEditorTool::AuthoringLine
            || state.currentTool == SectorEditorTool::AuthoringRectangle
            || state.pendingAuthoringLine.active
            || state.pendingAuthoringRectangle.active;
    const Vector2 snap = useCanonicalSectorPoint
            ? MapToScreen(SectorPointToVector2(CurrentSnappedSectorPoint()))
            : MapToScreen(state.snappedMouseMap);
    DrawLineEx(Vector2{snap.x - 9.0f, snap.y}, Vector2{snap.x + 9.0f, snap.y}, 2.0f, Color{235, 224, 130, 255});
    DrawLineEx(Vector2{snap.x, snap.y - 9.0f}, Vector2{snap.x, snap.y + 9.0f}, 2.0f, Color{235, 224, 130, 255});
}

void SectorEditor::DrawPendingAuthoringLine() const
{
    if (!state.pendingAuthoringLine.active) {
        return;
    }

    const SectorPoint start = SectorTopologyCoordPointToSectorPoint(
            state.pendingAuthoringLine.startPoint);
    const SectorPoint cursor = CurrentSnappedSectorPoint();
    const bool invalid = SamePoint(start, cursor)
            || !state.pendingAuthoringLine.errorMessage.empty();
    const Color lineColor = invalid ? Color{220, 88, 88, 190} : Color{122, 220, 244, 205};
    const Color startColor = Color{245, 226, 154, 255};
    const Color cursorColor = invalid ? Color{220, 88, 88, 255} : Color{120, 230, 154, 255};
    const Vector2 startScreen = MapToScreen(SectorPointToVector2(start));
    const Vector2 cursorScreen = MapToScreen(SectorPointToVector2(cursor));

    if (!SamePoint(start, cursor)) {
        DrawLineEx(startScreen, cursorScreen, 3.0f, lineColor);
    }
    DrawCircleV(startScreen, 5.5f, startColor);
    DrawCircleLines(
            static_cast<int>(std::round(startScreen.x)),
            static_cast<int>(std::round(startScreen.y)),
            8.0f,
            Color{20, 24, 32, 255});
    DrawCircleV(cursorScreen, 5.0f, cursorColor);
    DrawCircleLines(
            static_cast<int>(std::round(cursorScreen.x)),
            static_cast<int>(std::round(cursorScreen.y)),
            7.5f,
            Color{20, 24, 32, 255});
}

void SectorEditor::DrawPendingAuthoringRectangle() const
{
    if (!state.pendingAuthoringRectangle.active) {
        return;
    }

    const SectorPoint first = SectorTopologyCoordPointToSectorPoint(
            state.pendingAuthoringRectangle.firstCorner);
    const SectorPoint cursor = SectorTopologyCoordPointToSectorPoint(
            state.pendingAuthoringRectangle.currentCorner);
    const bool invalid = first.x == cursor.x
            || first.y == cursor.y
            || !state.pendingAuthoringRectangle.errorMessage.empty();
    const Color lineColor = invalid ? Color{220, 88, 88, 190} : Color{122, 220, 244, 205};
    const Color firstColor = Color{245, 226, 154, 255};
    const Color cursorColor = invalid ? Color{220, 88, 88, 255} : Color{120, 230, 154, 255};

    const float minX = std::min(first.x, cursor.x);
    const float maxX = std::max(first.x, cursor.x);
    const float minY = std::min(first.y, cursor.y);
    const float maxY = std::max(first.y, cursor.y);
    const Vector2 a = MapToScreen(Vector2{minX, minY});
    const Vector2 b = MapToScreen(Vector2{maxX, minY});
    const Vector2 c = MapToScreen(Vector2{maxX, maxY});
    const Vector2 d = MapToScreen(Vector2{minX, maxY});
    if (!invalid) {
        DrawLineEx(a, b, 3.0f, lineColor);
        DrawLineEx(b, c, 3.0f, lineColor);
        DrawLineEx(c, d, 3.0f, lineColor);
        DrawLineEx(d, a, 3.0f, lineColor);
    } else if (!SamePoint(first, cursor)) {
        DrawLineEx(
                MapToScreen(SectorPointToVector2(first)),
                MapToScreen(SectorPointToVector2(cursor)),
                3.0f,
                lineColor);
    }

    const Vector2 firstScreen = MapToScreen(SectorPointToVector2(first));
    const Vector2 cursorScreen = MapToScreen(SectorPointToVector2(cursor));
    DrawCircleV(firstScreen, 5.5f, firstColor);
    DrawCircleLines(
            static_cast<int>(std::round(firstScreen.x)),
            static_cast<int>(std::round(firstScreen.y)),
            8.0f,
            Color{20, 24, 32, 255});
    DrawCircleV(cursorScreen, 5.0f, cursorColor);
    DrawCircleLines(
            static_cast<int>(std::round(cursorScreen.x)),
            static_cast<int>(std::round(cursorScreen.y)),
            7.5f,
            Color{20, 24, 32, 255});
}

void SectorEditor::DrawPendingAuthoringInsertVertex() const
{
    if (state.currentTool != SectorEditorTool::AuthoringInsertVertex
            && !state.pendingAuthoringInsertVertex.active) {
        return;
    }

    const PendingAuthoringInsertVertex& pending = state.pendingAuthoringInsertVertex;
    const SectorAuthoringLine* line = FindSectorAuthoringLine(state.authoringGraph, pending.lineId);
    if (line == nullptr) {
        return;
    }
    const SectorAuthoringVertex* start =
            FindSectorAuthoringVertex(state.authoringGraph, line->startVertexId);
    const SectorAuthoringVertex* end =
            FindSectorAuthoringVertex(state.authoringGraph, line->endVertexId);
    if (start == nullptr || end == nullptr) {
        return;
    }

    const bool invalid = !pending.hasPreviewPoint || !pending.errorMessage.empty();
    const Color lineColor = invalid ? Color{220, 88, 88, 170} : Color{122, 220, 244, 205};
    const Color pointColor = invalid ? Color{220, 88, 88, 255} : Color{120, 230, 154, 255};
    const Vector2 startScreen = MapToScreen(Vector2{
            SectorCoordToVisibleAuthoring(start->x),
            SectorCoordToVisibleAuthoring(start->y)});
    const Vector2 endScreen = MapToScreen(Vector2{
            SectorCoordToVisibleAuthoring(end->x),
            SectorCoordToVisibleAuthoring(end->y)});
    DrawLineEx(startScreen, endScreen, 5.0f, lineColor);

    if (!pending.hasPreviewPoint) {
        return;
    }

    const Vector2 point = MapToScreen(Vector2{
            SectorCoordToVisibleAuthoring(pending.previewPoint.x),
            SectorCoordToVisibleAuthoring(pending.previewPoint.y)});
    DrawCircleV(point, 5.0f, pointColor);
    DrawCircleLines(
            static_cast<int>(std::round(point.x)),
            static_cast<int>(std::round(point.y)),
            9.0f,
            Color{20, 24, 32, 255});
    DrawLineEx(Vector2{point.x - 10.0f, point.y}, Vector2{point.x + 10.0f, point.y}, 2.0f, pointColor);
    DrawLineEx(Vector2{point.x, point.y - 10.0f}, Vector2{point.x, point.y + 10.0f}, 2.0f, pointColor);
}

void SectorEditor::DrawAuthoringVertexMoveOverlay() const
{
    if (state.currentTool != SectorEditorTool::AuthoringMove
            && !state.authoringVertexDrag.active) {
        return;
    }

    if (!state.authoringVertexDrag.active
            && state.hoveredAuthoring.kind == SectorAuthoringSelectionKind::Vertex) {
        const SectorAuthoringVertex* vertex =
                FindSectorAuthoringVertex(state.authoringGraph, state.hoveredAuthoring.vertexId);
        if (vertex == nullptr) {
            return;
        }

        const Vector2 point = MapToScreen(Vector2{
                SectorCoordToVisibleAuthoring(vertex->x),
                SectorCoordToVisibleAuthoring(vertex->y)});
        DrawCircleLines(
                static_cast<int>(std::round(point.x)),
                static_cast<int>(std::round(point.y)),
                12.0f,
                Color{122, 220, 244, 255});
        DrawCircleV(point, 4.5f, Color{122, 220, 244, 255});
        return;
    }

    if (!state.authoringVertexDrag.active) {
        return;
    }

    const bool invalid = !state.authoringVertexDrag.errorMessage.empty()
            || !state.authoringVertexDrag.hasPreviewPoint;
    const Color targetColor = invalid ? Color{230, 82, 82, 255} : Color{120, 230, 154, 255};
    const Color previewColor = invalid ? Color{230, 82, 82, 205} : Color{122, 220, 244, 220};
    const Color originalColor = Color{245, 226, 154, 230};
    const Vector2 original = MapToScreen(Vector2{
            SectorCoordToVisibleAuthoring(state.authoringVertexDrag.originalPoint.x),
            SectorCoordToVisibleAuthoring(state.authoringVertexDrag.originalPoint.y)});

    if (!state.authoringVertexDrag.hasPreviewPoint) {
        DrawCircleLines(
                static_cast<int>(std::round(original.x)),
                static_cast<int>(std::round(original.y)),
                10.0f,
                originalColor);
        return;
    }

    const int draggedVertexId = state.authoringVertexDrag.vertexId;
    const Vector2 previewMap{
            SectorCoordToVisibleAuthoring(state.authoringVertexDrag.previewPoint.x),
            SectorCoordToVisibleAuthoring(state.authoringVertexDrag.previewPoint.y)};
    for (const SectorAuthoringLine& line : state.authoringGraph.lines) {
        if (line.startVertexId != draggedVertexId && line.endVertexId != draggedVertexId) {
            continue;
        }

        const int otherVertexId = line.startVertexId == draggedVertexId
                ? line.endVertexId
                : line.startVertexId;
        const SectorAuthoringVertex* otherVertex =
                FindSectorAuthoringVertex(state.authoringGraph, otherVertexId);
        if (otherVertex == nullptr) {
            continue;
        }

        DrawLineEx(
                MapToScreen(previewMap),
                MapToScreen(Vector2{
                        SectorCoordToVisibleAuthoring(otherVertex->x),
                        SectorCoordToVisibleAuthoring(otherVertex->y)}),
                4.0f,
                previewColor);
    }

    const Vector2 target = MapToScreen(previewMap);
    DrawLineEx(original, target, 2.0f, WithAlpha(targetColor, 180));
    DrawCircleLines(
            static_cast<int>(std::round(original.x)),
            static_cast<int>(std::round(original.y)),
            10.0f,
            originalColor);
    DrawCircleLines(
            static_cast<int>(std::round(target.x)),
            static_cast<int>(std::round(target.y)),
            13.0f,
            targetColor);
    DrawCircleV(target, 5.0f, targetColor);
}

void SectorEditor::DrawLightMoveOverlay() const
{
    if (state.currentTool != SectorEditorTool::StaticLight
            && state.currentTool != SectorEditorTool::DynamicLight
            && state.currentTool != SectorEditorTool::Move) {
        return;
    }

    if (state.lightDrag.active) {
        if (state.topologySelectionKind == TopologySelectionKind::DynamicLight) {
            const SectorTopologyDynamicPointLight* light = FindSectorTopologyDynamicLight(
                    state.topologyMap,
                    state.lightDrag.topologyLightId);
            if (light == nullptr) {
                return;
            }

            const Vector2 center = MapToScreen(Vector2{light->position.x, light->position.z});
            const float radiusPixels = SectorAuthoringToWorldDistance(light->radius) * state.viewZoom;
            Color color = light->color;
            color.a = 245;
            DrawCircleLines(
                    static_cast<int>(std::round(center.x)),
                    static_cast<int>(std::round(center.y)),
                    radiusPixels,
                    WithAlpha(color, 165)
            );
            DrawCircleLines(static_cast<int>(std::round(center.x)), static_cast<int>(std::round(center.y)), 15.0f, Color{120, 230, 154, 255});
            DrawCircleV(center, 6.5f, Color{120, 230, 154, 255});
            return;
        }

        const SectorTopologyStaticPointLight* light = FindSectorTopologyStaticLight(
                state.topologyMap,
                state.lightDrag.topologyLightId);
        if (light == nullptr) {
            return;
        }

        const Vector2 center = MapToScreen(Vector2{light->position.x, light->position.z});
        const float radiusPixels = SectorAuthoringToWorldDistance(light->radius) * state.viewZoom;
        Color color = light->color;
        color.a = 245;
        DrawCircleLines(
                static_cast<int>(std::round(center.x)),
                static_cast<int>(std::round(center.y)),
                radiusPixels,
                WithAlpha(color, 165)
        );
        DrawCircleLines(static_cast<int>(std::round(center.x)), static_cast<int>(std::round(center.y)), 15.0f, Color{120, 230, 154, 255});
        DrawCircleV(center, 6.5f, Color{120, 230, 154, 255});
        return;
    }

    if (state.currentTool == SectorEditorTool::DynamicLight) {
        const SectorTopologyDynamicPointLight* light = FindSectorTopologyDynamicLight(
                state.topologyMap,
                state.hoveredTopologyDynamicLightId);
        if (light == nullptr) {
            return;
        }

        const Vector2 center = MapToScreen(Vector2{light->position.x, light->position.z});
        DrawCircleLines(static_cast<int>(std::round(center.x)), static_cast<int>(std::round(center.y)), 13.0f, Color{245, 226, 154, 255});
        return;
    }

    const SectorTopologyStaticPointLight* light = FindSectorTopologyStaticLight(
            state.topologyMap,
            state.hoveredTopologyLightId);
    if (light == nullptr) {
        return;
    }

    const Vector2 center = MapToScreen(Vector2{light->position.x, light->position.z});
    DrawCircleLines(static_cast<int>(std::round(center.x)), static_cast<int>(std::round(center.y)), 13.0f, Color{245, 226, 154, 255});
}

void SectorEditor::DrawToolsPanel(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    if (!IsToolAvailableInGraphAuthoritativeMode(state.currentTool)) {
        state.currentTool = SectorEditorTool::Select;
        statusText = LegacyTopologyMutationUnavailableMessage();
    }

    const engine::UIPanelResult panel = engine::BeginPanel(
            ui,
            config,
            assets,
            "sector_editor_tools",
            BuildLeftPanelRect(),
            font,
            "Tools"
    );

    const float rowH = 46.0f;
    const float gap = config.rowSpacing;
    const float separatorH = 22.0f;
    const float sectionLabelH = 26.0f;
    const float lightmapLabelH = 32.0f;
    const float bottomPadding = rowH + gap * 2.0f;
    const auto rowsHeight = [rowH, gap](int count) {
        return static_cast<float>(count) * (rowH + gap);
    };
    const float toolsContentH =
            sectionLabelH + rowsHeight(4)
            + separatorH + sectionLabelH + rowsHeight(2)
            + separatorH + rowsHeight(4)
            + lightmapLabelH + rowsHeight(5)
            + separatorH + rowsHeight(4)
            + separatorH + rowsHeight(1)
            + bottomPadding;
    const float scrollContentW = std::max(0.0f, panel.contentRect.width - config.scrollbarSize);
    engine::UIScrollAreaResult scroll = engine::BeginScrollArea(
            ui,
            config,
            input,
            "sector_editor_tools_scroll",
            panel.contentRect,
            Vector2{scrollContentW, toolsContentH},
            uiState.toolsScroll,
            false
    );

    const float contentW = scroll.viewport.width;
    float y = 0.0f;
    const auto separator = [&]() {
        engine::Separator(
                config,
                Rectangle{
                        scroll.viewport.x,
                        scroll.viewport.y - uiState.toolsScroll.offset.y + y,
                        contentW,
                        12.0f
                }
        );
        y += 22.0f;
    };
    const auto sectionLabel = [&](const char* label) {
        engine::Text(
                config,
                assets,
                Rectangle{
                        scroll.viewport.x,
                        scroll.viewport.y - uiState.toolsScroll.offset.y + y,
                        contentW,
                        20.0f
                },
                font,
                label,
                engine::UITextJustify::Left
        );
        y += 26.0f;
    };

    const auto drawToolButton = [&](SectorEditorTool tool) {
        const bool clicked = engine::ToolButton(
                ui,
                config,
                input,
                assets,
                TextFormat("sector_editor_tool_%s", ToolName(tool)),
                Rectangle{0.0f, y, contentW, rowH},
                font,
                ToolName(tool),
                state.currentTool == tool);
        y += rowH + gap;
        return clicked;
    };

    const auto selectTool = [&](SectorEditorTool tool) {
        if (!IsToolAvailableInGraphAuthoritativeMode(tool)) {
            statusText = LegacyTopologyMutationUnavailableMessage();
            return;
        }
        if (state.pendingAuthoringLine.active && tool != SectorEditorTool::AuthoringLine) {
            CancelPendingAuthoringLine("Cancelled authoring line");
        }
        if (state.pendingAuthoringRectangle.active && tool != SectorEditorTool::AuthoringRectangle) {
            CancelPendingAuthoringRectangle("Rectangle cancelled");
        }
        if (state.pendingAuthoringInsertVertex.active && tool != SectorEditorTool::AuthoringInsertVertex) {
            CancelPendingAuthoringInsertVertex("Insert Vertex cancelled");
        }
        if (state.authoringVertexDrag.active && tool != SectorEditorTool::AuthoringMove) {
            CancelAuthoringVertexDrag("Cancelled authoring vertex move");
        }
        if (state.lightDrag.active
                && tool != SectorEditorTool::Move
                && tool != SectorEditorTool::StaticLight
                && tool != SectorEditorTool::DynamicLight) {
            CancelLightDrag("Cancelled light move");
        }
        if (IsGraphAuthoringTool(tool)) {
            ClearTopologySelectionOnly();
        }
        state.currentTool = tool;
        if (tool == SectorEditorTool::AuthoringLine) {
            statusText = "Line: click start point";
        } else if (tool == SectorEditorTool::AuthoringInsertVertex) {
            state.pendingAuthoringInsertVertex = PendingAuthoringInsertVertex{};
            if (state.selectedAuthoring.kind == SectorAuthoringSelectionKind::Line
                    && FindSectorAuthoringLine(state.authoringGraph, state.selectedAuthoring.lineId) != nullptr) {
                state.pendingAuthoringInsertVertex.active = true;
                state.pendingAuthoringInsertVertex.lineId = state.selectedAuthoring.lineId;
                statusText = "Insert Vertex: click point on selected line, Esc/right click cancels";
            } else {
                statusText = "Insert Vertex: select or click an authoring line";
            }
        }
    };

    sectionLabel("Graph authoring");
    const SectorEditorTool graphTools[] = {
            SectorEditorTool::Select,
            SectorEditorTool::AuthoringLine,
            SectorEditorTool::AuthoringRectangle,
            SectorEditorTool::AuthoringInsertVertex,
            SectorEditorTool::AuthoringMove
    };
    for (SectorEditorTool tool : graphTools) {
        if (drawToolButton(tool)) {
            selectTool(tool);
        }
    }

    separator();
    sectionLabel("Map objects");
    const SectorEditorTool mapTools[] = {
            SectorEditorTool::StaticLight,
            SectorEditorTool::DynamicLight
    };
    for (SectorEditorTool tool : mapTools) {
        if (drawToolButton(tool)) {
            selectTool(tool);
        }
    }

    separator();

    const float documentButtonW = (contentW - gap) * 0.5f;
    if (engine::Button(ui, config, input, assets, "sector_editor_new", Rectangle{0.0f, y, documentButtonW, rowH}, font, "New")) {
        OpenNewConfirmation(assets);
    }
    if (engine::Button(ui, config, input, assets, "sector_editor_load", Rectangle{documentButtonW + gap, y, documentButtonW, rowH}, font, "Load")) {
        OpenLoadLevelModal();
    }
    y += rowH + gap;
    if (engine::Button(ui, config, input, assets, "sector_editor_save", Rectangle{0.0f, y, documentButtonW, rowH}, font, "Save")) {
        OpenSaveLevelModal();
    }
    if (engine::Button(ui, config, input, assets, "sector_editor_reload", Rectangle{documentButtonW + gap, y, documentButtonW, rowH}, font, "Reload")) {
        OpenReloadConfirmation(assets);
    }
    y += rowH + gap;

    if (engine::Button(ui, config, input, assets, "sector_editor_add_map_texture", Rectangle{0.0f, y, contentW, rowH}, font, "Add Map Texture")) {
        OpenAddMapTextureModal(assets);
    }
    y += rowH + gap;
    if (engine::Button(ui, config, input, assets, "sector_editor_preview_settings_2d", Rectangle{0.0f, y, contentW, rowH}, font, "Settings")) {
        OpenPreviewSettingsModal();
    }
    y += rowH + gap;

    engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 28.0f}, font, "Lightmap Settings", engine::UITextJustify::Left, config.mutedTextColor);
    y += 32.0f;

    const float lightmapLabelW = 180.0f;
    const auto drawLightmapSetting = [&](const char* id, const char* label, float& value, engine::UIFloatInputState& inputState, float minValue, float maxValue, int decimals, const char* status) {
        const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
                ui,
                config,
                input,
                assets,
                font,
                id,
                label,
                Rectangle{0.0f, y, lightmapLabelW, rowH},
                Rectangle{lightmapLabelW + gap, y, std::max(0.0f, contentW - lightmapLabelW - gap), rowH},
                engine::UITextJustify::Left,
                value,
                inputState,
                minValue,
                maxValue,
                decimals);
        if (result.changed && result.value != value) {
            value = result.value;
            state.hasUnsavedChanges = true;
            state.topologyDocumentDirty = true;
            statusText = status;
        }
        y += rowH + gap;
    };

    state.topologyMap.lightmapSettings.ambientOcclusionRadius = ClampAmbientOcclusionRadius(state.topologyMap.lightmapSettings.ambientOcclusionRadius);
    state.topologyMap.lightmapSettings.ambientOcclusionStrength = ClampAmbientOcclusionStrength(state.topologyMap.lightmapSettings.ambientOcclusionStrength);
    state.topologyMap.lightmapSettings.indirectBounceRadius = ClampIndirectBounceRadius(state.topologyMap.lightmapSettings.indirectBounceRadius);
    state.topologyMap.lightmapSettings.indirectBounceStrength = ClampIndirectBounceStrength(state.topologyMap.lightmapSettings.indirectBounceStrength);
    drawLightmapSetting(
            "sector_editor_ao_radius",
            "AO radius",
            state.topologyMap.lightmapSettings.ambientOcclusionRadius,
            uiState.ambientOcclusionRadiusInput,
            SectorWorldToAuthoringDistance(0.05f),
            SectorWorldToAuthoringDistance(16.0f),
            2,
            "Updated AO radius"
    );
    drawLightmapSetting(
            "sector_editor_ao_strength",
            "AO strength",
            state.topologyMap.lightmapSettings.ambientOcclusionStrength,
            uiState.ambientOcclusionStrengthInput,
            0.0f,
            1.0f,
            3,
            "Updated AO strength"
    );
    drawLightmapSetting(
            "sector_editor_bounce_radius",
            "Bounce radius",
            state.topologyMap.lightmapSettings.indirectBounceRadius,
            uiState.indirectBounceRadiusInput,
            SectorWorldToAuthoringDistance(0.05f),
            SectorWorldToAuthoringDistance(16.0f),
            2,
            "Updated bounce radius"
    );
    drawLightmapSetting(
            "sector_editor_bounce_strength",
            "Bounce strength",
            state.topologyMap.lightmapSettings.indirectBounceStrength,
            uiState.indirectBounceStrengthInput,
            0.0f,
            1.0f,
            3,
            "Updated bounce strength"
    );

    if (engine::Button(ui, config, input, assets, "sector_editor_bake_lightmaps", Rectangle{0.0f, y, contentW, rowH}, font, "Bake Lightmaps")) {
        BakeLightmaps();
    }
    y += rowH + gap;

    separator();

    const float gridLabelW = 64.0f;
    engine::Text(ui, config, assets, Rectangle{0.0f, y, gridLabelW, rowH}, font, "Grid", engine::UITextJustify::Left, config.mutedTextColor);
    engine::IntInput(
            ui,
            config,
            input,
            assets,
            "sector_editor_grid",
            Rectangle{gridLabelW + gap, y, std::max(0.0f, contentW - gridLabelW - gap), rowH},
            font,
            state.gridSize,
            uiState.gridSizeInput,
            1,
            64,
            1
    );
    y += rowH + gap;

    engine::Checkbox(ui, config, input, assets, "sector_editor_show_grid", Rectangle{0.0f, y, contentW, rowH}, font, "Show grid", state.showGrid);
    y += rowH + gap;
    engine::Checkbox(ui, config, input, assets, "sector_editor_show_axes", Rectangle{0.0f, y, contentW, rowH}, font, "Show axes", state.showAxes);
    y += rowH + gap;
    engine::Checkbox(ui, config, input, assets, "sector_editor_show_ids", Rectangle{0.0f, y, contentW, rowH}, font, "Show ids", state.showSectorIds);
    y += rowH + gap;

    separator();

    if (engine::Button(ui, config, input, assets, "sector_editor_preview_3d", Rectangle{0.0f, y, contentW, rowH}, font, "3D Mode")) {
        TryEnterPreview3D(assets, ui);
    }

    engine::EndScrollArea(ui, config, input, scroll, uiState.toolsScroll);
    engine::EndPanel(ui, config, panel);
}

void SectorEditor::DrawSectorsPanel(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    const engine::UIPanelResult panel = engine::BeginPanel(
            ui,
            config,
            assets,
            "sector_editor_sectors",
            BuildRightPanelRect(),
            font,
            "Inspector"
    );

    ClearStaleTopologySelection();
    SyncSelectedSectorIdBuffer();
    SyncSelectedLightIdBuffer();

    const bool hasSelectedTopologySector = SelectedTopologySector() != nullptr;
    const bool hasSelectedTopologyVertex = SelectedTopologyVertex() != nullptr;
    const bool hasSelectedTopologySideDef = SelectedTopologySideDef() != nullptr;
    const bool hasSelectedTopologyLineDef = state.topologySelectionKind == TopologySelectionKind::LineDef
            && SelectedTopologyLineDef() != nullptr;
    const bool hasSelectedLight = SelectedTopologyLight() != nullptr;
    const bool hasSelectedDynamicLight = SelectedTopologyDynamicLight() != nullptr;
    const SectorEditorInspectorTarget inspectorTarget = ResolveSectorEditorInspectorTarget(state);
    const bool allowLegacyTopologyInspector =
            inspectorTarget.kind == SectorEditorInspectorTargetKind::LegacyTopology
            || inspectorTarget.kind == SectorEditorInspectorTargetKind::None;
    const SectorAuthoringLine* selectedAuthoringLine =
            inspectorTarget.kind == SectorEditorInspectorTargetKind::AuthoringLine
            ? FindSectorAuthoringLine(state.authoringGraph, inspectorTarget.lineId)
            : nullptr;
    const SectorAuthoringFaceAnchor* selectedAuthoringFaceAnchor =
            inspectorTarget.kind == SectorEditorInspectorTargetKind::AuthoringFaceAnchor
            ? FindSectorAuthoringFaceAnchor(state.authoringGraph, inspectorTarget.faceAnchorId)
            : nullptr;
    const SectorAuthoringVertex* selectedAuthoringVertex =
            inspectorTarget.kind == SectorEditorInspectorTargetKind::AuthoringVertex
            ? FindSectorAuthoringVertex(state.authoringGraph, inspectorTarget.vertexId)
            : nullptr;
    const SectorTopologyVertex* inspectedVertex = FindSectorTopologyVertex(
            state.topologyMap,
            state.inspectedTopologyVertexId);
    const bool hasInspectedVertex = !hasSelectedTopologySector
            && !hasSelectedTopologyVertex
            && !hasSelectedTopologySideDef
            && !hasSelectedTopologyLineDef
            && !hasSelectedLight
            && !hasSelectedDynamicLight
            && state.currentTool == SectorEditorTool::Move
            && inspectedVertex != nullptr;

    const float rowH = 40.0f;
    const float gap = 8.0f;
    const float scrollContentW = std::max(0.0f, panel.contentRect.width - config.scrollbarSize);
    const auto inspectorContentHeight = [&]() {
        if (inspectorTarget.kind == SectorEditorInspectorTargetKind::AuthoringUnavailable) {
            return 120.0f;
        }
        if (hasSelectedLight) {
            return StaticLightInspectorContentHeight(rowH, gap, !uiState.idEditError.empty());
        }
        if (hasSelectedDynamicLight) {
            return DynamicLightInspectorContentHeight(rowH, gap, !uiState.idEditError.empty());
        }
        if (hasSelectedTopologySector && allowLegacyTopologyInspector) {
            return SectorInspectorContentHeight(rowH, gap, !uiState.idEditError.empty());
        }
        if (hasSelectedTopologyVertex && allowLegacyTopologyInspector) {
            return SelectedVertexInspectorContentHeight();
        }
        if (hasSelectedTopologySideDef && allowLegacyTopologyInspector) {
            return 1240.0f;
        }
        if (hasSelectedTopologyLineDef && allowLegacyTopologyInspector) {
            return 218.0f;
        }
        if (hasInspectedVertex) {
            return InspectedVertexInspectorContentHeight();
        }
        if (selectedAuthoringLine != nullptr) {
            return AuthoringLineInspectorContentHeight(*selectedAuthoringLine, state.authoringGraph, rowH, gap);
        }
        if (selectedAuthoringFaceAnchor != nullptr) {
            return AuthoringFaceInspectorContentHeight(*selectedAuthoringFaceAnchor, rowH, gap);
        }
        if (selectedAuthoringVertex != nullptr) {
            return 120.0f;
        }
        return 42.0f;
    };
    const float contentH = inspectorContentHeight();
    engine::UIScrollAreaResult scroll = engine::BeginScrollArea(
            ui,
            config,
            input,
            "sector_editor_inspector_scroll",
            panel.contentRect,
            Vector2{scrollContentW, contentH},
            uiState.inspectorScroll,
            false
    );

    const float contentW = scroll.viewport.width;
    float y = 0.0f;

    if (inspectorTarget.kind == SectorEditorInspectorTargetKind::AuthoringUnavailable) {
        engine::Text(
                ui,
                config,
                assets,
                Rectangle{0.0f, y, contentW, 34.0f},
                font,
                "Authoring Inspector",
                engine::UITextJustify::Left,
                config.textColor);
        y += 38.0f;
        engine::Text(
                ui,
                config,
                assets,
                Rectangle{0.0f, y, contentW, 64.0f},
                font,
                inspectorTarget.status.empty()
                        ? "Mapped authoring target is unavailable."
                        : inspectorTarget.status.c_str(),
                engine::UITextJustify::Left,
                config.invalidColor);
        engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
        engine::EndPanel(ui, config, panel);
        return;
    }

    if (hasSelectedTopologySector && allowLegacyTopologyInspector) {
        const auto hasAuthoringGraph = [this]() {
            return !state.authoringGraph.vertices.empty()
                    || !state.authoringGraph.lines.empty()
                    || !state.authoringGraph.lineSides.empty()
                    || !state.authoringGraph.faceAnchors.empty();
        };
        const auto selectedAuthoringFaceAnchorUnavailable = [this, hasAuthoringGraph]() {
            const SectorTopologySector* selectedSector = SelectedTopologySector();
            if (selectedSector == nullptr || !hasAuthoringGraph()) {
                return false;
            }
            if (state.authoringDerivationState != SectorEditorAuthoringDerivationState::ValidCurrent
                    || state.authoringDerivedTopologyStale
                    || !state.authoringDerivation.success) {
                return true;
            }
            return FindSectorEditorAuthoringFaceAnchorIdForTopologySector(
                           state,
                           selectedSector->id) < 0;
        };
        const auto reportAuthoringFaceAnchorUnavailable = [this]() {
            const char* message =
                    "Sector property edit unavailable: selected sector has no current face anchor mapping";
            statusText = message;
            return true;
        };
        const auto mutateSelectedAuthoringFaceAnchor =
                [this](const char* status, const std::function<bool(SectorAuthoringFaceAnchor&)>& mutate) {
                    const SectorTopologySector* selectedSector = SelectedTopologySector();
                    if (selectedSector == nullptr) {
                        return false;
                    }
                    if (state.authoringDerivationState != SectorEditorAuthoringDerivationState::ValidCurrent
                            || state.authoringDerivedTopologyStale
                            || !state.authoringDerivation.success) {
                        return false;
                    }
                    if (FindSectorEditorAuthoringFaceAnchorIdForTopologySector(
                                state,
                                selectedSector->id) < 0) {
                        return false;
                    }
                    MutateSectorEditorAuthoringFaceAnchorForTopologySector(
                            state,
                            selectedSector->id,
                            status,
                            mutate);
                    return true;
                };
        const auto mutateSelectedTopologySector =
                [this](const char* status, const std::function<bool(SectorTopologySector&)>& mutate) {
                    SectorTopologySector* selectedSector = SelectedTopologySector();
                    if (selectedSector == nullptr || !mutate || !mutate(*selectedSector)) {
                        return false;
                    }
                    MarkTopologyDocumentEdited(status);
                    return true;
                };
        const SectorEditorSectorInspectorCallbacks callbacks{
                [this]() { return TryRenameSelectedTopologySector(); },
                [this](const char* status) { statusText = status != nullptr ? status : ""; },
                [this](const char* status) { MarkTopologyDocumentEdited(status); },
                [mutateSelectedAuthoringFaceAnchor,
                 mutateSelectedTopologySector,
                 selectedAuthoringFaceAnchorUnavailable,
                 reportAuthoringFaceAnchorUnavailable](
                        float floorZ,
                        float ceilingZ) {
                    const char* status = "Updated sector height";
                    if (mutateSelectedAuthoringFaceAnchor(
                                status,
                                [floorZ, ceilingZ](SectorAuthoringFaceAnchor& anchor) {
                                    if (anchor.floorZ == floorZ && anchor.ceilingZ == ceilingZ) {
                                        return false;
                                    }
                                    anchor.floorZ = floorZ;
                                    anchor.ceilingZ = ceilingZ;
                                    return true;
                                })) {
                        return true;
                    }
                    if (selectedAuthoringFaceAnchorUnavailable()) {
                        return reportAuthoringFaceAnchorUnavailable();
                    }
                    return mutateSelectedTopologySector(
                            status,
                            [floorZ, ceilingZ](SectorTopologySector& sector) {
                                if (sector.floorZ == floorZ && sector.ceilingZ == ceilingZ) {
                                    return false;
                                }
                                sector.floorZ = floorZ;
                                sector.ceilingZ = ceilingZ;
                                return true;
                            });
                },
                [mutateSelectedAuthoringFaceAnchor,
                 mutateSelectedTopologySector,
                 selectedAuthoringFaceAnchorUnavailable,
                 reportAuthoringFaceAnchorUnavailable](bool ceilingSky) {
                    const char* status = "Updated sector ceiling sky";
                    if (mutateSelectedAuthoringFaceAnchor(
                                status,
                                [ceilingSky](SectorAuthoringFaceAnchor& anchor) {
                                    if (anchor.ceilingSky == ceilingSky) {
                                        return false;
                                    }
                                    anchor.ceilingSky = ceilingSky;
                                    return true;
                                })) {
                        return true;
                    }
                    if (selectedAuthoringFaceAnchorUnavailable()) {
                        return reportAuthoringFaceAnchorUnavailable();
                    }
                    return mutateSelectedTopologySector(
                            status,
                            [ceilingSky](SectorTopologySector& sector) {
                                if (sector.ceilingSky == ceilingSky) {
                                    return false;
                                }
                                sector.ceilingSky = ceilingSky;
                                return true;
                            });
                },
                [mutateSelectedAuthoringFaceAnchor,
                 mutateSelectedTopologySector,
                 selectedAuthoringFaceAnchorUnavailable,
                 reportAuthoringFaceAnchorUnavailable](float intensity) {
                    const char* status = "Updated sector ambient intensity";
                    if (mutateSelectedAuthoringFaceAnchor(
                                status,
                                [intensity](SectorAuthoringFaceAnchor& anchor) {
                                    if (anchor.ambientIntensity == intensity) {
                                        return false;
                                    }
                                    anchor.ambientIntensity = intensity;
                                    return true;
                                })) {
                        return true;
                    }
                    if (selectedAuthoringFaceAnchorUnavailable()) {
                        return reportAuthoringFaceAnchorUnavailable();
                    }
                    return mutateSelectedTopologySector(
                            status,
                            [intensity](SectorTopologySector& sector) {
                                if (sector.ambientIntensity == intensity) {
                                    return false;
                                }
                                sector.ambientIntensity = intensity;
                                return true;
                            });
                },
                [mutateSelectedAuthoringFaceAnchor,
                 mutateSelectedTopologySector,
                 selectedAuthoringFaceAnchorUnavailable,
                 reportAuthoringFaceAnchorUnavailable](Color color) {
                    const char* status = "Updated sector ambient color";
                    if (mutateSelectedAuthoringFaceAnchor(
                                status,
                                [color](SectorAuthoringFaceAnchor& anchor) {
                                    if (anchor.ambientColor.r == color.r
                                            && anchor.ambientColor.g == color.g
                                            && anchor.ambientColor.b == color.b
                                            && anchor.ambientColor.a == color.a) {
                                        return false;
                                    }
                                    anchor.ambientColor = color;
                                    return true;
                                })) {
                        return true;
                    }
                    if (selectedAuthoringFaceAnchorUnavailable()) {
                        return reportAuthoringFaceAnchorUnavailable();
                    }
                    return mutateSelectedTopologySector(
                            status,
                            [color](SectorTopologySector& sector) {
                                if (sector.ambientColor.r == color.r
                                        && sector.ambientColor.g == color.g
                                        && sector.ambientColor.b == color.b
                                        && sector.ambientColor.a == color.a) {
                                    return false;
                                }
                                sector.ambientColor = color;
                                return true;
                            });
                },
                [mutateSelectedAuthoringFaceAnchor,
                 mutateSelectedTopologySector,
                 selectedAuthoringFaceAnchorUnavailable,
                 reportAuthoringFaceAnchorUnavailable](
                        TopologySectorTextureField field,
                        const SectorTopologyUvSettings& uv) {
                    const char* status = "Updated sector UV";
                    const auto applyAnchorUv =
                            [field, uv](SectorAuthoringFaceAnchor& anchor) {
                                SectorTopologyUvSettings* target = nullptr;
                                switch (field) {
                                case TopologySectorTextureField::Floor:
                                    target = &anchor.floorUv;
                                    break;
                                case TopologySectorTextureField::Ceiling:
                                    target = &anchor.ceilingUv;
                                    break;
                                case TopologySectorTextureField::DefaultWall:
                                    target = &anchor.defaultWall.uv;
                                    break;
                                case TopologySectorTextureField::DefaultLower:
                                    target = &anchor.defaultLower.uv;
                                    break;
                                case TopologySectorTextureField::DefaultUpper:
                                    target = &anchor.defaultUpper.uv;
                                    break;
                                case TopologySectorTextureField::None:
                                    break;
                                }
                                if (target == nullptr) {
                                    return false;
                                }
                                *target = uv;
                                return true;
                    };
                    if (mutateSelectedAuthoringFaceAnchor(status, applyAnchorUv)) {
                        return true;
                    }
                    if (selectedAuthoringFaceAnchorUnavailable()) {
                        return reportAuthoringFaceAnchorUnavailable();
                    }
                    return mutateSelectedTopologySector(
                            status,
                            [field, uv](SectorTopologySector& sector) {
                                SectorTopologyUvSettings* target = nullptr;
                                switch (field) {
                                case TopologySectorTextureField::Floor:
                                    target = &sector.floorUv;
                                    break;
                                case TopologySectorTextureField::Ceiling:
                                    target = &sector.ceilingUv;
                                    break;
                                case TopologySectorTextureField::DefaultWall:
                                    target = &sector.defaultWall.uv;
                                    break;
                                case TopologySectorTextureField::DefaultLower:
                                    target = &sector.defaultLower.uv;
                                    break;
                                case TopologySectorTextureField::DefaultUpper:
                                    target = &sector.defaultUpper.uv;
                                    break;
                                case TopologySectorTextureField::None:
                                    break;
                                }
                                if (target == nullptr) {
                                    return false;
                                }
                                *target = uv;
                                return true;
                            });
                },
                [this](int sectorId, TopologySectorTextureField field, TopologyMaterialLayer layer) {
                    OpenTopologyTexturePicker(sectorId, field, layer);
                },
                [this](TopologySurfaceEditTarget target) { return CopyTopologyMaterial(target); },
                [this](TopologySurfaceEditTarget target, engine::AssetManager& callbackAssets) {
                    return PasteTopologyMaterial(target, callbackAssets);
                },
                [this](TopologySurfaceEditTarget target, float opacity) {
                    return ApplySurfaceDecalOpacity(target, opacity, nullptr);
                },
                [this](TopologySurfaceEditTarget target, bool emissive) {
                    return ApplySurfaceDecalEmissive(target, emissive, nullptr);
                },
                [this](TopologySurfaceEditTarget target, float bloomIntensity) {
                    return ApplySurfaceDecalBloomIntensity(target, bloomIntensity, nullptr);
                },
                [this](TopologySurfaceEditTarget target) { return OpenDecalTintModal(target); },
                [this](TopologySurfaceEditTarget target) { return FitSelectedDecal(target, nullptr); },
                [this](TopologySurfaceEditTarget target) { return ClearSurfaceDecal(target, nullptr); }
        };
        if (game::DrawTopologySectorInspector(
                    ui,
                    config,
                    input,
                    assets,
                    font,
                    scroll,
                    contentW,
                    rowH,
                    gap,
                    *SelectedTopologySector(),
                    state,
                    uiState,
                    callbacks)) {
            engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
            engine::EndPanel(ui, config, panel);
            return;
        }
    }

    if (allowLegacyTopologyInspector && (hasSelectedTopologySideDef || hasSelectedTopologyLineDef)) {
        if (DrawTopologySideDefInspector(ui, config, input, assets, font, scroll, contentW, rowH, gap)) {
            engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
            engine::EndPanel(ui, config, panel);
            return;
        }
    }

    if (hasSelectedLight) {
        const SectorEditorLightInspectorCallbacks callbacks{
                [this](const char* status) {
                    SectorEditorTopologyActionResult result;
                    result.changed = true;
                    result.status = status == nullptr ? "" : status;
                    FinishTopologyActionResult(result);
                },
                [this]() { return DeleteSelectedLight(); },
                [this]() { return BakeLightmaps(); }
        };
        DrawSelectedStaticLightInspector(
                ui,
                config,
                input,
                assets,
                font,
                scroll,
                contentW,
                rowH,
                gap,
                *SelectedTopologyLight(),
                uiState,
                callbacks);
        engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
        engine::EndPanel(ui, config, panel);
        return;
    }

    if (hasSelectedDynamicLight) {
        const SectorEditorLightInspectorCallbacks callbacks{
                [this](const char* status) {
                    SectorEditorTopologyActionResult result;
                    result.changed = true;
                    result.status = status == nullptr ? "" : status;
                    FinishTopologyActionResult(result);
                },
                [this]() { return DeleteSelectedLight(); },
                []() { return false; }
        };
        DrawSelectedDynamicLightInspector(
                ui,
                config,
                input,
                assets,
                font,
                scroll,
                contentW,
                rowH,
                gap,
                *SelectedTopologyDynamicLight(),
                uiState,
                callbacks);
        engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
        engine::EndPanel(ui, config, panel);
        return;
    }

    if ((allowLegacyTopologyInspector && hasSelectedTopologyVertex)
            || hasInspectedVertex) {
        const SectorEditorVertexInspectorCallbacks callbacks{
                [this]() { ClearStaleTopologySelection(); }
        };
        DrawTopologyVertexInspector(
                ui,
                config,
                input,
                assets,
                font,
                contentW,
                rowH,
                gap,
                inspectedVertex,
                hasSelectedTopologyVertex,
                state,
                callbacks);
        engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
        engine::EndPanel(ui, config, panel);
        return;
    }

    if (selectedAuthoringLine != nullptr) {
        const SectorAuthoringVertex* start =
                FindSectorAuthoringVertex(state.authoringGraph, selectedAuthoringLine->startVertexId);
        const SectorAuthoringVertex* end =
                FindSectorAuthoringVertex(state.authoringGraph, selectedAuthoringLine->endVertexId);
        engine::Text(
                ui,
                config,
                assets,
                Rectangle{0.0f, y, contentW, 34.0f},
                font,
                TextFormat("Authoring Line: %d", selectedAuthoringLine->id),
                engine::UITextJustify::Left,
                config.textColor);
        y += 38.0f;

        if (start != nullptr && end != nullptr) {
            engine::Text(
                    ui,
                    config,
                    assets,
                    Rectangle{0.0f, y, contentW, 30.0f},
                    font,
                    TextFormat(
                            "From %.2f, %.2f  To %.2f, %.2f",
                            SectorCoordToVisibleAuthoring(start->x),
                            SectorCoordToVisibleAuthoring(start->y),
                            SectorCoordToVisibleAuthoring(end->x),
                            SectorCoordToVisibleAuthoring(end->y)),
                    engine::UITextJustify::Left,
                    config.mutedTextColor);
            y += 32.0f;
        } else {
            engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 30.0f}, font, "Line endpoints are invalid", engine::UITextJustify::Left, config.invalidColor);
            y += 32.0f;
        }

        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_authoring_line_insert_vertex",
                    Rectangle{0.0f, y, contentW, rowH},
                    font,
                    "Insert Vertex")) {
            BeginPendingAuthoringInsertVertex(selectedAuthoringLine->id);
            engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
            engine::EndPanel(ui, config, panel);
            return;
        }
        y += rowH + gap;

        bool blocksPlayer = selectedAuthoringLine->flags.blocksPlayer;
        if (engine::Checkbox(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_authoring_line_blocks_player",
                    Rectangle{0.0f, y, contentW, 36.0f},
                    font,
                    "Blocks Player",
                    blocksPlayer)) {
            const int lineId = selectedAuthoringLine->id;
            MutateSectorEditorAuthoringLineById(
                    state,
                    lineId,
                    "Updated authoring line flags",
                    [blocksPlayer](SectorAuthoringLine& line) {
                        if (line.flags.blocksPlayer == blocksPlayer) {
                            return false;
                        }
                        line.flags.blocksPlayer = blocksPlayer;
                        return true;
                    });
            engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
            engine::EndPanel(ui, config, panel);
            return;
        }
        y += 36.0f + gap;

        const auto drawAuthoringSideSection =
                [&](SectorTopologySideKind sideKind, const char* title, const char* idPrefix) {
                    engine::Separator(config, Rectangle{scroll.viewport.x, scroll.viewport.y - uiState.inspectorScroll.offset.y + y, contentW, 12.0f});
                    y += 18.0f;
                    engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 30.0f}, font, title, engine::UITextJustify::Left, config.textColor);
                    y += 30.0f;

                    const SectorAuthoringSideId sideId{selectedAuthoringLine->id, sideKind};
                    const SectorAuthoringLineSide* authoringSide =
                            FindSectorAuthoringLineSide(state.authoringGraph, sideId);
                    const auto textureForPart = [authoringSide](TopologyWallPart part) -> std::string {
                        if (authoringSide == nullptr) {
                            return std::string{};
                        }
                        return TopologyWallPartSettingsFor(*authoringSide, part).textureId;
                    };
                    const auto decalForPart = [authoringSide](TopologyWallPart part) -> SectorTopologyDecalLayer {
                        if (authoringSide == nullptr) {
                            return SectorTopologyDecalLayer{};
                        }
                        return TopologyWallPartSettingsFor(*authoringSide, part).decal;
                    };
                    const auto mappedTargetForPart = [this, sideId](TopologyWallPart part, TopologySurfaceEditTarget& outTarget) {
                        if (state.authoringDerivationState != SectorEditorAuthoringDerivationState::ValidCurrent
                                || state.authoringDerivedTopologyStale
                                || !state.authoringDerivation.success) {
                            return false;
                        }
                        for (const SectorAuthoringDerivedSideMapping& mapping : state.authoringDerivation.mapping.sides) {
                            if (mapping.authoringLineId != sideId.lineId || mapping.authoringSide != sideId.side) {
                                continue;
                            }
                            const SectorTopologySideDef* sideDef =
                                    FindSectorTopologySideDef(state.topologyMap, mapping.topologySideDefId);
                            if (sideDef == nullptr) {
                                continue;
                            }
                            outTarget.kind = TopologyWallPartEditTargetKind(part);
                            outTarget.sectorId = sideDef->sectorId;
                            outTarget.lineDefId = sideDef->lineDefId;
                            outTarget.sideDefId = sideDef->id;
                            outTarget.side = sideDef->side;
                            return true;
                        }
                        return false;
                    };
                    const auto mutateSide = [this, sideId](const char* status, const std::function<bool(SectorAuthoringLineSide&)>& mutate) {
                        return MutateSectorEditorAuthoringSideById(state, sideId, status, mutate);
                    };
                    const auto drawTextureRow =
                            [&](const char* suffix, const char* label, TopologyWallPart part) {
                                const float buttonW = 38.0f;
                                const bool canClear = part == TopologyWallPart::Middle;
                                const float clearW = canClear ? 58.0f : 0.0f;
                                const std::string textureId = textureForPart(part);
                                const SectorEditorInspectorTextureRowLayout row =
                                        BuildSectorEditorInspectorTextureRowLayout(y, contentW, gap, buttonW, clearW);
                                const bool missing = !textureId.empty()
                                        && FindSectorTopologyTexture(state.topologyMap, textureId) == nullptr;
                                engine::Text(ui, config, assets, row.labelRect, font, label, engine::UITextJustify::Left, config.mutedTextColor);
                                engine::Text(
                                        ui,
                                        config,
                                        assets,
                                        row.valueRect,
                                        font,
                                        textureId.empty() ? "<none>" : textureId.c_str(),
                                        engine::UITextJustify::Left,
                                        missing ? config.invalidColor : config.mutedTextColor);
                                if (canClear
                                        && engine::Button(
                                                ui,
                                                config,
                                                input,
                                                assets,
                                                TextFormat("%s_%s_clear", idPrefix, suffix),
                                                row.clearButtonRect,
                                                font,
                                                "Clear")) {
                                    mutateSide(
                                            "Cleared authoring middle texture",
                                            [part](SectorAuthoringLineSide& side) {
                                                SectorTopologyWallPartSettings& settings =
                                                        TopologyWallPartSettingsFor(side, part);
                                                if (IsDefaultWallPartSettings(settings)) {
                                                    return false;
                                                }
                                                settings = SectorTopologyWallPartSettings{};
                                                return true;
                                            });
                                }
                                if (engine::Button(
                                            ui,
                                            config,
                                            input,
                                            assets,
                                            TextFormat("%s_%s", idPrefix, suffix),
                                            row.pickerButtonRect,
                                            font,
                                            ">")) {
                                    if (!OpenAuthoringSideTexturePickerById(
                                                state,
                                                sideId,
                                                part,
                                                TopologyMaterialLayer::Base)) {
                                        statusText = "Authoring side texture picker unavailable: derived mapping is not current";
                                    }
                                }
                                y += row.height + gap;
                            };
                    const auto drawDecalControls =
                            [&](const char* suffix, const char* title, TopologyWallPart part) {
                                const SectorTopologyDecalLayer decal = decalForPart(part);
                                const float buttonW = 38.0f;
                                const float clearW = 92.0f;
                                const SectorEditorInspectorTextureRowLayout row =
                                        BuildSectorEditorInspectorTextureRowLayout(y, contentW, gap, buttonW, clearW);
                                const bool missing = !decal.textureId.empty()
                                        && FindSectorTopologyTexture(state.topologyMap, decal.textureId) == nullptr;
                                engine::Text(ui, config, assets, row.labelRect, font, title, engine::UITextJustify::Left, config.mutedTextColor);
                                engine::Text(
                                        ui,
                                        config,
                                        assets,
                                        row.valueRect,
                                        font,
                                        decal.textureId.empty() ? "<none>" : decal.textureId.c_str(),
                                        engine::UITextJustify::Left,
                                        missing ? config.invalidColor : config.mutedTextColor);
                                if (engine::Button(
                                            ui,
                                            config,
                                            input,
                                            assets,
                                            TextFormat("%s_%s_clear_decal", idPrefix, suffix),
                                            row.clearButtonRect,
                                            font,
                                            "Clear")) {
                                    mutateSide(
                                            "Cleared authoring side decal",
                                            [part](SectorAuthoringLineSide& side) {
                                                SectorTopologyDecalLayer& target =
                                                        TopologyWallPartSettingsFor(side, part).decal;
                                                if (IsDefaultDecalLayer(target)) {
                                                    return false;
                                                }
                                                ResetDecalLayer(target);
                                                return true;
                                            });
                                }
                                if (engine::Button(
                                            ui,
                                            config,
                                            input,
                                            assets,
                                            TextFormat("%s_%s_pick_decal", idPrefix, suffix),
                                            row.pickerButtonRect,
                                            font,
                                            ">")) {
                                    if (!OpenAuthoringSideTexturePickerById(
                                                state,
                                                sideId,
                                                part,
                                                TopologyMaterialLayer::Decal)) {
                                        statusText = "Authoring side decal picker unavailable: derived mapping is not current";
                                    }
                                }
                                y += row.height + gap;

                                if (decal.textureId.empty()) {
                                    return;
                                }

                                const SectorEditorInspectorNumericRowLayout opacityLayout =
                                        BuildSectorEditorInspectorCompactNumericRowLayout(y, contentW, rowH);
                                const SectorEditorFloatInputResult opacityResult = DrawLabeledFloatInput(
                                        ui,
                                        config,
                                        input,
                                        assets,
                                        font,
                                        TextFormat("%s_%s_decal_opacity", idPrefix, suffix),
                                        "Opacity:",
                                        opacityLayout.labelRect,
                                        opacityLayout.inputRect,
                                        engine::UITextJustify::Left,
                                        decal.opacity,
                                        uiState.topologySideDefDecalOpacityInput,
                                        0.0f,
                                        1.0f,
                                        3);
                                if (opacityResult.changed && opacityResult.value != decal.opacity && opacityResult.finite) {
                                    mutateSide(
                                            "Updated authoring side decal opacity",
                                            [part, value = opacityResult.value](SectorAuthoringLineSide& side) {
                                                SectorTopologyDecalLayer& target =
                                                        TopologyWallPartSettingsFor(side, part).decal;
                                                if (target.textureId.empty() || target.opacity == value) {
                                                    return false;
                                                }
                                                target.opacity = value;
                                                return true;
                                            });
                                }
                                y += rowH + gap;

                                bool emissive = decal.emissive;
                                if (engine::Checkbox(
                                            ui,
                                            config,
                                            input,
                                            assets,
                                            TextFormat("%s_%s_decal_emissive", idPrefix, suffix),
                                            Rectangle{0.0f, y, contentW, 36.0f},
                                            font,
                                            "Emissive",
                                            emissive)) {
                                    mutateSide(
                                            "Updated authoring side decal emissive",
                                            [part, emissive](SectorAuthoringLineSide& side) {
                                                SectorTopologyDecalLayer& target =
                                                        TopologyWallPartSettingsFor(side, part).decal;
                                                if (target.textureId.empty() || target.emissive == emissive) {
                                                    return false;
                                                }
                                                target.emissive = emissive;
                                                return true;
                                            });
                                }
                                y += 36.0f + gap;

                                if (decal.emissive) {
                                    const SectorEditorInspectorNumericRowLayout bloomLayout =
                                            BuildSectorEditorInspectorCompactNumericRowLayout(y, contentW, rowH);
                                    const SectorEditorFloatInputResult bloomResult = DrawLabeledFloatInput(
                                            ui,
                                            config,
                                            input,
                                            assets,
                                            font,
                                            TextFormat("%s_%s_decal_bloom", idPrefix, suffix),
                                            "Bloom:",
                                            bloomLayout.labelRect,
                                            bloomLayout.inputRect,
                                            engine::UITextJustify::Left,
                                            decal.bloomIntensity,
                                            uiState.topologySideDefDecalBloomIntensityInput,
                                            0.0f,
                                            10.0f,
                                            3);
                                    if (bloomResult.changed && bloomResult.value != decal.bloomIntensity && bloomResult.finite) {
                                        mutateSide(
                                                "Updated authoring side decal bloom intensity",
                                                [part, value = bloomResult.value](SectorAuthoringLineSide& side) {
                                                    SectorTopologyDecalLayer& target =
                                                            TopologyWallPartSettingsFor(side, part).decal;
                                                    if (target.textureId.empty() || target.bloomIntensity == value) {
                                                        return false;
                                                    }
                                                    target.bloomIntensity = value;
                                                    return true;
                                                });
                                    }
                                    y += rowH + gap;
                                }

                                engine::Text(ui, config, assets, Rectangle{0.0f, y, 82.0f, rowH}, font, "Tint:", engine::UITextJustify::Left, config.mutedTextColor);
                                const Rectangle swatchLocal{82.0f, y + 3.0f, 56.0f, rowH - 6.0f};
                                if (engine::Button(
                                            ui,
                                            config,
                                            input,
                                            assets,
                                            TextFormat("%s_%s_decal_tint", idPrefix, suffix),
                                            swatchLocal,
                                            font,
                                            "")) {
                                    TopologySurfaceEditTarget target;
                                    if (mappedTargetForPart(part, target)) {
                                        OpenDecalTintModal(target);
                                    } else {
                                        statusText = "Authoring side decal tint unavailable: derived mapping is not current";
                                    }
                                }
                                const Rectangle swatchScreen{
                                        scroll.viewport.x + swatchLocal.x,
                                        scroll.viewport.y - uiState.inspectorScroll.offset.y + swatchLocal.y,
                                        swatchLocal.width,
                                        swatchLocal.height};
                                DrawColorSwatch(config, swatchScreen, DecalTintPreviewColor(decal.tint), config.borderThickness);
                                y += rowH + gap;

                                TopologySurfaceEditTarget fitTarget;
                                if (mappedTargetForPart(part, fitTarget)
                                        && engine::Button(
                                                ui,
                                                config,
                                                input,
                                                assets,
                                                TextFormat("%s_%s_fit_decal", idPrefix, suffix),
                                                Rectangle{0.0f, y, contentW, 36.0f},
                                                font,
                                                "Fit Decal")) {
                                    FitSelectedDecal(fitTarget, &assets);
                                }
                                y += 36.0f + gap;
                            };
                    drawTextureRow("wall", "Wall:", TopologyWallPart::Wall);
                    drawTextureRow("lower", "Lower:", TopologyWallPart::Lower);
                    drawTextureRow("upper", "Upper:", TopologyWallPart::Upper);
                    drawTextureRow("middle", "Middle:", TopologyWallPart::Middle);
                    drawDecalControls("wall", "Wall Decal:", TopologyWallPart::Wall);
                    drawDecalControls("lower", "Lower Decal:", TopologyWallPart::Lower);
                    drawDecalControls("upper", "Upper Decal:", TopologyWallPart::Upper);
                };

        drawAuthoringSideSection(SectorTopologySideKind::Front, "Front Side", "sector_editor_authoring_front_side");
        drawAuthoringSideSection(SectorTopologySideKind::Back, "Back Side", "sector_editor_authoring_back_side");
        engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
        engine::EndPanel(ui, config, panel);
        return;
    }

    if (selectedAuthoringFaceAnchor != nullptr) {
        engine::Text(
                ui,
                config,
                assets,
                Rectangle{0.0f, y, contentW, 34.0f},
                font,
                TextFormat("Authoring Face: %d", selectedAuthoringFaceAnchor->id),
                engine::UITextJustify::Left,
                config.textColor);
        y += 38.0f;
        engine::Text(
                ui,
                config,
                assets,
                Rectangle{0.0f, y, contentW, 30.0f},
                font,
                TextFormat(
                        "Anchor %.2f, %.2f",
                        SectorCoordToVisibleAuthoring(selectedAuthoringFaceAnchor->x),
                        SectorCoordToVisibleAuthoring(selectedAuthoringFaceAnchor->y)),
                engine::UITextJustify::Left,
                config.mutedTextColor);
        y += 32.0f;

        const float labelW = 92.0f;
        const float numberFieldW = 112.0f;
        const int faceAnchorId = selectedAuthoringFaceAnchor->id;
        const auto mutateFaceAnchor =
                [this, faceAnchorId](const char* status, const std::function<bool(SectorAuthoringFaceAnchor&)>& mutate) {
                    return MutateSectorEditorAuthoringFaceAnchorById(state, faceAnchorId, status, mutate);
                };

        bool isVoidFace = selectedAuthoringFaceAnchor->isVoid;
        if (engine::Checkbox(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_authoring_face_void",
                    Rectangle{0.0f, y, contentW, rowH},
                    font,
                    "Void Face",
                    isVoidFace)) {
            mutateFaceAnchor(
                    "Updated authoring face void state",
                    [isVoidFace](SectorAuthoringFaceAnchor& anchor) {
                        if (anchor.isVoid == isVoidFace) {
                            return false;
                        }
                        anchor.isVoid = isVoidFace;
                        return true;
                    });
        }
        y += rowH + gap;

        auto drawHeight = [&](const char* id, const char* label, float current, engine::UIFloatInputState& inputState, bool floorField) {
            const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
                    ui,
                    config,
                    input,
                    assets,
                    font,
                    id,
                    label,
                    Rectangle{0.0f, y, labelW, rowH},
                    Rectangle{labelW, y, numberFieldW, rowH},
                    engine::UITextJustify::Right,
                    current,
                    inputState,
                    -512.0f,
                    512.0f,
                    2);
            if (result.changed && result.value != current) {
                const float nextFloor = floorField ? result.value : selectedAuthoringFaceAnchor->floorZ;
                const float nextCeiling = floorField ? selectedAuthoringFaceAnchor->ceilingZ : result.value;
                if (!std::isfinite(nextFloor) || !std::isfinite(nextCeiling) || nextCeiling <= nextFloor) {
                    statusText = "Invalid authoring face heights: ceiling must be greater than floor";
                } else {
                    mutateFaceAnchor(
                            "Updated authoring face height",
                            [nextFloor, nextCeiling](SectorAuthoringFaceAnchor& anchor) {
                                if (anchor.floorZ == nextFloor && anchor.ceilingZ == nextCeiling) {
                                    return false;
                                }
                                anchor.floorZ = nextFloor;
                                anchor.ceilingZ = nextCeiling;
                                return true;
                            });
                }
            }
            y += rowH + gap;
        };
        drawHeight("sector_editor_authoring_face_floor", "Floor:", selectedAuthoringFaceAnchor->floorZ, uiState.floorInput, true);
        drawHeight("sector_editor_authoring_face_ceiling", "Ceiling:", selectedAuthoringFaceAnchor->ceilingZ, uiState.ceilingInput, false);

        bool ceilingSky = selectedAuthoringFaceAnchor->ceilingSky;
        if (engine::Checkbox(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_authoring_face_ceiling_sky",
                    Rectangle{0.0f, y, contentW, rowH},
                    font,
                    "Ceiling Sky",
                    ceilingSky)) {
            mutateFaceAnchor(
                    "Updated authoring face ceiling sky",
                    [ceilingSky](SectorAuthoringFaceAnchor& anchor) {
                        if (anchor.ceilingSky == ceilingSky) {
                            return false;
                        }
                        anchor.ceilingSky = ceilingSky;
                        return true;
                    });
        }
        y += rowH + gap;

        engine::Separator(config, Rectangle{scroll.viewport.x, scroll.viewport.y - uiState.inspectorScroll.offset.y + y, contentW, 12.0f});
        y += 18.0f;
        engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 30.0f}, font, "Lighting", engine::UITextJustify::Left, config.textColor);
        y += 30.0f;

        const float ambientIntensity = std::clamp(selectedAuthoringFaceAnchor->ambientIntensity, 0.0f, 1.0f);
        const SectorEditorFloatInputResult ambientResult = DrawLabeledFloatInput(
                ui,
                config,
                input,
                assets,
                font,
                "sector_editor_authoring_face_ambient_intensity",
                "Intensity:",
                Rectangle{0.0f, y, labelW, rowH},
                Rectangle{labelW, y, numberFieldW, rowH},
                engine::UITextJustify::Right,
                ambientIntensity,
                uiState.ambientIntensityInput,
                0.0f,
                1.0f,
                3);
        if (ambientResult.changed && ambientResult.value != selectedAuthoringFaceAnchor->ambientIntensity) {
            mutateFaceAnchor(
                    "Updated authoring face ambient intensity",
                    [value = ambientResult.value](SectorAuthoringFaceAnchor& anchor) {
                        if (anchor.ambientIntensity == value) {
                            return false;
                        }
                        anchor.ambientIntensity = value;
                        return true;
                    });
        }
        y += rowH + gap;

        auto drawAmbientChannel = [&](const char* id, const char* label, unsigned char current, engine::UIIntInputState& inputState, int channel) {
            const SectorEditorRgb8InputResult result = DrawRgb8ChannelInput(
                    ui,
                    config,
                    input,
                    assets,
                    font,
                    id,
                    label,
                    Rectangle{0.0f, y, labelW, rowH},
                    Rectangle{labelW, y, contentW - labelW, rowH},
                    engine::UITextJustify::Right,
                    current,
                    inputState);
            if (result.changed && result.channel != current) {
                mutateFaceAnchor(
                        "Updated authoring face ambient color",
                        [channel, value = result.channel](SectorAuthoringFaceAnchor& anchor) {
                            Color next = anchor.ambientColor;
                            if (channel == 0) {
                                next.r = value;
                            } else if (channel == 1) {
                                next.g = value;
                            } else {
                                next.b = value;
                            }
                            next.a = 255;
                            if (anchor.ambientColor.r == next.r
                                    && anchor.ambientColor.g == next.g
                                    && anchor.ambientColor.b == next.b
                                    && anchor.ambientColor.a == next.a) {
                                return false;
                            }
                            anchor.ambientColor = next;
                            return true;
                        });
            }
            y += rowH + gap;
        };
        drawAmbientChannel("sector_editor_authoring_face_ambient_r", "R:", selectedAuthoringFaceAnchor->ambientColor.r, uiState.ambientRedInput, 0);
        drawAmbientChannel("sector_editor_authoring_face_ambient_g", "G:", selectedAuthoringFaceAnchor->ambientColor.g, uiState.ambientGreenInput, 1);
        drawAmbientChannel("sector_editor_authoring_face_ambient_b", "B:", selectedAuthoringFaceAnchor->ambientColor.b, uiState.ambientBlueInput, 2);

        const auto drawTextureRow = [&](const char* id, const char* label, const std::string& textureId, TopologySectorTextureField field) {
            const float buttonW = 38.0f;
            const SectorEditorInspectorTextureRowLayout row =
                    BuildSectorEditorInspectorTextureRowLayout(y, contentW, gap, buttonW, 0.0f);
            const bool missing = !textureId.empty() && FindSectorTopologyTexture(state.topologyMap, textureId) == nullptr;
            engine::Text(ui, config, assets, row.labelRect, font, label, engine::UITextJustify::Left, config.mutedTextColor);
            engine::Text(
                    ui,
                    config,
                    assets,
                    row.valueRect,
                    font,
                    textureId.empty() ? "<none>" : textureId.c_str(),
                    engine::UITextJustify::Left,
                    missing ? config.invalidColor : config.mutedTextColor);
            if (engine::Button(ui, config, input, assets, id, row.pickerButtonRect, font, ">")) {
                if (!OpenAuthoringFaceAnchorTexturePickerById(
                            state,
                            faceAnchorId,
                            field,
                            TopologyMaterialLayer::Base)) {
                    statusText = "Authoring face texture picker unavailable: derived mapping is not current";
                }
            }
            y += row.height + gap;
        };
        const auto mappedFlatTargetForField = [this, faceAnchorId](TopologySectorTextureField field, TopologySurfaceEditTarget& outTarget) {
            if (state.authoringDerivationState != SectorEditorAuthoringDerivationState::ValidCurrent
                    || state.authoringDerivedTopologyStale
                    || !state.authoringDerivation.success) {
                return false;
            }
            for (const SectorAuthoringDerivedSectorMapping& mapping : state.authoringDerivation.mapping.sectors) {
                if (mapping.faceAnchorId != faceAnchorId) {
                    continue;
                }
                if (FindSectorTopologySector(state.topologyMap, mapping.topologySectorId) == nullptr) {
                    continue;
                }
                if (field == TopologySectorTextureField::Floor) {
                    outTarget.kind = TopologySurfaceEditTargetKind::SectorFloor;
                } else if (field == TopologySectorTextureField::Ceiling) {
                    outTarget.kind = TopologySurfaceEditTargetKind::SectorCeiling;
                } else {
                    return false;
                }
                outTarget.sectorId = mapping.topologySectorId;
                return true;
            }
            return false;
        };
        const auto drawFlatDecalControls =
                [&](const char* idPrefix, const char* label, const SectorTopologyDecalLayer& decal, TopologySectorTextureField field, int inputIndex) {
                    const float buttonW = 38.0f;
                    const float clearW = 92.0f;
                    const SectorEditorInspectorTextureRowLayout row =
                            BuildSectorEditorInspectorTextureRowLayout(y, contentW, gap, buttonW, clearW);
                    const bool missing = !decal.textureId.empty()
                            && FindSectorTopologyTexture(state.topologyMap, decal.textureId) == nullptr;
                    engine::Text(ui, config, assets, row.labelRect, font, label, engine::UITextJustify::Left, config.mutedTextColor);
                    engine::Text(
                            ui,
                            config,
                            assets,
                            row.valueRect,
                            font,
                            decal.textureId.empty() ? "<none>" : decal.textureId.c_str(),
                            engine::UITextJustify::Left,
                            missing ? config.invalidColor : config.mutedTextColor);
                    if (engine::Button(
                                ui,
                                config,
                                input,
                                assets,
                                TextFormat("%s_clear", idPrefix),
                                row.clearButtonRect,
                                font,
                                "Clear")) {
                        mutateFaceAnchor(
                                "Cleared authoring face decal",
                                [field](SectorAuthoringFaceAnchor& anchor) {
                                    SectorTopologyDecalLayer* target = nullptr;
                                    if (field == TopologySectorTextureField::Floor) {
                                        target = &anchor.floorDecal;
                                    } else if (field == TopologySectorTextureField::Ceiling) {
                                        target = &anchor.ceilingDecal;
                                    }
                                    if (target == nullptr || IsDefaultDecalLayer(*target)) {
                                        return false;
                                    }
                                    ResetDecalLayer(*target);
                                    return true;
                                });
                    }
                    if (engine::Button(
                                ui,
                                config,
                                input,
                                assets,
                                TextFormat("%s_pick", idPrefix),
                                row.pickerButtonRect,
                                font,
                                ">")) {
                        if (!OpenAuthoringFaceAnchorTexturePickerById(
                                    state,
                                    faceAnchorId,
                                    field,
                                    TopologyMaterialLayer::Decal)) {
                            statusText = "Authoring face decal picker unavailable: derived mapping is not current";
                        }
                    }
                    y += row.height + gap;

                    if (decal.textureId.empty()) {
                        return;
                    }

                    const SectorEditorInspectorNumericRowLayout opacityLayout =
                            BuildSectorEditorInspectorCompactNumericRowLayout(y, contentW, rowH);
                    const SectorEditorFloatInputResult opacityResult = DrawLabeledFloatInput(
                            ui,
                            config,
                            input,
                            assets,
                            font,
                            TextFormat("%s_opacity", idPrefix),
                            "Opacity:",
                            opacityLayout.labelRect,
                            opacityLayout.inputRect,
                            engine::UITextJustify::Left,
                            decal.opacity,
                            uiState.topologySectorDecalOpacityInputs[inputIndex],
                            0.0f,
                            1.0f,
                            3);
                    if (opacityResult.changed && opacityResult.value != decal.opacity && opacityResult.finite) {
                        mutateFaceAnchor(
                                "Updated authoring face decal opacity",
                                [field, value = opacityResult.value](SectorAuthoringFaceAnchor& anchor) {
                                    SectorTopologyDecalLayer* target = field == TopologySectorTextureField::Floor
                                            ? &anchor.floorDecal
                                            : &anchor.ceilingDecal;
                                    if (target->textureId.empty() || target->opacity == value) {
                                        return false;
                                    }
                                    target->opacity = value;
                                    return true;
                                });
                    }
                    y += rowH + gap;

                    bool emissive = decal.emissive;
                    if (engine::Checkbox(
                                ui,
                                config,
                                input,
                                assets,
                                TextFormat("%s_emissive", idPrefix),
                                Rectangle{0.0f, y, contentW, 36.0f},
                                font,
                                "Emissive",
                                emissive)) {
                        mutateFaceAnchor(
                                "Updated authoring face decal emissive",
                                [field, emissive](SectorAuthoringFaceAnchor& anchor) {
                                    SectorTopologyDecalLayer* target = field == TopologySectorTextureField::Floor
                                            ? &anchor.floorDecal
                                            : &anchor.ceilingDecal;
                                    if (target->textureId.empty() || target->emissive == emissive) {
                                        return false;
                                    }
                                    target->emissive = emissive;
                                    return true;
                                });
                    }
                    y += 36.0f + gap;

                    if (decal.emissive) {
                        const SectorEditorInspectorNumericRowLayout bloomLayout =
                                BuildSectorEditorInspectorCompactNumericRowLayout(y, contentW, rowH);
                        const SectorEditorFloatInputResult bloomResult = DrawLabeledFloatInput(
                                ui,
                                config,
                                input,
                                assets,
                                font,
                                TextFormat("%s_bloom", idPrefix),
                                "Bloom:",
                                bloomLayout.labelRect,
                                bloomLayout.inputRect,
                                engine::UITextJustify::Left,
                                decal.bloomIntensity,
                                uiState.topologySectorDecalBloomIntensityInputs[inputIndex],
                                0.0f,
                                10.0f,
                                3);
                        if (bloomResult.changed && bloomResult.value != decal.bloomIntensity && bloomResult.finite) {
                            mutateFaceAnchor(
                                    "Updated authoring face decal bloom intensity",
                                    [field, value = bloomResult.value](SectorAuthoringFaceAnchor& anchor) {
                                        SectorTopologyDecalLayer* target = field == TopologySectorTextureField::Floor
                                                ? &anchor.floorDecal
                                                : &anchor.ceilingDecal;
                                        if (target->textureId.empty() || target->bloomIntensity == value) {
                                            return false;
                                        }
                                        target->bloomIntensity = value;
                                        return true;
                                    });
                        }
                        y += rowH + gap;
                    }

                    engine::Text(ui, config, assets, Rectangle{0.0f, y, 82.0f, rowH}, font, "Tint:", engine::UITextJustify::Left, config.mutedTextColor);
                    const Rectangle swatchLocal{82.0f, y + 3.0f, 56.0f, rowH - 6.0f};
                    if (engine::Button(
                                ui,
                                config,
                                input,
                                assets,
                                TextFormat("%s_tint", idPrefix),
                                swatchLocal,
                                font,
                                "")) {
                        TopologySurfaceEditTarget target;
                        if (mappedFlatTargetForField(field, target)) {
                            OpenDecalTintModal(target);
                        } else {
                            statusText = "Authoring face decal tint unavailable: derived mapping is not current";
                        }
                    }
                    const Rectangle swatchScreen{
                            scroll.viewport.x + swatchLocal.x,
                            scroll.viewport.y - uiState.inspectorScroll.offset.y + swatchLocal.y,
                            swatchLocal.width,
                            swatchLocal.height};
                    DrawColorSwatch(config, swatchScreen, DecalTintPreviewColor(decal.tint), config.borderThickness);
                    y += rowH + gap;

                    TopologySurfaceEditTarget fitTarget;
                    if (mappedFlatTargetForField(field, fitTarget)
                            && engine::Button(
                                    ui,
                                    config,
                                    input,
                                    assets,
                                    TextFormat("%s_fit", idPrefix),
                                    Rectangle{0.0f, y, contentW, 36.0f},
                                    font,
                                    "Fit Decal")) {
                        FitSelectedDecal(fitTarget, &assets);
                    }
                    y += 36.0f + gap;
                };
        const auto drawDefaultDecalControls =
                [&](const char* idPrefix, const char* label, const SectorTopologyDecalLayer& decal, TopologySectorTextureField field, int inputIndex) {
                    const float buttonW = 38.0f;
                    const float clearW = 92.0f;
                    const SectorEditorInspectorTextureRowLayout row =
                            BuildSectorEditorInspectorTextureRowLayout(y, contentW, gap, buttonW, clearW);
                    const bool missing = !decal.textureId.empty()
                            && FindSectorTopologyTexture(state.topologyMap, decal.textureId) == nullptr;
                    engine::Text(ui, config, assets, row.labelRect, font, label, engine::UITextJustify::Left, config.mutedTextColor);
                    engine::Text(
                            ui,
                            config,
                            assets,
                            row.valueRect,
                            font,
                            decal.textureId.empty() ? "<none>" : decal.textureId.c_str(),
                            engine::UITextJustify::Left,
                            missing ? config.invalidColor : config.mutedTextColor);
                    auto defaultDecalForField = [field](SectorAuthoringFaceAnchor& anchor) -> SectorTopologyDecalLayer* {
                        if (field == TopologySectorTextureField::DefaultWall) {
                            return &anchor.defaultWall.decal;
                        }
                        if (field == TopologySectorTextureField::DefaultLower) {
                            return &anchor.defaultLower.decal;
                        }
                        if (field == TopologySectorTextureField::DefaultUpper) {
                            return &anchor.defaultUpper.decal;
                        }
                        return nullptr;
                    };
                    if (engine::Button(
                                ui,
                                config,
                                input,
                                assets,
                                TextFormat("%s_clear", idPrefix),
                                row.clearButtonRect,
                                font,
                                "Clear")) {
                        mutateFaceAnchor(
                                "Cleared authoring default decal",
                                [defaultDecalForField](SectorAuthoringFaceAnchor& anchor) {
                                    SectorTopologyDecalLayer* target = defaultDecalForField(anchor);
                                    if (target == nullptr || IsDefaultDecalLayer(*target)) {
                                        return false;
                                    }
                                    ResetDecalLayer(*target);
                                    return true;
                                });
                    }
                    if (engine::Button(
                                ui,
                                config,
                                input,
                                assets,
                                TextFormat("%s_pick", idPrefix),
                                row.pickerButtonRect,
                                font,
                                ">")) {
                        if (!OpenAuthoringFaceAnchorTexturePickerById(
                                    state,
                                    faceAnchorId,
                                    field,
                                    TopologyMaterialLayer::Decal)) {
                            statusText = "Authoring default decal picker unavailable: derived mapping is not current";
                        }
                    }
                    y += row.height + gap;

                    if (decal.textureId.empty()) {
                        return;
                    }

                    const SectorEditorInspectorNumericRowLayout opacityLayout =
                            BuildSectorEditorInspectorCompactNumericRowLayout(y, contentW, rowH);
                    const SectorEditorFloatInputResult opacityResult = DrawLabeledFloatInput(
                            ui,
                            config,
                            input,
                            assets,
                            font,
                            TextFormat("%s_opacity", idPrefix),
                            "Opacity:",
                            opacityLayout.labelRect,
                            opacityLayout.inputRect,
                            engine::UITextJustify::Left,
                            decal.opacity,
                            uiState.topologySectorDecalOpacityInputs[inputIndex],
                            0.0f,
                            1.0f,
                            3);
                    if (opacityResult.changed && opacityResult.value != decal.opacity && opacityResult.finite) {
                        mutateFaceAnchor(
                                "Updated authoring default decal opacity",
                                [defaultDecalForField, value = opacityResult.value](SectorAuthoringFaceAnchor& anchor) {
                                    SectorTopologyDecalLayer* target = defaultDecalForField(anchor);
                                    if (target == nullptr || target->textureId.empty() || target->opacity == value) {
                                        return false;
                                    }
                                    target->opacity = value;
                                    return true;
                                });
                    }
                    y += rowH + gap;

                    bool emissive = decal.emissive;
                    if (engine::Checkbox(
                                ui,
                                config,
                                input,
                                assets,
                                TextFormat("%s_emissive", idPrefix),
                                Rectangle{0.0f, y, contentW, 36.0f},
                                font,
                                "Emissive",
                                emissive)) {
                        mutateFaceAnchor(
                                "Updated authoring default decal emissive",
                                [defaultDecalForField, emissive](SectorAuthoringFaceAnchor& anchor) {
                                    SectorTopologyDecalLayer* target = defaultDecalForField(anchor);
                                    if (target == nullptr || target->textureId.empty() || target->emissive == emissive) {
                                        return false;
                                    }
                                    target->emissive = emissive;
                                    return true;
                                });
                    }
                    y += 36.0f + gap;

                    if (decal.emissive) {
                        const SectorEditorInspectorNumericRowLayout bloomLayout =
                                BuildSectorEditorInspectorCompactNumericRowLayout(y, contentW, rowH);
                        const SectorEditorFloatInputResult bloomResult = DrawLabeledFloatInput(
                                ui,
                                config,
                                input,
                                assets,
                                font,
                                TextFormat("%s_bloom", idPrefix),
                                "Bloom:",
                                bloomLayout.labelRect,
                                bloomLayout.inputRect,
                                engine::UITextJustify::Left,
                                decal.bloomIntensity,
                                uiState.topologySectorDecalBloomIntensityInputs[inputIndex],
                                0.0f,
                                10.0f,
                                3);
                        if (bloomResult.changed && bloomResult.value != decal.bloomIntensity && bloomResult.finite) {
                            mutateFaceAnchor(
                                    "Updated authoring default decal bloom intensity",
                                    [defaultDecalForField, value = bloomResult.value](SectorAuthoringFaceAnchor& anchor) {
                                        SectorTopologyDecalLayer* target = defaultDecalForField(anchor);
                                        if (target == nullptr || target->textureId.empty() || target->bloomIntensity == value) {
                                            return false;
                                        }
                                        target->bloomIntensity = value;
                                        return true;
                                    });
                        }
                        y += rowH + gap;
                    }
                };

        engine::Separator(config, Rectangle{scroll.viewport.x, scroll.viewport.y - uiState.inspectorScroll.offset.y + y, contentW, 12.0f});
        y += 18.0f;
        engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 30.0f}, font, "Materials", engine::UITextJustify::Left, config.textColor);
        y += 30.0f;
        drawTextureRow("sector_editor_authoring_face_pick_floor", "Floor:", selectedAuthoringFaceAnchor->floorTextureId, TopologySectorTextureField::Floor);
        drawTextureRow("sector_editor_authoring_face_pick_ceiling", "Ceiling:", selectedAuthoringFaceAnchor->ceilingTextureId, TopologySectorTextureField::Ceiling);
        drawTextureRow("sector_editor_authoring_face_pick_default_wall", "Wall:", selectedAuthoringFaceAnchor->defaultWall.textureId, TopologySectorTextureField::DefaultWall);
        drawTextureRow("sector_editor_authoring_face_pick_default_lower", "Lower:", selectedAuthoringFaceAnchor->defaultLower.textureId, TopologySectorTextureField::DefaultLower);
        drawTextureRow("sector_editor_authoring_face_pick_default_upper", "Upper:", selectedAuthoringFaceAnchor->defaultUpper.textureId, TopologySectorTextureField::DefaultUpper);
        drawFlatDecalControls("sector_editor_authoring_face_floor_decal", "Floor Decal:", selectedAuthoringFaceAnchor->floorDecal, TopologySectorTextureField::Floor, 0);
        drawFlatDecalControls("sector_editor_authoring_face_ceiling_decal", "Ceiling Decal:", selectedAuthoringFaceAnchor->ceilingDecal, TopologySectorTextureField::Ceiling, 1);
        drawDefaultDecalControls("sector_editor_authoring_face_default_wall_decal", "Wall Decal:", selectedAuthoringFaceAnchor->defaultWall.decal, TopologySectorTextureField::DefaultWall, 0);
        drawDefaultDecalControls("sector_editor_authoring_face_default_lower_decal", "Lower Decal:", selectedAuthoringFaceAnchor->defaultLower.decal, TopologySectorTextureField::DefaultLower, 1);
        drawDefaultDecalControls("sector_editor_authoring_face_default_upper_decal", "Upper Decal:", selectedAuthoringFaceAnchor->defaultUpper.decal, TopologySectorTextureField::DefaultUpper, 0);

        engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
        engine::EndPanel(ui, config, panel);
        return;
    }

    if (selectedAuthoringVertex != nullptr) {
        engine::Text(
                ui,
                config,
                assets,
                Rectangle{0.0f, y, contentW, 34.0f},
                font,
                TextFormat("Authoring Vertex: %d", selectedAuthoringVertex->id),
                engine::UITextJustify::Left,
                config.textColor);
        y += 38.0f;
        engine::Text(
                ui,
                config,
                assets,
                Rectangle{0.0f, y, contentW, 30.0f},
                font,
                TextFormat(
                        "%.2f, %.2f",
                        SectorCoordToVisibleAuthoring(selectedAuthoringVertex->x),
                        SectorCoordToVisibleAuthoring(selectedAuthoringVertex->y)),
                engine::UITextJustify::Left,
                config.mutedTextColor);
        engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
        engine::EndPanel(ui, config, panel);
        return;
    }

    engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 42.0f}, font, "Selected: none", engine::UITextJustify::Left, config.mutedTextColor);

    // TODO: Add undo/redo.
    // TODO: Add validation issue highlighting.

    engine::EndScrollArea(ui, config, input, scroll, uiState.inspectorScroll);
    engine::EndPanel(ui, config, panel);
}

bool SectorEditor::DrawTopologySideDefInspector(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::UIScrollAreaResult scroll,
        float contentW,
        float rowH,
        float gap)
{
    const SectorTopologyLineDef* lineDef = SelectedTopologyLineDef();
    if (lineDef == nullptr) {
        return false;
    }

    const SectorTopologyVertex* start = nullptr;
    const SectorTopologyVertex* end = nullptr;
    const bool hasEndpoints = GetSectorTopologyLineVertices(state.topologyMap, *lineDef, start, end);

    float y = 0.0f;
    SectorTopologySideDef* sideDef = SelectedTopologySideDef();
    if (sideDef != nullptr) {
        state.selectedTopologyWallPart = ValidTopologyWallPartForSideDef(
                state.topologyMap,
                sideDef,
                state.selectedTopologyWallPart);
    }
    engine::Text(
            ui,
            config,
            assets,
            Rectangle{0.0f, y, contentW, 34.0f},
            font,
            sideDef != nullptr
                    ? TextFormat(
                            "Topology %s SideDef: %d",
                            SectorTopologySideKindName(sideDef->side),
                            sideDef->id)
                    : TextFormat("Topology LineDef: %d", lineDef->id),
            engine::UITextJustify::Left,
            config.textColor);
    y += 38.0f;

    if (hasEndpoints) {
        const Vector2 from = SectorTopologyVertexToMap(*start);
        const Vector2 to = SectorTopologyVertexToMap(*end);
        engine::Text(
                ui,
                config,
                assets,
                Rectangle{0.0f, y, contentW, 30.0f},
                font,
                TextFormat("Line %d  From %.2f, %.2f  To %.2f, %.2f", lineDef->id, from.x, from.y, to.x, to.y),
                engine::UITextJustify::Left,
                config.mutedTextColor);
        y += 32.0f;
    } else {
        engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 30.0f}, font, "Line endpoints are invalid", engine::UITextJustify::Left, config.invalidColor);
        y += 32.0f;
    }

    engine::Text(
            ui,
            config,
            assets,
            Rectangle{0.0f, y, contentW, 38.0f},
            font,
            "Split Linedef retired; derivation auto-splits authoring lines.",
            engine::UITextJustify::Left,
            config.mutedTextColor);
    y += 38.0f + gap;

    engine::Text(
            ui,
            config,
            assets,
            Rectangle{0.0f, y, contentW, 38.0f},
            font,
            "Split At Point retired; move or redraw authoring vertices.",
            engine::UITextJustify::Left,
            config.mutedTextColor);
    y += 38.0f + gap;

    if (sideDef == nullptr) {
        const int preferredSideDefId = state.selectedTopologySideKind == SectorTopologySideKind::Front
                ? lineDef->frontSideDefId
                : lineDef->backSideDefId;
        const SectorTopologySideDef* preferredSideDef = FindSectorTopologySideDef(
                state.topologyMap,
                preferredSideDefId);
        const SectorTopologySideDef* opposite = preferredSideDef != nullptr
                ? FindOppositeSectorTopologySideDef(state.topologyMap, preferredSideDef->id)
                : nullptr;
        if (preferredSideDef != nullptr && opposite != nullptr
                && preferredSideDef->sectorId != opposite->sectorId) {
            engine::Text(
                    ui,
                    config,
                    assets,
                    Rectangle{0.0f, y, contentW, 38.0f},
                    font,
                    "Join Sectors retired; remove authoring boundaries instead.",
                    engine::UITextJustify::Left,
                    config.mutedTextColor);
            y += 38.0f + gap;
        }

        engine::Text(
                ui,
                config,
                assets,
                Rectangle{0.0f, y, contentW, 64.0f},
                font,
                preferredSideDef == nullptr
                        ? "Line-only selection: this linedef has no sidedef to edit."
                        : "Select a sidedef to edit wall settings.",
                engine::UITextJustify::Left,
                config.mutedTextColor);
        return true;
    }

    engine::Text(
            ui,
            config,
            assets,
            Rectangle{0.0f, y, contentW, 30.0f},
            font,
            TextFormat("Sector %d  Line %d", sideDef->sectorId, sideDef->lineDefId),
            engine::UITextJustify::Left,
            config.mutedTextColor);
    y += 34.0f;

    const SectorTopologySideDef* opposite = FindOppositeSectorTopologySideDef(
            state.topologyMap,
            sideDef->id);
    const bool middleEligible = IsTopologyMiddleEligible(state.topologyMap, sideDef);
    if (opposite != nullptr) {
        if (opposite->sectorId != sideDef->sectorId) {
            engine::Text(
                    ui,
                    config,
                    assets,
                    Rectangle{0.0f, y, contentW, 38.0f},
                    font,
                    "Join Sectors retired; remove authoring boundaries instead.",
                    engine::UITextJustify::Left,
                    config.mutedTextColor);
            y += 38.0f + gap;
        } else {
            engine::Text(
                    ui,
                    config,
                    assets,
                    Rectangle{0.0f, y, contentW, 34.0f},
                    font,
                    "Join Sectors unavailable: both sides already belong to the same sector.",
                    engine::UITextJustify::Left,
                    config.mutedTextColor);
            y += 38.0f;
        }

        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_topology_switch_opposite_side",
                    Rectangle{0.0f, y, contentW, 38.0f},
                    font,
                    TextFormat("Switch to opposite side (%s)", SectorTopologySideKindName(opposite->side)))) {
            const int oppositeId = opposite->id;
            SelectTopologySideDef(
                    oppositeId,
                    ValidTopologyWallPartForSideDef(
                            state.topologyMap,
                            FindSectorTopologySideDef(state.topologyMap, oppositeId),
                            state.selectedTopologyWallPart));
            statusText = TextFormat("Selected opposite topology sidedef %d", oppositeId);
            return true;
        }
        y += 38.0f + gap;
    } else {
        engine::Text(
                ui,
                config,
                assets,
                Rectangle{0.0f, y, contentW, 30.0f},
                font,
                "Join Sectors unavailable: opposite side is missing.",
                engine::UITextJustify::Left,
                config.mutedTextColor);
        y += 34.0f;
    }

    if (lineDef->frontSideDefId != -1 && lineDef->backSideDefId != -1) {
        bool blocksPlayer = lineDef->flags.blocksPlayer;
        if (engine::Checkbox(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_topology_linedef_blocks_player",
                    Rectangle{0.0f, y, contentW, 36.0f},
                    font,
                    "Blocks Player",
                    blocksPlayer)) {
            SetLineDefBlocksPlayer(lineDef->id, blocksPlayer);
            return true;
        }
        y += 36.0f + gap;
    }

    auto drawTextureRow = [&](const char* id, const char* label, const std::string& textureId, TopologyWallPart wallPart, TopologyMaterialLayer layer) {
        const float buttonW = 38.0f;
        const float labelColumnW = 74.0f;
        const Rectangle row{0.0f, y, contentW, 36.0f};
        const bool missing = !textureId.empty() && FindSectorTopologyTexture(state.topologyMap, textureId) == nullptr;
        engine::Text(ui, config, assets, Rectangle{row.x, row.y, labelColumnW, row.height}, font, label, engine::UITextJustify::Left, config.mutedTextColor);
        engine::Text(
                ui,
                config,
                assets,
                Rectangle{row.x + labelColumnW, row.y, row.width - labelColumnW - buttonW - gap, row.height},
                font,
                textureId.empty() ? "<none>" : textureId.c_str(),
                engine::UITextJustify::Left,
                missing ? config.invalidColor : config.mutedTextColor);
        if (engine::Button(ui, config, input, assets, id, Rectangle{row.x + row.width - buttonW, row.y, buttonW, row.height}, font, ">")) {
            OpenTopologySideDefTexturePicker(sideDef->id, wallPart, layer);
        }
        y += row.height + gap;
    };

    y += 4.0f;
    const int partCount = middleEligible ? 4 : 3;
    const float partButtonW = (contentW - gap * static_cast<float>(partCount - 1)) / static_cast<float>(partCount);
    const TopologyWallPart parts[] = {
            TopologyWallPart::Wall,
            TopologyWallPart::Lower,
            TopologyWallPart::Upper,
            TopologyWallPart::Middle};
    for (int i = 0; i < partCount; ++i) {
        const TopologyWallPart part = parts[i];
        if (engine::ToolButton(
                    ui,
                    config,
                    input,
                    assets,
                    TextFormat("sector_editor_topology_sidedef_part_%d", i),
                    Rectangle{static_cast<float>(i) * (partButtonW + gap), y, partButtonW, 38.0f},
                    font,
                    TopologyWallPartName(part),
                    state.selectedTopologyWallPart == part)) {
            state.selectedTopologyWallPart = part;
            for (engine::UIFloatInputState& inputState : uiState.topologySideDefUvInputs) {
                inputState = engine::UIFloatInputState{};
            }
            statusText = TextFormat("Editing topology %s UV", TopologyWallPartStatusName(part));
        }
    }
    y += 38.0f + gap;

    const bool selectedMiddle = state.selectedTopologyWallPart == TopologyWallPart::Middle;
    if (selectedMiddle && state.activeTopologyMaterialLayer != TopologyMaterialLayer::Base) {
        state.activeTopologyMaterialLayer = TopologyMaterialLayer::Base;
        for (engine::UIFloatInputState& inputState : uiState.topologySideDefUvInputs) {
            inputState = engine::UIFloatInputState{};
        }
        uiState.topologySideDefDecalOpacityInput = engine::UIFloatInputState{};
        uiState.topologySideDefDecalBloomIntensityInput = engine::UIFloatInputState{};
    }
    const TopologySurfaceEditTarget selectedMaterialTarget{
            TopologyWallPartEditTargetKind(state.selectedTopologyWallPart),
            sideDef->sectorId,
            sideDef->lineDefId,
            sideDef->id,
            sideDef->side};
    if (!selectedMiddle) {
        const float layerLabelW = 74.0f;
        const float layerButtonW = (contentW - layerLabelW - gap) * 0.5f;
        engine::Text(ui, config, assets, Rectangle{0.0f, y, layerLabelW, 36.0f}, font, "Layer:", engine::UITextJustify::Left, config.mutedTextColor);
        if (engine::ToolButton(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_topology_sidedef_layer_base",
                    Rectangle{layerLabelW, y, layerButtonW, 36.0f},
                    font,
                    "Base",
                    state.activeTopologyMaterialLayer == TopologyMaterialLayer::Base)) {
            state.activeTopologyMaterialLayer = TopologyMaterialLayer::Base;
            for (engine::UIFloatInputState& inputState : uiState.topologySideDefUvInputs) {
                inputState = engine::UIFloatInputState{};
            }
            uiState.topologySideDefDecalOpacityInput = engine::UIFloatInputState{};
            uiState.topologySideDefDecalBloomIntensityInput = engine::UIFloatInputState{};
        }
        if (engine::ToolButton(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_topology_sidedef_layer_decal",
                    Rectangle{layerLabelW + layerButtonW + gap, y, layerButtonW, 36.0f},
                    font,
                    "Decal",
                    state.activeTopologyMaterialLayer == TopologyMaterialLayer::Decal)) {
            state.activeTopologyMaterialLayer = TopologyMaterialLayer::Decal;
            for (engine::UIFloatInputState& inputState : uiState.topologySideDefUvInputs) {
                inputState = engine::UIFloatInputState{};
            }
            uiState.topologySideDefDecalOpacityInput = engine::UIFloatInputState{};
            uiState.topologySideDefDecalBloomIntensityInput = engine::UIFloatInputState{};
        }
        y += 36.0f + gap;
    }

    SectorTopologyWallPartSettings& selectedPart = TopologyWallPartSettingsFor(*sideDef, state.selectedTopologyWallPart);
    const TopologyMaterialLayer layer = selectedMiddle
            ? TopologyMaterialLayer::Base
            : state.activeTopologyMaterialLayer;
    drawTextureRow(
            "sector_editor_topology_sidedef_pick_selected_part",
            "Texture:",
            layer == TopologyMaterialLayer::Decal ? selectedPart.decal.textureId : selectedPart.textureId,
            state.selectedTopologyWallPart,
            layer);

    if (selectedMiddle && selectedPart.textureId.empty()) {
        engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 32.0f}, font, "No middle texture assigned", engine::UITextJustify::Left, config.mutedTextColor);
        y += 32.0f + gap;
        if (!IsDefaultWallPartSettings(selectedPart)) {
            if (engine::Button(
                        ui,
                        config,
                        input,
                        assets,
                        "sector_editor_topology_sidedef_clear_middle_empty",
                        Rectangle{0.0f, y, contentW, 38.0f},
                        font,
                        "Clear Middle")) {
                ClearMiddleTexture(selectedMaterialTarget, &assets);
            }
            y += 38.0f + gap;
        }
        return true;
    }

    if (layer == TopologyMaterialLayer::Decal && selectedPart.decal.textureId.empty()) {
        engine::Text(ui, config, assets, Rectangle{0.0f, y, contentW, 32.0f}, font, "No decal assigned", engine::UITextJustify::Left, config.mutedTextColor);
        y += 32.0f + gap;
        return true;
    }

    if (layer == TopologyMaterialLayer::Base && !selectedMiddle) {
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_topology_sidedef_copy_material",
                    Rectangle{0.0f, y, contentW, 38.0f},
                    font,
                    TextFormat("Copy %s Material", TopologyWallPartName(state.selectedTopologyWallPart)))) {
            CopyTopologyMaterial(selectedMaterialTarget);
        }
        y += 38.0f + gap;

        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_topology_sidedef_paste_material",
                    Rectangle{0.0f, y, contentW, 38.0f},
                    font,
                    TextFormat("Paste %s Material", TopologyWallPartName(state.selectedTopologyWallPart)))) {
            PasteTopologyMaterial(selectedMaterialTarget, assets);
        }
        y += 38.0f + gap;
    }

    SectorTopologyUvSettings& selectedUv = layer == TopologyMaterialLayer::Decal ? selectedPart.decal.uv : selectedPart.uv;
    const float uvColumnW = (contentW - gap) * 0.5f;
    const float uvBlockH = 62.0f;
    auto drawUvInput = [&](int stateIndex, const char* id, const char* label, float value, float minValue, float maxValue, Rectangle bounds, auto applyValue) {
        const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
                ui,
                config,
                input,
                assets,
                font,
                id,
                label,
                Rectangle{bounds.x, bounds.y, bounds.width, 26.0f},
                Rectangle{bounds.x, bounds.y + 26.0f, bounds.width, 36.0f},
                engine::UITextJustify::Left,
                value,
                uiState.topologySideDefUvInputs[stateIndex],
                minValue,
                maxValue,
                3);
        if (result.changed && result.value != value) {
            if (!result.finite) {
                statusText = "Invalid topology sidedef UV value";
                return;
            }
            applyValue(result.value);
            const char* status = TextFormat(
                    "Updated topology sidedef %d %s %s UV",
                    sideDef->id,
                    TopologyWallPartStatusName(state.selectedTopologyWallPart),
                    TopologyMaterialLayerStatusName(layer));
            if (selectedMiddle) {
                FinishTopologyMaterialMutation(status, &assets);
            } else {
                state.topologyRenderWarning.clear();
                MarkTopologyDocumentEdited(status);
            }
        }
    };

    drawUvInput(
            0,
            "sector_editor_topology_sidedef_uv_scale_u",
            "Scale U",
            selectedUv.scale.x,
            TopologyUvScaleMin,
            TopologyUvScaleMax,
            Rectangle{0.0f, y, uvColumnW, uvBlockH},
            [&](float value) { selectedUv.scale.x = value; });
    drawUvInput(
            1,
            "sector_editor_topology_sidedef_uv_scale_v",
            "Scale V",
            selectedUv.scale.y,
            TopologyUvScaleMin,
            TopologyUvScaleMax,
            Rectangle{uvColumnW + gap, y, uvColumnW, uvBlockH},
            [&](float value) { selectedUv.scale.y = value; });
    y += uvBlockH + gap;
    drawUvInput(
            2,
            "sector_editor_topology_sidedef_uv_offset_u",
            "Offset U",
            selectedUv.offset.x,
            -1024.0f,
            1024.0f,
            Rectangle{0.0f, y, uvColumnW, uvBlockH},
            [&](float value) { selectedUv.offset.x = value; });
    drawUvInput(
            3,
            "sector_editor_topology_sidedef_uv_offset_v",
            "Offset V",
            selectedUv.offset.y,
            -1024.0f,
            1024.0f,
            Rectangle{uvColumnW + gap, y, uvColumnW, uvBlockH},
            [&](float value) { selectedUv.offset.y = value; });
    y += uvBlockH + gap;

    if (layer == TopologyMaterialLayer::Decal) {
        const SectorEditorFloatInputResult opacityResult = DrawLabeledFloatInput(
                ui,
                config,
                input,
                assets,
                font,
                "sector_editor_topology_sidedef_decal_opacity",
                "Opacity:",
                Rectangle{0.0f, y, 82.0f, rowH},
                Rectangle{82.0f, y, contentW - 82.0f, rowH},
                engine::UITextJustify::Left,
                selectedPart.decal.opacity,
                uiState.topologySideDefDecalOpacityInput,
                0.0f,
                1.0f,
                3);
        if (opacityResult.changed && opacityResult.value != selectedPart.decal.opacity && opacityResult.finite) {
            ApplySurfaceDecalOpacity(selectedMaterialTarget, opacityResult.value, &assets);
        }
        y += rowH + gap;

        bool emissive = selectedPart.decal.emissive;
        if (engine::Checkbox(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_topology_sidedef_decal_emissive",
                    Rectangle{0.0f, y, contentW, 36.0f},
                    font,
                    "Emissive",
                    emissive)) {
            ApplySurfaceDecalEmissive(selectedMaterialTarget, emissive, &assets);
        }
        y += 36.0f + gap;

        if (selectedPart.decal.emissive) {
            const SectorEditorFloatInputResult bloomResult = DrawLabeledFloatInput(
                    ui,
                    config,
                    input,
                    assets,
                    font,
                    "sector_editor_topology_sidedef_decal_bloom_intensity",
                    "Bloom:",
                    Rectangle{0.0f, y, 82.0f, rowH},
                    Rectangle{82.0f, y, contentW - 82.0f, rowH},
                    engine::UITextJustify::Left,
                    selectedPart.decal.bloomIntensity,
                    uiState.topologySideDefDecalBloomIntensityInput,
                    0.0f,
                    10.0f,
                    3);
            if (bloomResult.changed && bloomResult.value != selectedPart.decal.bloomIntensity) {
                ApplySurfaceDecalBloomIntensity(selectedMaterialTarget, bloomResult.value, &assets);
            }
            y += rowH + gap;
        }

        engine::Text(ui, config, assets, Rectangle{0.0f, y, 82.0f, rowH}, font, "Tint:", engine::UITextJustify::Left, config.mutedTextColor);
        const Rectangle swatchLocal{82.0f, y + 3.0f, 56.0f, rowH - 6.0f};
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_topology_sidedef_decal_tint",
                    swatchLocal,
                    font,
                    "")) {
            OpenDecalTintModal(selectedMaterialTarget);
        }
        const Rectangle swatchScreen{
                scroll.viewport.x + swatchLocal.x,
                scroll.viewport.y - uiState.inspectorScroll.offset.y + swatchLocal.y,
                swatchLocal.width,
                swatchLocal.height};
        DrawColorSwatch(config, swatchScreen, DecalTintPreviewColor(selectedPart.decal.tint), config.borderThickness);
        y += rowH + gap;

        const float decalButtonW = (contentW - gap) * 0.5f;
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_topology_sidedef_fit_decal",
                    Rectangle{0.0f, y, decalButtonW, 38.0f},
                    font,
                    "Fit Decal")) {
            FitSelectedDecal(selectedMaterialTarget, &assets);
        }
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_topology_sidedef_clear_decal",
                    Rectangle{decalButtonW + gap, y, decalButtonW, 38.0f},
                    font,
                    "Clear Decal")) {
            ClearSurfaceDecal(selectedMaterialTarget, &assets);
        }
        y += 38.0f + gap;
    }

    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_topology_sidedef_reset_uv",
                Rectangle{0.0f, y, contentW, 38.0f},
                font,
                TextFormat("Reset %s UV", TopologyWallPartName(state.selectedTopologyWallPart)))) {
        ResetTopologyUv(selectedUv);
        for (engine::UIFloatInputState& inputState : uiState.topologySideDefUvInputs) {
            inputState = engine::UIFloatInputState{};
        }
        if (selectedMiddle) {
            FinishTopologyMaterialMutation("Reset middle UV.", &assets);
        } else {
            state.topologyRenderWarning.clear();
            MarkTopologyDocumentEdited(TextFormat(
                    "Reset topology sidedef %d %s %s UV",
                    sideDef->id,
                    TopologyWallPartStatusName(state.selectedTopologyWallPart),
                    TopologyMaterialLayerStatusName(layer)));
        }
    }
    y += 38.0f + gap;

    const float fitButtonW = (contentW - gap * 2.0f) / 3.0f;
    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_topology_sidedef_fit_width",
                Rectangle{0.0f, y, fitButtonW, 34.0f},
                font,
                "Fit Width")) {
        FitSelectedWallMaterial(selectedMaterialTarget, TopologyUvFitMode::Width, &assets, layer);
    }
    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_topology_sidedef_fit_height",
                Rectangle{fitButtonW + gap, y, fitButtonW, 34.0f},
                font,
                "Fit Height")) {
        FitSelectedWallMaterial(selectedMaterialTarget, TopologyUvFitMode::Height, &assets, layer);
    }
    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_topology_sidedef_fit_both",
                Rectangle{(fitButtonW + gap) * 2.0f, y, fitButtonW, 34.0f},
                font,
                "Fit Both")) {
        FitSelectedWallMaterial(selectedMaterialTarget, TopologyUvFitMode::Both, &assets, layer);
    }
    y += 34.0f + gap;

    if (selectedMiddle) {
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_topology_sidedef_clear_middle",
                    Rectangle{0.0f, y, contentW, 38.0f},
                    font,
                    "Clear Middle")) {
            ClearMiddleTexture(selectedMaterialTarget, &assets);
        }
        return true;
    }

    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_topology_sidedef_align_vertical",
                Rectangle{0.0f, y, contentW, 34.0f},
                font,
                "Align Vertical")) {
        AlignSelectedWallMaterialVertical(selectedMaterialTarget, &assets, layer);
    }
    y += 34.0f + gap;

    const float alignButtonW = (contentW - gap) * 0.5f;
    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_topology_sidedef_align_u_prev",
                Rectangle{0.0f, y, alignButtonW, 34.0f},
                font,
                "Align U Prev")) {
        AlignSelectedWallMaterialU(selectedMaterialTarget, TopologyUAlignDirection::Previous, &assets, layer);
    }
    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_topology_sidedef_align_u_next",
                Rectangle{alignButtonW + gap, y, alignButtonW, 34.0f},
                font,
                "Align U Next")) {
        AlignSelectedWallMaterialU(selectedMaterialTarget, TopologyUAlignDirection::Next, &assets, layer);
    }

    return true;
}

void SectorEditor::DrawAddMapTextureModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    const SectorEditorAddTextureModalCallbacks callbacks{
            [this, &assets]() { CloseAddMapTextureModal(assets); },
            [this, &assets]() { return AddSelectedMapTexture(assets); },
            [this](int pathIndex) { SelectAddMapTexturePath(pathIndex); },
            [this, &assets]() { RefreshAddMapTexturePreview(assets); },
            [this](std::string& error) { return ValidateAddMapTextureId(error); }
    };
    game::DrawAddMapTextureModal(ui, config, input, assets, font, state.addMapTexture, callbacks);
}

void SectorEditor::DrawTexturePickerModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    const SectorEditorTexturePickerCallbacks callbacks{
            [this]() { state.texturePicker = TexturePickerState{}; },
            [this, &assets]() { ApplyTexturePickerSelection(assets); },
            [this]() { return CurrentTextureForPickerTarget(); },
            [this](const std::string& textureId) { return EditorTextureHandleForId(textureId); }
    };
    game::DrawTexturePickerModal(ui, config, input, assets, font, state.texturePicker, state.topologyMap, callbacks);
}

void SectorEditor::DrawSaveLevelModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    SaveLevelModalState& modalState = state.saveLevelModal;
    if (!modalState.open) {
        return;
    }

    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [this](engine::InputEvent& event) {
                if (event.key.key == KEY_ESCAPE) {
                    state.saveLevelModal = SaveLevelModalState{};
                    engine::ConsumeEvent(event);
                } else if (event.key.key == KEY_ENTER || event.key.key == KEY_KP_ENTER) {
                    SaveLevelFromModal();
                    engine::ConsumeEvent(event);
                }
            }
    );
    if (!modalState.open) {
        return;
    }

    DrawRectangle(0, 0, static_cast<int>(EditorWidth), static_cast<int>(EditorHeight), Color{0, 0, 0, 135});
    const Rectangle modal{
            (EditorWidth - 660.0f) * 0.5f,
            (EditorHeight - 300.0f) * 0.5f,
            660.0f,
            300.0f
    };
    DrawRectangleRec(modal, Color{20, 24, 32, 245});
    DrawRectangleLinesEx(modal, config.borderThickness, config.borderColor);

    engine::Text(config, assets, Rectangle{modal.x + 24.0f, modal.y + 20.0f, modal.width - 48.0f, 40.0f}, font, "Save Level");
    engine::Text(config, assets, Rectangle{modal.x + 24.0f, modal.y + 82.0f, 100.0f, 42.0f}, font, "Name:", engine::UITextJustify::Left, config.mutedTextColor);
    const engine::UITextInputResult inputResult = engine::TextInput(
            ui,
            config,
            input,
            assets,
            "sector_editor_save_level_name",
            Rectangle{modal.x + 126.0f, modal.y + 80.0f, modal.width - 150.0f, 42.0f},
            font,
            modalState.nameBuffer,
            sizeof(modalState.nameBuffer),
            0,
            sizeof(modalState.nameBuffer) - 1
    );
    if (inputResult.changed) {
        modalState.errorMessage.clear();
    }

    if (!modalState.errorMessage.empty()) {
        engine::Text(
                config,
                assets,
                Rectangle{modal.x + 24.0f, modal.y + 140.0f, modal.width - 48.0f, 48.0f},
                font,
                modalState.errorMessage.c_str(),
                engine::UITextJustify::Left,
                config.invalidColor
        );
    }

    const float buttonY = modal.y + modal.height - 66.0f;
    const float buttonW = 150.0f;
    if (engine::Button(ui, config, input, assets, "sector_editor_save_level_confirm", Rectangle{modal.x + modal.width - buttonW * 2.0f - 36.0f, buttonY, buttonW, 44.0f}, font, "Save")) {
        SaveLevelFromModal();
    }
    if (engine::Button(ui, config, input, assets, "sector_editor_save_level_cancel", Rectangle{modal.x + modal.width - buttonW - 24.0f, buttonY, buttonW, 44.0f}, font, "Cancel")) {
        state.saveLevelModal = SaveLevelModalState{};
    }

    input.ForEachEvent(engine::InputEventType::Any, true, [](engine::InputEvent& event) {
        engine::ConsumeEvent(event);
    });
}

void SectorEditor::DrawLoadLevelModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    LoadLevelModalState& modalState = state.loadLevelModal;
    if (!modalState.open) {
        return;
    }

    const auto requestLoad = [this, &assets]() {
        LoadLevelModalState& loadState = state.loadLevelModal;
        if (loadState.selectedIndex < 0
                || loadState.selectedIndex >= static_cast<int>(loadState.levels.size())) {
            loadState.errorMessage = "Select a level to load";
            return;
        }
        const LevelListEntry selected = loadState.levels[static_cast<size_t>(loadState.selectedIndex)];
        if (state.topologyDocumentDirty) {
            OpenConfirmation(
                    "Load Level",
                    "Discard unsaved changes and load selected level?",
                    [this, &assets, selected]() { LoadLevel(assets, selected.name, selected.jsonAssetPath); }
            );
        } else {
            LoadLevel(assets, selected.name, selected.jsonAssetPath);
        }
    };

    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [this, &requestLoad](engine::InputEvent& event) {
                if (event.key.key == KEY_ESCAPE) {
                    state.loadLevelModal = LoadLevelModalState{};
                    engine::ConsumeEvent(event);
                } else if (event.key.key == KEY_ENTER || event.key.key == KEY_KP_ENTER) {
                    requestLoad();
                    engine::ConsumeEvent(event);
                }
            }
    );
    if (!modalState.open) {
        return;
    }

    DrawRectangle(0, 0, static_cast<int>(EditorWidth), static_cast<int>(EditorHeight), Color{0, 0, 0, 135});
    const Rectangle modal{
            (EditorWidth - 760.0f) * 0.5f,
            (EditorHeight - 660.0f) * 0.5f,
            760.0f,
            660.0f
    };
    DrawRectangleRec(modal, Color{20, 24, 32, 245});
    DrawRectangleLinesEx(modal, config.borderThickness, config.borderColor);
    engine::Text(config, assets, Rectangle{modal.x + 24.0f, modal.y + 20.0f, modal.width - 48.0f, 40.0f}, font, "Load Level");

    const Rectangle listBounds{modal.x + 24.0f, modal.y + 74.0f, modal.width - 48.0f, 450.0f};
    const Vector2 contentSize{
            listBounds.width,
            std::max(listBounds.height, config.listItemHeight * static_cast<float>(modalState.optionLabels.size()))
    };
    engine::UIScrollAreaResult scroll = engine::BeginScrollArea(
            ui,
            config,
            input,
            "sector_editor_load_level_scroll",
            listBounds,
            contentSize,
            modalState.scroll
    );
    if (!modalState.optionLabels.empty()) {
        engine::List(
                ui,
                config,
                input,
                assets,
                "sector_editor_load_level_list",
                Rectangle{0.0f, 0.0f, listBounds.width - (scroll.scrollY ? config.scrollbarSize : 0.0f), contentSize.y},
                font,
                modalState.optionLabels.data(),
                modalState.optionLabels.size(),
                modalState.selectedIndex
        );
    }
    engine::EndScrollArea(ui, config, input, scroll, modalState.scroll);

    const char* message = modalState.errorMessage.empty()
            ? (modalState.levels.empty() ? "No levels found." : "")
            : modalState.errorMessage.c_str();
    if (message[0] != '\0') {
        engine::Text(
                config,
                assets,
                Rectangle{modal.x + 24.0f, modal.y + 536.0f, modal.width - 48.0f, 40.0f},
                font,
                message,
                engine::UITextJustify::Left,
                modalState.errorMessage.empty() ? config.mutedTextColor : config.invalidColor
        );
    }

    const float buttonY = modal.y + modal.height - 66.0f;
    const float buttonW = 150.0f;
    if (engine::Button(ui, config, input, assets, "sector_editor_load_level_confirm", Rectangle{modal.x + modal.width - buttonW * 2.0f - 36.0f, buttonY, buttonW, 44.0f}, font, "Load")) {
        requestLoad();
    }
    if (engine::Button(ui, config, input, assets, "sector_editor_load_level_cancel", Rectangle{modal.x + modal.width - buttonW - 24.0f, buttonY, buttonW, 44.0f}, font, "Cancel")) {
        state.loadLevelModal = LoadLevelModalState{};
    }

    input.ForEachEvent(engine::InputEventType::Any, true, [](engine::InputEvent& event) {
        engine::ConsumeEvent(event);
    });
}

void SectorEditor::DrawConfirmationModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    ConfirmationModalState& modalState = state.confirmationModal;
    if (!modalState.open) {
        return;
    }

    bool okayRequested = false;
    bool cancelRequested = false;
    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [&okayRequested, &cancelRequested](engine::InputEvent& event) {
                if (event.key.key == KEY_ESCAPE) {
                    cancelRequested = true;
                    engine::ConsumeEvent(event);
                }
            }
    );

    DrawRectangle(0, 0, static_cast<int>(EditorWidth), static_cast<int>(EditorHeight), Color{0, 0, 0, 145});
    const Rectangle modal{
            (EditorWidth - 680.0f) * 0.5f,
            (EditorHeight - 300.0f) * 0.5f,
            680.0f,
            300.0f
    };
    DrawRectangleRec(modal, Color{20, 24, 32, 248});
    DrawRectangleLinesEx(modal, config.borderThickness, config.borderColor);
    engine::Text(config, assets, Rectangle{modal.x + 26.0f, modal.y + 22.0f, modal.width - 52.0f, 42.0f}, font, modalState.title.c_str());
    engine::Text(config, assets, Rectangle{modal.x + 26.0f, modal.y + 86.0f, modal.width - 52.0f, 88.0f}, font, modalState.message.c_str(), engine::UITextJustify::Left, config.mutedTextColor);

    const float buttonY = modal.y + modal.height - 68.0f;
    const float buttonW = 150.0f;
    okayRequested = okayRequested || engine::Button(ui, config, input, assets, "sector_editor_confirmation_okay", Rectangle{modal.x + modal.width - buttonW * 2.0f - 38.0f, buttonY, buttonW, 44.0f}, font, "Okay");
    cancelRequested = cancelRequested || engine::Button(ui, config, input, assets, "sector_editor_confirmation_cancel", Rectangle{modal.x + modal.width - buttonW - 26.0f, buttonY, buttonW, 44.0f}, font, "Cancel");

    input.ForEachEvent(engine::InputEventType::Any, true, [](engine::InputEvent& event) {
        engine::ConsumeEvent(event);
    });

    if (cancelRequested) {
        state.confirmationModal = ConfirmationModalState{};
        return;
    }
    if (okayRequested) {
        std::function<void()> action = std::move(state.confirmationModal.onOkay);
        state.confirmationModal = ConfirmationModalState{};
        if (action) {
            action();
        }
    }
}

void SectorEditor::DrawDecalTintModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    DecalTintModalState& modalState = state.decalTintModal;
    if (!modalState.open) {
        return;
    }

    bool okayRequested = false;
    bool cancelRequested = false;
    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [&okayRequested, &cancelRequested](engine::InputEvent& event) {
                if (event.key.key == KEY_ESCAPE) {
                    cancelRequested = true;
                    engine::ConsumeEvent(event);
                }
            }
    );

    DrawRectangle(0, 0, static_cast<int>(EditorWidth), static_cast<int>(EditorHeight), Color{0, 0, 0, 145});
    const Rectangle modal{
            (EditorWidth - 560.0f) * 0.5f,
            (EditorHeight - 390.0f) * 0.5f,
            560.0f,
            390.0f
    };
    DrawRectangleRec(modal, Color{20, 24, 32, 248});
    DrawRectangleLinesEx(modal, config.borderThickness, config.borderColor);

    float y = modal.y + 22.0f;
    engine::Text(config, assets, Rectangle{modal.x + 26.0f, y, modal.width - 52.0f, 42.0f}, font, "Decal Tint");
    y += 58.0f;

    const float labelW = 72.0f;
    const float inputW = 120.0f;
    const float inputH = 38.0f;
    const float gap = 12.0f;
    auto drawFloat = [&](const char* id, const char* label, float& value, engine::UIFloatInputState& inputState) {
        const SectorEditorTintFloatInputResult result = DrawNormalizedTintFloatInput(
                ui,
                config,
                input,
                assets,
                font,
                id,
                label,
                Rectangle{modal.x + 28.0f, y, labelW, inputH},
                Rectangle{modal.x + 28.0f + labelW, y, inputW, inputH},
                engine::UITextJustify::Left,
                value,
                inputState);
        if (result.finite) {
            value = result.value;
        }
        if (result.changed) {
            if (!result.finite) {
                modalState.errorMessage = "Tint values must be finite.";
            } else {
                modalState.errorMessage.clear();
            }
        }
        y += inputH + gap;
    };

    drawFloat("sector_editor_decal_tint_r", "R", modalState.tint.x, modalState.redInput);
    drawFloat("sector_editor_decal_tint_g", "G", modalState.tint.y, modalState.greenInput);
    drawFloat("sector_editor_decal_tint_b", "B", modalState.tint.z, modalState.blueInput);

    const Rectangle swatch{modal.x + 270.0f, modal.y + 88.0f, 210.0f, 124.0f};
    DrawColorSwatch(config, swatch, DecalTintPreviewColor(modalState.tint), config.borderThickness);
    engine::Text(
            config,
            assets,
            Rectangle{swatch.x, swatch.y + swatch.height + 10.0f, swatch.width, 26.0f},
            font,
            "Preview",
            engine::UITextJustify::Center,
            config.mutedTextColor);

    if (!modalState.errorMessage.empty()) {
        engine::Text(
                config,
                assets,
                Rectangle{modal.x + 28.0f, modal.y + 250.0f, modal.width - 56.0f, 34.0f},
                font,
                modalState.errorMessage.c_str(),
                engine::UITextJustify::Left,
                config.invalidColor);
    }

    const float buttonY = modal.y + modal.height - 66.0f;
    const float buttonW = 124.0f;
    if (engine::Button(ui, config, input, assets, "sector_editor_decal_tint_reset", Rectangle{modal.x + 28.0f, buttonY, buttonW, 44.0f}, font, "Reset White")) {
        modalState.tint = Vector3{1.0f, 1.0f, 1.0f};
        modalState.redInput = engine::UIFloatInputState{};
        modalState.greenInput = engine::UIFloatInputState{};
        modalState.blueInput = engine::UIFloatInputState{};
        modalState.errorMessage.clear();
    }
    okayRequested = okayRequested || engine::Button(ui, config, input, assets, "sector_editor_decal_tint_ok", Rectangle{modal.x + modal.width - buttonW * 2.0f - 38.0f, buttonY, buttonW, 44.0f}, font, "OK");
    cancelRequested = cancelRequested || engine::Button(ui, config, input, assets, "sector_editor_decal_tint_cancel", Rectangle{modal.x + modal.width - buttonW - 26.0f, buttonY, buttonW, 44.0f}, font, "Cancel");

    input.ForEachEvent(engine::InputEventType::Any, true, [](engine::InputEvent& event) {
        engine::ConsumeEvent(event);
    });

    if (cancelRequested) {
        state.decalTintModal = DecalTintModalState{};
        return;
    }
    if (!okayRequested) {
        return;
    }
    if (!IsValidDecalTint(modalState.tint)) {
        modalState.errorMessage = "Tint values must be between 0 and 1.";
        statusText = modalState.errorMessage;
        return;
    }

    const TopologySurfaceEditTarget target = modalState.target;
    const SectorTopologyDecalLayer* decal = DecalForSurface(target);
    if (!IsValidTopologySurfaceEditTarget(target) || decal == nullptr || decal->textureId.empty()) {
        modalState.errorMessage = "Decal target is no longer valid.";
        statusText = modalState.errorMessage;
        return;
    }

    const Vector3 tint = modalState.tint;
    const bool changed = !SameTint(decal->tint, tint);
    if (changed && !ApplySurfaceDecalTint(target, tint, &assets)) {
        modalState.errorMessage = statusText.empty() ? "Could not set decal tint." : statusText;
        return;
    }
    state.decalTintModal = DecalTintModalState{};
}

void SectorEditor::DrawPreviewSettingsModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    const SectorEditorPreviewSettingsModalCallbacks callbacks{
            [this]() { state.previewSettingsModal = SectorPreviewSettingsModalState{}; },
            [this, &assets]() { ApplyPreviewSettingsModal(assets); },
            [this]() { OpenMapSkyTexturePicker(); }
    };
    game::DrawPreviewSettingsModal(
            ui,
            config,
            input,
            assets,
            font,
            state.previewSettingsModal,
            state.texturePicker.open,
            callbacks);
}

void SectorEditor::DrawLightmapBakeModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    const SectorEditorLightmapBakeModalCallbacks callbacks{
            [this]() { return IsLightmapBakeBlocking(); },
            [this]() { RequestLightmapBakeCancel(); },
            [this]() {
                lightmapBake.modalOpen = false;
                lightmapBake.awaitingAcknowledgement = false;
                lightmapBake.cancelButtonPressed = false;
                lightmapBake.terminalMessage.clear();
                lightmapBake.temporaryOutputPath.clear();
                lightmapBake.progress.phase.store(SectorLightmapBakePhase::Idle);
            }
    };
    game::DrawLightmapBakeModal(ui, config, input, assets, font, lightmapBake, callbacks);
}

void SectorEditor::DrawStatusPanel(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::AssetManager& assets,
        engine::FontHandle font)
{
    (void)ui;
    const Rectangle panel = BuildBottomPanelRect();
    DrawRectangleRec(panel, config.panelColor);
    DrawRectangleLinesEx(panel, config.borderThickness, config.borderColor);

    std::string selectedLabel = "none";
    if (const SectorTopologyStaticPointLight* light = SelectedTopologyLight()) {
        selectedLabel = TextFormat("static light %d", light->id);
    } else if (const SectorTopologyDynamicPointLight* light = SelectedTopologyDynamicLight()) {
        selectedLabel = TextFormat("dynamic light %d", light->id);
    } else if (const SectorTopologySector* topologySector = SelectedTopologySector()) {
        selectedLabel = topologySector->name.empty()
                ? TextFormat("topology sector %d", topologySector->id)
                : TextFormat("%s (%d)", topologySector->name.c_str(), topologySector->id);
    } else if (state.selectedAuthoring.kind == SectorAuthoringSelectionKind::Line) {
        selectedLabel = TextFormat("authoring line %d", state.selectedAuthoring.lineId);
    } else if (state.selectedAuthoring.kind == SectorAuthoringSelectionKind::Vertex) {
        selectedLabel = TextFormat("authoring vertex %d", state.selectedAuthoring.vertexId);
    } else if (state.selectedAuthoring.kind == SectorAuthoringSelectionKind::FaceAnchor) {
        selectedLabel = TextFormat("authoring face anchor %d", state.selectedAuthoring.faceAnchorId);
    }

    std::string pendingText;
    if (state.pendingAuthoringLine.active) {
        pendingText = " | authoring line";
    } else if (state.pendingAuthoringRectangle.active) {
        pendingText = " | rectangle";
    } else if (state.pendingAuthoringInsertVertex.active) {
        pendingText = " | insert vertex";
    } else if (state.authoringVertexDrag.active) {
        pendingText = " | authoring vertex move";
    }
    const std::string shortMapPath = state.hasCurrentLevelPath
            ? state.currentLevelPath
            : std::string{"<untitled>"};
    const char* lightmapText = SectorLightmapStatusText(GetSectorLightmapStatus(state.topologyMap));
    std::string status = statusText.empty() ? "Ready" : statusText;
    if (!state.topologyRenderWarning.empty()) {
        status += " | ";
        status += state.topologyRenderWarning;
    }
    const char* text = TextFormat(
            "%s%s | %s%s | map %s%s | grid %d | %s | selected %s",
            status.c_str(),
            state.topologyDocumentDirty ? " *modified" : "",
            ToolHelpText(state.currentTool),
            pendingText.c_str(),
            shortMapPath.c_str(),
            state.topologyDocumentDirty ? "*" : "",
            state.gridSize,
            lightmapText,
            selectedLabel.c_str()
    );

    engine::Text(
            config,
            assets,
            Rectangle{panel.x + 18.0f, panel.y + 17.0f, panel.width - 36.0f, 44.0f},
            font,
            text,
            engine::UITextJustify::Left
    );
}

void SectorEditor::ResetToBlankMap(engine::AssetManager& assets)
{
    ShutdownLightmapBake();
    preview.ShutdownRendererResources(assets);
    if (!engine::IsNull(state.editorTextureScope)) {
        assets.UnloadScope(state.editorTextureScope);
    }
    if (!engine::IsNull(state.addMapTexture.previewScope)) {
        assets.UnloadScope(state.addMapTexture.previewScope);
    }

    state = SectorEditorState{};
    uiState = SectorEditorUiState{};
    ResetEditorTopologyDocumentState(state);
    state.viewCenter = Vector2{9.0f, 6.0f};
    state.viewZoom = 48.0f;
    state.gridSize = 8;
    RefreshDefaultTextures();
    RefreshEditorTextureAssets(assets);
    initialized = true;
    statusText = "New blank level";
}

bool SectorEditor::LoadLevel(
        engine::AssetManager& assets,
        const std::string& levelName,
        const std::string& jsonAssetPath)
{
    SectorEditorLoadedDocument loaded;
    if (!LoadSectorEditorDocumentFromAsset(jsonAssetPath, loaded, state.loadLevelModal.errorMessage)) {
        statusText = state.loadLevelModal.errorMessage;
        return false;
    }

    preview.ShutdownRendererResources(assets);
    CancelAuthoringVertexDrag(nullptr);
    CancelLightDrag(nullptr);
    bool loadedAuthoringGraph = false;
    bool authoringDerivationCurrent = false;
    if (loaded.format == SectorEditorDocumentFormat::AuthoringGraph) {
        loadedAuthoringGraph = true;
        state.topologyMap = std::move(loaded.mapData);
        const SectorLightmapMetadata loadedBakedLightmap = state.topologyMap.bakedLightmap;
        state.authoringGraph = std::move(loaded.authoringGraph);
        state.authoringDerivation = SectorAuthoringDerivationResult{};
        state.lastValidAuthoringDerivedTopology.reset();
        state.authoringDerivationState = SectorEditorAuthoringDerivationState::InvalidNoDerived;
        state.authoringDerivedTopologyStale = true;
        const std::string successStatus = TextFormat(
                "Authoring graph: loaded %s; derived topology current",
                jsonAssetPath.c_str());
        const std::string failureStatus = TextFormat(
                "Authoring graph: loaded %s; derivation failed",
                jsonAssetPath.c_str());
        authoringDerivationCurrent = RefreshSectorEditorAuthoringDerivation(
                state,
                successStatus.c_str(),
                failureStatus.c_str());
        if (authoringDerivationCurrent) {
            state.topologyMap.bakedLightmap = loadedBakedLightmap;
            state.authoringDerivation.topology.bakedLightmap = loadedBakedLightmap;
            if (state.lastValidAuthoringDerivedTopology.has_value()) {
                state.lastValidAuthoringDerivedTopology->bakedLightmap = loadedBakedLightmap;
            }
        }
    } else {
        state.topologyMap = std::move(loaded.mapData);
        InitializeSectorEditorAuthoringStateFromTopology(state, state.topologyMap);
    }
    InvalidateTopologyRenderCache();
    state.fpsControllerConfig = SectorFpsControllerConfigFromPreviewSettings(
            state.topologyMap.previewSettings);
    state.topologyDocumentInitialized = true;
    state.topologyDocumentDirty = false;
    if (!loadedAuthoringGraph) {
        state.topologyDocumentStatus = TextFormat(
                "Topology document: imported legacy topology %s",
                jsonAssetPath.c_str());
    }
    state.currentLevelName = levelName;
    state.currentLevelPath = jsonAssetPath;
    state.hasCurrentLevelPath = true;
    state.hasUnsavedChanges = false;
    state.mode = SectorEditorMode::Edit2D;
    state.hasPreviewPose = false;
    state.hoveredSurface3D = SectorSurfaceHit{};
    state.texturePicker = TexturePickerState{};
    state.loadLevelModal = LoadLevelModalState{};
    state.saveLevelModal = SaveLevelModalState{};
    state.confirmationModal = ConfirmationModalState{};
    state.decalTintModal = DecalTintModalState{};
    ClearSelection();
    state.hoveredTopologyLightId = -1;
    state.hoveredTopologyDynamicLightId = -1;
    state.hasHoveredVertex = false;
    state.hoveredTopologyVertexId = -1;
    state.hoveredTopologyVertexPoint = SectorTopologyCoordPoint{};
    state.authoringVertexDrag = AuthoringVertexDragState{};
    state.lightDrag = LightDragState{};
    RefreshDefaultTextures();
    RefreshEditorTextureAssets(assets);
    if (loadedAuthoringGraph) {
        const char* loadedText = authoringDerivationCurrent
                ? "Loaded authoring graph"
                : "Loaded authoring graph with derivation diagnostics";
        statusText = TextFormat("%s %s", loadedText, jsonAssetPath.c_str());
    } else {
        statusText = TextFormat("Imported legacy topology %s", jsonAssetPath.c_str());
    }
    return true;
}

void SectorEditor::OpenConfirmation(const char* title, const char* message, std::function<void()> onOkay)
{
    OpenConfirmationModal(state.confirmationModal, title, message, std::move(onOkay));
}

void SectorEditor::OpenNewConfirmation(engine::AssetManager& assets)
{
    OpenConfirmation(
            "New Level",
            "Discard current level and create a new blank map?",
            [this, &assets]() { ResetToBlankMap(assets); }
    );
}

void SectorEditor::OpenReloadConfirmation(engine::AssetManager& assets)
{
    if (!state.hasCurrentLevelPath) {
        statusText = "No saved level to reload.";
        return;
    }
    const std::string name = state.currentLevelName;
    const std::string path = state.currentLevelPath;
    OpenConfirmation(
            "Reload Level",
            "Reload current level from disk and discard unsaved changes?",
            [this, &assets, name, path]() { LoadLevel(assets, name, path); }
    );
}

void SectorEditor::OpenSaveLevelModal()
{
    OpenSaveLevelModalState(
            state.saveLevelModal,
            state.hasCurrentLevelPath,
            state.currentLevelName);
}

void SectorEditor::RefreshLevelList()
{
    RefreshLoadLevelModalState(state.loadLevelModal);
}

void SectorEditor::OpenLoadLevelModal()
{
    OpenLoadLevelModalState(state.loadLevelModal);
}

bool SectorEditor::SaveLevelFromModal(bool overwriteConfirmed)
{
    SaveLevelModalState& modal = state.saveLevelModal;
    const std::string name = modal.nameBuffer;
    SectorEditorSaveLevelPlan savePlan;
    if (!PrepareSaveLevelPlan(
                name,
                state.hasCurrentLevelPath,
                state.currentLevelPath,
                overwriteConfirmed,
                savePlan,
                modal.errorMessage)) {
        return false;
    }

    if (savePlan.needsOverwriteConfirmation) {
        OpenConfirmation(
                "Overwrite Level",
                "Level already exists. Overwrite it?",
                [this]() { SaveLevelFromModal(true); }
        );
        return false;
    }

    if (!EnsureSaveLevelDirectory(savePlan.paths, modal.errorMessage)) {
        return false;
    }

    if (!SaveSectorEditorAuthoringDocument(savePlan.paths, state, modal.errorMessage)) {
        statusText = TextFormat("Save failed: %s", savePlan.paths.jsonAssetPath.c_str());
        return false;
    }

    state.currentLevelName = name;
    state.currentLevelPath = savePlan.paths.jsonAssetPath;
    state.hasCurrentLevelPath = true;
    state.hasUnsavedChanges = false;
    state.topologyDocumentInitialized = true;
    state.topologyDocumentDirty = false;
    state.topologyDocumentStatus = TextFormat("Authoring graph: saved %s", savePlan.paths.jsonAssetPath.c_str());
    state.saveLevelModal = SaveLevelModalState{};
    state.confirmationModal = ConfirmationModalState{};
    state.decalTintModal = DecalTintModalState{};
    statusText = TextFormat("Saved authoring graph %s", savePlan.paths.jsonAssetPath.c_str());
    return true;
}

bool SectorEditor::HasDocumentModalOpen() const
{
    return state.saveLevelModal.open
            || state.loadLevelModal.open
            || state.confirmationModal.open
            || state.decalTintModal.open
            || state.previewSettingsModal.open;
}

bool SectorEditor::TryEnterPreview3D(engine::AssetManager& assets, engine::UIContext& ui)
{
    if (!initialized) {
        statusText = "3D mode failed: no map loaded";
        return false;
    }

    CancelAuthoringVertexDrag(nullptr);
    CancelLightDrag(nullptr);
    ui.hotId = 0;
    ui.activeId = 0;
    ui.openOptionId = 0;
    ui.focusedId = 0;
    uiState.keyboardCaptured = false;

    std::string gateMessage;
    if (!CanUseCurrentAuthoringDerivedTopologyForPreview(state, &gateMessage)) {
        statusText = gateMessage.empty() ? "3D mode failed: derived topology is not current" : gateMessage;
        return false;
    }

    std::string error;
    if (!preview.RebuildRendererResources(assets, state.topologyMap, "sector_editor_preview", error)) {
        state.sectorCollisionWorldValid = false;
        state.sectorCollisionWorldWarning.clear();
        state.previewCollisionSectorId = 0;
        state.fpsControllerState.currentSectorId = 0;
        state.previewVerticalResult = SectorFpsVerticalResult{};
        state.previewMoveResult = SectorCollisionMoveResult{};
        state.previewCollisionNoclipFallback = false;
        state.visualStepOffsetY = 0.0f;
        ClearSectorFpsHeadBob(state.headBobState);
        ClearSectorFpsLandingDip(state.landingDipState);
        state.mode = SectorEditorMode::Edit2D;
        if (StartsWith(error, "Preview failed:")) {
            statusText = std::string{"3D mode failed:"} + error.substr(std::strlen("Preview failed:"));
        } else {
            statusText = error.empty() ? "3D mode failed" : error;
        }
        return false;
    }

    if (state.hasPreviewPose) {
        preview.ApplyRendererPose(state.lastPreviewPose);
    }

    state.previewControlMode = SectorPreviewControlMode::FreeFly;
    ResetSectorFreeflyController(state.freeflyController, preview.RendererPose());
    EnterSectorFreeflyController(state.freeflyController);
    preview.ApplyRendererPose(state.freeflyController.pose);
    state.visualStepOffsetY = 0.0f;
    ClearSectorFpsHeadBob(state.headBobState);
    ClearSectorFpsLandingDip(state.landingDipState);
    state.fpsControllerConfig = NormalizeSectorFpsControllerConfig(state.fpsControllerConfig);
    state.mode = SectorEditorMode::Preview3D;
    state.previewUiHidden = false;
    state.hoveredSurface3D = SectorSurfaceHit{};
    state.selectedSurface3D = SectorSurfaceRef{};
    state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ResetSurface3DUiState();
    RebuildSectorCollisionWorld();
    statusText = TextFormat(
            "3D mode rebuilt: %zu batches, %d triangles",
            preview.BatchCount(),
            preview.TriangleCount()
    );
    return true;
}

void SectorEditor::LeavePreview3D()
{
    if (state.previewControlMode == SectorPreviewControlMode::Gameplay) {
        ClearSectorFpsLandingDip(state.landingDipState);
        ApplyGameplayPoseToPreview();
    }
    state.lastPreviewPose = ActivePreviewPose();
    state.hasPreviewPose = true;
    state.visualStepOffsetY = 0.0f;
    ClearSectorFpsHeadBob(state.headBobState);
    ClearSectorFpsLandingDip(state.landingDipState);
    state.previewControlMode = SectorPreviewControlMode::FreeFly;
    state.mode = SectorEditorMode::Edit2D;
    state.hoveredSurface3D = SectorSurfaceHit{};
    state.previewSettingsModal = SectorPreviewSettingsModalState{};
    LeaveSectorFreeflyController();
    statusText = "Returned to 2D editor";
}

SectorViewPose SectorEditor::ActivePreviewPose() const
{
    return ActiveSectorEditorPreviewPose(state, preview);
}

void SectorEditor::ApplyGameplayPoseToPreview()
{
    ApplySectorEditorGameplayPoseToPreview(state, preview);
}

void SectorEditor::TogglePreviewControlMode()
{
    if (!ToggleSectorEditorPreviewControlMode(state, preview)) {
        return;
    }

    statusText = TextFormat("3D control mode: %s", PreviewControlModeName(state.previewControlMode));
}

bool SectorEditor::RebuildSectorCollisionWorld()
{
    return RebuildSectorEditorCollisionWorld(state);
}

SectorFpsVerticalContext SectorEditor::BuildGameplayVerticalContext()
{
    return BuildSectorEditorGameplayVerticalContext(state);
}

void SectorEditor::RefreshGameplaySectorAndVerticalContext()
{
    RefreshSectorEditorGameplaySectorAndVerticalContext(state);
}

void SectorEditor::InitializeGameplayVerticalState()
{
    InitializeSectorEditorGameplayVerticalState(state);
}

void SectorEditor::OpenPreviewSettingsModal()
{
    state.previewSettingsModal = SectorPreviewSettingsModalState{};
    state.previewSettingsModal.open = true;
    state.previewSettingsModal.draftConfig = NormalizeSectorFpsControllerConfig(state.fpsControllerConfig);
    state.previewSettingsModal.draftSkySettings = NormalizeSectorTopologySkySettings(state.topologyMap.skySettings);
    state.previewSettingsModal.draftDirectionalLight =
            NormalizeSectorTopologyDirectionalLightSettings(state.topologyMap.directionalLight);
}

void SectorEditor::ApplyPreviewSettingsModal(engine::AssetManager& assets)
{
    if (!state.previewSettingsModal.open) {
        return;
    }

    const SectorFpsControllerConfig draftConfig = NormalizeSectorFpsControllerConfig(
            state.previewSettingsModal.draftConfig);
    const SectorPreviewSettings draftPreviewSettings = SectorPreviewSettingsFromFpsControllerConfig(draftConfig);
    const SectorTopologySkySettings draftSkySettings = NormalizeSectorTopologySkySettings(
            state.previewSettingsModal.draftSkySettings);
    const SectorTopologyDirectionalLightSettings draftDirectionalLight =
            NormalizeSectorTopologyDirectionalLightSettings(
                    state.previewSettingsModal.draftDirectionalLight);
    const bool previewChanged = !SamePreviewSettings(
            state.topologyMap.previewSettings,
            draftPreviewSettings);
    const bool skyChanged = !SameSkySettings(state.topologyMap.skySettings, draftSkySettings);
    const bool directionalChanged = !SameDirectionalLightSettings(
            state.topologyMap.directionalLight,
            draftDirectionalLight);
    if (!previewChanged && !skyChanged && !directionalChanged) {
        state.previewSettingsModal = SectorPreviewSettingsModalState{};
        statusText = "Preview settings unchanged";
        return;
    }

    const float previousGravity = NormalizeSectorFpsControllerConfig(state.fpsControllerConfig).gravity;
    state.fpsControllerConfig = draftConfig;
    if (previousGravity > 0.0f && state.fpsControllerConfig.gravity == 0.0f) {
        state.fpsControllerState.verticalVelocity = 0.0f;
    }
    state.topologyMap.previewSettings = draftPreviewSettings;
    state.topologyMap.skySettings = draftSkySettings;
    state.topologyMap.directionalLight = draftDirectionalLight;
    MarkTopologyDocumentEdited("Preview settings updated");
    state.previewSettingsModal = SectorPreviewSettingsModalState{};
    if (skyChanged && state.mode == SectorEditorMode::Preview3D && preview.IsRendererReady()) {
        RebuildPreviewMeshesPreservingView(assets);
    }
    if (state.mode == SectorEditorMode::Preview3D
            && state.previewControlMode == SectorPreviewControlMode::Gameplay
            && preview.IsRendererReady()) {
        state.previewVerticalResult = UpdateSectorFpsVerticalPhysics(
                state.fpsControllerState,
                state.fpsControllerConfig,
                BuildGameplayVerticalContext(),
                0.0f);
        state.visualStepOffsetY = 0.0f;
        ClearSectorFpsHeadBob(state.headBobState);
        ClearSectorFpsLandingDip(state.landingDipState);
        ApplyGameplayPoseToPreview();
    }
    statusText = TextFormat(
            "Preview settings updated: walk %.1f run %.1f eye %.1f gravity %.1f radius %.2f height %.2f step %.2f jump %.2f bob %.3f freq %.1f",
            state.fpsControllerConfig.walkSpeed,
            state.fpsControllerConfig.runSpeed,
            state.fpsControllerConfig.eyeHeight,
            state.fpsControllerConfig.gravity,
            state.fpsControllerConfig.playerRadius,
            state.fpsControllerConfig.playerHeight,
            state.fpsControllerConfig.stepHeight,
            state.fpsControllerConfig.jumpHeight,
            state.fpsControllerConfig.headBobStrength,
            state.fpsControllerConfig.headBobFrequency);
}

void SectorEditor::RefreshDefaultTextures()
{
    auto findTexture = [this](const char* preferred, const std::string& fallback = std::string{}) {
        const auto preferredIt = state.topologyMap.texturesById.find(preferred);
        if (preferredIt != state.topologyMap.texturesById.end()) {
            return preferredIt->first;
        }
        if (!fallback.empty()) {
            return fallback;
        }
        const std::vector<std::string> textureIds = SortedSectorTopologyTextureIds(state.topologyMap);
        return textureIds.empty() ? std::string{} : textureIds.front();
    };

    state.defaultWallTextureId = findTexture("wall");
    state.defaultFloorTextureId = findTexture("floor");
    state.defaultCeilingTextureId = findTexture("ceiling");
    state.defaultLowerWallTextureId = findTexture("step_wall", state.defaultWallTextureId);
    state.defaultUpperWallTextureId = findTexture("upper_wall", state.defaultWallTextureId);
}

void SectorEditor::RefreshEditorTextureAssets(engine::AssetManager& assets)
{
    if (!engine::IsNull(state.editorTextureScope)) {
        assets.UnloadScope(state.editorTextureScope);
        state.editorTextureScope = engine::NullAssetScopeHandle();
    }
    state.editorTextureHandlesById.clear();

    if (state.topologyMap.texturesById.empty()) {
        return;
    }

    state.editorTextureScope = assets.CreateScope("sector_editor_textures");
    if (engine::IsNull(state.editorTextureScope)) {
        return;
    }

    for (const std::string& textureId : SortedSectorTopologyTextureIds(state.topologyMap)) {
        const SectorTextureDefinition* texture = FindSectorTopologyTexture(state.topologyMap, textureId);
        if (texture == nullptr) {
            continue;
        }

        const std::string resolvedPath = ResolveEditorAssetPath(texture->path);
        state.editorTextureHandlesById.emplace(
                texture->id,
                assets.RequestTexture(
                        state.editorTextureScope,
                        texture->id.c_str(),
                        resolvedPath.c_str(),
                        SectorTextureLoadFlags(texture->filter)
                )
        );
    }
}

engine::TextureHandle SectorEditor::EditorTextureHandleForId(const std::string& textureId) const
{
    const auto it = state.editorTextureHandlesById.find(textureId);
    return it == state.editorTextureHandlesById.end() ? engine::NullTextureHandle() : it->second;
}

void SectorEditor::OpenAddMapTextureModal(engine::AssetManager& assets)
{
    CloseAddMapTextureModal(assets);
    state.addMapTexture.open = true;
    RefreshAddMapTextureScan();
    SelectAddMapTexturePath(state.addMapTexture.selectedPathIndex);
    statusText = "Add topology texture";
}

void SectorEditor::CloseAddMapTextureModal(engine::AssetManager& assets)
{
    if (!engine::IsNull(state.addMapTexture.previewScope)) {
        assets.UnloadScope(state.addMapTexture.previewScope);
    }
    state.addMapTexture = AddMapTextureState{};
}

void SectorEditor::RefreshAddMapTextureScan()
{
    game::RefreshAddMapTextureScan(state.addMapTexture);
}

void SectorEditor::SelectAddMapTexturePath(int pathIndex)
{
    game::SelectAddMapTexturePath(state.addMapTexture, state.topologyMap, pathIndex);
}

void SectorEditor::RefreshAddMapTexturePreview(engine::AssetManager& assets)
{
    game::RefreshAddMapTexturePreview(state.addMapTexture, assets);
}

bool SectorEditor::ValidateAddMapTextureId(std::string& error) const
{
    return game::ValidateAddMapTextureId(state.addMapTexture, error);
}

bool SectorEditor::AddSelectedMapTexture(engine::AssetManager& assets)
{
    const SectorEditorAddTextureResult result = game::AddSelectedMapTexture(state);
    if (!result.success) {
        return false;
    }

    RefreshEditorTextureAssets(assets);
    state.hasUnsavedChanges = true;
    state.topologyDocumentDirty = true;
    statusText = TextFormat("%s texture %s", result.replacing ? "Updated" : "Added", result.textureId.c_str());
    CloseAddMapTextureModal(assets);
    return true;
}

bool SectorEditor::PointInTopologyLoop(Vector2 mapPoint, const SectorTopologyLoop& loop) const
{
    std::vector<SectorPoint> points;
    points.reserve(loop.vertexIds.size());
    for (int vertexId : loop.vertexIds) {
        const SectorTopologyVertex* vertex = FindSectorTopologyVertex(state.topologyMap, vertexId);
        if (vertex == nullptr) {
            return false;
        }
        points.push_back(Vector2ToSectorPoint(SectorTopologyVertexToMap(*vertex)));
    }
    const SectorPoint point = Vector2ToSectorPoint(mapPoint);
    return StrictPointInPolygon(point, points) || PointOnPolygonBoundary(point, points);
}

bool SectorEditor::PointInTopologySector(Vector2 mapPoint, const SectorTopologySector& sector) const
{
    SectorTopologyLoopSet loops;
    std::vector<SectorTopologyValidationIssue> loopIssues;
    if (!ExtractSectorTopologyLoops(state.topologyMap, sector.id, loops, &loopIssues)) {
        return false;
    }
    if (!PointInTopologyLoop(mapPoint, loops.outer)) {
        return false;
    }
    for (const SectorTopologyLoop& hole : loops.holes) {
        if (PointInTopologyLoop(mapPoint, hole)) {
            return false;
        }
    }
    return true;
}

int SectorEditor::FindTopologySectorAt(Vector2 mapPoint, bool* outMultipleMatches) const
{
    if (outMultipleMatches != nullptr) {
        *outMultipleMatches = false;
    }

    int selectedId = -1;
    int matchCount = 0;
    for (const SectorTopologySector& sector : state.topologyMap.sectors) {
        if (!PointInTopologySector(mapPoint, sector)) {
            continue;
        }
        ++matchCount;
        if (selectedId < 0 || sector.id < selectedId) {
            selectedId = sector.id;
        }
    }

    if (outMultipleMatches != nullptr) {
        *outMultipleMatches = matchCount > 1;
    }
    return selectedId;
}

int SectorEditor::FindTopologyLightNearScreenPoint(Vector2 screenPoint) const
{
    float bestDistance2 = ScreenLightPickPixels * ScreenLightPickPixels;
    int bestId = -1;
    for (const SectorTopologyStaticPointLight& light : state.topologyMap.staticLights) {
        const Vector2 center = MapToScreen(Vector2{light.position.x, light.position.z});
        const float dx = center.x - screenPoint.x;
        const float dy = center.y - screenPoint.y;
        const float distance2 = dx * dx + dy * dy;
        if (distance2 < bestDistance2 - 0.001f
                || (std::fabs(distance2 - bestDistance2) <= 0.001f
                        && (bestId < 0 || light.id < bestId))) {
            bestDistance2 = distance2;
            bestId = light.id;
        }
    }
    return bestId;
}

int SectorEditor::FindTopologyDynamicLightNearScreenPoint(Vector2 screenPoint) const
{
    float bestDistance2 = ScreenLightPickPixels * ScreenLightPickPixels;
    int bestId = -1;
    for (const SectorTopologyDynamicPointLight& light : state.topologyMap.dynamicPointLights) {
        const Vector2 center = MapToScreen(Vector2{light.position.x, light.position.z});
        const float dx = center.x - screenPoint.x;
        const float dy = center.y - screenPoint.y;
        const float distance2 = dx * dx + dy * dy;
        if (distance2 < bestDistance2 - 0.001f
                || (std::fabs(distance2 - bestDistance2) <= 0.001f
                        && (bestId < 0 || light.id < bestId))) {
            bestDistance2 = distance2;
            bestId = light.id;
        }
    }
    return bestId;
}

bool SectorEditor::FindTopologyLineNearScreenPoint(
        Vector2 screenPoint,
        Vector2 mapPoint,
        int& outLineDefId,
        int& outSideDefId,
        SectorTopologySideKind& outSide,
        bool& outPreferredMissing) const
{
    outLineDefId = -1;
    outSideDefId = -1;
    outSide = SectorTopologySideKind::Front;
    outPreferredMissing = false;

    float bestDistance = ScreenEdgePickPixels;
    const SectorTopologyLineDef* bestLine = nullptr;
    Vector2 bestStart{};
    Vector2 bestEnd{};

    for (const SectorTopologyLineDef& lineDef : state.topologyMap.lineDefs) {
        const SectorTopologyVertex* start = nullptr;
        const SectorTopologyVertex* end = nullptr;
        if (!GetSectorTopologyLineVertices(state.topologyMap, lineDef, start, end)) {
            continue;
        }

        const Vector2 screenStart = MapToScreen(SectorTopologyVertexToMap(*start));
        const Vector2 screenEnd = MapToScreen(SectorTopologyVertexToMap(*end));
        const float distance = DistancePointToSegment(screenPoint, screenStart, screenEnd);
        if (distance > ScreenEdgePickPixels) {
            continue;
        }
        if (bestLine != nullptr
                && (distance > bestDistance + 0.001f
                        || (std::fabs(distance - bestDistance) <= 0.001f
                                && lineDef.id >= bestLine->id))) {
            continue;
        }

        bestDistance = distance;
        bestLine = &lineDef;
        bestStart = SectorTopologyVertexToMap(*start);
        bestEnd = SectorTopologyVertexToMap(*end);
    }

    if (bestLine == nullptr) {
        return false;
    }

    const bool hasFront = bestLine->frontSideDefId >= 0
            && FindSectorTopologySideDef(state.topologyMap, bestLine->frontSideDefId) != nullptr;
    const bool hasBack = bestLine->backSideDefId >= 0
            && FindSectorTopologySideDef(state.topologyMap, bestLine->backSideDefId) != nullptr;

    SectorTopologySideKind preferredSide = SectorTopologySideKind::Front;
    const float sideCross = Cross(bestStart, bestEnd, mapPoint);
    if (sideCross > GeometryEpsilon) {
        preferredSide = SectorTopologySideKind::Front;
    } else if (sideCross < -GeometryEpsilon) {
        preferredSide = SectorTopologySideKind::Back;
    } else if (!hasFront && hasBack) {
        preferredSide = SectorTopologySideKind::Back;
    }

    const int preferredSideDefId = preferredSide == SectorTopologySideKind::Front
            ? bestLine->frontSideDefId
            : bestLine->backSideDefId;
    const int oppositeSideDefId = preferredSide == SectorTopologySideKind::Front
            ? bestLine->backSideDefId
            : bestLine->frontSideDefId;
    const bool preferredExists = preferredSide == SectorTopologySideKind::Front ? hasFront : hasBack;
    const bool oppositeExists = preferredSide == SectorTopologySideKind::Front ? hasBack : hasFront;

    outLineDefId = bestLine->id;
    if (preferredExists) {
        outSide = preferredSide;
        outSideDefId = preferredSideDefId;
    } else if (oppositeExists) {
        outSide = OppositeSectorTopologySideKind(preferredSide);
        outSideDefId = oppositeSideDefId;
        outPreferredMissing = true;
    } else {
        outSide = preferredSide;
        outSideDefId = -1;
    }
    return true;
}

int SectorEditor::FindAuthoringLineNearScreenPoint(Vector2 screenPoint) const
{
    int lineId = -1;
    const float maxDistance = SectorWorldToAuthoringDistance(
            ScreenEdgePickPixels / std::max(1.0f, state.viewZoom));
    if (!FindSectorEditorAuthoringLineNearMapPoint(
                state.authoringGraph,
                ScreenToMap(screenPoint),
                maxDistance,
                &lineId)) {
        return -1;
    }
    return lineId;
}

bool SectorEditor::FindAuthoringVertexNearScreenPoint(
        Vector2 screenPoint,
        int& outVertexId,
        SectorTopologyCoordPoint& outPoint) const
{
    outVertexId = -1;
    outPoint = SectorTopologyCoordPoint{};

    float bestDistance2 = ScreenVertexSnapPixels * ScreenVertexSnapPixels;
    int bestVertexId = -1;
    SectorTopologyCoordPoint bestPoint{};
    for (const SectorAuthoringVertex& vertex : state.authoringGraph.vertices) {
        const Vector2 screenVertex = MapToScreen(Vector2{
                SectorCoordToVisibleAuthoring(vertex.x),
                SectorCoordToVisibleAuthoring(vertex.y)});
        const float dx = screenVertex.x - screenPoint.x;
        const float dy = screenVertex.y - screenPoint.y;
        const float distance2 = dx * dx + dy * dy;
        if (distance2 > bestDistance2) {
            continue;
        }
        if (bestVertexId >= 0
                && std::fabs(distance2 - bestDistance2) <= 0.001f
                && vertex.id >= bestVertexId) {
            continue;
        }
        bestDistance2 = distance2;
        bestVertexId = vertex.id;
        bestPoint = SectorTopologyCoordPoint{vertex.x, vertex.y};
    }

    if (bestVertexId < 0) {
        return false;
    }
    outVertexId = bestVertexId;
    outPoint = bestPoint;
    return true;
}

bool SectorEditor::FindAuthoringSelectionNearScreenPoint(
        Vector2 screenPoint,
        SectorAuthoringSelectionTarget& outTarget,
        SectorTopologyCoordPoint& outVertexPoint) const
{
    const float vertexMaxDistance = SectorWorldToAuthoringDistance(
            ScreenVertexSnapPixels / std::max(1.0f, state.viewZoom));
    const float lineMaxDistance = SectorWorldToAuthoringDistance(
            ScreenEdgePickPixels / std::max(1.0f, state.viewZoom));
    return FindSectorEditorAuthoringSelectionAtMapPoint(
            state,
            ScreenToMap(screenPoint),
            vertexMaxDistance,
            lineMaxDistance,
            &outTarget,
            &outVertexPoint);
}

void SectorEditor::SelectTopologySector(int sectorId)
{
    if (FindSectorTopologySector(state.topologyMap, sectorId) == nullptr) {
        ClearSelection();
        return;
    }

    state.topologySelectionKind = TopologySelectionKind::Sector;
    state.selectedTopologySectorId = sectorId;
    state.selectedTopologyVertexId = -1;
    state.selectedTopologySideDefId = -1;
    state.selectedTopologyLineDefId = -1;
    state.selectedTopologyLightId = -1;
    state.selectedTopologyDynamicLightId = -1;
    state.selectedTopologySideKind = SectorTopologySideKind::Front;
    state.inspectedTopologyVertexId = -1;
    state.selectedSurface3D = SectorSurfaceRef{};
    state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ClearSectorEditorAuthoringSelection(state);
    ResetSurface3DUiState();
    uiState.idBufferLightIndex = -1;
    uiState.inspectorScroll.offset = Vector2{};
    uiState.floorInput = engine::UIFloatInputState{};
    uiState.ceilingInput = engine::UIFloatInputState{};
    uiState.ambientIntensityInput = engine::UIFloatInputState{};
    uiState.ambientRedInput = engine::UIIntInputState{};
    uiState.ambientGreenInput = engine::UIIntInputState{};
    uiState.ambientBlueInput = engine::UIIntInputState{};
    for (engine::UIFloatInputState& inputState : uiState.topologySectorUvInputs) {
        inputState = engine::UIFloatInputState{};
    }
    SyncSelectedSectorIdBuffer();
    SyncSelectedLightIdBuffer();
}

void SectorEditor::SelectTopologyVertex(int vertexId)
{
    const SectorTopologyVertex* vertex = FindSectorTopologyVertex(state.topologyMap, vertexId);
    if (vertex == nullptr) {
        ClearSelection();
        return;
    }

    state.topologySelectionKind = TopologySelectionKind::Vertex;
    state.selectedTopologySectorId = -1;
    state.selectedTopologyVertexId = vertex->id;
    state.selectedTopologySideDefId = -1;
    state.selectedTopologyLineDefId = -1;
    state.selectedTopologyLightId = -1;
    state.selectedTopologyDynamicLightId = -1;
    state.selectedTopologySideKind = SectorTopologySideKind::Front;
    state.inspectedTopologyVertexId = vertex->id;
    state.selectedSurface3D = SectorSurfaceRef{};
    state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ClearSectorEditorAuthoringSelection(state);
    ResetSurface3DUiState();
    uiState.idBufferLightIndex = -1;
    uiState.inspectorScroll.offset = Vector2{};
    SyncSelectedSectorIdBuffer();
    SyncSelectedLightIdBuffer();
}

void SectorEditor::SelectTopologySideDef(int sideDefId, TopologyWallPart wallPart)
{
    const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(state.topologyMap, sideDefId);
    if (sideDef == nullptr) {
        ClearSelection();
        return;
    }
    const SectorTopologyLineDef* lineDef = FindSectorTopologyLineDef(state.topologyMap, sideDef->lineDefId);
    if (lineDef == nullptr) {
        ClearSelection();
        return;
    }

    state.topologySelectionKind = TopologySelectionKind::SideDef;
    state.selectedTopologySectorId = -1;
    state.selectedTopologyVertexId = -1;
    state.selectedTopologySideDefId = sideDef->id;
    state.selectedTopologyLineDefId = lineDef->id;
    state.selectedTopologyLightId = -1;
    state.selectedTopologyDynamicLightId = -1;
    state.selectedTopologySideKind = sideDef->side;
    state.selectedTopologyWallPart = ValidTopologyWallPartForSideDef(
            state.topologyMap,
            sideDef,
            wallPart);
    state.inspectedTopologyVertexId = -1;
    state.selectedSurface3D = SectorSurfaceRef{};
    state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ClearSectorEditorAuthoringSelection(state);
    ResetSurface3DUiState();
    uiState.idBufferLightIndex = -1;
    uiState.inspectorScroll.offset = Vector2{};
    for (engine::UIFloatInputState& inputState : uiState.topologySideDefUvInputs) {
        inputState = engine::UIFloatInputState{};
    }
    SyncSelectedSectorIdBuffer();
    SyncSelectedLightIdBuffer();
}

void SectorEditor::SelectTopologyLineDef(
        int lineDefId,
        SectorTopologySideKind side,
        TopologyWallPart wallPart)
{
    if (FindSectorTopologyLineDef(state.topologyMap, lineDefId) == nullptr) {
        ClearSelection();
        return;
    }

    state.topologySelectionKind = TopologySelectionKind::LineDef;
    state.selectedTopologySectorId = -1;
    state.selectedTopologyVertexId = -1;
    state.selectedTopologySideDefId = -1;
    state.selectedTopologyLineDefId = lineDefId;
    state.selectedTopologyLightId = -1;
    state.selectedTopologyDynamicLightId = -1;
    state.selectedTopologySideKind = side;
    state.selectedTopologyWallPart = wallPart == TopologyWallPart::Middle
            ? TopologyWallPart::Wall
            : wallPart;
    state.inspectedTopologyVertexId = -1;
    state.selectedSurface3D = SectorSurfaceRef{};
    state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ClearSectorEditorAuthoringSelection(state);
    ResetSurface3DUiState();
    uiState.idBufferLightIndex = -1;
    uiState.inspectorScroll.offset = Vector2{};
    for (engine::UIFloatInputState& inputState : uiState.topologySideDefUvInputs) {
        inputState = engine::UIFloatInputState{};
    }
    SyncSelectedSectorIdBuffer();
    SyncSelectedLightIdBuffer();
}

void SectorEditor::SelectTopologyLight(int topologyLightId)
{
    if (FindSectorTopologyStaticLight(state.topologyMap, topologyLightId) == nullptr) {
        ClearSelection();
        return;
    }

    state.selectedTopologyLightId = topologyLightId;
    state.selectedTopologyDynamicLightId = -1;
    state.topologySelectionKind = TopologySelectionKind::StaticLight;
    state.selectedTopologySectorId = -1;
    state.selectedTopologyVertexId = -1;
    state.selectedTopologySideDefId = -1;
    state.selectedTopologyLineDefId = -1;
    state.selectedTopologySideKind = SectorTopologySideKind::Front;
    state.inspectedTopologyVertexId = -1;
    state.selectedSurface3D = SectorSurfaceRef{};
    state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ClearSectorEditorAuthoringSelection(state);
    ResetSurface3DUiState();
    uiState.inspectorScroll.offset = Vector2{};
    uiState.lightXInput = engine::UIFloatInputState{};
    uiState.lightYInput = engine::UIFloatInputState{};
    uiState.lightZInput = engine::UIFloatInputState{};
    uiState.lightIntensityInput = engine::UIFloatInputState{};
    uiState.lightRadiusInput = engine::UIFloatInputState{};
    uiState.lightSourceRadiusInput = engine::UIFloatInputState{};
    uiState.lightRedInput = engine::UIIntInputState{};
    uiState.lightGreenInput = engine::UIIntInputState{};
    uiState.lightBlueInput = engine::UIIntInputState{};
    SyncSelectedLightIdBuffer();
    SyncSelectedSectorIdBuffer();
}

void SectorEditor::SelectTopologyDynamicLight(int topologyLightId)
{
    if (FindSectorTopologyDynamicLight(state.topologyMap, topologyLightId) == nullptr) {
        ClearSelection();
        return;
    }

    state.selectedTopologyDynamicLightId = topologyLightId;
    state.selectedTopologyLightId = -1;
    state.topologySelectionKind = TopologySelectionKind::DynamicLight;
    state.selectedTopologySectorId = -1;
    state.selectedTopologyVertexId = -1;
    state.selectedTopologySideDefId = -1;
    state.selectedTopologyLineDefId = -1;
    state.selectedTopologySideKind = SectorTopologySideKind::Front;
    state.inspectedTopologyVertexId = -1;
    state.selectedSurface3D = SectorSurfaceRef{};
    state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ClearSectorEditorAuthoringSelection(state);
    ResetSurface3DUiState();
    uiState.inspectorScroll.offset = Vector2{};
    uiState.lightXInput = engine::UIFloatInputState{};
    uiState.lightYInput = engine::UIFloatInputState{};
    uiState.lightZInput = engine::UIFloatInputState{};
    uiState.lightIntensityInput = engine::UIFloatInputState{};
    uiState.lightRadiusInput = engine::UIFloatInputState{};
    uiState.lightSourceRadiusInput = engine::UIFloatInputState{};
    uiState.lightRedInput = engine::UIIntInputState{};
    uiState.lightGreenInput = engine::UIIntInputState{};
    uiState.lightBlueInput = engine::UIIntInputState{};
    SyncSelectedLightIdBuffer();
    SyncSelectedSectorIdBuffer();
}

void SectorEditor::SelectSurface3D(SectorSurfaceRef surface)
{
    const TopologySurfaceEditTarget target = TopologyEditTargetForSurface(surface);
    if (!IsValidSurfaceRef(surface) || !IsValidTopologySurfaceEditTarget(target)) {
        state.selectedSurface3D = SectorSurfaceRef{};
        state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
        return;
    }
    SectorEditorAuthoringSurfaceTarget authoringTarget;
    const bool hasAuthoringGraph = HasAuthoringGraphData();
    if (hasAuthoringGraph) {
        std::string unavailableStatus;
        if (!ResolveSectorEditorAuthoringSurfaceTarget(
                    state,
                    surface,
                    authoringTarget,
                    &unavailableStatus)) {
            state.selectedSurface3D = SectorSurfaceRef{};
            state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
            statusText = unavailableStatus;
            return;
        }
    }

    if (!SameSurfaceRef(state.selectedSurface3D, surface)) {
        ResetSurface3DUiState();
    }

    if (IsWallSurface(surface.kind)) {
        SelectTopologySideDef(
                surface.topologySideDefId,
                SurfaceKindToTopologyWallPart(surface.kind));
    } else {
        SelectTopologySector(surface.topologySectorId);
    }
    if (hasAuthoringGraph) {
        const SectorAuthoringSelectionTarget authoringSelection =
                MakeSectorEditorAuthoringSelectionTargetForSurfaceTarget(authoringTarget);
        if (authoringSelection.kind == SectorAuthoringSelectionKind::Line) {
            SelectSectorEditorAuthoringLine(state, authoringSelection.lineId);
        } else if (authoringSelection.kind == SectorAuthoringSelectionKind::FaceAnchor) {
            SelectSectorEditorAuthoringFaceAnchor(state, authoringSelection.faceAnchorId);
        }
    }
    state.selectedSurface3D = surface;
    state.selectedTopologySurface3D = target;
}

bool SectorEditor::IsValidSurfaceRef(SectorSurfaceRef surface) const
{
    if (surface.kind == SectorSurfaceKind::None) {
        return false;
    }

    if (IsWallSurface(surface.kind)) {
        const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(
                state.topologyMap,
                surface.topologySideDefId);
        if (sideDef == nullptr
                || sideDef->lineDefId != surface.topologyLineDefId
                || sideDef->side != surface.topologySide) {
            return false;
        }
        return surface.kind != SectorSurfaceKind::Middle
                || IsTopologyMiddleEligible(state.topologyMap, sideDef);
    }
    return FindSectorTopologySector(state.topologyMap, surface.topologySectorId) != nullptr;
}

bool SectorEditor::SameSurfaceRef(SectorSurfaceRef a, SectorSurfaceRef b) const
{
    return a.kind == b.kind
            && a.topologySectorId == b.topologySectorId
            && a.topologyLineDefId == b.topologyLineDefId
            && a.topologySideDefId == b.topologySideDefId
            && a.topologySide == b.topologySide;
}

TopologySurfaceEditTarget SectorEditor::TopologyEditTargetForSurface(SectorSurfaceRef surface) const
{
    TopologySurfaceEditTarget target;
    target.kind = SurfaceKindToTopologyEditTargetKind(surface.kind);
    target.sectorId = surface.topologySectorId;
    target.lineDefId = surface.topologyLineDefId;
    target.sideDefId = surface.topologySideDefId;
    target.side = surface.topologySide;
    return target;
}

bool SectorEditor::IsValidTopologySurfaceEditTarget(TopologySurfaceEditTarget target) const
{
    return game::IsValidMaterialSurfaceTarget(state.topologyMap, target);
}

void SectorEditor::ResetSurface3DUiState()
{
    uiState.surface3DUvScaleUInput = engine::UIFloatInputState{};
    uiState.surface3DUvScaleVInput = engine::UIFloatInputState{};
    uiState.surface3DUvOffsetUInput = engine::UIFloatInputState{};
    uiState.surface3DUvOffsetVInput = engine::UIFloatInputState{};
    uiState.surface3DDecalOpacityInput = engine::UIFloatInputState{};
    uiState.surface3DDecalBloomIntensityInput = engine::UIFloatInputState{};
}

void SectorEditor::ClearTopologySelectionOnly()
{
    state.lightDrag = LightDragState{};
    state.topologySelectionKind = TopologySelectionKind::None;
    state.selectedTopologySectorId = -1;
    state.selectedTopologyVertexId = -1;
    state.selectedTopologySideDefId = -1;
    state.selectedTopologyLineDefId = -1;
    state.selectedTopologyLightId = -1;
    state.selectedTopologyDynamicLightId = -1;
    state.selectedTopologySideKind = SectorTopologySideKind::Front;
    state.inspectedTopologyVertexId = -1;
    state.selectedSurface3D = SectorSurfaceRef{};
    state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ResetSurface3DUiState();
    uiState.idBufferSectorIndex = -1;
    uiState.idBufferLightIndex = -1;
    SyncSelectedSectorIdBuffer();
    SyncSelectedLightIdBuffer();
}

void SectorEditor::ClearSelection()
{
    state.authoringVertexDrag = AuthoringVertexDragState{};
    state.topologySelectionKind = TopologySelectionKind::None;
    state.selectedTopologySectorId = -1;
    state.selectedTopologyVertexId = -1;
    state.selectedTopologySideDefId = -1;
    state.selectedTopologyLineDefId = -1;
    state.selectedTopologyLightId = -1;
    state.selectedTopologyDynamicLightId = -1;
    state.selectedTopologySideKind = SectorTopologySideKind::Front;
    state.inspectedTopologyVertexId = -1;
    state.selectedSurface3D = SectorSurfaceRef{};
    state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ClearSectorEditorAuthoringSelection(state);
    ResetSurface3DUiState();
    uiState.ambientIntensityInput = engine::UIFloatInputState{};
    uiState.ambientRedInput = engine::UIIntInputState{};
    uiState.ambientGreenInput = engine::UIIntInputState{};
    uiState.ambientBlueInput = engine::UIIntInputState{};
    uiState.inspectorScroll.offset = Vector2{};
    SyncSelectedSectorIdBuffer();
    SyncSelectedLightIdBuffer();
}

void SectorEditor::SelectAuthoringLine(int lineId)
{
    if (FindSectorAuthoringLine(state.authoringGraph, lineId) == nullptr) {
        ClearSelection();
        return;
    }

    ClearSelection();
    SelectSectorEditorAuthoringLine(state, lineId);
    uiState.inspectorScroll.offset = Vector2{};
}

bool SectorEditor::DeleteSelectedAuthoringLine()
{
    const int lineId = state.selectedAuthoring.lineId;
    if (!DeleteSectorEditorSelectedAuthoringLine(state)) {
        statusText = "Select an authoring line to delete.";
        return false;
    }

    statusText = TextFormat("Deleted authoring line %d", lineId);
    return true;
}

void SectorEditor::SelectAuthoringVertex(int vertexId)
{
    if (FindSectorAuthoringVertex(state.authoringGraph, vertexId) == nullptr) {
        ClearSelection();
        return;
    }

    ClearSelection();
    SelectSectorEditorAuthoringVertex(state, vertexId);
    uiState.inspectorScroll.offset = Vector2{};
}

void SectorEditor::SelectAuthoringFaceAnchor(int faceAnchorId)
{
    if (FindSectorAuthoringFaceAnchor(state.authoringGraph, faceAnchorId) == nullptr) {
        ClearSelection();
        return;
    }

    ClearSelection();
    SelectSectorEditorAuthoringFaceAnchor(state, faceAnchorId);
    uiState.inspectorScroll.offset = Vector2{};
    uiState.floorInput = engine::UIFloatInputState{};
    uiState.ceilingInput = engine::UIFloatInputState{};
    uiState.ambientIntensityInput = engine::UIFloatInputState{};
    uiState.ambientRedInput = engine::UIIntInputState{};
    uiState.ambientGreenInput = engine::UIIntInputState{};
    uiState.ambientBlueInput = engine::UIIntInputState{};
    for (engine::UIFloatInputState& inputState : uiState.topologySectorUvInputs) {
        inputState = engine::UIFloatInputState{};
    }
}

bool SectorEditor::DeleteSelectedAuthoringVertex()
{
    const int vertexId = state.selectedAuthoring.vertexId;
    if (!DeleteSectorEditorSelectedAuthoringVertex(state)) {
        statusText = state.authoringDerivationStatus.empty()
                ? "Select an isolated authoring vertex to delete."
                : state.authoringDerivationStatus;
        return false;
    }

    statusText = TextFormat("Deleted authoring vertex %d", vertexId);
    return true;
}

bool SectorEditor::HasAuthoringGraphData() const
{
    return !state.authoringGraph.vertices.empty()
            || !state.authoringGraph.lines.empty()
            || !state.authoringGraph.lineSides.empty()
            || !state.authoringGraph.faceAnchors.empty();
}

bool SectorEditor::IsSelectedSurface3DFlatTarget(TopologySurfaceEditTarget target) const
{
    if (target.kind == TopologySurfaceEditTargetKind::SectorFloor) {
        return state.selectedSurface3D.kind == SectorSurfaceKind::Floor
                && state.selectedSurface3D.topologySectorId == target.sectorId;
    }
    if (target.kind == TopologySurfaceEditTargetKind::SectorCeiling) {
        return state.selectedSurface3D.kind == SectorSurfaceKind::Ceiling
                && state.selectedSurface3D.topologySectorId == target.sectorId;
    }
    return false;
}

bool SectorEditor::EnsureSelectedSurface3DAuthoringMappingCurrent()
{
    std::string unavailableStatus;
    if (ClearSelectedSectorEditorSurface3DIfAuthoringMappingUnavailable(
                state,
                &unavailableStatus)) {
        return true;
    }
    ResetSurface3DUiState();
    statusText = unavailableStatus;
    return false;
}

bool SectorEditor::FinishAuthoringSideMaterialActionResult(
        TopologySurfaceEditTarget target,
        const SectorEditorMaterialActionResult& result,
        const SectorTopologyMap& editedTopology,
        engine::AssetManager* assets)
{
    if (!result.changed) {
        if (!result.status.empty()) {
            statusText = result.status;
        }
        return false;
    }

    const SectorTopologySideDef* editedSideDef =
            FindSectorTopologySideDef(editedTopology, target.sideDefId);
    if (editedSideDef == nullptr) {
        statusText = "Selected authoring side material target is no longer valid.";
        return false;
    }

    if (result.resetSurface3DUi) {
        ResetSurface3DUiState();
    }
    if (result.resetSectorUvInputs) {
        for (engine::UIFloatInputState& inputState : uiState.topologySectorUvInputs) {
            inputState = engine::UIFloatInputState{};
        }
    }
    if (result.resetSideDefUvInputs) {
        for (engine::UIFloatInputState& inputState : uiState.topologySideDefUvInputs) {
            inputState = engine::UIFloatInputState{};
        }
    }
    if (result.resetDecalInputs) {
        for (engine::UIFloatInputState& inputState : uiState.topologySectorDecalOpacityInputs) {
            inputState = engine::UIFloatInputState{};
        }
        for (engine::UIFloatInputState& inputState : uiState.topologySectorDecalBloomIntensityInputs) {
            inputState = engine::UIFloatInputState{};
        }
        uiState.topologySideDefDecalOpacityInput = engine::UIFloatInputState{};
        uiState.topologySideDefDecalBloomIntensityInput = engine::UIFloatInputState{};
        uiState.surface3DDecalOpacityInput = engine::UIFloatInputState{};
        uiState.surface3DDecalBloomIntensityInput = engine::UIFloatInputState{};
    }
    if (result.closeDecalTintModal) {
        state.decalTintModal = DecalTintModalState{};
    }

    state.topologyRenderWarning.clear();
    const bool refreshed = MutateSectorEditorAuthoringSideForTopologySideDef(
            state,
            target.sideDefId,
            result.status.c_str(),
            [editedSideDef](SectorAuthoringLineSide& side) {
                side.wall = editedSideDef->wall;
                side.lower = editedSideDef->lower;
                side.upper = editedSideDef->upper;
                side.middle = editedSideDef->middle;
                return true;
            });
    if (!refreshed) {
        statusText = "Wall material edit unavailable: selected sidedef has no authoring side mapping";
    }
    if (assets != nullptr && refreshed && state.mode == SectorEditorMode::Preview3D && preview.IsRendererReady()) {
        return RebuildPreviewMeshesPreservingView(*assets);
    }
    return refreshed;
}

bool SectorEditor::ApplyAuthoringSideMaterialAction(
        TopologySurfaceEditTarget target,
        engine::AssetManager* assets,
        const std::function<SectorEditorMaterialActionResult(SectorTopologyMap&)>& action)
{
    if (!IsWallTopologyEditTarget(target.kind) || !HasAuthoringGraphData()) {
        return false;
    }
    if (state.authoringDerivationState != SectorEditorAuthoringDerivationState::ValidCurrent
            || state.authoringDerivedTopologyStale
            || !state.authoringDerivation.success) {
        statusText = "Wall material edit unavailable: derived topology is not current";
        return true;
    }
    SectorAuthoringSideId sideId;
    if (!FindSectorEditorAuthoringSideIdForTopologySideDef(
                state,
                target.sideDefId,
                sideId)) {
        statusText = "Wall material edit unavailable: selected sidedef has no authoring side mapping";
        return true;
    }
    if (!action) {
        return false;
    }

    SectorTopologyMap editedTopology = state.topologyMap;
    return FinishAuthoringSideMaterialActionResult(
            target,
            action(editedTopology),
            editedTopology,
            assets);
}

bool SectorEditor::ApplyAuthoringFaceAnchorFlatMaterialAction(
        TopologySurfaceEditTarget target,
        engine::AssetManager* assets,
        const std::function<SectorEditorMaterialActionResult(SectorTopologyMap&)>& action)
{
    SectorSurfaceRef surface = state.selectedSurface3D;
    if (!IsSelectedSurface3DFlatTarget(target)
            && (target.kind == TopologySurfaceEditTargetKind::SectorFloor
                    || target.kind == TopologySurfaceEditTargetKind::SectorCeiling)) {
        surface = SectorSurfaceRef{};
        surface.kind = target.kind == TopologySurfaceEditTargetKind::SectorFloor
                ? SectorSurfaceKind::Floor
                : SectorSurfaceKind::Ceiling;
        surface.topologySectorId = target.sectorId;
    }

    SectorEditorAuthoringFlatMaterialActionResult result;
    if (!game::ApplySectorEditorAuthoringFaceAnchorFlatMaterialAction(
                state,
                surface,
                target,
                action,
                &result)) {
        return false;
    }

    if (result.materialResult.resetSurface3DUi) {
        ResetSurface3DUiState();
    }
    if (result.materialResult.resetSectorUvInputs) {
        for (engine::UIFloatInputState& inputState : uiState.topologySectorUvInputs) {
            inputState = engine::UIFloatInputState{};
        }
    }
    if (result.materialResult.resetDecalInputs) {
        for (engine::UIFloatInputState& inputState : uiState.topologySectorDecalOpacityInputs) {
            inputState = engine::UIFloatInputState{};
        }
        for (engine::UIFloatInputState& inputState : uiState.topologySectorDecalBloomIntensityInputs) {
            inputState = engine::UIFloatInputState{};
        }
        uiState.surface3DDecalOpacityInput = engine::UIFloatInputState{};
        uiState.surface3DDecalBloomIntensityInput = engine::UIFloatInputState{};
    }
    if (result.materialResult.closeDecalTintModal) {
        state.decalTintModal = DecalTintModalState{};
    }

    if (!result.status.empty()) {
        statusText = result.status;
    }
    if (assets != nullptr && result.changed && state.mode == SectorEditorMode::Preview3D && preview.IsRendererReady()) {
        RebuildPreviewMeshesPreservingView(*assets);
    }
    return true;
}

const SectorTopologyDecalLayer* SectorEditor::DecalForSurface(TopologySurfaceEditTarget target) const
{
    return game::DecalForMaterialSurface(state.topologyMap, target);
}

SectorTopologyDecalLayer* SectorEditor::MutableDecalForSurface(TopologySurfaceEditTarget target)
{
    return game::MutableDecalForMaterialSurface(state.topologyMap, target);
}

const SectorTopologyUvSettings* SectorEditor::UvForSurface(
        TopologySurfaceEditTarget target,
        TopologyMaterialLayer layer) const
{
    return game::UvForMaterialSurface(state.topologyMap, target, layer);
}

SectorTopologyUvSettings* SectorEditor::MutableUvForSurface(
        TopologySurfaceEditTarget target,
        TopologyMaterialLayer layer)
{
    return game::MutableUvForMaterialSurface(state.topologyMap, target, layer);
}

bool SectorEditor::IsDecalAssigned(TopologySurfaceEditTarget target) const
{
    return game::IsMaterialDecalAssigned(state.topologyMap, target);
}

std::string SectorEditor::CurrentTextureForSurface(
        TopologySurfaceEditTarget target,
        TopologyMaterialLayer layer) const
{
    return game::CurrentTextureForMaterialSurface(state.topologyMap, target, layer);
}

bool SectorEditor::CopyTopologyMaterial(TopologySurfaceEditTarget target)
{
    TopologyMaterialPayload payload;
    std::string status;
    if (!game::CopyMaterialSurface(state.topologyMap, target, payload, status)) {
        statusText = status;
        return false;
    }
    state.copiedTopologyMaterial = payload;
    statusText = status;
    return true;
}

bool SectorEditor::PasteTopologyMaterial(TopologySurfaceEditTarget target, engine::AssetManager& assets)
{
    if (IsWallTopologyEditTarget(target.kind) && HasAuthoringGraphData()) {
        return ApplyAuthoringSideMaterialAction(
                target,
                &assets,
                [this, target](SectorTopologyMap& map) {
                    return game::PasteMaterialSurface(map, target, state.copiedTopologyMaterial);
                });
    }
    if (IsFlatTopologySurfaceTarget(target) && HasAuthoringGraphData()) {
        return ApplyAuthoringFaceAnchorFlatMaterialAction(
                target,
                &assets,
                [this, target](SectorTopologyMap& map) {
                    return game::PasteMaterialSurface(map, target, state.copiedTopologyMaterial);
                });
    }
    return FinishMaterialActionResult(
            game::PasteMaterialSurface(state.topologyMap, target, state.copiedTopologyMaterial),
            &assets);
}

bool SectorEditor::ApplySurface3DUvValue(
        TopologySurfaceEditTarget target,
        TopologyMaterialLayer layer,
        int component,
        float value,
        engine::AssetManager& assets)
{
    if (IsWallTopologyEditTarget(target.kind) && HasAuthoringGraphData()) {
        const SectorSurfaceKind surfaceKind = state.selectedSurface3D.kind;
        return ApplyAuthoringSideMaterialAction(
                target,
                &assets,
                [target, layer, component, value, surfaceKind](SectorTopologyMap& map) {
                    return game::ApplySurfaceUvValue(
                            map,
                            target,
                            layer,
                            surfaceKind,
                            component,
                            value);
                });
    }
    if (IsFlatTopologySurfaceTarget(target) && HasAuthoringGraphData()) {
        const SectorSurfaceKind surfaceKind = state.selectedSurface3D.kind;
        return ApplyAuthoringFaceAnchorFlatMaterialAction(
                target,
                &assets,
                [target, layer, component, value, surfaceKind](SectorTopologyMap& map) {
                    return game::ApplySurfaceUvValue(
                            map,
                            target,
                            layer,
                            surfaceKind,
                            component,
                            value);
                });
    }
    return FinishMaterialActionResult(
            game::ApplySurfaceUvValue(
                    state.topologyMap,
                    target,
                    layer,
                    state.selectedSurface3D.kind,
                    component,
                    value),
            &assets);
}

bool SectorEditor::ApplySurfaceDecalOpacity(
        TopologySurfaceEditTarget target,
        float opacity,
        engine::AssetManager* assets)
{
    if (IsWallTopologyEditTarget(target.kind) && HasAuthoringGraphData()) {
        return ApplyAuthoringSideMaterialAction(
                target,
                assets,
                [target, opacity](SectorTopologyMap& map) {
                    return game::ApplySurfaceDecalOpacity(map, target, opacity);
                });
    }
    if (IsFlatTopologySurfaceTarget(target) && HasAuthoringGraphData()) {
        return ApplyAuthoringFaceAnchorFlatMaterialAction(
                target,
                assets,
                [target, opacity](SectorTopologyMap& map) {
                    return game::ApplySurfaceDecalOpacity(map, target, opacity);
                });
    }
    return FinishMaterialActionResult(game::ApplySurfaceDecalOpacity(state.topologyMap, target, opacity), assets);
}

bool SectorEditor::ApplySurfaceDecalEmissive(
        TopologySurfaceEditTarget target,
        bool emissive,
        engine::AssetManager* assets)
{
    if (IsWallTopologyEditTarget(target.kind) && HasAuthoringGraphData()) {
        return ApplyAuthoringSideMaterialAction(
                target,
                assets,
                [target, emissive](SectorTopologyMap& map) {
                    return game::ApplySurfaceDecalEmissive(map, target, emissive);
                });
    }
    if (IsFlatTopologySurfaceTarget(target) && HasAuthoringGraphData()) {
        return ApplyAuthoringFaceAnchorFlatMaterialAction(
                target,
                assets,
                [target, emissive](SectorTopologyMap& map) {
                    return game::ApplySurfaceDecalEmissive(map, target, emissive);
                });
    }
    return FinishMaterialActionResult(game::ApplySurfaceDecalEmissive(state.topologyMap, target, emissive), assets);
}

bool SectorEditor::ApplySurfaceDecalTint(
        TopologySurfaceEditTarget target,
        Vector3 tint,
        engine::AssetManager* assets)
{
    if (IsWallTopologyEditTarget(target.kind) && HasAuthoringGraphData()) {
        return ApplyAuthoringSideMaterialAction(
                target,
                assets,
                [target, tint](SectorTopologyMap& map) {
                    return game::ApplySurfaceDecalTint(map, target, tint);
                });
    }
    if (IsFlatTopologySurfaceTarget(target) && HasAuthoringGraphData()) {
        return ApplyAuthoringFaceAnchorFlatMaterialAction(
                target,
                assets,
                [target, tint](SectorTopologyMap& map) {
                    return game::ApplySurfaceDecalTint(map, target, tint);
                });
    }
    return FinishMaterialActionResult(game::ApplySurfaceDecalTint(state.topologyMap, target, tint), assets);
}

bool SectorEditor::ApplySurfaceDecalBloomIntensity(
        TopologySurfaceEditTarget target,
        float bloomIntensity,
        engine::AssetManager* assets)
{
    if (IsWallTopologyEditTarget(target.kind) && HasAuthoringGraphData()) {
        return ApplyAuthoringSideMaterialAction(
                target,
                assets,
                [target, bloomIntensity](SectorTopologyMap& map) {
                    return game::ApplySurfaceDecalBloomIntensity(map, target, bloomIntensity);
                });
    }
    if (IsFlatTopologySurfaceTarget(target) && HasAuthoringGraphData()) {
        return ApplyAuthoringFaceAnchorFlatMaterialAction(
                target,
                assets,
                [target, bloomIntensity](SectorTopologyMap& map) {
                    return game::ApplySurfaceDecalBloomIntensity(map, target, bloomIntensity);
                });
    }
    return FinishMaterialActionResult(
            game::ApplySurfaceDecalBloomIntensity(state.topologyMap, target, bloomIntensity),
            assets);
}

bool SectorEditor::OpenDecalTintModal(TopologySurfaceEditTarget target)
{
    DecalTintModalState modal;
    std::string status;
    if (!game::BuildDecalTintModal(state.topologyMap, target, modal, status)) {
        statusText = status;
        return false;
    }
    state.decalTintModal = modal;
    return true;
}

bool SectorEditor::ClearSurfaceDecal(
        TopologySurfaceEditTarget target,
        engine::AssetManager* assets)
{
    if (IsWallTopologyEditTarget(target.kind) && HasAuthoringGraphData()) {
        return ApplyAuthoringSideMaterialAction(
                target,
                assets,
                [target](SectorTopologyMap& map) {
                    return game::ClearSurfaceDecal(map, target);
                });
    }
    if (IsFlatTopologySurfaceTarget(target) && HasAuthoringGraphData()) {
        return ApplyAuthoringFaceAnchorFlatMaterialAction(
                target,
                assets,
                [target](SectorTopologyMap& map) {
                    return game::ClearSurfaceDecal(map, target);
                });
    }
    return FinishMaterialActionResult(game::ClearSurfaceDecal(state.topologyMap, target), assets);
}

bool SectorEditor::ClearMiddleTexture(
        TopologySurfaceEditTarget target,
        engine::AssetManager* assets)
{
    if (IsWallTopologyEditTarget(target.kind) && HasAuthoringGraphData()) {
        return ApplyAuthoringSideMaterialAction(
                target,
                assets,
                [target](SectorTopologyMap& map) {
                    return game::ClearMiddleTexture(map, target);
                });
    }
    return FinishMaterialActionResult(game::ClearMiddleTexture(state.topologyMap, target), assets);
}

bool SectorEditor::ResetSurface3DUv(
        TopologySurfaceEditTarget target,
        TopologyMaterialLayer layer,
        engine::AssetManager& assets)
{
    if (IsWallTopologyEditTarget(target.kind) && HasAuthoringGraphData()) {
        const SectorSurfaceKind surfaceKind = state.selectedSurface3D.kind;
        return ApplyAuthoringSideMaterialAction(
                target,
                &assets,
                [target, layer, surfaceKind](SectorTopologyMap& map) {
                    return game::ResetSurfaceUv(map, target, layer, surfaceKind);
                });
    }
    if (IsFlatTopologySurfaceTarget(target) && HasAuthoringGraphData()) {
        const SectorSurfaceKind surfaceKind = state.selectedSurface3D.kind;
        return ApplyAuthoringFaceAnchorFlatMaterialAction(
                target,
                &assets,
                [target, layer, surfaceKind](SectorTopologyMap& map) {
                    return game::ResetSurfaceUv(map, target, layer, surfaceKind);
                });
    }
    return FinishMaterialActionResult(
            game::ResetSurfaceUv(state.topologyMap, target, layer, state.selectedSurface3D.kind),
            &assets);
}

bool SectorEditor::FitSelectedDecal(
        TopologySurfaceEditTarget target,
        engine::AssetManager* assets)
{
    if (IsWallTopologyEditTarget(target.kind) && HasAuthoringGraphData()) {
        return ApplyAuthoringSideMaterialAction(
                target,
                assets,
                [target](SectorTopologyMap& map) {
                    return game::FitSelectedDecal(map, target);
                });
    }
    if (IsFlatTopologySurfaceTarget(target) && HasAuthoringGraphData()) {
        return ApplyAuthoringFaceAnchorFlatMaterialAction(
                target,
                assets,
                [target](SectorTopologyMap& map) {
                    return game::FitSelectedDecal(map, target);
                });
    }
    return FinishMaterialActionResult(game::FitSelectedDecal(state.topologyMap, target), assets);
}

bool SectorEditor::FitSelectedFlatDecal(
        TopologySurfaceEditTarget target,
        engine::AssetManager* assets)
{
    if (IsFlatTopologySurfaceTarget(target) && HasAuthoringGraphData()) {
        return ApplyAuthoringFaceAnchorFlatMaterialAction(
                target,
                assets,
                [target](SectorTopologyMap& map) {
                    return game::FitSelectedFlatDecal(map, target);
                });
    }
    return FinishMaterialActionResult(game::FitSelectedFlatDecal(state.topologyMap, target), assets);
}

bool SectorEditor::FitSelectedWallMaterial(
        TopologySurfaceEditTarget target,
        TopologyUvFitMode mode,
        engine::AssetManager* assets,
        TopologyMaterialLayer layer)
{
    if (IsWallTopologyEditTarget(target.kind) && HasAuthoringGraphData()) {
        return ApplyAuthoringSideMaterialAction(
                target,
                assets,
                [target, mode, layer](SectorTopologyMap& map) {
                    return game::FitSelectedWallMaterial(map, target, mode, layer);
                });
    }
    return FinishMaterialActionResult(
            game::FitSelectedWallMaterial(state.topologyMap, target, mode, layer),
            assets);
}

bool SectorEditor::AlignSelectedWallMaterialVertical(
        TopologySurfaceEditTarget target,
        engine::AssetManager* assets,
        TopologyMaterialLayer layer)
{
    if (IsWallTopologyEditTarget(target.kind) && HasAuthoringGraphData()) {
        return ApplyAuthoringSideMaterialAction(
                target,
                assets,
                [target, layer](SectorTopologyMap& map) {
                    return game::AlignSelectedWallMaterialVertical(map, target, layer);
                });
    }
    return FinishMaterialActionResult(
            game::AlignSelectedWallMaterialVertical(state.topologyMap, target, layer),
            assets);
}

bool SectorEditor::AlignSelectedWallMaterialU(
        TopologySurfaceEditTarget target,
        TopologyUAlignDirection direction,
        engine::AssetManager* assets,
        TopologyMaterialLayer layer)
{
    if (IsWallTopologyEditTarget(target.kind) && HasAuthoringGraphData()) {
        return ApplyAuthoringSideMaterialAction(
                target,
                assets,
                [target, direction, layer](SectorTopologyMap& map) {
                    return game::AlignSelectedWallMaterialU(map, target, direction, layer);
                });
    }
    return FinishMaterialActionResult(
            game::AlignSelectedWallMaterialU(state.topologyMap, target, direction, layer),
            assets);
}

bool SectorEditor::RebuildPreviewMeshesPreservingView(engine::AssetManager& assets)
{
    if (!preview.IsRendererReady()) {
        return false;
    }

    std::string gateMessage;
    if (!CanUseCurrentAuthoringDerivedTopologyForPreview(state, &gateMessage)) {
        statusText = gateMessage.empty() ? "3D mode rebuild failed: derived topology is not current" : gateMessage;
        return false;
    }

    if (state.previewControlMode == SectorPreviewControlMode::Gameplay) {
        ClearSectorFpsLandingDip(state.landingDipState);
        ApplyGameplayPoseToPreview();
    }
    const SectorViewPose pose = preview.RendererPose();
    const bool mouseLook = state.freeflyController.mouseLookEnabled;
    const SectorSurfaceRef selected = state.selectedSurface3D;
    const TopologySurfaceEditTarget selectedTarget = state.selectedTopologySurface3D;

    std::string error;
    if (!preview.RebuildRendererResources(assets, state.topologyMap, "sector_editor_preview", error)) {
        state.sectorCollisionWorldValid = false;
        state.sectorCollisionWorldWarning.clear();
        state.previewCollisionSectorId = 0;
        state.fpsControllerState.currentSectorId = 0;
        state.previewVerticalResult = SectorFpsVerticalResult{};
        state.previewMoveResult = SectorCollisionMoveResult{};
        state.previewCollisionNoclipFallback = false;
        ClearSectorFpsLandingDip(state.landingDipState);
        if (StartsWith(error, "Preview failed:")) {
            statusText = std::string{"3D mode failed:"} + error.substr(std::strlen("Preview failed:"));
        } else {
            statusText = error.empty() ? "3D mode rebuild failed" : error;
        }
        state.mode = SectorEditorMode::Edit2D;
        LeaveSectorFreeflyController();
        return false;
    }

    preview.ApplyRendererPose(pose);
    ResetSectorFreeflyController(state.freeflyController, pose);
    SetSectorFreeflyMouseLookEnabled(state.freeflyController, mouseLook);
    const bool selectedStillValid = IsValidSurfaceRef(selected);
    state.selectedSurface3D = selectedStillValid ? selected : SectorSurfaceRef{};
    state.selectedTopologySurface3D = selectedStillValid && IsValidTopologySurfaceEditTarget(selectedTarget)
            ? selectedTarget
            : TopologySurfaceEditTarget{};
    RebuildSectorCollisionWorld();
    return true;
}

std::string SectorEditor::CurrentTextureForPickerTarget() const
{
    return game::CurrentTextureForPickerTarget(state);
}

void SectorEditor::OpenTopologyTexturePicker(
        int sectorId,
        TopologySectorTextureField field,
        TopologyMaterialLayer layer)
{
    const bool opened = HasAuthoringGraphData()
            ? game::OpenAuthoringFaceAnchorTexturePicker(state, sectorId, field, layer)
            : game::OpenTopologyTexturePicker(state, sectorId, field, layer);
    if (!opened) {
        statusText = "No topology sector texture target";
    }
}

void SectorEditor::OpenTopologySideDefTexturePicker(
        int sideDefId,
        TopologyWallPart wallPart,
        TopologyMaterialLayer layer)
{
    const bool opened = HasAuthoringGraphData()
            ? game::OpenAuthoringSideTexturePicker(state, sideDefId, wallPart, layer)
            : game::OpenTopologySideDefTexturePicker(state, sideDefId, wallPart, layer);
    if (!opened) {
        statusText = "No topology sidedef texture target";
    }
}

void SectorEditor::OpenMapSkyTexturePicker()
{
    if (!game::OpenMapSkyTexturePicker(state)) {
        statusText = "No sky texture target";
    }
}

void SectorEditor::ApplyTexturePickerSelection(engine::AssetManager& assets)
{
    if (state.texturePicker.topologyTargetKind == TopologyTexturePickerTargetKind::AuthoringFaceAnchor
            || state.texturePicker.topologyTargetKind == TopologyTexturePickerTargetKind::AuthoringSide) {
        const SectorEditorTexturePickerApplyResult result =
                game::ApplyAuthoringTexturePickerSelection(state);
        if (!result.status.empty()) {
            statusText = result.status;
        }
        if (result.changed
                && result.rebuildPreviewOnApply
                && state.mode == SectorEditorMode::Preview3D
                && preview.IsRendererReady()) {
            RebuildPreviewMeshesPreservingView(assets);
        }
        return;
    }

    const bool routeAuthoringSideMaterial =
            HasAuthoringGraphData()
            && state.texturePicker.open
            && state.texturePicker.topologyTargetKind == TopologyTexturePickerTargetKind::SideDef;
    const bool routeAuthoringFlatMaterial =
            HasAuthoringGraphData()
            && state.texturePicker.open
            && state.texturePicker.authoringSurface3DFlatTarget
            && state.texturePicker.topologyTargetKind == TopologyTexturePickerTargetKind::Sector
            && (state.texturePicker.topologyField == TopologySectorTextureField::Floor
                    || state.texturePicker.topologyField == TopologySectorTextureField::Ceiling);
    const int authoringSideDefId = routeAuthoringSideMaterial
            ? state.texturePicker.topologySideDefId
            : -1;
    const TopologyMaterialLayer authoringPickerLayer = routeAuthoringSideMaterial
            ? state.texturePicker.topologyLayer
            : TopologyMaterialLayer::Base;
    TopologySurfaceEditTarget authoringSideTarget;
    TopologySurfaceEditTarget authoringFlatTarget;
    SectorSurfaceRef authoringFlatSurface;
    SectorTopologyMap topologyBeforePicker;
    if (routeAuthoringSideMaterial || routeAuthoringFlatMaterial) {
        if (state.authoringDerivationState != SectorEditorAuthoringDerivationState::ValidCurrent
                || state.authoringDerivedTopologyStale
                || !state.authoringDerivation.success) {
            statusText = routeAuthoringSideMaterial
                    ? "Wall material edit unavailable: derived topology is not current"
                    : "3D surface edit unavailable: derived topology is not current";
            state.texturePicker = TexturePickerState{};
            return;
        }
    }
    if (routeAuthoringSideMaterial) {
        SectorAuthoringSideId sideId;
        if (!FindSectorEditorAuthoringSideIdForTopologySideDef(
                    state,
                    authoringSideDefId,
                    sideId)) {
            statusText = "Wall material edit unavailable: selected sidedef has no authoring side mapping";
            state.texturePicker = TexturePickerState{};
            return;
        }
        topologyBeforePicker = state.topologyMap;
        const SectorTopologySideDef* sideDef =
                FindSectorTopologySideDef(state.topologyMap, authoringSideDefId);
        if (sideDef != nullptr) {
            switch (state.texturePicker.topologyWallPart) {
                case TopologyWallPart::Wall:
                    authoringSideTarget.kind = TopologySurfaceEditTargetKind::SideDefWall;
                    break;
                case TopologyWallPart::Lower:
                    authoringSideTarget.kind = TopologySurfaceEditTargetKind::SideDefLower;
                    break;
                case TopologyWallPart::Upper:
                    authoringSideTarget.kind = TopologySurfaceEditTargetKind::SideDefUpper;
                    break;
                case TopologyWallPart::Middle:
                    authoringSideTarget.kind = TopologySurfaceEditTargetKind::SideDefMiddle;
                    break;
            }
            authoringSideTarget.sectorId = sideDef->sectorId;
            authoringSideTarget.lineDefId = sideDef->lineDefId;
            authoringSideTarget.sideDefId = sideDef->id;
            authoringSideTarget.side = sideDef->side;
        }
    }
    if (routeAuthoringFlatMaterial) {
        authoringFlatTarget.kind = state.texturePicker.topologyField == TopologySectorTextureField::Floor
                ? TopologySurfaceEditTargetKind::SectorFloor
                : TopologySurfaceEditTargetKind::SectorCeiling;
        authoringFlatTarget.sectorId = state.texturePicker.topologySectorId;
        authoringFlatSurface.kind = state.texturePicker.topologyField == TopologySectorTextureField::Floor
                ? SectorSurfaceKind::Floor
                : SectorSurfaceKind::Ceiling;
        authoringFlatSurface.topologySectorId = state.texturePicker.topologySectorId;

        SectorEditorAuthoringSurfaceTarget surfaceTarget;
        std::string unavailableStatus;
        if (!ResolveSectorEditorAuthoringSurfaceTarget(
                    state,
                    authoringFlatSurface,
                    surfaceTarget,
                    &unavailableStatus)
                || surfaceTarget.kind != SectorEditorAuthoringSurfaceTargetKind::FaceAnchor) {
            statusText = unavailableStatus.empty()
                    ? "3D flat surface edit unavailable: selected surface has no face anchor mapping"
                    : unavailableStatus;
            state.texturePicker = TexturePickerState{};
            return;
        }
        topologyBeforePicker = state.topologyMap;
    }

    const SectorEditorTexturePickerApplyResult result = game::ApplyTexturePickerSelection(state);
    if (routeAuthoringSideMaterial) {
        const SectorTopologyMap editedTopology = state.topologyMap;
        state.topologyMap = topologyBeforePicker;
        if (result.changed) {
            SectorEditorMaterialActionResult materialResult;
            materialResult.changed = true;
            materialResult.status = result.status;
            materialResult.resetSideDefUvInputs = true;
            materialResult.resetDecalInputs = authoringPickerLayer == TopologyMaterialLayer::Decal;
            FinishAuthoringSideMaterialActionResult(
                    authoringSideTarget,
                    materialResult,
                    editedTopology,
                    &assets);
        }
        return;
    }
    if (routeAuthoringFlatMaterial) {
        const SectorTopologyMap editedTopology = state.topologyMap;
        state.topologyMap = topologyBeforePicker;
        if (result.changed) {
            ApplyAuthoringFaceAnchorFlatMaterialAction(
                    authoringFlatTarget,
                    &assets,
                    [authoringFlatTarget, editedTopology, result](SectorTopologyMap& map) {
                        map = editedTopology;
                        SectorEditorMaterialActionResult materialResult;
                        materialResult.changed = true;
                        materialResult.status = result.status;
                        materialResult.resetSurface3DUi = true;
                        materialResult.resetSectorUvInputs = true;
                        materialResult.resetDecalInputs = true;
                        return materialResult;
                    });
        }
        return;
    }

    if (result.changed) {
        if (result.useMaterialMutationFinish) {
            FinishTopologyMaterialMutation(result.status.c_str(), &assets);
        } else {
            state.topologyRenderWarning.clear();
            MarkTopologyDocumentEdited(result.status.c_str());
            if (result.rebuildPreviewOnApply && state.mode == SectorEditorMode::Preview3D && preview.IsRendererReady()) {
                RebuildPreviewMeshesPreservingView(assets);
            }
        }
    }
}

} // namespace game
