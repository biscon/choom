#include "sector_editor/tools/placed_objects/SectorEditorPlacedObjectDrag.h"

#include "sector_editor/SectorEditorHelpers.h"
#include "sector_demo/SectorTopologyMap.h"

#include <raylib.h>

#include <cmath>

namespace game {

void StartSectorEditorPlacedObjectDrag(
        SectorEditorPlacedObjectDragContext& context,
        int objectId)
{
    const SectorPlacedRuntimeObject* object = FindSectorPlacedRuntimeObject(
            context.state.topologyMap,
            objectId);
    if (object == nullptr) {
        return;
    }
    if (object->kind == "door") {
        context.statusText = "Door movement unavailable: doors stay anchored to portal lines";
        return;
    }

    if (context.selectRuntimeObject) {
        context.selectRuntimeObject(objectId);
    }
    context.state.runtimeObjectDrag.active = true;
    context.state.runtimeObjectDrag.objectId = objectId;
    context.state.runtimeObjectDrag.originalPosition = object->position;
    context.state.runtimeObjectDrag.snappedPosition = object->position;
    context.statusText = TextFormat("Moving object %d", objectId);
}

void UpdateSectorEditorPlacedObjectDrag(
        SectorEditorPlacedObjectDragContext& context,
        Vector2 mousePosition)
{
    if (!context.state.runtimeObjectDrag.active) {
        return;
    }

    SectorPlacedRuntimeObject* object = FindSectorPlacedRuntimeObject(
            context.state.topologyMap,
            context.state.runtimeObjectDrag.objectId);
    if (object == nullptr) {
        context.state.runtimeObjectDrag = RuntimeObjectDragState{};
        return;
    }

    const Vector2 mapPoint = context.screenToMap
            ? context.screenToMap(mousePosition)
            : Vector2{};
    const Vector2 snapped = context.snapMapPoint
            ? context.snapMapPoint(mapPoint)
            : mapPoint;
    context.state.runtimeObjectDrag.snappedPosition = Vector3{
            snapped.x,
            context.state.runtimeObjectDrag.originalPosition.y,
            snapped.y};
    object->position = context.state.runtimeObjectDrag.snappedPosition;
    if (context.updateCachedRuntimeObjectDraw) {
        context.updateCachedRuntimeObjectDraw(*object);
    }
    context.statusText = TextFormat("Moving object %d", object->id);
}

void FinishSectorEditorPlacedObjectDrag(
        SectorEditorPlacedObjectDragContext& context)
{
    if (!context.state.runtimeObjectDrag.active) {
        return;
    }

    const int objectId = context.state.runtimeObjectDrag.objectId;
    const Vector3 original = context.state.runtimeObjectDrag.originalPosition;
    context.state.runtimeObjectDrag = RuntimeObjectDragState{};

    SectorPlacedRuntimeObject* object = FindSectorPlacedRuntimeObject(
            context.state.topologyMap,
            objectId);
    if (object == nullptr) {
        return;
    }

    if (context.selectRuntimeObject) {
        context.selectRuntimeObject(objectId);
    }
    const bool moved = std::fabs(object->position.x - original.x) > GeometryEpsilon
            || std::fabs(object->position.z - original.z) > GeometryEpsilon;
    if (!moved) {
        object->position = original;
        if (context.updateCachedRuntimeObjectDraw) {
            context.updateCachedRuntimeObjectDraw(*object);
        }
        context.statusText = TextFormat("Object %d unchanged", objectId);
        return;
    }

    if (context.markTopologyDocumentEdited) {
        context.markTopologyDocumentEdited(TextFormat("Moved object %d", objectId));
    }
    if (context.refreshRuntimeObjectsAfterAuthoringEdit) {
        context.refreshRuntimeObjectsAfterAuthoringEdit();
    }
}

void CancelSectorEditorPlacedObjectDrag(
        SectorEditorPlacedObjectDragContext& context,
        const char* message)
{
    if (context.state.runtimeObjectDrag.active) {
        SectorPlacedRuntimeObject* object = FindSectorPlacedRuntimeObject(
                context.state.topologyMap,
                context.state.runtimeObjectDrag.objectId);
        if (object != nullptr) {
            object->position = context.state.runtimeObjectDrag.originalPosition;
            if (context.updateCachedRuntimeObjectDraw) {
                context.updateCachedRuntimeObjectDraw(*object);
            }
            if (context.selectRuntimeObject) {
                context.selectRuntimeObject(object->id);
            }
        }
    }

    context.state.runtimeObjectDrag = RuntimeObjectDragState{};
    if (message != nullptr && message[0] != '\0') {
        context.statusText = message;
    }
}

} // namespace game
