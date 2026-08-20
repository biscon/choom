#pragma once

#include "engine/ui/UI.h"

#include <array>

namespace game {

struct FogVolumeEditingUiState {
    std::array<engine::UIFloatInputState, 17> floatInputs{};
    std::array<engine::UIIntInputState, 3> colorInputs{};
};

} // namespace game
