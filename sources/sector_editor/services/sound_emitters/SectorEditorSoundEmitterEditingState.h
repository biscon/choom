#pragma once

#include "engine/ui/UI.h"
#include "sector_editor/SectorEditorSelectionTypes.h"

#include <string>

namespace game {

struct SoundEmitterEditingState {
    SoundEmitterDragState drag;
};

struct SoundEmitterEditingUiState {
    char referenceIdBuffer[64] = {};
    char soundIdBuffer[64] = {};
    int bufferedEmitterId = -1;
    std::string bufferedSoundId;
    std::string referenceIdError;
    std::string soundIdError;
    engine::UIFloatInputState xInput;
    engine::UIFloatInputState yInput;
    engine::UIFloatInputState zInput;
    engine::UIFloatInputState volumeInput;
};

} // namespace game
