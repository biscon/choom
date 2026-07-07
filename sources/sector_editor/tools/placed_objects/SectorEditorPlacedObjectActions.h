#pragma once

#include "sector_editor/SectorEditorTypes.h"
#include "sector_editor/selection/SectorEditorSelectionState.h"

#include <raylib.h>

#include <functional>
#include <string>

namespace engine {
struct EngineContext;
}

namespace game {

struct SectorRuntimeObjectState;

struct SectorEditorPlacedObjectActionContext {
    SectorEditorState& state;
    SectorTopologyMap& topologyMap;
    SectorRuntimeObjectState& runtimeObjects;
    SelectionState& selectionState;
    std::string& statusText;
    engine::EngineContext* engineContext = nullptr;

    std::function<int(Vector2)> findTopologySectorAt;
    std::function<bool(Vector2, Vector2, int&, int&, SectorTopologySideKind&, bool&)>
            findTopologyLineNearScreenPoint;
    std::function<Vector2(Vector2)> screenToMap;
    std::function<void(int)> selectRuntimeObject;
    std::function<void()> clearSelection;
    std::function<void()> clearStaleTopologySelection;
    std::function<void(const char*)> markTopologyDocumentEdited;
};

struct SectorEditorPlacedObjectDeleteConfirmation {
    bool requested = false;
    int objectId = -1;
    std::string title;
    std::string message;
};

SectorEditorPlacedObjectDeleteConfirmation RequestDeleteSelectedSectorEditorPlacedObject(
        SectorEditorPlacedObjectActionContext& context);

bool DeleteSectorEditorPlacedObjectById(
        SectorEditorPlacedObjectActionContext& context,
        int objectId);

bool MutateSelectedSectorEditorPlacedObject(
        SectorEditorPlacedObjectActionContext& context,
        const char* status,
        const std::function<bool(SectorPlacedRuntimeObject&)>& mutate);

void RefreshSectorEditorPlacedObjectsAfterAuthoringEdit(
        SectorEditorPlacedObjectActionContext& context);

} // namespace game
