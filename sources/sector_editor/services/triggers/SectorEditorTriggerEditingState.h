#pragma once

#include "engine/ui/UI.h"
#include "sector_demo/SectorTopologyMap.h"

#include <string>
#include <vector>

namespace game {

enum class TriggerDrawMode { Rectangle, Polygon };

struct PendingTriggerDrawState {
    bool active = false;
    std::vector<SectorTriggerPoint> points;
    SectorTriggerPoint hoverPoint{};
    bool hasHoverPoint = false;
    std::string error;
};

struct TriggerDragState {
    bool active = false;
    int triggerId = -1;
    std::vector<SectorTriggerPoint> originalPoints;
    std::vector<SectorTriggerPoint> previewPoints;
    SectorTriggerPoint pressPoint{};
};

struct TriggerEditingState {
    TriggerDrawMode drawMode = TriggerDrawMode::Rectangle;
    PendingTriggerDrawState pending;
    TriggerDragState drag;
};

struct TriggerEditingUiState {
    char idBuffer[64] = {};
    char scriptBuffer[128] = {};
    int bufferedTriggerId = -1;
    std::string idError;
    std::string scriptError;
    engine::UIIntInputState delayInput;
};

} // namespace game
