#pragma once

#include "sector_editor/SectorEditorSelectionTypes.h"

namespace game {

struct ManipulationState {
    SelectDragArmState selectDragArm;
    AuthoringVertexDragState authoringVertexDrag;
};

} // namespace game
