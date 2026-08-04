#pragma once

#include "sector_editor/SectorEditorAuthoringState.h"
#include "sector_editor/selection/SectorEditorManipulationService.h"
#include "sector_editor/selection/SectorEditorSelectionService.h"
#include "sector_editor/selection/SectorEditorSelectionState.h"
#include "sector_editor/SectorEditorTypes.h"
#include "sector_editor/services/fog_volumes/SectorEditorAuthoringFogVolumeEditingService.h"

#include <raylib.h>

#include <functional>
#include <string>
#include <vector>

namespace engine {
class Input;
struct InputEvent;
}

namespace game {

struct SectorEditorToolContext {
    // Transitional v0 glue: tool modules can inspect editor state directly while
    // mutation/commit paths stay behind narrow callbacks.
    SectorEditorState& state;
    SectorEditorTool& currentTool;
    PendingAuthoringLineDraw& pendingAuthoringLine;
    PendingAuthoringRectangleDraw& pendingAuthoringRectangle;
    PendingAuthoringInsertVertex& pendingAuthoringInsertVertex;
    SectorAuthoringGraph& authoringGraph;
    SelectionState& selectionState;
    std::string& statusText;
    const std::string& authoringDerivationStatus;
    engine::Input* input = nullptr;
    Rectangle canvasRect = {};
    SectorEditorAuthoringFogVolumeEditingService* fogVolumeEditing = nullptr;

    std::function<SectorPoint()> currentSnappedSectorPoint;
    std::function<bool(SectorPoint, SectorTopologyCoordPoint&, std::string&)> toTopologyCoordPoint;
    std::function<Vector2(Vector2)> mapToScreen;
    std::function<Vector2(Vector2)> screenToMap;
    std::function<void()> clearTopologySelectionOnly;
    std::function<void()> clearSelection;
    std::function<void(int)> selectAuthoringLine;
    std::function<void(int)> hoverAuthoringLine;
    std::function<int(Vector2)> findAuthoringLineNearScreenPoint;
    std::function<SectorEditorAuthoringLineToolClickResult(SectorTopologyCoordPoint)>
            commitAuthoringLinePoint;
    std::function<void()> cancelAuthoringLineChain;
    std::function<bool(
            SectorTopologyCoordPoint,
            SectorTopologyCoordPoint,
            SectorEditorAuthoringRectangleResult*)>
            commitAuthoringRectangle;
    std::function<bool(int, Vector2, SectorTopologyCoordPoint&, std::string&)>
            resolveAuthoringInsertVertexPoint;
    std::function<bool(int, SectorTopologyCoordPoint, SectorAuthoringInsertVertexResult*)>
            commitAuthoringInsertVertex;

    std::function<std::vector<SectorEditorPickCandidate>(Vector2)> buildSelectPickCandidates;
    std::function<SectorEditorPickTarget()> currentPickSelectionTarget;
    std::function<SectorEditorSelectionServiceContext()> buildSelectionServiceContext;
    std::function<SectorEditorManipulationServiceContext()> buildManipulationServiceContext;
};

struct SectorEditorToolModule {
    SectorEditorTool id;
    const char* label = "";

    void (*updateHover)(SectorEditorToolContext& context, Vector2 mapPoint) = nullptr;
    bool (*updateEarly)(SectorEditorToolContext& context) = nullptr;
    bool (*handleMousePress)(SectorEditorToolContext& context, const engine::InputEvent& event) = nullptr;
    bool (*update)(SectorEditorToolContext& context) = nullptr;
    void (*drawCanvasOverlay)(SectorEditorToolContext& context) = nullptr;
    bool (*cancel)(SectorEditorToolContext& context, const char* message) = nullptr;
};

const SectorEditorToolModule* FindSectorEditorToolModule(SectorEditorTool tool);

} // namespace game
