#pragma once

#include "sector_editor/SectorEditorSelectionTypes.h"
#include "sector_demo/SectorTopologyMap.h"

#include <raylib.h>

#include <functional>
#include <string>

namespace game {

struct SectorEditorPlacedObjectDragContext {
    SectorTopologyMap& topologyMap;
    RuntimeObjectDragState& runtimeObjectDrag;
    std::string& statusText;

    std::function<Vector2(Vector2)> screenToMap;
    std::function<Vector2(Vector2)> snapMapPoint;
    std::function<void(int)> selectRuntimeObject;
    std::function<void(const SectorPlacedRuntimeObject&)> updateCachedRuntimeObjectDraw;
    std::function<void(const char*)> markTopologyDocumentEdited;
    std::function<void()> refreshRuntimeObjectsAfterAuthoringEdit;
};

void StartSectorEditorPlacedObjectDrag(
        SectorEditorPlacedObjectDragContext& context,
        int objectId);

void UpdateSectorEditorPlacedObjectDrag(
        SectorEditorPlacedObjectDragContext& context,
        Vector2 mousePosition);

void FinishSectorEditorPlacedObjectDrag(
        SectorEditorPlacedObjectDragContext& context);

void CancelSectorEditorPlacedObjectDrag(
        SectorEditorPlacedObjectDragContext& context,
        const char* message);

} // namespace game
