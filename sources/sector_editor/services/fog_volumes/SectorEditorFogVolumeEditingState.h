#pragma once

#include "engine/ui/UI.h"

#include <array>
#include <string>

namespace game {

struct FogVolumeEditingUiState {
    int instanceIdVolumeId = -1;
    char instanceIdBuffer[64]{};
    std::string instanceIdError;
    std::array<engine::UIFloatInputState, 17> floatInputs{};
    std::array<engine::UIIntInputState, 3> colorInputs{};
};

} // namespace game
