#include "sector_editor/tools/placed_objects/SectorEditorPlacedObjectMoveProvider.h"

#include "sector_editor/selection/SectorEditorMoveContext.h"
#include "sector_editor/tools/placed_objects/SectorEditorPlacedObjectDrag.h"

namespace game {

namespace {

SectorEditorPlacedObjectDragContext MakePlacedObjectDragContext(SectorEditorMoveContext& context)
{
    return SectorEditorPlacedObjectDragContext{
            context.state,
            context.statusText,
            context.screenToMap,
            context.snapMapPoint,
            context.selectRuntimeObject,
            context.updateCachedRuntimeObjectDraw,
            context.markTopologyDocumentEdited,
            context.refreshRuntimeObjectsAfterAuthoringEdit};
}

bool CanMovePlacedObject(SectorEditorMoveContext&, SectorEditorSelectionTarget target)
{
    return IsPlacedObjectTarget(target);
}

bool BeginPlacedObjectMove(
        SectorEditorMoveContext& context,
        SectorEditorSelectionTarget target,
        Vector2)
{
    if (!IsPlacedObjectTarget(target)) {
        return false;
    }

    SectorEditorPlacedObjectDragContext dragContext = MakePlacedObjectDragContext(context);
    StartSectorEditorPlacedObjectDrag(dragContext, target.id);
    return context.state.runtimeObjectDrag.active;
}

void UpdatePlacedObjectMove(SectorEditorMoveContext& context, Vector2 mousePosition)
{
    SectorEditorPlacedObjectDragContext dragContext = MakePlacedObjectDragContext(context);
    UpdateSectorEditorPlacedObjectDrag(dragContext, mousePosition);
}

void FinishPlacedObjectMove(SectorEditorMoveContext& context)
{
    SectorEditorPlacedObjectDragContext dragContext = MakePlacedObjectDragContext(context);
    FinishSectorEditorPlacedObjectDrag(dragContext);
}

void CancelPlacedObjectMove(SectorEditorMoveContext& context, const char* message)
{
    SectorEditorPlacedObjectDragContext dragContext = MakePlacedObjectDragContext(context);
    CancelSectorEditorPlacedObjectDrag(dragContext, message);
}

} // namespace

const SectorEditorMoveProvider& SectorEditorPlacedObjectMoveProvider()
{
    static const SectorEditorMoveProvider provider{
            CanMovePlacedObject,
            BeginPlacedObjectMove,
            UpdatePlacedObjectMove,
            FinishPlacedObjectMove,
            CancelPlacedObjectMove};
    return provider;
}

} // namespace game
