#pragma once

#include <string>

namespace game {

struct InspectorIdUiState {
    char selectedSectorIdBuffer[64] = {};
    int idBufferSectorIndex = -1;
    char selectedLightIdBuffer[64] = {};
    int idBufferLightIndex = -1;
    std::string idEditError;
};

} // namespace game
