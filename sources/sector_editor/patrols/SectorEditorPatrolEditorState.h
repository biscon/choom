#pragma once

#include "engine/ui/UI.h"
#include "sector_demo/SectorAuthoringGraph.h"

#include <string>
#include <vector>

namespace game {

struct SectorEditorPatrolDraft {
    SectorAuthoringPatrol patrol;
};

struct SectorEditorPatrolEditorState {
    bool open = false;
    std::vector<SectorEditorPatrolDraft> drafts;
    std::vector<std::string> listLabelStorage;
    std::vector<const char*> listLabels;
    int selectedIndex = -1;
    engine::UIScrollState listScroll;
    engine::UIScrollState formScroll;
    char idBuffer[64] = {};
    std::vector<int> markerIds;
    std::vector<std::string> markerLabelStorage;
    std::vector<const char*> markerLabels;
    std::vector<engine::UIFloatInputState> delayInputs;
    std::vector<engine::UIFloatInputState> arcInputs;
    std::string validationMessage;
    bool deleteConfirmationOpen = false;
    int deleteConfirmationEditorId = -1;
};

} // namespace game
