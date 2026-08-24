#pragma once

#include "engine/ui/UI.h"

#include <array>
#include <string>

namespace game {

struct InspectorIdUiState {
    char selectedSectorIdBuffer[64] = {};
    int idBufferSectorIndex = -1;
    char selectedLightIdBuffer[64] = {};
    int idBufferLightIndex = -1;
    std::string idEditError;
};

struct ReflectionProbeEditingUiState {
    std::array<engine::UIFloatInputState, 12> floatInputs{};
    engine::UIIntInputState priorityInput;
};

} // namespace game
