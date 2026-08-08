#pragma once

#include "engine/ui/UI.h"
#include "sector_editor/SectorEditorSelectionTypes.h"

#include <string>

namespace game {

struct LevelMarkerEditingState {
    LevelMarkerDragState drag;
};

struct LevelMarkerEditingUiState {
    char referenceIdBuffer[64] = {};
    int bufferedMarkerId = -1;
    std::string referenceIdError;
    engine::UIFloatInputState xInput;
    engine::UIFloatInputState yInput;
    engine::UIFloatInputState zInput;
    engine::UIFloatInputState orientationInput;
};

} // namespace game
