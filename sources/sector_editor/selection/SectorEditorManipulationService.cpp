#include "sector_editor/selection/SectorEditorManipulationService.h"

#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/SectorEditorTypes.h"
#include "sector_demo/SectorAuthoringGraph.h"
#include "sector_demo/SectorTopologyUnits.h"

namespace game {

namespace {

SectorEditorPickTarget CurrentSelectionTargetForManipulation(SectorEditorManipulationServiceContext& context)
{
    return context.currentPickSelectionTarget != nullptr
            ? context.currentPickSelectionTarget(context.userData)
            : SectorEditorPickTarget{};
}

std::vector<SectorEditorPickCandidate> SelectPickCandidatesForManipulation(
        SectorEditorManipulationServiceContext& context,
        Vector2 screenPoint)
{
    return context.buildSelectPickCandidates != nullptr
            ? context.buildSelectPickCandidates(context.userData, screenPoint)
            : std::vector<SectorEditorPickCandidate>{};
}

SectorEditorMoveContext BuildMoveContext(SectorEditorManipulationServiceContext& context)
{
    return SectorEditorMoveContext{
            context.topologyMap,
            context.runtimeObjectDrag,
            context.statusText,
            context.screenToMap,
            context.snapMapPoint,
            context.selectRuntimeObject,
            context.updateCachedRuntimeObjectDraw,
            context.markTopologyDocumentEdited,
            context.refreshRuntimeObjectsAfterAuthoringEdit};
}

SectorEditorSelectionTarget MakeRuntimeObjectSelectionTarget(int objectId)
{
    return SectorEditorSelectionTarget{SectorEditorSelectionTargetKind::RuntimeObject, objectId};
}

} // namespace

bool IsAnySectorEditorManipulationActive(const SectorEditorManipulationServiceContext& context)
{
    return context.manipulationState.authoringVertexDrag.active
            || context.manipulationState.authoringFogVolumeDrag.active
            || (context.levelMarkerEditing != nullptr && context.levelMarkerEditing->Drag().active)
            || (context.triggerEditing != nullptr && context.triggerEditing->IsMoving())
            || context.lightState.lightDrag.active
            || context.runtimeObjectDrag.active;
}

void UpdateActiveSectorEditorMapPointManipulations(
        SectorEditorManipulationServiceContext& context,
        Vector2 screenPoint)
{
    if (context.manipulationState.authoringFogVolumeDrag.active
            && context.fogVolumeEditing != nullptr
            && context.screenToMap
            && context.snapMapPoint) {
        const Vector2 snapped = context.snapMapPoint(context.screenToMap(screenPoint));
        SectorCoord x = 0;
        SectorCoord y = 0;
        if (VisibleAuthoringToSectorCoord(snapped.x, x)
                && VisibleAuthoringToSectorCoord(snapped.y, y)) {
            context.fogVolumeEditing->UpdateMove(SectorTopologyCoordPoint{x, y});
        }
    }
    if (context.levelMarkerEditing != nullptr
            && context.levelMarkerEditing->Drag().active
            && context.screenToMap
            && context.snapMapPoint) {
        context.levelMarkerEditing->UpdateMove(
                context.snapMapPoint(context.screenToMap(screenPoint)));
    }
    if (context.triggerEditing != nullptr && context.triggerEditing->IsMoving()
            && context.screenToMap && context.snapMapPoint) {
        const Vector2 snapped = context.snapMapPoint(context.screenToMap(screenPoint));
        SectorCoord x = 0;
        SectorCoord z = 0;
        if (VisibleAuthoringToSectorCoord(snapped.x, x)
                && VisibleAuthoringToSectorCoord(snapped.y, z)) {
            context.triggerEditing->UpdateMove(SectorTriggerPoint{x, z});
        }
    }
}

void UpdateActiveSectorEditorManipulation(
        SectorEditorManipulationServiceContext& context,
        engine::Input& input)
{
    if (context.manipulationState.authoringVertexDrag.active && context.updateAuthoringVertexDrag != nullptr) {
        context.updateAuthoringVertexDrag(context.userData, input);
    }
    UpdateActiveSectorEditorMapPointManipulations(context, input.MousePosition());
    if (context.lightState.lightDrag.active && context.updateLightDrag != nullptr) {
        context.updateLightDrag(context.userData, input);
    }
    if (context.runtimeObjectDrag.active) {
        if (context.placedObjectMoveProvider != nullptr
                && context.placedObjectMoveProvider->updateMove != nullptr) {
            SectorEditorMoveContext moveContext = BuildMoveContext(context);
            context.placedObjectMoveProvider->updateMove(moveContext, input.MousePosition());
        } else if (context.updateRuntimeObjectDrag != nullptr) {
            context.updateRuntimeObjectDrag(context.userData, input);
        }
    }
}

void FinishActiveSectorEditorManipulation(SectorEditorManipulationServiceContext& context)
{
    if (context.manipulationState.authoringVertexDrag.active && context.finishAuthoringVertexDrag != nullptr) {
        context.finishAuthoringVertexDrag(context.userData);
    }
    if (context.manipulationState.authoringFogVolumeDrag.active
            && context.fogVolumeEditing != nullptr) {
        context.fogVolumeEditing->FinishMove();
    }
    if (context.levelMarkerEditing != nullptr && context.levelMarkerEditing->Drag().active) {
        context.levelMarkerEditing->FinishMove();
    }
    if (context.triggerEditing != nullptr && context.triggerEditing->IsMoving()) {
        context.triggerEditing->FinishMove();
    }
    if (context.lightState.lightDrag.active && context.finishLightDrag != nullptr) {
        context.finishLightDrag(context.userData);
    }
    if (context.runtimeObjectDrag.active) {
        if (context.placedObjectMoveProvider != nullptr
                && context.placedObjectMoveProvider->finishMove != nullptr) {
            SectorEditorMoveContext moveContext = BuildMoveContext(context);
            context.placedObjectMoveProvider->finishMove(moveContext);
        } else if (context.finishRuntimeObjectDrag != nullptr) {
            context.finishRuntimeObjectDrag(context.userData);
        }
    }
}

bool CancelFirstActiveSectorEditorManipulation(
        SectorEditorManipulationServiceContext& context,
        const char* authoringVertexMessage,
        const char* lightMessage,
        const char* runtimeObjectMessage)
{
    if (context.manipulationState.authoringVertexDrag.active && context.cancelAuthoringVertexDrag != nullptr) {
        context.cancelAuthoringVertexDrag(context.userData, authoringVertexMessage);
        return true;
    }
    if (context.manipulationState.authoringFogVolumeDrag.active
            && context.fogVolumeEditing != nullptr) {
        context.fogVolumeEditing->CancelMove(authoringVertexMessage);
        return true;
    }
    if (context.levelMarkerEditing != nullptr && context.levelMarkerEditing->Drag().active) {
        context.levelMarkerEditing->CancelMove(authoringVertexMessage);
        return true;
    }
    if (context.triggerEditing != nullptr && context.triggerEditing->IsMoving()) {
        context.triggerEditing->CancelMove(authoringVertexMessage);
        return true;
    }
    if (context.lightState.lightDrag.active && context.cancelLightDrag != nullptr) {
        context.cancelLightDrag(context.userData, lightMessage);
        return true;
    }
    if (context.runtimeObjectDrag.active) {
        if (context.placedObjectMoveProvider != nullptr
                && context.placedObjectMoveProvider->cancelMove != nullptr) {
            SectorEditorMoveContext moveContext = BuildMoveContext(context);
            context.placedObjectMoveProvider->cancelMove(moveContext, runtimeObjectMessage);
        } else if (context.cancelRuntimeObjectDrag != nullptr) {
            context.cancelRuntimeObjectDrag(context.userData, runtimeObjectMessage);
        }
        return true;
    }
    return false;
}

void CancelActiveSectorEditorManipulation(
        SectorEditorManipulationServiceContext& context,
        const char* authoringVertexMessage,
        const char* lightMessage,
        const char* runtimeObjectMessage)
{
    if (context.manipulationState.authoringVertexDrag.active && context.cancelAuthoringVertexDrag != nullptr) {
        context.cancelAuthoringVertexDrag(context.userData, authoringVertexMessage);
    }
    if (context.manipulationState.authoringFogVolumeDrag.active
            && context.fogVolumeEditing != nullptr) {
        context.fogVolumeEditing->CancelMove(authoringVertexMessage);
    }
    if (context.levelMarkerEditing != nullptr && context.levelMarkerEditing->Drag().active) {
        context.levelMarkerEditing->CancelMove(authoringVertexMessage);
    }
    if (context.triggerEditing != nullptr && context.triggerEditing->IsMoving()) {
        context.triggerEditing->CancelMove(authoringVertexMessage);
    }
    if (context.lightState.lightDrag.active && context.cancelLightDrag != nullptr) {
        context.cancelLightDrag(context.userData, lightMessage);
    }
    if (context.runtimeObjectDrag.active) {
        if (context.placedObjectMoveProvider != nullptr
                && context.placedObjectMoveProvider->cancelMove != nullptr) {
            SectorEditorMoveContext moveContext = BuildMoveContext(context);
            context.placedObjectMoveProvider->cancelMove(moveContext, runtimeObjectMessage);
        } else if (context.cancelRuntimeObjectDrag != nullptr) {
            context.cancelRuntimeObjectDrag(context.userData, runtimeObjectMessage);
        }
    }
}

void UpdateSectorEditorSelectDragArm(
        SectorEditorManipulationServiceContext& context,
        engine::Input& input)
{
    if (!context.manipulationState.selectDragArm.active) {
        return;
    }
    if (context.currentTool != SectorEditorTool::Select
            || !input.IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        context.manipulationState.selectDragArm = SelectDragArmState{};
        return;
    }
    if (!ShouldStartSectorEditorSelectDrag(
                context.manipulationState.selectDragArm.pressPosition,
                input.MousePosition())) {
        return;
    }

    const SectorEditorPickTarget target = context.manipulationState.selectDragArm.target;
    context.manipulationState.selectDragArm = SelectDragArmState{};
    StartSectorEditorSelectedManipulation(context, target, input.MousePosition());
}

void ArmSectorEditorSelectedDrag(
        SectorEditorManipulationServiceContext& context,
        Vector2 pressPosition)
{
    context.manipulationState.selectDragArm = SelectDragArmState{};
    SectorEditorPickTarget target;
    if (!FindSectorEditorSelectedMovablePickTargetAtScreenPoint(context, pressPosition, target)) {
        return;
    }
    context.manipulationState.selectDragArm.active = true;
    context.manipulationState.selectDragArm.target = target;
    context.manipulationState.selectDragArm.pressPosition = pressPosition;
}

void StartSectorEditorSelectedManipulation(
        SectorEditorManipulationServiceContext& context,
        SectorEditorPickTarget target,
        Vector2 screenPoint)
{
    if (!IsSectorEditorPickTargetMovable(target)) {
        return;
    }

    switch (target.kind) {
        case SectorEditorPickKind::RuntimeObject:
            if (context.placedObjectMoveProvider != nullptr
                    && context.placedObjectMoveProvider->canMove != nullptr
                    && context.placedObjectMoveProvider->beginMove != nullptr) {
                SectorEditorMoveContext moveContext = BuildMoveContext(context);
                const SectorEditorSelectionTarget selectionTarget =
                        MakeRuntimeObjectSelectionTarget(target.id);
                if (context.placedObjectMoveProvider->canMove(moveContext, selectionTarget)) {
                    context.placedObjectMoveProvider->beginMove(
                            moveContext,
                            selectionTarget,
                            screenPoint);
                }
            } else if (context.startRuntimeObjectDrag != nullptr) {
                context.startRuntimeObjectDrag(context.userData, target.id);
            }
            break;
        case SectorEditorPickKind::DynamicSpotLight:
        case SectorEditorPickKind::DynamicLight:
        case SectorEditorPickKind::StaticSpotLight:
        case SectorEditorPickKind::StaticLight:
            if (context.startLightDrag != nullptr) {
                context.startLightDrag(context.userData, target.id, target.spotHandle);
            }
            break;
        case SectorEditorPickKind::AuthoringVertex: {
            (void) screenPoint;
            const SectorAuthoringVertex* vertex = FindSectorAuthoringVertex(
                    context.authoringGraph,
                    target.id);
            if (vertex != nullptr && context.startAuthoringVertexDrag != nullptr) {
                context.startAuthoringVertexDrag(
                        context.userData,
                        target.id,
                        SectorTopologyCoordPoint{vertex->x, vertex->y});
            }
            break;
        }
        case SectorEditorPickKind::AuthoringFogVolume:
            if (context.fogVolumeEditing != nullptr) {
                context.fogVolumeEditing->BeginMove(target.id);
            }
            break;
        case SectorEditorPickKind::LevelMarker:
            if (context.levelMarkerEditing != nullptr) {
                context.levelMarkerEditing->BeginMove(target.id);
            }
            break;
        case SectorEditorPickKind::Trigger:
            if (context.triggerEditing != nullptr && context.screenToMap && context.snapMapPoint) {
                const Vector2 snapped = context.snapMapPoint(context.screenToMap(screenPoint));
                SectorCoord x = 0;
                SectorCoord z = 0;
                if (VisibleAuthoringToSectorCoord(snapped.x, x)
                        && VisibleAuthoringToSectorCoord(snapped.y, z)) {
                    context.triggerEditing->BeginMove(target.id, SectorTriggerPoint{x, z});
                }
            }
            break;
        case SectorEditorPickKind::None:
        case SectorEditorPickKind::AuthoringLine:
        case SectorEditorPickKind::AuthoringFaceAnchor:
            break;
    }
}

bool FindSectorEditorSelectedMovablePickTargetAtScreenPoint(
        SectorEditorManipulationServiceContext& context,
        Vector2 screenPoint,
        SectorEditorPickTarget& outTarget)
{
    outTarget = SectorEditorPickTarget{};
    const SectorEditorPickTarget selected = CurrentSelectionTargetForManipulation(context);
    if (!IsSectorEditorPickTargetMovable(selected)) {
        return false;
    }

    if (selected.kind == SectorEditorPickKind::StaticSpotLight
            && context.findStaticSpotLightHandle != nullptr) {
        int lightId = -1;
        SpotLightHandle handle = SpotLightHandle::Origin;
        if (context.findStaticSpotLightHandle(context.userData, screenPoint, lightId, handle)
                && lightId == selected.id) {
            outTarget = SectorEditorPickTarget{selected.kind, selected.id, handle};
            return true;
        }
    }
    if (selected.kind == SectorEditorPickKind::DynamicSpotLight
            && context.findDynamicSpotLightHandle != nullptr) {
        int lightId = -1;
        SpotLightHandle handle = SpotLightHandle::Origin;
        if (context.findDynamicSpotLightHandle(context.userData, screenPoint, lightId, handle)
                && lightId == selected.id) {
            outTarget = SectorEditorPickTarget{selected.kind, selected.id, handle};
            return true;
        }
    }

    const std::vector<SectorEditorPickCandidate> candidates =
            SelectPickCandidatesForManipulation(context, screenPoint);
    for (const SectorEditorPickCandidate& candidate : candidates) {
        if (!SameSectorEditorPickTarget(candidate.target, selected)) {
            continue;
        }
        outTarget = selected;
        return true;
    }
    return false;
}

} // namespace game
