#pragma once

#include "sector_editor/SectorEditorTypes.h"

#include <raylib.h>

#include <functional>
#include <string>

namespace engine {
struct EngineContext;
}

namespace game {

struct SectorEditorRuntimeObjectActionContext {
    SectorEditorState& state;
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

struct SectorEditorRuntimeObjectDeleteConfirmation {
    bool requested = false;
    int objectId = -1;
    std::string title;
    std::string message;
};

void AddSectorEditorRuntimeObject(
        SectorEditorRuntimeObjectActionContext& context,
        Vector2 mapPoint);

void AddSectorEditorDoorRuntimeObject(
        SectorEditorRuntimeObjectActionContext& context,
        Vector2 screenPoint);

SectorEditorRuntimeObjectDeleteConfirmation RequestDeleteSelectedSectorEditorRuntimeObject(
        SectorEditorRuntimeObjectActionContext& context);

bool DeleteSectorEditorRuntimeObjectById(
        SectorEditorRuntimeObjectActionContext& context,
        int objectId);

bool MutateSelectedSectorEditorRuntimeObject(
        SectorEditorRuntimeObjectActionContext& context,
        const char* status,
        const std::function<bool(SectorPlacedRuntimeObject&)>& mutate);

void RefreshSectorEditorRuntimeObjectsAfterAuthoringEdit(
        SectorEditorRuntimeObjectActionContext& context);

} // namespace game
