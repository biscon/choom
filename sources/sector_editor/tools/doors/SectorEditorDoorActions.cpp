#include "sector_editor/tools/doors/SectorEditorDoorActions.h"

#include "sector_demo/SectorTopologyMap.h"
#include "sector_editor/SectorEditorTopologyActions.h"

namespace game {

void AddSectorEditorDoor(SectorEditorPlacedObjectActionContext &context,
                         Vector2 screenPoint) {
  int lineDefId = -1;
  int sideDefId = -1;
  SectorTopologySideKind side = SectorTopologySideKind::Front;
  bool preferredMissing = false;
  const Vector2 mapPoint =
      context.screenToMap ? context.screenToMap(screenPoint) : Vector2{};
  if (!context.findTopologyLineNearScreenPoint ||
      !context.findTopologyLineNearScreenPoint(screenPoint, mapPoint, lineDefId,
                                               sideDefId, side,
                                               preferredMissing)) {
    context.statusText = "Door placement failed: click a two-sided portal";
    return;
  }

  const SectorEditorAddDoorResult result =
      AddDoorToPortal(context.state.topologyMap, lineDefId);
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
  RefreshSectorEditorPlacedObjectsAfterAuthoringEdit(context);
}

} // namespace game
