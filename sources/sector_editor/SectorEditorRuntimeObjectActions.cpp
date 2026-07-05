#include "sector_editor/SectorEditorRuntimeObjectActions.h"

#include "engine/EngineContext.h"
#include "sector_editor/SectorEditorTopologyActions.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorTopologyMap.h"

namespace game {

void AddSectorEditorRuntimeObject(
        SectorEditorRuntimeObjectActionContext& context,
        Vector2 mapPoint)
{
    const int sectorId = context.findTopologySectorAt
            ? context.findTopologySectorAt(mapPoint)
            : -1;
    const SectorEditorAddBillboardResult result = AddBillboardToSector(
            context.state.topologyMap,
            sectorId,
            mapPoint);
    if (!result.changed) {
        if (!result.status.empty()) {
            context.statusText = result.status;
        }
        return;
    }

    if (context.selectRuntimeObject) {
        context.selectRuntimeObject(result.objectId);
    }
    if (context.markTopologyDocumentEdited) {
        context.markTopologyDocumentEdited(result.status.c_str());
    }
    RefreshSectorEditorRuntimeObjectsAfterAuthoringEdit(context);
}

void AddSectorEditorDoorRuntimeObject(
        SectorEditorRuntimeObjectActionContext& context,
        Vector2 screenPoint)
{
    int lineDefId = -1;
    int sideDefId = -1;
    SectorTopologySideKind side = SectorTopologySideKind::Front;
    bool preferredMissing = false;
    const Vector2 mapPoint = context.screenToMap
            ? context.screenToMap(screenPoint)
            : Vector2{};
    if (!context.findTopologyLineNearScreenPoint
            || !context.findTopologyLineNearScreenPoint(
                    screenPoint,
                    mapPoint,
                    lineDefId,
                    sideDefId,
                    side,
                    preferredMissing)) {
        context.statusText = "Door placement failed: click a two-sided portal";
        return;
    }

    const SectorEditorAddDoorResult result = AddDoorToPortal(context.state.topologyMap, lineDefId);
    if (!result.changed) {
        if (!result.status.empty()) {
            context.statusText = result.status;
        }
        return;
    }

    if (context.selectRuntimeObject) {
        context.selectRuntimeObject(result.objectId);
    }
    if (context.markTopologyDocumentEdited) {
        context.markTopologyDocumentEdited(result.status.c_str());
    }
    RefreshSectorEditorRuntimeObjectsAfterAuthoringEdit(context);
}

SectorEditorRuntimeObjectDeleteConfirmation RequestDeleteSelectedSectorEditorRuntimeObject(
        SectorEditorRuntimeObjectActionContext& context)
{
    const SectorPlacedRuntimeObject* object =
            FindSectorPlacedRuntimeObject(context.state.topologyMap, context.state.selectedRuntimeObjectId);
    if (object == nullptr) {
        return SectorEditorRuntimeObjectDeleteConfirmation{};
    }

    return SectorEditorRuntimeObjectDeleteConfirmation{
            true,
            object->id,
            "Delete Object",
            TextFormat("Delete object %d?", object->id)};
}

bool DeleteSectorEditorRuntimeObjectById(
        SectorEditorRuntimeObjectActionContext& context,
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
    if (context.state.selectedRuntimeObjectId == objectId && context.clearSelection) {
        context.clearSelection();
    }
    if (context.state.runtimeObjectDrag.objectId == objectId) {
        context.state.runtimeObjectDrag = RuntimeObjectDragState{};
    }
    if (context.markTopologyDocumentEdited) {
        context.markTopologyDocumentEdited(TextFormat("Deleted object %d", objectId));
    }
    RefreshSectorEditorRuntimeObjectsAfterAuthoringEdit(context);
    return true;
}

bool MutateSelectedSectorEditorRuntimeObject(
        SectorEditorRuntimeObjectActionContext& context,
        const char* status,
        const std::function<bool(SectorPlacedRuntimeObject&)>& mutate)
{
    SectorPlacedRuntimeObject* object =
            FindSectorPlacedRuntimeObject(context.state.topologyMap, context.state.selectedRuntimeObjectId);
    if (object == nullptr || !mutate || !mutate(*object)) {
        return false;
    }

    if (context.markTopologyDocumentEdited) {
        context.markTopologyDocumentEdited(status);
    }
    RefreshSectorEditorRuntimeObjectsAfterAuthoringEdit(context);
    return true;
}

void RefreshSectorEditorRuntimeObjectsAfterAuthoringEdit(
        SectorEditorRuntimeObjectActionContext& context)
{
    if (context.engineContext == nullptr) {
        return;
    }

    SpawnPlacedRuntimeObjects(
            context.engineContext->world,
            context.engineContext->assets,
            context.state.runtimeObjects,
            context.state.topologyMap);
}

} // namespace game
