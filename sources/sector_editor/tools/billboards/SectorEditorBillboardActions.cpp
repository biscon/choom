#include "sector_editor/tools/billboards/SectorEditorBillboardActions.h"

#include "sector_demo/SectorTopologyMap.h"
#include "sector_editor/SectorEditorTopologyActions.h"

namespace game {

void AddSectorEditorBillboard(SectorEditorPlacedObjectActionContext &context,
                              Vector2 mapPoint) {
  const int sectorId = context.findTopologySectorAt
                           ? context.findTopologySectorAt(mapPoint)
                           : -1;
  const SectorEditorAddBillboardResult result =
      AddBillboardToSector(context.state.topologyMap, sectorId, mapPoint);
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
