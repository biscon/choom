#pragma once

#include "sector_editor/tools/placed_objects/SectorEditorPlacedObjectActions.h"

#include <raylib.h>

namespace game {

void AddSectorEditorDoor(SectorEditorPlacedObjectActionContext &context,
                         Vector2 screenPoint);

} // namespace game
