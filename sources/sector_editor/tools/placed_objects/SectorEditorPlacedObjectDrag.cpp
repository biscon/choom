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
            context.topologyMap,
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
    context.runtimeObjectDrag.active = true;
    context.runtimeObjectDrag.objectId = objectId;
    context.runtimeObjectDrag.originalPosition = object->position;
    context.runtimeObjectDrag.snappedPosition = object->position;
    context.statusText = TextFormat("Moving object %d", objectId);
}

void UpdateSectorEditorPlacedObjectDrag(
        SectorEditorPlacedObjectDragContext& context,
        Vector2 mousePosition)
{
    if (!context.runtimeObjectDrag.active) {
        return;
    }

    SectorPlacedRuntimeObject* object = FindSectorPlacedRuntimeObject(
            context.topologyMap,
            context.runtimeObjectDrag.objectId);
    if (object == nullptr) {
        context.runtimeObjectDrag = RuntimeObjectDragState{};
        return;
    }

    const Vector2 mapPoint = context.screenToMap
            ? context.screenToMap(mousePosition)
            : Vector2{};
    const Vector2 snapped = context.snapMapPoint
            ? context.snapMapPoint(mapPoint)
            : mapPoint;
    context.runtimeObjectDrag.snappedPosition = Vector3{
            snapped.x,
            context.runtimeObjectDrag.originalPosition.y,
            snapped.y};
    object->position = context.runtimeObjectDrag.snappedPosition;
    if (context.updateCachedRuntimeObjectDraw) {
        context.updateCachedRuntimeObjectDraw(*object);
    }
    context.statusText = TextFormat("Moving object %d", object->id);
}

void FinishSectorEditorPlacedObjectDrag(
        SectorEditorPlacedObjectDragContext& context)
{
    if (!context.runtimeObjectDrag.active) {
        return;
    }

    const int objectId = context.runtimeObjectDrag.objectId;
    const Vector3 original = context.runtimeObjectDrag.originalPosition;
    context.runtimeObjectDrag = RuntimeObjectDragState{};

    SectorPlacedRuntimeObject* object = FindSectorPlacedRuntimeObject(
            context.topologyMap,
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
    if (context.runtimeObjectDrag.active) {
        SectorPlacedRuntimeObject* object = FindSectorPlacedRuntimeObject(
                context.topologyMap,
                context.runtimeObjectDrag.objectId);
        if (object != nullptr) {
            object->position = context.runtimeObjectDrag.originalPosition;
            if (context.updateCachedRuntimeObjectDraw) {
                context.updateCachedRuntimeObjectDraw(*object);
            }
            if (context.selectRuntimeObject) {
                context.selectRuntimeObject(object->id);
            }
        }
    }

    context.runtimeObjectDrag = RuntimeObjectDragState{};
    if (message != nullptr && message[0] != '\0') {
        context.statusText = message;
    }
}

} // namespace game
