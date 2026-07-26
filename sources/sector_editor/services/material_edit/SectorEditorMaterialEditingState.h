#pragma once

#include "engine/ui/UI.h"
#include "sector_editor/SectorEditorTypes.h"

namespace game {

struct MaterialEditingState {
    TopologyMaterialPayload copiedMaterial;
};

struct MaterialEditingUiState {
    engine::UIFloatInputState surface3DUvScaleUInput;
    engine::UIFloatInputState surface3DUvScaleVInput;
    engine::UIFloatInputState surface3DUvOffsetUInput;
    engine::UIFloatInputState surface3DUvOffsetVInput;
    engine::UIFloatInputState surface3DDecalOpacityInput;
    engine::UIFloatInputState surface3DDecalBloomIntensityInput;
    engine::UIFloatInputState topologySectorUvInputs[20];
    engine::UIFloatInputState topologySectorDecalOpacityInputs[2];
    engine::UIFloatInputState topologySectorDecalBloomIntensityInputs[2];
    engine::UIFloatInputState topologySideDefUvInputs[4];
    engine::UIFloatInputState topologySideDefDecalOpacityInput;
    engine::UIFloatInputState topologySideDefDecalBloomIntensityInput;
};

} // namespace game
