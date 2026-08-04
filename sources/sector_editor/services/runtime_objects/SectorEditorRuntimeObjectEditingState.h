#pragma once

#include "engine/ui/UI.h"
#include "sector_editor/SectorEditorModalTypes.h"
#include "sector_editor/SectorEditorSelectionTypes.h"

#include <string>
#include <vector>

namespace game {

struct StaticModelPickerState {
    bool open = false;
    bool scanned = false;
    std::string scanMessage;
    engine::UIScrollState scroll;
    std::vector<std::string> modelPaths;
    std::vector<const char*> optionLabels;
    int selectedModelIndex = -1;
    std::string requestedModelPath;
};

struct RuntimeObjectEditingState {
    RuntimeObjectDragState drag;
    SectorSpriteMetadataCatalog spriteMetadataCatalog;
    int billboardMetadataObjectId = -1;
    std::string billboardMetadataSpriteAnimationPath;
    bool billboardMetadataInitialRepairAttempted = false;
    SpritePickerState spritePicker;
    StaticModelPickerState staticModelPicker;
};

struct RuntimeObjectEditingUiState {
    engine::UIFloatInputState xInput;
    engine::UIFloatInputState yInput;
    engine::UIFloatInputState zInput;
    engine::UIFloatInputState yawInput;
    engine::UIFloatInputState heightOffsetInput;
    engine::UIFloatInputState scaleInput;
    engine::UIFloatInputState widthInput;
    engine::UIFloatInputState heightInput;
    engine::UIFloatInputState thicknessInput;
    engine::UIFloatInputState normalOffsetInput;
    engine::UIFloatInputState openDistanceInput;
    engine::UIFloatInputState speedInput;
    engine::UIFloatInputState initialOpenFractionInput;
    engine::UIFloatInputState autoOpenDistanceInput;
    engine::UIFloatInputState interactionDistanceInput;
    engine::UIFloatInputState originXInput;
    engine::UIFloatInputState originYInput;
};

} // namespace game
