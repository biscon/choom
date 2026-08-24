#pragma once

#include "engine/ui/UI.h"
#include "sector_editor/services/sounds/SectorEditorAudioAssetPicker.h"
#include "sector_demo/SectorTopologyMap.h"

#include <string>
#include <vector>

namespace game {

struct SectorEditorSoundDraft {
    SectorSoundDefinition definition;
    std::string originalId;
};

struct SectorEditorSoundEditorState {
    bool open = false;
    std::vector<SectorEditorSoundDraft> drafts;
    std::vector<std::string> listLabelStorage;
    std::vector<const char*> listLabels;
    int selectedIndex = -1;
    engine::UIScrollState listScroll;
    engine::UIScrollState formScroll;
    char idBuffer[96] = {};
    std::string validationMessage;
    std::string usageText;
    bool deleteConfirmationOpen = false;
    std::string deleteConfirmationId;
    SectorEditorAudioAssetPickerState assetPicker;
};

} // namespace game
