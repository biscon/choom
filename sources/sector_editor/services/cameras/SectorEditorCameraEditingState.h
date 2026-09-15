#pragma once

#include "engine/ui/UI.h"
#include "sector_demo/SectorViewPose.h"
#include "sector_editor/SectorEditorSelectionTypes.h"

#include <string>

namespace game {

struct CameraEditingState {
    CameraDragState drag;
    bool pilotActive = false;
    int pilotCameraId = -1;
    SectorViewPose originalPreviewPose;
    bool originalMouseLook = false;
    float originalFov = 75.0f;
    float pilotFov = 75.0f;
    engine::UIFloatInputState pilotFovInput;
};

struct CameraEditingUiState {
    char referenceIdBuffer[64] = {};
    int bufferedCameraId = -1;
    std::string referenceIdError;
    engine::UIFloatInputState xInput;
    engine::UIFloatInputState yInput;
    engine::UIFloatInputState zInput;
    engine::UIFloatInputState orientationInput;
    engine::UIFloatInputState pitchInput, rollInput, fovInput;
};

} // namespace game
