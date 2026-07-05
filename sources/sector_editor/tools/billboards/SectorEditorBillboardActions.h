#pragma once

#include "sector_editor/tools/placed_objects/SectorEditorPlacedObjectActions.h"

#include <raylib.h>

namespace game {

void AddSectorEditorBillboard(SectorEditorPlacedObjectActionContext &context,
                              Vector2 mapPoint);

} // namespace game
