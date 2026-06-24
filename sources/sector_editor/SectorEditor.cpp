#include "sector_editor/SectorEditor.h"

#include "engine/assets/TextureLoadFlags.h"
#include "engine/input/InputEvents.h"
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
#include "sector_demo/SectorGeneratedGeometry.h"
#include "sector_demo/SectorLightmap.h"
#include "sector_demo/SectorTextureTypes.h"
#include "sector_demo/SectorTopologyCreation.h"
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
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>
#include <vector>

namespace game {

bool SectorEditor::Init(engine::AssetManager& assets)
{
    Shutdown(assets);
    ResetToBlankMap(assets);
    return true;
}

void SectorEditor::Shutdown(engine::AssetManager& assets)
{
    ShutdownLightmapBake();
    preview.Shutdown(assets);
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
        CancelVertexDrag(nullptr);
        CancelLightDrag(nullptr);
        CancelPendingSector(nullptr);
        return;
    }

    if (state.mode == SectorEditorMode::Preview3D) {
        if (state.texturePicker.open || HasDocumentModalOpen()) {
            return;
        }
        UpdatePreview3D(input, dt);
        return;
    }

    if (state.pendingSector.active
            && state.pendingSector.kind == PendingSectorDrawKind::InsertInside
            && !IsPendingInsertParentValid()) {
        CancelPendingSector("Insert sector cancelled: parent sector changed");
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

    if (state.currentTool != SectorEditorTool::Sector
            && state.currentTool != SectorEditorTool::InsertSectorInside) {
        return snapped;
    }

    const float threshold = std::max(
            SectorWorldToAuthoringDistance(ScreenVertexSnapPixels / std::max(1.0f, state.viewZoom)),
            grid * 0.20f
    );
    float bestDistance2 = threshold * threshold;
    bool found = false;
    Vector2 best = snapped;
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
    state.hasHoveredVertex = false;
    state.hoveredTopologyLightId = -1;
    state.hoveredTopologyVertexId = -1;
    state.hoveredTopologyVertexPoint = SectorTopologyCoordPoint{};

    if (!initialized || !IsMouseOverCanvas(input)) {
        return;
    }

    if (state.currentTool == SectorEditorTool::Move || state.currentTool == SectorEditorTool::Select) {
        const int lightId = FindTopologyLightNearScreenPoint(input.MousePosition());
        if (lightId >= 0) {
            state.hoveredTopologyLightId = lightId;
            if (!state.pendingTopologyVertexMerge.active) {
                state.inspectedTopologyVertexId = -1;
            }
        } else {
            int vertexId = -1;
            SectorTopologyCoordPoint point;
            if (FindTopologyVertexNearScreenPoint(input.MousePosition(), vertexId, point)) {
                state.hasHoveredVertex = true;
                state.hoveredTopologyVertexId = vertexId;
                state.hoveredTopologyVertexPoint = point;
                state.inspectedTopologyVertexId = vertexId;
            } else if (!state.pendingTopologyVertexMerge.active) {
                state.inspectedTopologyVertexId = -1;
            }
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
                    if (state.vertexDrag.active) {
                        CancelVertexDrag("Cancelled vertex move");
                    } else if (state.lightDrag.active) {
                        CancelLightDrag("Cancelled light move");
                    } else if (state.pendingTopologyVertexMerge.active) {
                        CancelPendingTopologyVertexMerge("Cancelled vertex merge");
                    } else if (state.pendingTopologyLineSplitAtPoint.active) {
                        CancelPendingTopologyLineSplitAtPoint("Cancelled split at point");
                    } else if (state.pendingTopologySectorCut.active) {
                        CancelPendingTopologySectorCut("Cancelled sector cut");
                    } else if (state.pendingSector.active) {
                        CancelPendingSector("Cancelled sector");
                    } else if (state.selectedTopologyLightId >= 0
                            || state.topologySelectionKind != TopologySelectionKind::None) {
                        ClearSelection();
                    } else {
                        state.currentTool = SectorEditorTool::Select;
                    }
                    engine::ConsumeEvent(event);
                    return;
                }

                if (state.pendingSector.active && event.key.key == KEY_BACKSPACE) {
                    RemoveLastPendingSectorPoint();
                    engine::ConsumeEvent(event);
                    return;
                }

                if (state.pendingSector.active && event.key.key == KEY_ENTER) {
                    FinalizePendingSector();
                    engine::ConsumeEvent(event);
                    return;
                }

                if (event.key.key == KEY_DELETE) {
                    if (state.topologySelectionKind == TopologySelectionKind::Light
                            && state.selectedTopologyLightId >= 0) {
                        DeleteSelectedLight();
                    } else if (state.topologySelectionKind == TopologySelectionKind::Sector
                            && state.selectedTopologySectorId >= 0) {
                        OpenDeleteSelectedTopologySectorConfirmation();
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

    if (state.pendingTopologyVertexMerge.active) {
        UpdatePendingTopologyVertexMerge(input);

        input.ForEachEvent(
                engine::InputEventType::MouseButtonPressed,
                true,
                [this](engine::InputEvent& event) {
                    if (event.mouseButton.button == MOUSE_RIGHT_BUTTON) {
                        CancelPendingTopologyVertexMerge("Cancelled vertex merge");
                        engine::ConsumeEvent(event);
                    }
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
                        CancelPendingTopologyVertexMerge("Cancelled vertex merge");
                        engine::ConsumeEvent(event);
                        return;
                    }
                    if (event.mouseClick.button == MOUSE_LEFT_BUTTON) {
                        CommitPendingTopologyVertexMerge();
                        engine::ConsumeEvent(event);
                    }
                }
        );
        return;
    }

    if (state.pendingTopologySectorCut.active) {
        UpdatePendingTopologySectorCut(input);

        input.ForEachEvent(
                engine::InputEventType::MouseButtonPressed,
                true,
                [this](engine::InputEvent& event) {
                    if (event.mouseButton.button == MOUSE_RIGHT_BUTTON) {
                        CancelPendingTopologySectorCut("Cancelled sector cut");
                        engine::ConsumeEvent(event);
                    }
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
                        CancelPendingTopologySectorCut("Cancelled sector cut");
                        engine::ConsumeEvent(event);
                        return;
                    }
                    if (event.mouseClick.button == MOUSE_LEFT_BUTTON) {
                        CommitPendingTopologySectorCut();
                        engine::ConsumeEvent(event);
                    }
                }
        );
        return;
    }

    if (state.vertexDrag.active || state.lightDrag.active) {
        if (state.vertexDrag.active) {
            UpdateVertexDrag(input);
        }
        if (state.lightDrag.active) {
            UpdateLightDrag(input);
        }

        input.ForEachEvent(
                engine::InputEventType::MouseButtonPressed,
                true,
                [this](engine::InputEvent& event) {
                    if (event.mouseButton.button == MOUSE_RIGHT_BUTTON) {
                        if (state.vertexDrag.active) {
                            CancelVertexDrag("Cancelled vertex move");
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
                        if (state.vertexDrag.active) {
                            FinishVertexDrag();
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

    if (state.pendingTopologyLineSplitAtPoint.active) {
        UpdatePendingTopologyLineSplitAtPoint(input);

        input.ForEachEvent(
                engine::InputEventType::MouseButtonPressed,
                true,
                [this](engine::InputEvent& event) {
                    if (event.mouseButton.button == MOUSE_RIGHT_BUTTON) {
                        CancelPendingTopologyLineSplitAtPoint("Cancelled split at point");
                        engine::ConsumeEvent(event);
                    }
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
                        CancelPendingTopologyLineSplitAtPoint("Cancelled split at point");
                        engine::ConsumeEvent(event);
                        return;
                    }
                    if (event.mouseClick.button == MOUSE_LEFT_BUTTON) {
                        CommitPendingTopologyLineSplitAtPoint();
                        engine::ConsumeEvent(event);
                    }
                }
        );
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

    if (state.pendingTopologyLineSplitAtPoint.active || state.pendingTopologySectorCut.active) {
        return;
    }

    input.ForEachEvent(
            engine::InputEventType::MouseButtonPressed,
            true,
            [this](engine::InputEvent& event) {
                if (state.currentTool != SectorEditorTool::Move
                        || event.mouseButton.button != MOUSE_LEFT_BUTTON
                        || !Contains(canvasRect, event.mouseButton.position)) {
                    return;
                }

                const int lightId = FindTopologyLightNearScreenPoint(event.mouseButton.position);
                if (lightId >= 0) {
                    StartLightDrag(lightId);
                } else {
                    int vertexId = -1;
                    SectorTopologyCoordPoint point;
                    if (FindTopologyVertexNearScreenPoint(event.mouseButton.position, vertexId, point)) {
                        StartVertexDrag(vertexId, point);
                    } else {
                        statusText = "Move: click a topology light or vertex";
                    }
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

                if (event.mouseClick.button == MOUSE_RIGHT_BUTTON && state.pendingSector.active) {
                    CancelPendingSector("Cancelled sector");
                    engine::ConsumeEvent(event);
                    return;
                }

                if (event.mouseClick.button != MOUSE_LEFT_BUTTON) {
                    return;
                }

                if (state.currentTool == SectorEditorTool::Select) {
                    const int lightId = FindTopologyLightNearScreenPoint(event.mouseClick.releasePosition);
                    if (lightId >= 0) {
                        SelectTopologyLight(lightId);
                        statusText = TextFormat("Selected topology light %d", lightId);
                        engine::ConsumeEvent(event);
                        return;
                    }

                    int vertexId = -1;
                    SectorTopologyCoordPoint vertexPoint;
                    if (FindTopologyVertexNearScreenPoint(
                                event.mouseClick.releasePosition,
                                vertexId,
                                vertexPoint)) {
                        SelectTopologyVertex(vertexId);
                        statusText = TextFormat("Selected topology vertex %d", vertexId);
                        engine::ConsumeEvent(event);
                        return;
                    }

                    int lineDefId = -1;
                    int sideDefId = -1;
                    SectorTopologySideKind side = SectorTopologySideKind::Front;
                    bool preferredMissing = false;
                    if (FindTopologyLineNearScreenPoint(
                                event.mouseClick.releasePosition,
                                ScreenToMap(event.mouseClick.releasePosition),
                                lineDefId,
                                sideDefId,
                                side,
                                preferredMissing)) {
                        if (sideDefId >= 0) {
                            SelectTopologySideDef(sideDefId, state.selectedTopologyWallPart);
                            statusText = preferredMissing
                                    ? TextFormat(
                                            "Selected topology %s sidedef %d; clicked side has no sidedef",
                                            SectorTopologySideKindName(side),
                                            sideDefId)
                                    : TextFormat(
                                            "Selected topology %s sidedef %d",
                                            SectorTopologySideKindName(side),
                                            sideDefId);
                        } else {
                            SelectTopologyLineDef(lineDefId, side, state.selectedTopologyWallPart);
                            statusText = TextFormat("Selected topology linedef %d (no sidedef)", lineDefId);
                        }
                        engine::ConsumeEvent(event);
                        return;
                    }

                    bool multipleMatches = false;
                    const int sectorId = FindTopologySectorAt(
                            ScreenToMap(event.mouseClick.releasePosition),
                            &multipleMatches);
                    if (sectorId >= 0) {
                        SelectTopologySector(sectorId);
                        statusText = multipleMatches
                                ? TextFormat("Selected topology sector %d (lowest ID on boundary)", sectorId)
                                : TextFormat("Selected topology sector %d", sectorId);
                    } else {
                        ClearSelection();
                        statusText = "Selected: none";
                    }
                    engine::ConsumeEvent(event);
                    return;
                }

                if (state.currentTool == SectorEditorTool::Sector
                        || state.currentTool == SectorEditorTool::InsertSectorInside) {
                    if (state.currentTool == SectorEditorTool::InsertSectorInside
                            && (!state.pendingSector.active
                                    || state.pendingSector.kind != PendingSectorDrawKind::InsertInside)) {
                        statusText = "Select a topology sector before inserting inside it.";
                        engine::ConsumeEvent(event);
                        return;
                    }

                    const SectorPoint point = CurrentSnappedSectorPoint();
                    if (CanClosePendingSectorAt(point)) {
                        FinalizePendingSector();
                    } else {
                        AddPendingSectorPoint(point);
                    }
                    engine::ConsumeEvent(event);
                    return;
                }

                if (state.currentTool == SectorEditorTool::Light) {
                    AddStaticLightAt(SnapMapPoint(ScreenToMap(event.mouseClick.releasePosition)));
                    engine::ConsumeEvent(event);
                    return;
                }

                if (state.currentTool == SectorEditorTool::Erase) {
                    const int sectorId = FindTopologySectorAt(
                            ScreenToMap(event.mouseClick.releasePosition));
                    if (sectorId >= 0) {
                        SelectTopologySector(sectorId);
                        OpenDeleteTopologySectorConfirmation(sectorId);
                    } else {
                        statusText = "Select a topology sector to delete.";
                    }
                    engine::ConsumeEvent(event);
                    return;
                }

                if (state.currentTool == SectorEditorTool::Move) {
                    statusText = "Move: click a topology light or vertex";
                    engine::ConsumeEvent(event);
                }
            }
    );
}

void SectorEditor::StartVertexDrag(int vertexId, SectorTopologyCoordPoint point)
{
    if (!IsValidSectorTopologyId(vertexId)
            || FindSectorTopologyVertex(state.topologyMap, vertexId) == nullptr) {
        return;
    }

    SelectTopologyVertex(vertexId);
    state.vertexDrag.active = true;
    state.vertexDrag.topologyVertexId = vertexId;
    state.vertexDrag.originalPoint = point;
    state.vertexDrag.previewPoint = point;
    state.vertexDrag.lastValidatedPoint = point;
    state.vertexDrag.hasPreviewPoint = true;
    state.vertexDrag.hasValidatedPreview = true;
    state.vertexDrag.lastPreviewValid = true;
    state.vertexDrag.errorMessage.clear();

    size_t connectedCount = 0;
    for (const SectorTopologyLineDef& lineDef : state.topologyMap.lineDefs) {
        if (lineDef.startVertexId == vertexId || lineDef.endVertexId == vertexId) {
            ++connectedCount;
        }
    }
    statusText = connectedCount > 1
            ? TextFormat("Moving topology vertex %d (%zu connected linedefs)", vertexId, connectedCount)
            : TextFormat("Moving topology vertex %d", vertexId);
}

void SectorEditor::UpdateVertexDrag(engine::Input& input)
{
    if (!state.vertexDrag.active) {
        return;
    }

    std::string error;
    SectorTopologyCoordPoint snappedPoint;
    if (!SnapTopologyVertexMoveTarget(ScreenToMap(input.MousePosition()), snappedPoint, error)) {
        state.vertexDrag.errorMessage = error;
        state.vertexDrag.hasPreviewPoint = false;
        state.vertexDrag.lastPreviewValid = false;
        state.vertexDrag.hasMergeTarget = false;
        state.vertexDrag.mergeTargetVertexId = -1;
        statusText = TextFormat("Move rejected: %s", error.c_str());
        return;
    }

    state.vertexDrag.previewPoint = snappedPoint;
    state.vertexDrag.hasPreviewPoint = true;
    const int mergeTargetVertexId = FindTopologyVertexAtCoordPoint(
            state.vertexDrag.previewPoint,
            state.vertexDrag.topologyVertexId);
    if (SameTopologyPoint(state.vertexDrag.previewPoint, state.vertexDrag.originalPoint)) {
        state.vertexDrag.errorMessage.clear();
        state.vertexDrag.hasValidatedPreview = true;
        state.vertexDrag.lastValidatedPoint = state.vertexDrag.previewPoint;
        state.vertexDrag.lastPreviewValid = true;
        state.vertexDrag.hasMergeTarget = false;
        state.vertexDrag.mergeTargetVertexId = -1;
        statusText = "Moving vertex: original point";
        return;
    }

    if (mergeTargetVertexId >= 0) {
        state.vertexDrag.errorMessage.clear();
        state.vertexDrag.lastPreviewValid = true;
        state.vertexDrag.hasMergeTarget = true;
        state.vertexDrag.mergeTargetVertexId = mergeTargetVertexId;
        state.vertexDrag.hasValidatedPreview = true;
        state.vertexDrag.lastValidatedPoint = state.vertexDrag.previewPoint;
        statusText = TextFormat("Release to merge into vertex %d", mergeTargetVertexId);
        return;
    }

    state.vertexDrag.hasMergeTarget = false;
    state.vertexDrag.mergeTargetVertexId = -1;
    if (!state.vertexDrag.hasValidatedPreview
            || !SameTopologyPoint(state.vertexDrag.previewPoint, state.vertexDrag.lastValidatedPoint)) {
        SectorTopologyMap previewMap = state.topologyMap;
        state.vertexDrag.lastPreviewValid = MoveSectorTopologyVertex(
                previewMap,
                state.vertexDrag.topologyVertexId,
                state.vertexDrag.previewPoint,
                &error);
        state.vertexDrag.lastValidatedPoint = state.vertexDrag.previewPoint;
        state.vertexDrag.hasValidatedPreview = true;
        state.vertexDrag.errorMessage = state.vertexDrag.lastPreviewValid ? std::string{} : error;
    }

    if (state.vertexDrag.lastPreviewValid) {
        state.vertexDrag.errorMessage.clear();
        statusText = TextFormat("Moving topology vertex %d", state.vertexDrag.topologyVertexId);
    } else {
        statusText = TextFormat("Move rejected: %s", state.vertexDrag.errorMessage.c_str());
    }
}

void SectorEditor::FinishVertexDrag()
{
    if (!state.vertexDrag.active) {
        return;
    }

    const int vertexId = state.vertexDrag.topologyVertexId;
    const SectorTopologyCoordPoint original = state.vertexDrag.originalPoint;
    const SectorTopologyCoordPoint target = state.vertexDrag.previewPoint;

    if (!state.vertexDrag.hasPreviewPoint) {
        const std::string error = state.vertexDrag.errorMessage.empty()
                ? "Move target is outside topology coordinate range"
                : state.vertexDrag.errorMessage;
        state.vertexDrag = VertexDragState{};
        statusText = TextFormat("Move rejected: %s", error.c_str());
        return;
    }

    if (SameTopologyPoint(target, original)) {
        state.vertexDrag = VertexDragState{};
        statusText = "Vertex unchanged";
        return;
    }

    if (state.vertexDrag.hasMergeTarget) {
        const int targetVertexId = state.vertexDrag.mergeTargetVertexId;
        SectorTopologyMergeVerticesResult merge;
        std::string error;
        if (!MergeSectorTopologyVertices(state.topologyMap, vertexId, targetVertexId, &merge, &error)) {
            state.vertexDrag = VertexDragState{};
            statusText = TextFormat("Merge rejected: %s", error.c_str());
            return;
        }

        ClearTransientTopologyEditStateAfterGeometryChange();
        SelectTopologyVertex(merge.mergedVertexId);
        state.inspectedTopologyVertexId = merge.mergedVertexId;
        state.hasHoveredVertex = true;
        state.hoveredTopologyVertexId = merge.mergedVertexId;
        const SectorTopologyVertex* merged = FindSectorTopologyVertex(
                state.topologyMap,
                merge.mergedVertexId);
        state.hoveredTopologyVertexPoint = merged != nullptr
                ? SectorTopologyCoordPoint{merged->x, merged->y}
                : SectorTopologyCoordPoint{};
        state.vertexDrag = VertexDragState{};
        MarkTopologyDocumentEdited(TextFormat(
                "Merged vertex %d into vertex %d.",
                merge.removedVertexId,
                merge.mergedVertexId));
        return;
    }

    std::string error;
    if (!MoveSectorTopologyVertex(state.topologyMap, vertexId, target, &error)) {
        state.vertexDrag = VertexDragState{};
        statusText = TextFormat("Move rejected: %s", error.c_str());
        return;
    }

    ClearTransientTopologyEditStateAfterGeometryChange();
    SelectTopologyVertex(vertexId);
    state.vertexDrag = VertexDragState{};
    MarkTopologyDocumentEdited(TextFormat(
            "Moved topology vertex %d %.2f,%.2f -> %.2f,%.2f",
            vertexId,
            SectorCoordToVisibleAuthoring(original.x),
            SectorCoordToVisibleAuthoring(original.y),
            SectorCoordToVisibleAuthoring(target.x),
            SectorCoordToVisibleAuthoring(target.y)
    ));
}

void SectorEditor::CancelVertexDrag(const char* message)
{
    state.vertexDrag = VertexDragState{};
    if (message != nullptr && message[0] != '\0') {
        statusText = message;
    }
}

void SectorEditor::StartLightDrag(int topologyLightId)
{
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
    statusText = TextFormat("Moving topology light %d", light->id);
}

void SectorEditor::UpdateLightDrag(engine::Input& input)
{
    if (!state.lightDrag.active) {
        return;
    }

    SectorTopologyStaticPointLight* light = FindSectorTopologyStaticLight(
            state.topologyMap,
            state.lightDrag.topologyLightId);
    if (light == nullptr) {
        return;
    }

    const Vector2 snapped = SnapMapPoint(ScreenToMap(input.MousePosition()));
    state.lightDrag.snappedPosition = Vector3{snapped.x, state.lightDrag.originalPosition.y, snapped.y};
    light->position.x = state.lightDrag.snappedPosition.x;
    light->position.y = state.lightDrag.originalPosition.y;
    light->position.z = state.lightDrag.snappedPosition.z;
    statusText = TextFormat("Moving topology light %d", light->id);
}

void SectorEditor::FinishLightDrag()
{
    if (!state.lightDrag.active) {
        return;
    }

    const int lightId = state.lightDrag.topologyLightId;
    const Vector3 original = state.lightDrag.originalPosition;
    state.lightDrag = LightDragState{};
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
        SectorTopologyStaticPointLight* light = FindSectorTopologyStaticLight(
                state.topologyMap,
                state.lightDrag.topologyLightId);
        if (light != nullptr) {
            light->position = state.lightDrag.originalPosition;
            SelectTopologyLight(light->id);
        }
    }

    state.lightDrag = LightDragState{};
    if (message != nullptr && message[0] != '\0') {
        statusText = message;
    }
}

void SectorEditor::StartPendingTopologyVertexMerge(int sourceVertexId)
{
    const SectorTopologyVertex* source = FindSectorTopologyVertex(state.topologyMap, sourceVertexId);
    if (source == nullptr) {
        state.inspectedTopologyVertexId = -1;
        statusText = "Select a topology vertex before merging.";
        return;
    }

    CancelPendingSector(nullptr);
    CancelVertexDrag(nullptr);
    CancelLightDrag(nullptr);
    CancelPendingTopologyLineSplitAtPoint(nullptr);
    CancelPendingTopologySectorCut(nullptr);

    PendingTopologyVertexMerge pending;
    pending.active = true;
    pending.sourceVertexId = sourceVertexId;
    pending.message = "Click target vertex to merge, Escape/right click to cancel.";
    state.pendingTopologyVertexMerge = pending;
    state.currentTool = SectorEditorTool::Move;
    state.hoveredSurface3D = SectorSurfaceHit{};
    state.inspectedTopologyVertexId = sourceVertexId;
    statusText = pending.message;
}

void SectorEditor::CancelPendingTopologyVertexMerge(const char* message)
{
    state.pendingTopologyVertexMerge = PendingTopologyVertexMerge{};
    if (message != nullptr && message[0] != '\0') {
        statusText = message;
    }
}

void SectorEditor::UpdatePendingTopologyVertexMerge(engine::Input& input)
{
    PendingTopologyVertexMerge& pending = state.pendingTopologyVertexMerge;
    if (!pending.active) {
        return;
    }

    if (FindSectorTopologyVertex(state.topologyMap, pending.sourceVertexId) == nullptr) {
        CancelPendingTopologyVertexMerge("Vertex merge cancelled: source vertex no longer exists.");
        return;
    }

    pending.hoveredTargetVertexId = -1;
    pending.hasValidTarget = false;
    pending.message = "Click target vertex to merge, Escape/right click to cancel.";

    if (!IsMouseOverCanvas(input)) {
        statusText = pending.message;
        return;
    }

    int targetVertexId = -1;
    SectorTopologyCoordPoint point;
    if (!FindTopologyVertexNearScreenPoint(input.MousePosition(), targetVertexId, point)) {
        pending.message = "Click target vertex to merge, Escape/right click to cancel.";
        statusText = pending.message;
        return;
    }

    pending.hoveredTargetVertexId = targetVertexId;
    pending.hasValidTarget = targetVertexId != pending.sourceVertexId;
    pending.message = pending.hasValidTarget
            ? TextFormat("Click to merge into vertex %d.", targetVertexId)
            : "Choose a different target vertex.";
    statusText = pending.message;
}

void SectorEditor::CommitPendingTopologyVertexMerge()
{
    if (!state.pendingTopologyVertexMerge.active) {
        return;
    }

    const PendingTopologyVertexMerge pending = state.pendingTopologyVertexMerge;
    if (!pending.hasValidTarget) {
        statusText = pending.message.empty() ? "Choose a target vertex to merge into." : pending.message;
        return;
    }

    SectorTopologyMergeVerticesResult merge;
    std::string error;
    if (!MergeSectorTopologyVertices(
                state.topologyMap,
                pending.sourceVertexId,
                pending.hoveredTargetVertexId,
                &merge,
                &error)) {
        state.pendingTopologyVertexMerge.message =
                error.empty() ? "Merge rejected." : TextFormat("Merge rejected: %s", error.c_str());
        statusText = state.pendingTopologyVertexMerge.message;
        return;
    }

    ClearTransientTopologyEditStateAfterGeometryChange();
    state.pendingTopologyVertexMerge = PendingTopologyVertexMerge{};
    SelectTopologyVertex(merge.mergedVertexId);
    state.inspectedTopologyVertexId = merge.mergedVertexId;
    state.hasHoveredVertex = true;
    state.hoveredTopologyVertexId = merge.mergedVertexId;
    const SectorTopologyVertex* merged = FindSectorTopologyVertex(state.topologyMap, merge.mergedVertexId);
    state.hoveredTopologyVertexPoint = merged != nullptr
            ? SectorTopologyCoordPoint{merged->x, merged->y}
            : SectorTopologyCoordPoint{};
    MarkTopologyDocumentEdited(TextFormat(
            "Merged vertex %d into vertex %d.",
            merge.removedVertexId,
            merge.mergedVertexId));
}

void SectorEditor::StartPendingTopologyLineSplitAtPoint()
{
    const SectorTopologyLineDef* lineDef = SelectedTopologyLineDef();
    if (lineDef == nullptr) {
        ClearStaleTopologySelection();
        statusText = "Select a topology linedef before splitting at point.";
        return;
    }

    CancelPendingSector(nullptr);
    CancelVertexDrag(nullptr);
    CancelLightDrag(nullptr);
    CancelPendingTopologyVertexMerge(nullptr);
    CancelPendingTopologySectorCut(nullptr);

    PendingTopologyLineSplitAtPoint pending;
    pending.active = true;
    pending.lineDefId = lineDef->id;
    pending.sideDefId = state.topologySelectionKind == TopologySelectionKind::SideDef
            ? state.selectedTopologySideDefId
            : -1;
    pending.side = state.selectedTopologySideKind;
    pending.wallPart = state.selectedTopologyWallPart;
    pending.message = "Click a snapped point on the selected linedef; Escape or right click cancels.";
    state.pendingTopologyLineSplitAtPoint = pending;
    state.currentTool = SectorEditorTool::Select;
    state.hoveredSurface3D = SectorSurfaceHit{};
    statusText = pending.message;
}

void SectorEditor::CancelPendingTopologyLineSplitAtPoint(const char* message)
{
    state.pendingTopologyLineSplitAtPoint = PendingTopologyLineSplitAtPoint{};
    if (message != nullptr && message[0] != '\0') {
        statusText = message;
    }
}

bool SectorEditor::ValidatePendingTopologyLineSplitAtPointTarget(const char* staleMessage)
{
    if (!state.pendingTopologyLineSplitAtPoint.active) {
        return false;
    }

    const PendingTopologyLineSplitAtPoint& pending = state.pendingTopologyLineSplitAtPoint;
    const SectorTopologyLineDef* lineDef = FindSectorTopologyLineDef(
            state.topologyMap,
            pending.lineDefId);
    if (lineDef == nullptr) {
        CancelPendingTopologyLineSplitAtPoint(staleMessage);
        return false;
    }

    if (pending.sideDefId >= 0) {
        const SectorTopologySideDef* sideDef = FindSectorTopologySideDef(
                state.topologyMap,
                pending.sideDefId);
        if (sideDef == nullptr || sideDef->lineDefId != lineDef->id) {
            CancelPendingTopologyLineSplitAtPoint(staleMessage);
            return false;
        }
    }

    const SectorTopologyVertex* start = nullptr;
    const SectorTopologyVertex* end = nullptr;
    if (!GetSectorTopologyLineVertices(state.topologyMap, *lineDef, start, end)) {
        CancelPendingTopologyLineSplitAtPoint(staleMessage);
        return false;
    }
    return true;
}

void SectorEditor::UpdatePendingTopologyLineSplitAtPoint(engine::Input& input)
{
    if (!ValidatePendingTopologyLineSplitAtPointTarget(
                "Split at point cancelled: target linedef no longer exists.")) {
        return;
    }

    PendingTopologyLineSplitAtPoint& pending = state.pendingTopologyLineSplitAtPoint;
    pending.hasCandidatePoint = false;
    pending.hasValidCandidate = false;

    const SectorTopologyLineDef* lineDef = FindSectorTopologyLineDef(
            state.topologyMap,
            pending.lineDefId);
    const SectorTopologyVertex* start = nullptr;
    const SectorTopologyVertex* end = nullptr;
    if (lineDef == nullptr || !GetSectorTopologyLineVertices(state.topologyMap, *lineDef, start, end)) {
        CancelPendingTopologyLineSplitAtPoint("Split at point cancelled: target linedef no longer exists.");
        return;
    }

    if (!Contains(canvasRect, input.MousePosition())) {
        pending.message = "Move over the selected linedef to choose a split point.";
        statusText = pending.message;
        return;
    }

    SectorTopologyCoordPoint candidate;
    std::string error;
    if (!ToTopologyCoordPoint(Vector2ToSectorPoint(state.snappedMouseMap), candidate, error)) {
        pending.message = error;
        statusText = error;
        return;
    }

    pending.candidatePoint = candidate;
    pending.hasCandidatePoint = true;

    const SectorTopologyCoordPoint startPoint{start->x, start->y};
    const SectorTopologyCoordPoint endPoint{end->x, end->y};
    if (!SectorTopologyPointStrictlyInsideSegment(candidate, startPoint, endPoint)) {
        pending.message = "Split point must be inside the selected linedef.";
        statusText = pending.message;
        return;
    }

    for (const SectorTopologyVertex& vertex : state.topologyMap.vertices) {
        if (vertex.x == candidate.x && vertex.y == candidate.y) {
            pending.message = "Split point is already occupied by a topology vertex.";
            statusText = pending.message;
            return;
        }
    }

    pending.hasValidCandidate = true;
    pending.message = "Click to split selected linedef; Escape or right click cancels.";
    statusText = pending.message;
}

void SectorEditor::CommitPendingTopologyLineSplitAtPoint()
{
    if (!ValidatePendingTopologyLineSplitAtPointTarget(
                "Split at point cancelled: target linedef no longer exists.")) {
        return;
    }

    const PendingTopologyLineSplitAtPoint pending = state.pendingTopologyLineSplitAtPoint;
    if (!pending.hasCandidatePoint || !pending.hasValidCandidate) {
        statusText = pending.message.empty()
                ? "Choose a valid split point on the selected linedef."
                : pending.message;
        return;
    }

    SectorTopologySplitLineResult split;
    std::string error;
    if (!SplitSectorTopologyLineDefAtPoint(
                state.topologyMap,
                pending.lineDefId,
                pending.candidatePoint,
                &split,
                &error)) {
        statusText = error.empty() ? "Cannot split topology linedef at point" : error;
        return;
    }

    state.pendingTopologyLineSplitAtPoint = PendingTopologyLineSplitAtPoint{};
    state.hasHoveredVertex = false;
    state.hoveredTopologyVertexId = -1;
    state.hoveredTopologyVertexPoint = SectorTopologyCoordPoint{};
    state.hoveredSurface3D = SectorSurfaceHit{};
    state.selectedSurface3D = SectorSurfaceRef{};
    state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ResetSurface3DUiState();
    for (engine::UIFloatInputState& inputState : uiState.topologySideDefUvInputs) {
        inputState = engine::UIFloatInputState{};
    }

    if (pending.sideDefId >= 0) {
        const int secondSideDefId = pending.side == SectorTopologySideKind::Front
                ? split.secondFrontSideDefId
                : split.secondBackSideDefId;
        if (FindSectorTopologySideDef(state.topologyMap, secondSideDefId) != nullptr) {
            SelectTopologySideDef(secondSideDefId, pending.wallPart);
        } else {
            SelectTopologyLineDef(split.secondLineDefId, pending.side, pending.wallPart);
        }
    } else {
        SelectTopologyLineDef(split.secondLineDefId, pending.side, pending.wallPart);
    }

    state.topologyRenderWarning.clear();
    MarkTopologyDocumentEdited(TextFormat(
            "Split topology linedef %d at point; selected linedef %d",
            pending.lineDefId,
            split.secondLineDefId));
}

void SectorEditor::StartPendingTopologySectorCut()
{
    const SectorTopologySector* sector = SelectedTopologySector();
    if (sector == nullptr) {
        ClearStaleTopologySelection();
        statusText = "Select a topology sector before cutting it.";
        return;
    }

    CancelPendingSector(nullptr);
    CancelVertexDrag(nullptr);
    CancelLightDrag(nullptr);
    CancelPendingTopologyVertexMerge(nullptr);
    CancelPendingTopologyLineSplitAtPoint(nullptr);

    PendingTopologySectorCut pending;
    pending.active = true;
    pending.sectorId = sector->id;
    pending.message = "Click first boundary point for sector cut; Escape or right click cancels.";
    state.pendingTopologySectorCut = pending;
    state.currentTool = SectorEditorTool::Select;
    state.hoveredSurface3D = SectorSurfaceHit{};
    state.selectedSurface3D = SectorSurfaceRef{};
    state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ResetSurface3DUiState();
    statusText = pending.message;
}

void SectorEditor::CancelPendingTopologySectorCut(const char* message)
{
    state.pendingTopologySectorCut = PendingTopologySectorCut{};
    if (message != nullptr && message[0] != '\0') {
        statusText = message;
    }
}

bool SectorEditor::ValidatePendingTopologySectorCutTarget(const char* staleMessage)
{
    if (!state.pendingTopologySectorCut.active) {
        return false;
    }
    if (FindSectorTopologySector(state.topologyMap, state.pendingTopologySectorCut.sectorId) == nullptr) {
        CancelPendingTopologySectorCut(staleMessage);
        return false;
    }
    return true;
}

void SectorEditor::UpdatePendingTopologySectorCut(engine::Input& input)
{
    if (!ValidatePendingTopologySectorCutTarget(
                "Sector cut cancelled: selected sector no longer exists.")) {
        return;
    }

    PendingTopologySectorCut& pending = state.pendingTopologySectorCut;
    pending.hasCandidatePoint = false;
    pending.hasValidCandidate = false;

    if (!Contains(canvasRect, input.MousePosition())) {
        pending.message = pending.hasFirstPoint
                ? "Move over the selected sector boundary to choose the second cut point."
                : "Move over the selected sector boundary to choose the first cut point.";
        statusText = pending.message;
        return;
    }

    SectorTopologyBoundaryCutPoint candidate;
    if (!FindSelectedSectorBoundaryCutPointNearScreenPoint(input.MousePosition(), candidate)) {
        pending.message = pending.hasFirstPoint
                ? "Choose a second point on the selected sector outer boundary."
                : "Choose a first point on the selected sector outer boundary.";
        statusText = pending.message;
        return;
    }

    pending.candidatePoint = candidate;
    pending.hasCandidatePoint = true;

    std::string endpointError;
    if (!ValidateSectorTopologySectorBoundaryCutPoint(
                state.topologyMap,
                pending.sectorId,
                pending.candidatePoint,
                &endpointError)) {
        pending.hasValidCandidate = false;
        pending.cacheHasFirstPoint = false;
        pending.message = endpointError.empty() ? "Sector cut endpoint rejected." : endpointError;
        statusText = pending.message;
        return;
    }

    if (!pending.hasFirstPoint) {
        pending.hasValidCandidate = true;
        pending.message = "Click to place first cut point.";
        statusText = pending.message;
        return;
    }

    const bool cacheMatches =
            pending.cacheHasFirstPoint
            && pending.cachedSectorId == pending.sectorId
            && SameBoundaryCutPoint(pending.cachedFirstPoint, pending.firstPoint)
            && SameBoundaryCutPoint(pending.cachedCandidatePoint, pending.candidatePoint);
    if (!cacheMatches) {
        SectorTopologyMap candidateMap = state.topologyMap;
        SectorTopologyCutSectorResult previewResult;
        std::string error;
        pending.cachedValid = CutSectorTopologySectorBetweenBoundaryPoints(
                candidateMap,
                pending.sectorId,
                pending.firstPoint,
                pending.candidatePoint,
                &previewResult,
                &error);
        pending.cachedError = pending.cachedValid ? std::string{} : error;
        pending.cachedSectorId = pending.sectorId;
        pending.cachedFirstPoint = pending.firstPoint;
        pending.cachedCandidatePoint = pending.candidatePoint;
        pending.cacheHasFirstPoint = true;
    }

    pending.hasValidCandidate = pending.cachedValid;
    pending.message = pending.cachedValid
            ? "Click to cut selected sector; Escape or right click cancels."
            : (pending.cachedError.empty() ? "Sector cut rejected." : pending.cachedError);
    statusText = pending.message;
}

void SectorEditor::CommitPendingTopologySectorCut()
{
    if (!ValidatePendingTopologySectorCutTarget(
                "Sector cut cancelled: selected sector no longer exists.")) {
        return;
    }

    PendingTopologySectorCut& pending = state.pendingTopologySectorCut;
    if (!pending.hasCandidatePoint || !pending.hasValidCandidate) {
        statusText = pending.message.empty()
                ? "Choose a valid boundary point for sector cut."
                : pending.message;
        return;
    }

    if (!pending.hasFirstPoint) {
        pending.firstPoint = pending.candidatePoint;
        pending.hasFirstPoint = true;
        pending.cacheHasFirstPoint = false;
        pending.message = "Click second boundary point for sector cut; Escape or right click cancels.";
        statusText = pending.message;
        return;
    }

    const PendingTopologySectorCut committed = pending;
    SectorTopologyCutSectorResult cut;
    std::string error;
    if (!CutSectorTopologySectorBetweenBoundaryPoints(
                state.topologyMap,
                committed.sectorId,
                committed.firstPoint,
                committed.candidatePoint,
                &cut,
                &error)) {
        pending.cacheHasFirstPoint = false;
        pending.hasValidCandidate = false;
        pending.message = error.empty() ? "Sector cut rejected." : error;
        statusText = pending.message;
        return;
    }

    ClearTransientTopologyEditStateAfterGeometryChange();
    state.pendingTopologySectorCut = PendingTopologySectorCut{};
    SelectTopologySector(cut.originalSectorId);
    MarkTopologyDocumentEdited(TextFormat(
            "Cut topology sector %d; created sector %d.",
            cut.originalSectorId,
            cut.newSectorId));
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
            preview.Update(input, dt);
        } else {
            if (!std::isfinite(state.landingDipState.offsetY)) {
                ClearSectorFpsLandingDip(state.landingDipState);
            }
            const float previousVisualEyeY = preview.Pose().position.y;
            const float previousStepVisualEyeY =
                    previousVisualEyeY - state.landingDipState.offsetY;
            input.ForEachEvent(
                    engine::InputEventType::KeyPressed,
                    true,
                    [this](engine::InputEvent& event) {
                        if (event.key.key != KEY_F11) {
                            return;
                        }

                        preview.SetMouseLookEnabled(!preview.IsMouseLookEnabled());
                        engine::ConsumeEvent(event);
                    }
            );

            SectorFpsControllerInput controllerInput;
            controllerInput.moveForward = input.IsKeyDown(KEY_W);
            controllerInput.moveBackward = input.IsKeyDown(KEY_S);
            controllerInput.strafeLeft = input.IsKeyDown(KEY_A);
            controllerInput.strafeRight = input.IsKeyDown(KEY_D);
            controllerInput.run = input.IsKeyDown(KEY_LEFT_SHIFT) || input.IsKeyDown(KEY_RIGHT_SHIFT);
            controllerInput.mouseLookEnabled = preview.IsMouseLookEnabled();
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
            UpdateSectorFpsMouseLook(
                    state.fpsControllerState,
                    state.fpsControllerConfig,
                    controllerInput);
            const Vector2 desiredHorizontalMovement = ComputeSectorFpsHorizontalMovementDelta(
                    state.fpsControllerState,
                    state.fpsControllerConfig,
                    controllerInput,
                    dt);
            const Vector2 previousFeetXZ{
                    state.fpsControllerState.feetPosition.x,
                    state.fpsControllerState.feetPosition.z};
            state.previewMoveResult = SectorCollisionMoveResult{};
            state.previewCollisionNoclipFallback = false;
            if (state.sectorCollisionWorldValid) {
                const Vector2 feetXZ{
                        state.fpsControllerState.feetPosition.x,
                        state.fpsControllerState.feetPosition.z};
                if (state.sectorCollisionWorld.FindSector(
                            state.fpsControllerState.currentSectorId) == nullptr) {
                    state.fpsControllerState.currentSectorId =
                            state.sectorCollisionWorld.FindSectorContainingPoint(feetXZ);
                }
                const int previousSectorId = state.fpsControllerState.currentSectorId;
                const float previousFeetY = state.fpsControllerState.feetPosition.y;
                const bool wasGrounded = state.fpsControllerState.grounded;

                if (state.fpsControllerState.currentSectorId != 0) {
                    const SectorFpsControllerConfig normalizedConfig =
                            NormalizeSectorFpsControllerConfig(state.fpsControllerConfig);
                    SectorCollisionMoveResult moveResult =
                            state.sectorCollisionWorld.ResolveMovement(
                                    SectorCollisionMoveState{
                                            feetXZ,
                                            state.fpsControllerState.feetPosition.y,
                                            state.fpsControllerState.currentSectorId,
                                            state.fpsControllerState.grounded},
                                    desiredHorizontalMovement,
                                    SectorCollisionMoveConfig{
                                            normalizedConfig.playerRadius,
                                            normalizedConfig.playerHeight,
                                            normalizedConfig.stepHeight,
                                            4});
                    SectorCollisionHeights movedHeights;
                    if (wasGrounded
                            && moveResult.currentSectorId != previousSectorId
                            && state.sectorCollisionWorld.GetSectorFloorCeiling(
                                    moveResult.currentSectorId,
                                    &movedHeights)
                            && movedHeights.floorZ - previousFeetY
                                    > normalizedConfig.stepHeight + GameplayFloorSnapEpsilon) {
                        moveResult.positionXZ = feetXZ;
                        moveResult.currentSectorId = previousSectorId;
                        moveResult.blockedByStep = true;
                    }
                    state.previewMoveResult = moveResult;
                    state.fpsControllerState.feetPosition.x = moveResult.positionXZ.x;
                    state.fpsControllerState.feetPosition.z = moveResult.positionXZ.y;
                    state.fpsControllerState.currentSectorId = moveResult.currentSectorId;
                } else {
                    state.previewCollisionNoclipFallback = true;
                    state.fpsControllerState.feetPosition.x += desiredHorizontalMovement.x;
                    state.fpsControllerState.feetPosition.z += desiredHorizontalMovement.y;
                }
            } else {
                state.previewCollisionNoclipFallback = true;
                state.fpsControllerState.feetPosition.x += desiredHorizontalMovement.x;
                state.fpsControllerState.feetPosition.z += desiredHorizontalMovement.y;
            }
            RefreshGameplaySectorAndVerticalContext();
            bool startedJump = false;
            if (controllerInput.jumpPressed) {
                startedJump = TryStartSectorFpsJump(
                        state.fpsControllerState,
                        state.fpsControllerConfig);
                if (startedJump) {
                    ClearSectorFpsLandingDip(state.landingDipState);
                }
            }
            state.previewVerticalResult = UpdateSectorFpsVerticalPhysics(
                    state.fpsControllerState,
                    state.fpsControllerConfig,
                    BuildGameplayVerticalContext(),
                    dt);
            if (state.previewCollisionNoclipFallback || !state.previewVerticalResult.hasSector) {
                state.visualStepOffsetY = 0.0f;
                ClearSectorFpsLandingDip(state.landingDipState);
            } else if (startedJump) {
                state.visualStepOffsetY = 0.0f;
            } else {
                ApplySectorFpsVisualStepSmoothing(
                        state.visualStepOffsetY,
                        state.previewVerticalResult.transition,
                        previousStepVisualEyeY,
                        state.fpsControllerState,
                        state.fpsControllerConfig,
                        DefaultSectorFpsStepSmoothingRate(),
                        dt);
                UpdateSectorFpsLandingDip(
                        state.landingDipState,
                        state.previewVerticalResult,
                        dt);
            }
            const Vector2 resolvedHorizontalMovement{
                    state.fpsControllerState.feetPosition.x - previousFeetXZ.x,
                    state.fpsControllerState.feetPosition.z - previousFeetXZ.y};
            const float resolvedHorizontalSpeed = dt > 0.0f
                    ? Vector2Length(resolvedHorizontalMovement) / dt
                    : 0.0f;
            const bool headBobActive = !state.previewCollisionNoclipFallback
                    && state.previewVerticalResult.hasSector
                    && state.fpsControllerState.grounded
                    && !state.previewSettingsModal.open;
            UpdateSectorFpsHeadBob(
                    state.headBobState,
                    state.fpsControllerConfig,
                    headBobActive,
                    resolvedHorizontalSpeed,
                    state.fpsControllerState.yawRadians,
                    dt);
            ApplyGameplayPoseToPreview();
        }
        UpdatePreview3DSelection(input);
    }
}

void SectorEditor::UpdatePreview3DSelection(engine::Input& input)
{
    if (!initialized
            || !preview.IsReady()
            || preview.IsMouseLookEnabled()
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

void SectorEditor::CancelPendingSector(const char* message)
{
    const bool wasInsert = state.pendingSector.kind == PendingSectorDrawKind::InsertInside;
    state.pendingSector = PendingSectorDraw{};
    if (wasInsert) {
        state.currentTool = SectorEditorTool::Select;
    }
    if (message != nullptr && message[0] != '\0') {
        statusText = message;
    }
}

void SectorEditor::RemoveLastPendingSectorPoint()
{
    if (!state.pendingSector.active || state.pendingSector.points.empty()) {
        return;
    }

    state.pendingSector.points.pop_back();
    state.pendingSector.errorMessage.clear();
    if (state.pendingSector.points.empty()) {
        CancelPendingSector("Cancelled sector");
    } else {
        statusText = state.pendingSector.kind == PendingSectorDrawKind::InsertInside
                ? TextFormat(
                        "Insert sector: %zu points inside %s",
                        state.pendingSector.points.size(),
                        state.pendingSector.parentSectorLabel.c_str())
                : TextFormat("Pending sector: %zu points", state.pendingSector.points.size());
    }
}

void SectorEditor::AddPendingSectorPoint(SectorPoint point)
{
    const PendingSectorDrawKind pendingKind = state.pendingSector.active
            ? state.pendingSector.kind
            : PendingSectorDrawKind::NewSector;
    std::string error;
    SectorPoint canonicalPoint;
    if (!ToCanonicalSectorPoint(point, canonicalPoint, error)) {
        state.pendingSector.errorMessage = error;
        statusText = error;
        return;
    }
    if (!ValidatePendingTopologyPoint(canonicalPoint, error)) {
        state.pendingSector.errorMessage = error;
        statusText = error;
        return;
    }

    state.pendingSector.active = true;
    state.pendingSector.kind = pendingKind;
    state.pendingSector.points.push_back(canonicalPoint);
    state.pendingSector.errorMessage.clear();
    if (state.pendingSector.kind == PendingSectorDrawKind::InsertInside) {
        statusText = TextFormat(
                "Insert sector: %zu points inside %s",
                state.pendingSector.points.size(),
                state.pendingSector.parentSectorLabel.c_str());
    } else {
        statusText = TextFormat("Pending sector: %zu points", state.pendingSector.points.size());
    }
}

void SectorEditor::FinalizePendingSector()
{
    if (!state.pendingSector.active || state.pendingSector.points.size() < 3) {
        state.pendingSector.errorMessage = "Need at least 3 points to close sector";
        statusText = state.pendingSector.errorMessage;
        return;
    }

    std::string error;
    std::vector<SectorTopologyCoordPoint> topologyPoints;
    if (!BuildPendingTopologyPoints(topologyPoints, error)) {
        state.pendingSector.errorMessage = error;
        statusText = error;
        return;
    }

    int createdSectorId = -1;
    bool created = false;
    if (state.pendingSector.kind == PendingSectorDrawKind::InsertInside) {
        SectorTopologyInsertPolygonOptions options;
        created = InsertSectorTopologyPolygon(
                state.topologyMap,
                state.pendingSector.parentTopologySectorId,
                topologyPoints,
                options,
                &createdSectorId,
                &error);
    } else {
        SectorTopologyCreatePolygonOptions options = BuildTopologyCreateOptions();
        created = CreateSectorTopologyPolygon(
                state.topologyMap,
                topologyPoints,
                options,
                &createdSectorId,
                &error);
    }
    if (!created) {
        state.pendingSector.errorMessage = error.empty() ? "Could not create topology sector" : error;
        statusText = state.pendingSector.errorMessage;
        return;
    }

    const bool insertedInside = state.pendingSector.kind == PendingSectorDrawKind::InsertInside;
    state.pendingSector = PendingSectorDraw{};
    state.hasHoveredVertex = false;
    state.hoveredTopologyVertexId = -1;
    state.hoveredTopologyVertexPoint = SectorTopologyCoordPoint{};
    state.hoveredSurface3D = SectorSurfaceHit{};
    state.selectedSurface3D = SectorSurfaceRef{};
    state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    SelectTopologySector(createdSectorId);
    state.topologyRenderWarning.clear();
    MarkTopologyDocumentEdited(insertedInside
            ? TextFormat("Inserted topology sector %d", createdSectorId)
            : TextFormat("Created topology sector %d", createdSectorId));
}

void SectorEditor::StartInsertSectorInside()
{
    const SectorTopologySector* parent = SelectedTopologySector();
    if (parent == nullptr) {
        ClearStaleTopologySelection();
        statusText = "Select a topology sector before inserting inside it.";
        return;
    }

    PendingSectorDraw pending;
    pending.active = true;
    pending.kind = PendingSectorDrawKind::InsertInside;
    pending.parentTopologySectorId = parent->id;
    pending.parentSectorLabel = parent->name.empty()
            ? TextFormat("sector %d", parent->id)
            : parent->name;
    state.pendingSector = std::move(pending);
    state.currentTool = SectorEditorTool::InsertSectorInside;
    state.hoveredSurface3D = SectorSurfaceHit{};
    statusText = TextFormat("Draw a sector inside %s", state.pendingSector.parentSectorLabel.c_str());
}

bool SectorEditor::IsPendingInsertParentValid() const
{
    if (!state.pendingSector.active
            || state.pendingSector.kind != PendingSectorDrawKind::InsertInside
            || state.pendingSector.parentTopologySectorId < 0) {
        return false;
    }
    return FindSectorTopologySector(state.topologyMap, state.pendingSector.parentTopologySectorId) != nullptr;
}

bool SectorEditor::CanClosePendingSectorAt(SectorPoint point) const
{
    if (!state.pendingSector.active
            || state.pendingSector.points.size() < 3) {
        return false;
    }

    SectorTopologyCoordPoint candidate;
    SectorTopologyCoordPoint first;
    std::string error;
    return ToTopologyCoordPoint(point, candidate, error)
            && ToTopologyCoordPoint(state.pendingSector.points.front(), first, error)
            && candidate.x == first.x
            && candidate.y == first.y;
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

bool SectorEditor::BuildPendingTopologyPoints(
        std::vector<SectorTopologyCoordPoint>& outPoints,
        std::string& error) const
{
    if (!state.pendingSector.active
            || (state.pendingSector.kind != PendingSectorDrawKind::NewSector
                    && state.pendingSector.kind != PendingSectorDrawKind::InsertInside)) {
        error = "No topology sector draw is active";
        return false;
    }

    outPoints.clear();
    outPoints.reserve(state.pendingSector.points.size());
    for (SectorPoint point : state.pendingSector.points) {
        SectorTopologyCoordPoint topologyPoint;
        if (!ToTopologyCoordPoint(point, topologyPoint, error)) {
            return false;
        }
        outPoints.push_back(topologyPoint);
    }
    error.clear();
    return true;
}

bool SectorEditor::ValidatePendingTopologyPoint(SectorPoint point, std::string& error) const
{
    SectorTopologyCoordPoint candidate;
    if (!ToTopologyCoordPoint(point, candidate, error)) {
        return false;
    }

    if (!state.pendingSector.active || state.pendingSector.points.empty()) {
        error.clear();
        return true;
    }
    std::vector<SectorTopologyCoordPoint> existing;
    if (!BuildPendingTopologyPoints(existing, error)) {
        return false;
    }

    const SectorTopologyCoordPoint previous = existing.back();
    if (candidate.x == previous.x && candidate.y == previous.y) {
        error = "Duplicate point";
        return false;
    }

    if (existing.size() >= 2
            && candidate.x == existing.front().x
            && candidate.y == existing.front().y) {
        error = existing.size() >= 3
                ? "Click first point to close sector"
                : "Need at least 3 points to close sector";
        return false;
    }

    for (SectorTopologyCoordPoint point : existing) {
        if (candidate.x == point.x && candidate.y == point.y) {
            error = "Duplicate point";
            return false;
        }
    }

    error.clear();
    return true;
}

SectorTopologyCreatePolygonOptions SectorEditor::BuildTopologyCreateOptions() const
{
    SectorTopologyCreatePolygonOptions options;
    options.floorZ = state.defaultSectorFloorZ;
    options.ceilingZ = state.defaultSectorCeilingZ;
    options.floorTextureId = state.defaultFloorTextureId;
    options.ceilingTextureId = state.defaultCeilingTextureId;
    options.defaultWall.textureId = state.defaultWallTextureId;
    options.defaultLower.textureId = state.defaultLowerWallTextureId.empty()
            ? state.defaultWallTextureId
            : state.defaultLowerWallTextureId;
    options.defaultUpper.textureId = state.defaultUpperWallTextureId.empty()
            ? state.defaultWallTextureId
            : state.defaultUpperWallTextureId;
    return options;
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
    if (state.topologySelectionKind != TopologySelectionKind::Light) {
        return nullptr;
    }
    return FindSectorTopologyStaticLight(state.topologyMap, state.selectedTopologyLightId);
}

const SectorTopologyStaticPointLight* SectorEditor::SelectedTopologyLight() const
{
    if (state.topologySelectionKind != TopologySelectionKind::Light) {
        return nullptr;
    }
    return FindSectorTopologyStaticLight(state.topologyMap, state.selectedTopologyLightId);
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
    } else if (state.topologySelectionKind == TopologySelectionKind::Light) {
        stale = state.selectedTopologyLightId < 0
                || FindSectorTopologyStaticLight(state.topologyMap, state.selectedTopologyLightId) == nullptr;
    }

    if (stale) {
        if (state.pendingTopologyLineSplitAtPoint.active
                && (state.pendingTopologyLineSplitAtPoint.lineDefId == state.selectedTopologyLineDefId
                        || FindSectorTopologyLineDef(
                                state.topologyMap,
                                state.pendingTopologyLineSplitAtPoint.lineDefId) == nullptr)) {
            CancelPendingTopologyLineSplitAtPoint(
                    "Split at point cancelled: target linedef no longer exists.");
        }
        state.topologySelectionKind = TopologySelectionKind::None;
        state.selectedTopologySectorId = -1;
        state.selectedTopologyVertexId = -1;
        state.selectedTopologySideDefId = -1;
        state.selectedTopologyLineDefId = -1;
        state.selectedTopologyLightId = -1;
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
    if (assets != nullptr && state.mode == SectorEditorMode::Preview3D && preview.IsReady()) {
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
    const SectorEditorTopologyActionResult result = game::SetPortalBlocksPlayer(
            state.topologyMap,
            lineDefId,
            blocksPlayer);
    const bool changed = FinishTopologyActionResult(result);
    if (changed) {
        RebuildSectorCollisionWorld();
    }
    return changed || result.status.empty();
}

void SectorEditor::ClearTransientTopologyEditStateAfterGeometryChange()
{
    ClearStaleTopologySelection();
    state.topologyRenderWarning.clear();
    state.pendingTopologySectorCut = PendingTopologySectorCut{};
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
    if (light == nullptr) {
        uiState.selectedLightIdBuffer[0] = '\0';
        uiState.idBufferLightIndex = -1;
        if (state.topologySelectionKind == TopologySelectionKind::None) {
            uiState.idEditError.clear();
        }
        return;
    }

    if (uiState.idBufferLightIndex == light->id) {
        return;
    }

    std::snprintf(uiState.selectedLightIdBuffer, sizeof(uiState.selectedLightIdBuffer), "%d", light->id);
    uiState.idBufferLightIndex = light->id;
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

    sector->name = newName;
    uiState.idEditError.clear();
    MarkTopologyDocumentEdited(TextFormat("Renamed topology sector %d", sector->id));
    return true;
}

bool SectorEditor::TryRenameSelectedLight()
{
    if (SelectedTopologyLight() == nullptr) {
        uiState.idEditError = "No light selected";
        statusText = uiState.idEditError;
        return false;
    }

    uiState.idEditError = "Topology light IDs are stable";
    statusText = uiState.idEditError;
    return false;
}

void SectorEditor::OpenDeleteSelectedTopologySectorConfirmation()
{
    const SectorTopologySector* sector = SelectedTopologySector();
    if (sector == nullptr) {
        ClearStaleTopologySelection();
        statusText = "Select a topology sector to delete.";
        return;
    }
    OpenDeleteTopologySectorConfirmation(sector->id);
}

void SectorEditor::OpenDeleteTopologySectorConfirmation(int sectorId)
{
    const SectorTopologySector* sector = FindSectorTopologySector(state.topologyMap, sectorId);
    if (sector == nullptr) {
        ClearStaleTopologySelection();
        statusText = "Select a topology sector to delete.";
        return;
    }

    const std::string label = sector->name.empty()
            ? TextFormat("topology sector %d", sector->id)
            : TextFormat("%s", sector->name.c_str());
    OpenConfirmation(
            "Delete Sector",
            TextFormat("Delete sector \"%s\"?", label.c_str()),
            [this, sectorId]() { DeleteSelectedTopologySectorConfirmed(sectorId); });
}

bool SectorEditor::DeleteSelectedTopologySectorConfirmed(int sectorId)
{
    const bool hadPendingSplit = state.pendingTopologyLineSplitAtPoint.active;
    const int pendingSplitLineDefId = state.pendingTopologyLineSplitAtPoint.lineDefId;
    SectorTopologyDeleteSectorResult result;
    std::string error;
    if (!DeleteSectorTopologySector(state.topologyMap, sectorId, &result, &error)) {
        statusText = error.empty() ? "Cannot delete topology sector" : error;
        return false;
    }

    state.hasHoveredVertex = false;
    state.hoveredTopologyVertexId = -1;
    state.hoveredTopologyVertexPoint = SectorTopologyCoordPoint{};
    state.hoveredSurface3D = SectorSurfaceHit{};
    state.selectedSurface3D = SectorSurfaceRef{};
    state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ResetSurface3DUiState();
    for (engine::UIFloatInputState& inputState : uiState.topologySectorUvInputs) {
        inputState = engine::UIFloatInputState{};
    }
    for (engine::UIFloatInputState& inputState : uiState.topologySideDefUvInputs) {
        inputState = engine::UIFloatInputState{};
    }
    state.pendingTopologyLineSplitAtPoint = PendingTopologyLineSplitAtPoint{};
    ClearStaleTopologySelection();
    state.topologyRenderWarning.clear();
    const bool pendingSplitTargetRemoved = hadPendingSplit
            && FindSectorTopologyLineDef(state.topologyMap, pendingSplitLineDefId) == nullptr;
    MarkTopologyDocumentEdited(pendingSplitTargetRemoved
            ? TextFormat(
                    "Deleted topology sector %d; split at point cancelled",
                    result.deletedSectorId)
            : TextFormat(
            "Deleted topology sector %d; removed %d sidedefs, %d linedefs, %d vertices",
            result.deletedSectorId,
            result.removedSideDefCount,
            result.removedLineDefCount,
            result.removedVertexCount));
    return true;
}

bool SectorEditor::DeleteSelectedLight()
{
    const SectorTopologyStaticPointLight* light = SelectedTopologyLight();
    if (light == nullptr) {
        return false;
    }

    const int lightId = light->id;
    OpenConfirmation(
            "Delete Light",
            TextFormat("Delete topology light %d?", lightId),
            [this, lightId]() { DeleteLightById(lightId); });
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

    if (state.mode == SectorEditorMode::Preview3D && preview.IsReady()) {
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

bool SectorEditor::FindSelectedSectorBoundaryCutPointNearScreenPoint(
        Vector2 screenPoint,
        SectorTopologyBoundaryCutPoint& outPoint) const
{
    outPoint = SectorTopologyBoundaryCutPoint{};
    const SectorTopologySector* sector = SelectedTopologySector();
    if (sector == nullptr) {
        return false;
    }

    SectorTopologyLoopSet loops;
    if (!ExtractSectorTopologyLoops(state.topologyMap, sector->id, loops)) {
        return false;
    }

    float bestVertexDistance2 = ScreenVertexSnapPixels * ScreenVertexSnapPixels;
    int bestVertexId = -1;
    SectorTopologyCoordPoint bestVertexPoint{};
    for (int vertexId : loops.outer.vertexIds) {
        const SectorTopologyVertex* vertex = FindSectorTopologyVertex(state.topologyMap, vertexId);
        if (vertex == nullptr) {
            continue;
        }
        const Vector2 screenVertex = MapToScreen(SectorTopologyVertexToMap(*vertex));
        const float dx = screenVertex.x - screenPoint.x;
        const float dy = screenVertex.y - screenPoint.y;
        const float distance2 = dx * dx + dy * dy;
        if (distance2 > bestVertexDistance2) {
            continue;
        }
        if (bestVertexId >= 0
                && std::fabs(distance2 - bestVertexDistance2) <= 0.001f
                && vertex->id >= bestVertexId) {
            continue;
        }
        bestVertexDistance2 = distance2;
        bestVertexId = vertex->id;
        bestVertexPoint = SectorTopologyCoordPoint{vertex->x, vertex->y};
    }
    if (bestVertexId >= 0) {
        outPoint.vertexId = bestVertexId;
        outPoint.point = bestVertexPoint;
        return true;
    }

    SectorTopologyCoordPoint snappedPoint;
    std::string error;
    if (!ToTopologyCoordPoint(Vector2ToSectorPoint(state.snappedMouseMap), snappedPoint, error)) {
        return false;
    }

    float bestLineDistance = ScreenEdgePickPixels;
    int bestLineDefId = -1;
    for (const SectorTopologyLoopEdge& edge : loops.outer.edges) {
        const SectorTopologyLineDef* lineDef = FindSectorTopologyLineDef(state.topologyMap, edge.lineDefId);
        if (lineDef == nullptr) {
            continue;
        }
        const SectorTopologyVertex* start = nullptr;
        const SectorTopologyVertex* end = nullptr;
        if (!GetSectorTopologyLineVertices(state.topologyMap, *lineDef, start, end)) {
            continue;
        }
        const SectorTopologyCoordPoint startPoint{start->x, start->y};
        const SectorTopologyCoordPoint endPoint{end->x, end->y};
        if (!SectorTopologyPointStrictlyInsideSegment(snappedPoint, startPoint, endPoint)) {
            continue;
        }

        const Vector2 screenStart = MapToScreen(SectorTopologyVertexToMap(*start));
        const Vector2 screenEnd = MapToScreen(SectorTopologyVertexToMap(*end));
        const float distance = DistancePointToSegment(screenPoint, screenStart, screenEnd);
        if (distance > ScreenEdgePickPixels) {
            continue;
        }
        if (bestLineDefId >= 0
                && (distance > bestLineDistance + 0.001f
                        || (std::fabs(distance - bestLineDistance) <= 0.001f
                                && lineDef->id >= bestLineDefId))) {
            continue;
        }
        bestLineDistance = distance;
        bestLineDefId = lineDef->id;
    }

    if (bestLineDefId < 0) {
        return false;
    }

    outPoint.lineDefId = bestLineDefId;
    outPoint.point = snappedPoint;
    return true;
}

int SectorEditor::FindTopologyVertexAtCoordPoint(
        SectorTopologyCoordPoint point,
        int excludedVertexId) const
{
    int bestVertexId = -1;
    for (const SectorTopologyVertex& vertex : state.topologyMap.vertices) {
        if (vertex.id == excludedVertexId) {
            continue;
        }
        if (vertex.x != point.x || vertex.y != point.y) {
            continue;
        }
        if (bestVertexId < 0 || vertex.id < bestVertexId) {
            bestVertexId = vertex.id;
        }
    }
    return bestVertexId;
}

bool SectorEditor::SnapTopologyVertexMoveTarget(
        Vector2 mapPoint,
        SectorTopologyCoordPoint& outPoint,
        std::string& error) const
{
    const float grid = static_cast<float>(std::max(1, state.gridSize));
    Vector2 snapped{
            std::round(mapPoint.x / grid) * grid,
            std::round(mapPoint.y / grid) * grid
    };

    const float threshold = std::max(
            SectorWorldToAuthoringDistance(ScreenVertexSnapPixels / std::max(1.0f, state.viewZoom)),
            grid * 0.20f
    );
    float bestDistance2 = threshold * threshold;
    bool found = false;
    Vector2 best = snapped;
    int bestVertexId = -1;

    for (const SectorTopologyVertex& vertex : state.topologyMap.vertices) {
        if (state.vertexDrag.active && vertex.id == state.vertexDrag.topologyVertexId) {
            continue;
        }

        const Vector2 vertexMap = SectorTopologyVertexToMap(vertex);
        const float dx = vertexMap.x - mapPoint.x;
        const float dy = vertexMap.y - mapPoint.y;
        const float distance2 = dx * dx + dy * dy;
        if (distance2 > bestDistance2) {
            continue;
        }
        if (found
                && std::fabs(distance2 - bestDistance2) <= 0.001f
                && vertex.id >= bestVertexId) {
            continue;
        }

        bestDistance2 = distance2;
        best = vertexMap;
        bestVertexId = vertex.id;
        found = true;
    }

    const Vector2 canonical = found ? best : snapped;
    SectorCoord x = 0;
    SectorCoord y = 0;
    if (!VisibleAuthoringToSectorCoord(canonical.x, x)
            || !VisibleAuthoringToSectorCoord(canonical.y, y)) {
        error = "Move target is outside topology coordinate range";
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
    preview.Render(assets, state.useBakedAmbientOcclusion);
}

void SectorEditor::ApplyPreview3DBloom(engine::AssetManager& assets, RenderTexture2D& sceneTarget)
{
    if (state.mode != SectorEditorMode::Preview3D) {
        return;
    }
    preview.ApplyEmissiveDecalBloom(assets, sceneTarget);
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
    if (!preview.IsReady()) {
        return best;
    }

    const Vector2 localMouse{
            mousePosition.x - viewportRect.x,
            mousePosition.y - viewportRect.y
    };
    const Ray ray = GetScreenToWorldRayEx(
            localMouse,
            preview.Camera(),
            static_cast<int>(std::round(viewportRect.width)),
            static_cast<int>(std::round(viewportRect.height))
    );

    const SectorGeneratedSurfaceHit hit = PickSectorGeneratedGeometry(
            preview.GeneratedGeometry(),
            ray,
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
    if (!preview.IsReady() || preview.IsMouseLookEnabled()) {
        return;
    }

    auto drawSurface = [this](SectorSurfaceRef surface, Color color, float thickness) {
        if (!IsValidSurfaceRef(surface)) {
            return;
        }
        const float lift = IsWallSurface(surface.kind) ? PreviewHighlightLift : PreviewHighlightLift * 2.0f;
        for (const SectorGeneratedSurface& generated : preview.GeneratedGeometry().surfaces) {
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

    BeginMode3D(preview.Camera());
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
    const Rectangle panel{32.0f, 32.0f, 980.0f, 202.0f};
    DrawRectangleRec(panel, Color{12, 15, 20, 205});
    DrawRectangleLinesEx(panel, config.borderThickness, config.borderColor);

    const SectorMeshPreviewPose pose = ActivePreviewPose();
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
    const char* interactionText = preview.IsMouseLookEnabled()
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
    const char* previewStatusText = statusText.empty() ? "Ready" : statusText.c_str();
    const char* collisionWarningText = state.sectorCollisionWorldWarning.empty()
            ? previewStatusText
            : state.sectorCollisionWorldWarning.c_str();
    engine::Text(
            config,
            assets,
            Rectangle{panel.x + 18.0f, panel.y + 160.0f, panel.width - 36.0f, 30.0f},
            font,
            state.previewControlMode == SectorPreviewControlMode::Gameplay
                    ? TextFormat(
                            "assets %.0f%% | Lightmap: %s | AO: %s | walk %.1f | run %.1f | eye %.1f | height %.1f | gravity %.1f | jump %.1f | %s%s",
                            preview.AssetProgress(assets) * 100.0f,
                            preview.LightmapStatusText(),
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
                            preview.AssetProgress(assets) * 100.0f,
                            preview.LightmapStatusText(),
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
    if (preview.IsMouseLookEnabled()) {
        return;
    }

    if (!IsValidSurfaceRef(state.selectedSurface3D)
            || !IsValidTopologySurfaceEditTarget(state.selectedTopologySurface3D)) {
        state.selectedSurface3D = SectorSurfaceRef{};
        state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
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
    const SectorEditorTopologyDrawContext drawContext{
            canvasRect,
            state.viewCenter,
            state.viewZoom,
            state.showSectorIds,
            state.currentTool,
            state.topologySelectionKind,
            state.selectedTopologySectorId,
            state.selectedTopologyVertexId,
            state.selectedTopologyLightId,
            state.hasHoveredVertex,
            state.hoveredTopologyVertexId,
            state.hoveredTopologyLightId
    };
    DrawCachedTopologySectors(state.topologyRenderCache, drawContext);

    DrawTopologySelectedLineHighlight();
    DrawCachedTopologyLineDefs(state.topologyRenderCache, drawContext);
    DrawCachedTopologyVertices(state.topologyRenderCache, drawContext);
    DrawVertexMoveOverlay();
    DrawPendingTopologyVertexMerge();
    DrawPendingTopologyLineSplitAtPoint();
    DrawPendingTopologySectorCut();
    DrawCachedTopologyStaticLights(state.topologyRenderCache, drawContext);
    DrawLightMoveOverlay();
    DrawPendingSector();
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

    const bool useCanonicalSectorPoint = state.currentTool == SectorEditorTool::Sector
            || state.pendingSector.active;
    const Vector2 snap = useCanonicalSectorPoint
            ? MapToScreen(SectorPointToVector2(CurrentSnappedSectorPoint()))
            : MapToScreen(state.snappedMouseMap);
    DrawLineEx(Vector2{snap.x - 9.0f, snap.y}, Vector2{snap.x + 9.0f, snap.y}, 2.0f, Color{235, 224, 130, 255});
    DrawLineEx(Vector2{snap.x, snap.y - 9.0f}, Vector2{snap.x, snap.y + 9.0f}, 2.0f, Color{235, 224, 130, 255});
}

void SectorEditor::DrawPendingSector() const
{
    if (!state.pendingSector.active || state.pendingSector.points.empty()) {
        return;
    }

    const SectorPoint cursorPoint = CurrentSnappedSectorPoint();
    const bool closeable = CanClosePendingSectorAt(cursorPoint);
    std::string previewError = state.pendingSector.errorMessage;
    if (!closeable && state.pendingSector.active) {
        std::string candidateError;
        if (!ValidatePendingTopologyPoint(cursorPoint, candidateError)
                && candidateError != "Click first point to close sector") {
            previewError = candidateError;
        }
    }

    if (state.pendingSector.points.size() >= 3 && previewError.empty()) {
        std::string finalError;
        std::vector<SectorTopologyCoordPoint> topologyPoints;
        SectorTopologyMap previewMap = state.topologyMap;
        int previewSectorId = -1;
        if (!BuildPendingTopologyPoints(topologyPoints, finalError)) {
            previewError = finalError;
        } else if (state.pendingSector.kind == PendingSectorDrawKind::InsertInside) {
            SectorTopologyInsertPolygonOptions options;
            if (!InsertSectorTopologyPolygon(
                        previewMap,
                        state.pendingSector.parentTopologySectorId,
                        topologyPoints,
                        options,
                        &previewSectorId,
                        &finalError)) {
                previewError = finalError;
            }
        } else if (!CreateSectorTopologyPolygon(
                    previewMap,
                    topologyPoints,
                    BuildTopologyCreateOptions(),
                    &previewSectorId,
                    &finalError)) {
            previewError = finalError;
        }
    }

    const bool invalid = !previewError.empty() && previewError != "Click first point to close sector";
    const Color lineColor = invalid ? Color{220, 88, 88, 245} : Color{236, 196, 92, 245};
    const Color previewColor = invalid ? Color{220, 88, 88, 170} : Color{236, 196, 92, 175};
    const Color pointColor = Color{245, 226, 154, 255};
    const Color firstColor = closeable ? Color{120, 230, 154, 255} : Color{245, 226, 154, 255};

    for (size_t i = 0; i + 1 < state.pendingSector.points.size(); ++i) {
        const Vector2 a = MapToScreen(SectorPointToVector2(state.pendingSector.points[i]));
        const Vector2 b = MapToScreen(SectorPointToVector2(state.pendingSector.points[i + 1]));
        DrawLineEx(a, b, 3.0f, lineColor);
    }

    const Vector2 last = MapToScreen(SectorPointToVector2(state.pendingSector.points.back()));
    const Vector2 cursor = MapToScreen(SectorPointToVector2(cursorPoint));
    if (!SamePoint(cursorPoint, state.pendingSector.points.back())) {
        DrawLineEx(last, cursor, 2.0f, previewColor);
    }

    if (state.pendingSector.points.size() >= 2) {
        const Vector2 first = MapToScreen(SectorPointToVector2(state.pendingSector.points.front()));
        DrawLineEx(cursor, first, 1.5f, WithAlpha(previewColor, closeable ? 230 : 120));
    }

    for (size_t i = 0; i < state.pendingSector.points.size(); ++i) {
        const Vector2 point = MapToScreen(SectorPointToVector2(state.pendingSector.points[i]));
        const bool first = i == 0;
        DrawCircleV(point, first ? 6.0f : 5.0f, first ? firstColor : pointColor);
        DrawCircleLines(static_cast<int>(std::round(point.x)), static_cast<int>(std::round(point.y)), first ? 8.0f : 7.0f, Color{20, 24, 32, 255});
    }
}

void SectorEditor::DrawVertexMoveOverlay() const
{
    if (state.currentTool != SectorEditorTool::Move) {
        return;
    }

    if (!state.vertexDrag.active && state.hasHoveredVertex) {
        const Vector2 point = MapToScreen(SectorPointToVector2(
                SectorTopologyCoordPointToSectorPoint(state.hoveredTopologyVertexPoint)));
        size_t connectedCount = 0;
        for (const SectorTopologyLineDef& lineDef : state.topologyMap.lineDefs) {
            if (lineDef.startVertexId == state.hoveredTopologyVertexId
                    || lineDef.endVertexId == state.hoveredTopologyVertexId) {
                ++connectedCount;
            }
        }
        const Color color = connectedCount > 1
                ? Color{122, 220, 244, 255}
                : Color{245, 226, 154, 255};
        DrawCircleLines(static_cast<int>(std::round(point.x)), static_cast<int>(std::round(point.y)), 11.0f, color);
        DrawCircleV(point, 4.5f, color);
        return;
    }

    if (!state.vertexDrag.active) {
        return;
    }

    const bool invalid = !state.vertexDrag.errorMessage.empty();
    const Color targetColor = invalid ? Color{230, 82, 82, 255} : Color{120, 230, 154, 255};
    const Color previewColor = invalid ? Color{230, 82, 82, 205} : Color{122, 220, 244, 220};
    const Color originalColor = Color{245, 226, 154, 230};
    const Vector2 original = MapToScreen(SectorPointToVector2(
            SectorTopologyCoordPointToSectorPoint(state.vertexDrag.originalPoint)));

    if (!state.vertexDrag.hasPreviewPoint) {
        DrawCircleLines(static_cast<int>(std::round(original.x)), static_cast<int>(std::round(original.y)), 10.0f, originalColor);
        return;
    }

    const int draggedVertexId = state.vertexDrag.topologyVertexId;
    const Vector2 previewMap = SectorPointToVector2(
            SectorTopologyCoordPointToSectorPoint(state.vertexDrag.previewPoint));
    for (const SectorTopologyLineDef& lineDef : state.topologyMap.lineDefs) {
        if (lineDef.startVertexId != draggedVertexId && lineDef.endVertexId != draggedVertexId) {
            continue;
        }

        const int otherVertexId = lineDef.startVertexId == draggedVertexId
                ? lineDef.endVertexId
                : lineDef.startVertexId;
        const SectorTopologyVertex* otherVertex = FindSectorTopologyVertex(state.topologyMap, otherVertexId);
        if (otherVertex == nullptr) {
            continue;
        }
        DrawLineEx(
                MapToScreen(previewMap),
                MapToScreen(SectorTopologyVertexToMap(*otherVertex)),
                4.0f,
                previewColor
        );
    }

    const Vector2 target = MapToScreen(previewMap);
    DrawLineEx(original, target, 2.0f, WithAlpha(targetColor, 180));
    DrawCircleLines(static_cast<int>(std::round(original.x)), static_cast<int>(std::round(original.y)), 10.0f, originalColor);
    DrawCircleLines(static_cast<int>(std::round(target.x)), static_cast<int>(std::round(target.y)), state.vertexDrag.hasMergeTarget ? 17.0f : 13.0f, targetColor);
    if (state.vertexDrag.hasMergeTarget) {
        DrawCircleLines(static_cast<int>(std::round(target.x)), static_cast<int>(std::round(target.y)), 22.0f, Color{236, 196, 92, 235});
    }
    DrawCircleV(target, 5.0f, targetColor);
}

void SectorEditor::DrawPendingTopologyVertexMerge() const
{
    if (!state.pendingTopologyVertexMerge.active) {
        return;
    }

    const PendingTopologyVertexMerge& pending = state.pendingTopologyVertexMerge;
    const SectorTopologyVertex* source = FindSectorTopologyVertex(
            state.topologyMap,
            pending.sourceVertexId);
    if (source == nullptr) {
        return;
    }

    const Vector2 sourceScreen = MapToScreen(SectorTopologyVertexToMap(*source));
    DrawCircleLines(
            static_cast<int>(std::round(sourceScreen.x)),
            static_cast<int>(std::round(sourceScreen.y)),
            17.0f,
            Color{236, 196, 92, 255});
    DrawCircleLines(
            static_cast<int>(std::round(sourceScreen.x)),
            static_cast<int>(std::round(sourceScreen.y)),
            23.0f,
            Color{236, 196, 92, 170});

    if (pending.hoveredTargetVertexId < 0) {
        return;
    }

    const SectorTopologyVertex* target = FindSectorTopologyVertex(
            state.topologyMap,
            pending.hoveredTargetVertexId);
    if (target == nullptr) {
        return;
    }

    const Color targetColor = pending.hasValidTarget
            ? Color{120, 230, 154, 255}
            : Color{230, 82, 82, 255};
    const Vector2 targetScreen = MapToScreen(SectorTopologyVertexToMap(*target));
    DrawLineEx(sourceScreen, targetScreen, 2.0f, WithAlpha(targetColor, 170));
    DrawCircleLines(
            static_cast<int>(std::round(targetScreen.x)),
            static_cast<int>(std::round(targetScreen.y)),
            18.0f,
            targetColor);
    DrawCircleV(targetScreen, 5.5f, targetColor);
}

void SectorEditor::DrawLightMoveOverlay() const
{
    if (state.currentTool != SectorEditorTool::Move) {
        return;
    }

    if (state.lightDrag.active) {
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

    const SectorTopologyStaticPointLight* light = FindSectorTopologyStaticLight(
            state.topologyMap,
            state.hoveredTopologyLightId);
    if (light == nullptr) {
        return;
    }

    const Vector2 center = MapToScreen(Vector2{light->position.x, light->position.z});
    DrawCircleLines(static_cast<int>(std::round(center.x)), static_cast<int>(std::round(center.y)), 13.0f, Color{245, 226, 154, 255});
}

void SectorEditor::DrawPendingTopologyLineSplitAtPoint() const
{
    if (!state.pendingTopologyLineSplitAtPoint.active) {
        return;
    }

    const PendingTopologyLineSplitAtPoint& pending = state.pendingTopologyLineSplitAtPoint;
    const SectorTopologyLineDef* lineDef = FindSectorTopologyLineDef(
            state.topologyMap,
            pending.lineDefId);
    if (lineDef == nullptr) {
        return;
    }

    const SectorTopologyVertex* start = nullptr;
    const SectorTopologyVertex* end = nullptr;
    if (!GetSectorTopologyLineVertices(state.topologyMap, *lineDef, start, end)) {
        return;
    }

    const Vector2 a = MapToScreen(SectorTopologyVertexToMap(*start));
    const Vector2 b = MapToScreen(SectorTopologyVertexToMap(*end));
    DrawLineEx(a, b, 13.0f, Color{236, 196, 92, 120});
    DrawLineEx(a, b, 4.0f, Color{236, 196, 92, 245});

    if (!pending.hasCandidatePoint) {
        return;
    }

    const Vector2 candidate = MapToScreen(SectorPointToVector2(
            SectorTopologyCoordPointToSectorPoint(pending.candidatePoint)));
    const Color color = pending.hasValidCandidate
            ? Color{120, 230, 154, 255}
            : Color{230, 82, 82, 245};
    DrawCircleLines(
            static_cast<int>(std::round(candidate.x)),
            static_cast<int>(std::round(candidate.y)),
            pending.hasValidCandidate ? 13.0f : 11.0f,
            color);
    DrawCircleV(candidate, pending.hasValidCandidate ? 5.5f : 4.5f, color);
}

void SectorEditor::DrawPendingTopologySectorCut() const
{
    if (!state.pendingTopologySectorCut.active) {
        return;
    }

    const PendingTopologySectorCut& pending = state.pendingTopologySectorCut;
    const SectorTopologySector* sector = FindSectorTopologySector(state.topologyMap, pending.sectorId);
    if (sector == nullptr) {
        return;
    }

    auto pointToScreen = [this](const SectorTopologyBoundaryCutPoint& point, Vector2& outScreen) {
        if (point.vertexId >= 0) {
            const SectorTopologyVertex* vertex = FindSectorTopologyVertex(state.topologyMap, point.vertexId);
            if (vertex == nullptr) {
                return false;
            }
            outScreen = MapToScreen(SectorTopologyVertexToMap(*vertex));
            return true;
        }
        outScreen = MapToScreen(SectorPointToVector2(
                SectorTopologyCoordPointToSectorPoint(point.point)));
        return true;
    };

    Vector2 firstScreen{};
    if (pending.hasFirstPoint && pointToScreen(pending.firstPoint, firstScreen)) {
        DrawCircleLines(
                static_cast<int>(std::round(firstScreen.x)),
                static_cast<int>(std::round(firstScreen.y)),
                15.0f,
                Color{236, 196, 92, 255});
        DrawCircleV(firstScreen, 5.5f, Color{236, 196, 92, 255});
    }

    if (!pending.hasCandidatePoint) {
        return;
    }

    Vector2 candidateScreen{};
    if (!pointToScreen(pending.candidatePoint, candidateScreen)) {
        return;
    }

    const Color color = pending.hasFirstPoint
            ? (pending.hasValidCandidate ? Color{120, 230, 154, 255} : Color{230, 82, 82, 245})
            : (pending.hasValidCandidate ? Color{236, 196, 92, 255} : Color{230, 82, 82, 245});
    if (pending.hasFirstPoint) {
        DrawLineEx(firstScreen, candidateScreen, 3.0f, WithAlpha(color, 210));
    }
    DrawCircleLines(
            static_cast<int>(std::round(candidateScreen.x)),
            static_cast<int>(std::round(candidateScreen.y)),
            pending.hasValidCandidate ? 13.0f : 11.0f,
            color);
    DrawCircleV(candidateScreen, pending.hasValidCandidate ? 5.0f : 4.0f, color);
}

void SectorEditor::DrawToolsPanel(
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
            "sector_editor_tools",
            BuildLeftPanelRect(),
            font,
            "Tools"
    );

    const float rowH = 46.0f;
    const float gap = config.rowSpacing;
    const float toolsContentH = 1166.0f;
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

    const SectorEditorTool tools[] = {
            SectorEditorTool::Select,
            SectorEditorTool::Sector,
            SectorEditorTool::Light,
            SectorEditorTool::Move,
            SectorEditorTool::Erase
    };

    for (SectorEditorTool tool : tools) {
        if (engine::ToolButton(
                ui,
                config,
                input,
                assets,
                TextFormat("sector_editor_tool_%s", ToolName(tool)),
                Rectangle{0.0f, y, contentW, rowH},
                font,
                ToolName(tool),
                state.currentTool == tool)) {
            if ((state.currentTool == SectorEditorTool::Sector
                        || state.currentTool == SectorEditorTool::InsertSectorInside)
                    && tool != state.currentTool) {
                CancelPendingSector("Cancelled sector");
            }
            if (state.vertexDrag.active && tool != SectorEditorTool::Move) {
                CancelVertexDrag("Cancelled vertex move");
            }
            if (state.lightDrag.active && tool != SectorEditorTool::Move) {
                CancelLightDrag("Cancelled light move");
            }
            if (state.pendingTopologyLineSplitAtPoint.active) {
                CancelPendingTopologyLineSplitAtPoint("Cancelled split at point");
            }
            if (state.pendingTopologyVertexMerge.active) {
                CancelPendingTopologyVertexMerge("Cancelled vertex merge");
            }
            state.currentTool = tool;
        }
        y += rowH + gap;
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
    const SectorTopologyVertex* inspectedVertex = FindSectorTopologyVertex(
            state.topologyMap,
            state.inspectedTopologyVertexId);
    const bool hasInspectedVertex = !hasSelectedTopologySector
            && !hasSelectedTopologyVertex
            && !hasSelectedTopologySideDef
            && !hasSelectedTopologyLineDef
            && !hasSelectedLight
            && state.currentTool == SectorEditorTool::Move
            && inspectedVertex != nullptr;

    const float rowH = 40.0f;
    const float gap = 8.0f;
    const float scrollContentW = std::max(0.0f, panel.contentRect.width - config.scrollbarSize);
    const auto inspectorContentHeight = [&]() {
        if (hasSelectedLight) {
            return StaticLightInspectorContentHeight(rowH, gap, !uiState.idEditError.empty());
        }
        if (hasSelectedTopologySector) {
            return SectorInspectorContentHeight(rowH, gap, !uiState.idEditError.empty());
        }
        if (hasSelectedTopologyVertex) {
            return SelectedVertexInspectorContentHeight();
        }
        if (hasSelectedTopologySideDef) {
            return 1240.0f;
        }
        if (hasSelectedTopologyLineDef) {
            return 218.0f;
        }
        if (hasInspectedVertex || state.pendingTopologyVertexMerge.active) {
            return InspectedVertexInspectorContentHeight();
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

    if (hasSelectedTopologySector) {
        const SectorEditorSectorInspectorCallbacks callbacks{
                [this]() { return TryRenameSelectedTopologySector(); },
                [this]() { OpenDeleteSelectedTopologySectorConfirmation(); },
                [this]() { StartInsertSectorInside(); },
                [this]() { StartPendingTopologySectorCut(); },
                [this](const char* status) { statusText = status != nullptr ? status : ""; },
                [this](const char* status) { MarkTopologyDocumentEdited(status); },
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

    if (hasSelectedTopologySideDef || hasSelectedTopologyLineDef) {
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

    if (hasSelectedTopologyVertex || hasInspectedVertex || state.pendingTopologyVertexMerge.active) {
        const SectorEditorVertexInspectorCallbacks callbacks{
                [this](const char* message) { CancelPendingTopologyVertexMerge(message); },
                [this]() { return DissolveSelectedTopologyVertex(); },
                [this](int vertexId) { StartPendingTopologyVertexMerge(vertexId); },
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

    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_topology_split_linedef",
                Rectangle{0.0f, y, contentW, 38.0f},
                font,
                "Split Linedef")) {
        SplitSelectedTopologyLineDef();
        return true;
    }
    y += 38.0f + gap;

    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_topology_split_linedef_at_point",
                Rectangle{0.0f, y, contentW, 38.0f},
                font,
                "Split At Point")) {
        StartPendingTopologyLineSplitAtPoint();
        return true;
    }
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
            if (engine::Button(
                        ui,
                        config,
                        input,
                        assets,
                        "sector_editor_topology_join_sectors",
                        Rectangle{0.0f, y, contentW, 38.0f},
                        font,
                        "Join Sectors")) {
                JoinSelectedTopologySectors();
                return true;
            }
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
            if (engine::Button(
                        ui,
                        config,
                        input,
                        assets,
                        "sector_editor_topology_join_sectors",
                        Rectangle{0.0f, y, contentW, 38.0f},
                        font,
                        "Join Sectors")) {
                JoinSelectedTopologySectors();
                return true;
            }
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
        selectedLabel = TextFormat("topology light %d", light->id);
    } else if (const SectorTopologySector* topologySector = SelectedTopologySector()) {
        selectedLabel = topologySector->name.empty()
                ? TextFormat("topology sector %d", topologySector->id)
                : TextFormat("%s (%d)", topologySector->name.c_str(), topologySector->id);
    }

    std::string pendingText;
    if (state.pendingSector.active) {
        pendingText = TextFormat(" | pending %zu pts", state.pendingSector.points.size());
    } else if (state.pendingTopologyLineSplitAtPoint.active) {
        pendingText = " | split at point";
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
    preview.Shutdown(assets);
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
    const bool hadPendingSplit = state.pendingTopologyLineSplitAtPoint.active;
    SectorTopologyMap loaded;
    if (!LoadSectorTopologyDocumentFromAsset(jsonAssetPath, loaded, state.loadLevelModal.errorMessage)) {
        statusText = state.loadLevelModal.errorMessage;
        return false;
    }

    preview.Shutdown(assets);
    CancelPendingSector(nullptr);
    CancelPendingTopologyVertexMerge(nullptr);
    CancelPendingTopologyLineSplitAtPoint(nullptr);
    CancelPendingTopologySectorCut(nullptr);
    CancelVertexDrag(nullptr);
    CancelLightDrag(nullptr);
    state.topologyMap = std::move(loaded);
    InvalidateTopologyRenderCache();
    state.fpsControllerConfig = SectorFpsControllerConfigFromPreviewSettings(
            state.topologyMap.previewSettings);
    state.topologyDocumentInitialized = true;
    state.topologyDocumentDirty = false;
    state.topologyDocumentStatus = TextFormat("Topology document: loaded %s", jsonAssetPath.c_str());
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
    state.hasHoveredVertex = false;
    state.hoveredTopologyVertexId = -1;
    state.hoveredTopologyVertexPoint = SectorTopologyCoordPoint{};
    state.pendingSector = PendingSectorDraw{};
    state.vertexDrag = VertexDragState{};
    state.lightDrag = LightDragState{};
    RefreshDefaultTextures();
    RefreshEditorTextureAssets(assets);
    statusText = hadPendingSplit
            ? TextFormat("Loaded topology %s; split at point cancelled", jsonAssetPath.c_str())
            : TextFormat("Loaded topology %s", jsonAssetPath.c_str());
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

    if (!SaveSectorTopologyDocument(savePlan.paths, state.topologyMap, modal.errorMessage)) {
        statusText = TextFormat("Save failed: %s", savePlan.paths.jsonAssetPath.c_str());
        return false;
    }

    state.currentLevelName = name;
    state.currentLevelPath = savePlan.paths.jsonAssetPath;
    state.hasCurrentLevelPath = true;
    state.hasUnsavedChanges = false;
    state.topologyDocumentInitialized = true;
    state.topologyDocumentDirty = false;
    state.topologyDocumentStatus = TextFormat("Topology document: saved %s", savePlan.paths.jsonAssetPath.c_str());
    state.saveLevelModal = SaveLevelModalState{};
    state.confirmationModal = ConfirmationModalState{};
    state.decalTintModal = DecalTintModalState{};
    statusText = TextFormat("Saved topology %s", savePlan.paths.jsonAssetPath.c_str());
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

    CancelPendingSector(nullptr);
    CancelPendingTopologyVertexMerge(nullptr);
    CancelPendingTopologyLineSplitAtPoint(nullptr);
    CancelPendingTopologySectorCut(nullptr);
    CancelVertexDrag(nullptr);
    CancelLightDrag(nullptr);
    ui.hotId = 0;
    ui.activeId = 0;
    ui.openOptionId = 0;
    ui.focusedId = 0;
    uiState.keyboardCaptured = false;

    std::string error;
    if (!preview.Rebuild(assets, state.topologyMap, "sector_editor_preview", error)) {
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
        preview.ApplyPose(state.lastPreviewPose);
    }

    state.previewControlMode = SectorPreviewControlMode::FreeFly;
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
    preview.Leave();
    statusText = "Returned to 2D editor";
}

SectorMeshPreviewPose SectorEditor::ActivePreviewPose() const
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
    if (skyChanged && state.mode == SectorEditorMode::Preview3D && preview.IsReady()) {
        RebuildPreviewMeshesPreservingView(assets);
    }
    if (state.mode == SectorEditorMode::Preview3D
            && state.previewControlMode == SectorPreviewControlMode::Gameplay
            && preview.IsReady()) {
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
    state.selectedTopologySideKind = SectorTopologySideKind::Front;
    state.inspectedTopologyVertexId = -1;
    state.selectedSurface3D = SectorSurfaceRef{};
    state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
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
    state.selectedTopologySideKind = SectorTopologySideKind::Front;
    state.inspectedTopologyVertexId = vertex->id;
    state.selectedSurface3D = SectorSurfaceRef{};
    state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
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
    state.selectedTopologySideKind = sideDef->side;
    state.selectedTopologyWallPart = ValidTopologyWallPartForSideDef(
            state.topologyMap,
            sideDef,
            wallPart);
    state.inspectedTopologyVertexId = -1;
    state.selectedSurface3D = SectorSurfaceRef{};
    state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
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
    state.selectedTopologySideKind = side;
    state.selectedTopologyWallPart = wallPart == TopologyWallPart::Middle
            ? TopologyWallPart::Wall
            : wallPart;
    state.inspectedTopologyVertexId = -1;
    state.selectedSurface3D = SectorSurfaceRef{};
    state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
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
    state.topologySelectionKind = TopologySelectionKind::Light;
    state.selectedTopologySectorId = -1;
    state.selectedTopologyVertexId = -1;
    state.selectedTopologySideDefId = -1;
    state.selectedTopologyLineDefId = -1;
    state.selectedTopologySideKind = SectorTopologySideKind::Front;
    state.inspectedTopologyVertexId = -1;
    state.selectedSurface3D = SectorSurfaceRef{};
    state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
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

void SectorEditor::ClearSelection()
{
    state.pendingTopologyLineSplitAtPoint = PendingTopologyLineSplitAtPoint{};
    state.pendingTopologyVertexMerge = PendingTopologyVertexMerge{};
    state.pendingTopologySectorCut = PendingTopologySectorCut{};
    state.topologySelectionKind = TopologySelectionKind::None;
    state.selectedTopologySectorId = -1;
    state.selectedTopologyVertexId = -1;
    state.selectedTopologySideDefId = -1;
    state.selectedTopologyLineDefId = -1;
    state.selectedTopologyLightId = -1;
    state.selectedTopologySideKind = SectorTopologySideKind::Front;
    state.inspectedTopologyVertexId = -1;
    state.selectedSurface3D = SectorSurfaceRef{};
    state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ResetSurface3DUiState();
    uiState.ambientIntensityInput = engine::UIFloatInputState{};
    uiState.ambientRedInput = engine::UIIntInputState{};
    uiState.ambientGreenInput = engine::UIIntInputState{};
    uiState.ambientBlueInput = engine::UIIntInputState{};
    uiState.inspectorScroll.offset = Vector2{};
    SyncSelectedSectorIdBuffer();
    SyncSelectedLightIdBuffer();
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
    return FinishMaterialActionResult(game::ApplySurfaceDecalOpacity(state.topologyMap, target, opacity), assets);
}

bool SectorEditor::ApplySurfaceDecalEmissive(
        TopologySurfaceEditTarget target,
        bool emissive,
        engine::AssetManager* assets)
{
    return FinishMaterialActionResult(game::ApplySurfaceDecalEmissive(state.topologyMap, target, emissive), assets);
}

bool SectorEditor::ApplySurfaceDecalTint(
        TopologySurfaceEditTarget target,
        Vector3 tint,
        engine::AssetManager* assets)
{
    return FinishMaterialActionResult(game::ApplySurfaceDecalTint(state.topologyMap, target, tint), assets);
}

bool SectorEditor::ApplySurfaceDecalBloomIntensity(
        TopologySurfaceEditTarget target,
        float bloomIntensity,
        engine::AssetManager* assets)
{
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
    return FinishMaterialActionResult(game::ClearSurfaceDecal(state.topologyMap, target), assets);
}

bool SectorEditor::ClearMiddleTexture(
        TopologySurfaceEditTarget target,
        engine::AssetManager* assets)
{
    return FinishMaterialActionResult(game::ClearMiddleTexture(state.topologyMap, target), assets);
}

bool SectorEditor::ResetSurface3DUv(
        TopologySurfaceEditTarget target,
        TopologyMaterialLayer layer,
        engine::AssetManager& assets)
{
    return FinishMaterialActionResult(
            game::ResetSurfaceUv(state.topologyMap, target, layer, state.selectedSurface3D.kind),
            &assets);
}

bool SectorEditor::FitSelectedDecal(
        TopologySurfaceEditTarget target,
        engine::AssetManager* assets)
{
    return FinishMaterialActionResult(game::FitSelectedDecal(state.topologyMap, target), assets);
}

bool SectorEditor::FitSelectedFlatDecal(
        TopologySurfaceEditTarget target,
        engine::AssetManager* assets)
{
    return FinishMaterialActionResult(game::FitSelectedFlatDecal(state.topologyMap, target), assets);
}

bool SectorEditor::FitSelectedWallMaterial(
        TopologySurfaceEditTarget target,
        TopologyUvFitMode mode,
        engine::AssetManager* assets,
        TopologyMaterialLayer layer)
{
    return FinishMaterialActionResult(
            game::FitSelectedWallMaterial(state.topologyMap, target, mode, layer),
            assets);
}

bool SectorEditor::AlignSelectedWallMaterialVertical(
        TopologySurfaceEditTarget target,
        engine::AssetManager* assets,
        TopologyMaterialLayer layer)
{
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
    return FinishMaterialActionResult(
            game::AlignSelectedWallMaterialU(state.topologyMap, target, direction, layer),
            assets);
}

bool SectorEditor::RebuildPreviewMeshesPreservingView(engine::AssetManager& assets)
{
    if (!preview.IsReady()) {
        return false;
    }

    if (state.previewControlMode == SectorPreviewControlMode::Gameplay) {
        ClearSectorFpsLandingDip(state.landingDipState);
        ApplyGameplayPoseToPreview();
    }
    const SectorMeshPreviewPose pose = preview.Pose();
    const bool mouseLook = preview.IsMouseLookEnabled();
    const SectorSurfaceRef selected = state.selectedSurface3D;
    const TopologySurfaceEditTarget selectedTarget = state.selectedTopologySurface3D;

    std::string error;
    if (!preview.Rebuild(assets, state.topologyMap, "sector_editor_preview", error)) {
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
        return false;
    }

    preview.ApplyPose(pose);
    preview.SetMouseLookEnabled(mouseLook);
    const bool selectedStillValid = IsValidSurfaceRef(selected);
    state.selectedSurface3D = selectedStillValid ? selected : SectorSurfaceRef{};
    state.selectedTopologySurface3D = selectedStillValid && IsValidTopologySurfaceEditTarget(selectedTarget)
            ? selectedTarget
            : TopologySurfaceEditTarget{};
    RebuildSectorCollisionWorld();
    return true;
}

bool SectorEditor::SplitSelectedTopologyLineDef()
{
    const SectorTopologyLineDef* lineDef = SelectedTopologyLineDef();
    if (lineDef == nullptr) {
        ClearStaleTopologySelection();
        statusText = "Select a topology linedef before splitting.";
        return false;
    }

    const int originalLineDefId = lineDef->id;
    const TopologySelectionKind previousSelectionKind = state.topologySelectionKind;
    const SectorTopologySideKind previousSide = state.selectedTopologySideKind;
    const TopologyWallPart previousWallPart = state.selectedTopologyWallPart;

    SectorTopologySplitLineResult split;
    std::string error;
    if (!SplitSectorTopologyLineDef(state.topologyMap, originalLineDefId, &split, &error)) {
        statusText = error.empty() ? "Cannot split topology linedef" : error;
        return false;
    }

    state.pendingTopologyLineSplitAtPoint = PendingTopologyLineSplitAtPoint{};
    state.pendingTopologySectorCut = PendingTopologySectorCut{};
    state.hasHoveredVertex = false;
    state.hoveredTopologyVertexId = -1;
    state.hoveredTopologyVertexPoint = SectorTopologyCoordPoint{};
    state.hoveredSurface3D = SectorSurfaceHit{};
    state.selectedSurface3D = SectorSurfaceRef{};
    state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ResetSurface3DUiState();
    for (engine::UIFloatInputState& inputState : uiState.topologySideDefUvInputs) {
        inputState = engine::UIFloatInputState{};
    }

    if (previousSelectionKind == TopologySelectionKind::SideDef) {
        const int secondSideDefId = previousSide == SectorTopologySideKind::Front
                ? split.secondFrontSideDefId
                : split.secondBackSideDefId;
        if (FindSectorTopologySideDef(state.topologyMap, secondSideDefId) != nullptr) {
            SelectTopologySideDef(secondSideDefId, previousWallPart);
        } else {
            SelectTopologyLineDef(split.secondLineDefId, previousSide, previousWallPart);
        }
    } else {
        SelectTopologyLineDef(split.secondLineDefId, previousSide, previousWallPart);
    }

    state.topologyRenderWarning.clear();
    MarkTopologyDocumentEdited(TextFormat(
            "Split topology linedef %d; selected linedef %d",
            originalLineDefId,
            split.secondLineDefId));
    return true;
}

bool SectorEditor::DissolveSelectedTopologyVertex()
{
    const SectorTopologyVertex* vertex = SelectedTopologyVertex();
    if (vertex == nullptr) {
        ClearStaleTopologySelection();
        statusText = "Select a topology vertex before dissolving.";
        return false;
    }

    const int vertexId = vertex->id;
    SectorTopologyDissolveVertexResult dissolve;
    std::string error;
    if (!DissolveSectorTopologyVertex(state.topologyMap, vertexId, &dissolve, &error)) {
        statusText = error.empty() ? "Cannot dissolve topology vertex" : error;
        return false;
    }

    state.pendingTopologyLineSplitAtPoint = PendingTopologyLineSplitAtPoint{};
    state.pendingTopologyVertexMerge = PendingTopologyVertexMerge{};
    state.pendingTopologySectorCut = PendingTopologySectorCut{};
    state.hasHoveredVertex = false;
    state.hoveredTopologyVertexId = -1;
    state.hoveredTopologyVertexPoint = SectorTopologyCoordPoint{};
    state.inspectedTopologyVertexId = -1;
    state.hoveredSurface3D = SectorSurfaceHit{};
    state.selectedSurface3D = SectorSurfaceRef{};
    state.selectedTopologySurface3D = TopologySurfaceEditTarget{};
    ResetSurface3DUiState();
    for (engine::UIFloatInputState& inputState : uiState.topologySideDefUvInputs) {
        inputState = engine::UIFloatInputState{};
    }

    SelectTopologyLineDef(
            dissolve.replacementLineDefId,
            SectorTopologySideKind::Front,
            state.selectedTopologyWallPart);
    state.topologyRenderWarning.clear();
    MarkTopologyDocumentEdited(TextFormat(
            "Dissolved topology vertex %d; selected linedef %d",
            dissolve.removedVertexId,
            dissolve.replacementLineDefId));
    return true;
}

bool SectorEditor::JoinSelectedTopologySectors()
{
    const SectorTopologyLineDef* lineDef = SelectedTopologyLineDef();
    if (lineDef == nullptr) {
        ClearStaleTopologySelection();
        statusText = "Select a two-sided topology portal before joining sectors.";
        return false;
    }

    const SectorTopologySideDef* winnerSideDef = SelectedTopologySideDef();
    if (winnerSideDef == nullptr) {
        const int sideDefId = state.selectedTopologySideKind == SectorTopologySideKind::Front
                ? lineDef->frontSideDefId
                : lineDef->backSideDefId;
        winnerSideDef = FindSectorTopologySideDef(state.topologyMap, sideDefId);
    }
    if (winnerSideDef == nullptr || winnerSideDef->lineDefId != lineDef->id) {
        statusText = "Join Sectors requires a selected side of a two-sided topology portal.";
        return false;
    }

    const SectorTopologySideDef* otherSideDef = FindOppositeSectorTopologySideDef(
            state.topologyMap,
            winnerSideDef->id);
    if (otherSideDef == nullptr) {
        statusText = "Join Sectors requires a two-sided topology portal.";
        return false;
    }
    if (winnerSideDef->sectorId == otherSideDef->sectorId) {
        statusText = "Join Sectors requires two different adjacent sectors.";
        return false;
    }

    SectorTopologyJoinSectorsResult join;
    std::string error;
    if (!JoinSectorTopologySectors(
                state.topologyMap,
                winnerSideDef->sectorId,
                otherSideDef->sectorId,
                &join,
                &error)) {
        statusText = error.empty() ? "Join Sectors rejected." : error;
        return false;
    }

    CancelPendingSector(nullptr);
    CancelVertexDrag(nullptr);
    CancelLightDrag(nullptr);
    state.pendingTopologyLineSplitAtPoint = PendingTopologyLineSplitAtPoint{};
    state.pendingTopologyVertexMerge = PendingTopologyVertexMerge{};
    state.pendingTopologySectorCut = PendingTopologySectorCut{};
    state.hasHoveredVertex = false;
    state.hoveredTopologyVertexId = -1;
    state.hoveredTopologyVertexPoint = SectorTopologyCoordPoint{};
    state.hoveredTopologyLightId = -1;
    ClearTransientTopologyEditStateAfterGeometryChange();
    SelectTopologySector(join.survivingSectorId);
    MarkTopologyDocumentEdited(TextFormat(
            "Joined topology sectors %d and %d; kept sector %d.",
            join.survivingSectorId,
            join.removedSectorId,
            join.survivingSectorId));
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
    if (!game::OpenTopologyTexturePicker(state, sectorId, field, layer)) {
        statusText = "No topology sector texture target";
    }
}

void SectorEditor::OpenTopologySideDefTexturePicker(
        int sideDefId,
        TopologyWallPart wallPart,
        TopologyMaterialLayer layer)
{
    if (!game::OpenTopologySideDefTexturePicker(state, sideDefId, wallPart, layer)) {
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
    const SectorEditorTexturePickerApplyResult result = game::ApplyTexturePickerSelection(state);
    if (result.changed) {
        if (result.useMaterialMutationFinish) {
            FinishTopologyMaterialMutation(result.status.c_str(), &assets);
        } else {
            state.topologyRenderWarning.clear();
            MarkTopologyDocumentEdited(result.status.c_str());
            if (result.rebuildPreviewOnApply && state.mode == SectorEditorMode::Preview3D && preview.IsReady()) {
                RebuildPreviewMeshesPreservingView(assets);
            }
        }
    }
}

} // namespace game
