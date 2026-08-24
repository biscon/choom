#pragma once

#include "sector_editor/SectorEditorSelectionTypes.h"

namespace game {

struct ManipulationState {
    SelectDragArmState selectDragArm;
    AuthoringVertexDragState authoringVertexDrag;
    AuthoringFogVolumeDragState authoringFogVolumeDrag;
    AuthoringReflectionProbeDragState authoringReflectionProbeDrag;
};

} // namespace game
