#include "sector_editor/tools/placed_objects/SectorEditorPlacedObjectActions.h"

#include "engine/EngineContext.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorTopologyMap.h"

namespace game {

SectorEditorPlacedObjectDeleteConfirmation RequestDeleteSelectedSectorEditorPlacedObject(
        SectorEditorPlacedObjectActionContext& context)
{
    const SectorPlacedRuntimeObject* object =
            FindSectorPlacedRuntimeObject(context.state.topologyMap, context.selectionState.selectedRuntimeObjectId);
    if (object == nullptr) {
        return SectorEditorPlacedObjectDeleteConfirmation{};
    }

    return SectorEditorPlacedObjectDeleteConfirmation{
            true,
            object->id,
            "Delete Object",
            TextFormat("Delete object %d?", object->id)};
}

bool DeleteSectorEditorPlacedObjectById(
        SectorEditorPlacedObjectActionContext& context,
        int objectId)
{
    if (FindSectorPlacedRuntimeObject(context.state.topologyMap, objectId) == nullptr) {
        if (context.clearStaleTopologySelection) {
            context.clearStaleTopologySelection();
        }
        return false;
    }

    if (!RemoveSectorPlacedRuntimeObject(context.state.topologyMap, objectId)) {
        return false;
    }
    if (context.selectionState.selectedRuntimeObjectId == objectId && context.clearSelection) {
        context.clearSelection();
    }
    if (context.state.runtimeObjectDrag.objectId == objectId) {
        context.state.runtimeObjectDrag = RuntimeObjectDragState{};
    }
    if (context.markTopologyDocumentEdited) {
        context.markTopologyDocumentEdited(TextFormat("Deleted object %d", objectId));
    }
    RefreshSectorEditorPlacedObjectsAfterAuthoringEdit(context);
    return true;
}

bool MutateSelectedSectorEditorPlacedObject(
        SectorEditorPlacedObjectActionContext& context,
        const char* status,
        const std::function<bool(SectorPlacedRuntimeObject&)>& mutate)
{
    SectorPlacedRuntimeObject* object =
            FindSectorPlacedRuntimeObject(context.state.topologyMap, context.selectionState.selectedRuntimeObjectId);
    if (object == nullptr || !mutate || !mutate(*object)) {
        return false;
    }

    if (context.markTopologyDocumentEdited) {
        context.markTopologyDocumentEdited(status);
    }
    RefreshSectorEditorPlacedObjectsAfterAuthoringEdit(context);
    return true;
}

void RefreshSectorEditorPlacedObjectsAfterAuthoringEdit(
        SectorEditorPlacedObjectActionContext& context)
{
    if (context.engineContext == nullptr) {
        return;
    }

    SpawnPlacedRuntimeObjects(
            context.engineContext->world,
            context.engineContext->assets,
            context.runtimeObjects,
            context.state.topologyMap);
}

} // namespace game
