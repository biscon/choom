#pragma once

#include "sector_editor/SectorEditorTypes.h"

#include <raylib.h>

#include <functional>
#include <string>

namespace game {

struct SectorEditorRuntimeObjectDragContext {
    SectorEditorState& state;
    std::string& statusText;

    std::function<Vector2(Vector2)> screenToMap;
    std::function<Vector2(Vector2)> snapMapPoint;
    std::function<void(int)> selectRuntimeObject;
    std::function<void(const SectorPlacedRuntimeObject&)> updateCachedRuntimeObjectDraw;
    std::function<void(const char*)> markTopologyDocumentEdited;
    std::function<void()> refreshRuntimeObjectsAfterAuthoringEdit;
};

void StartSectorEditorRuntimeObjectDrag(
        SectorEditorRuntimeObjectDragContext& context,
        int objectId);

void UpdateSectorEditorRuntimeObjectDrag(
        SectorEditorRuntimeObjectDragContext& context,
        Vector2 mousePosition);

void FinishSectorEditorRuntimeObjectDrag(
        SectorEditorRuntimeObjectDragContext& context);

void CancelSectorEditorRuntimeObjectDrag(
        SectorEditorRuntimeObjectDragContext& context,
        const char* message);

} // namespace game
