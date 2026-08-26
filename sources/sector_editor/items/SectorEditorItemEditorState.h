#pragma once

#include "engine/ui/UI.h"
#include "game/items/ItemDefinitions.h"

#include <array>
#include <string>
#include <vector>

namespace game {

struct SectorEditorItemEditorSessionState {
    std::string selectedItemId;
    engine::UIScrollState listScroll;
    engine::UIScrollState formScroll;
};

struct SectorEditorItemEditorState {
    bool open = false;
    ItemRegistry draftRegistry;
    int selectedIndex = -1;
    std::vector<std::string> listLabelStorage;
    std::vector<const char*> listLabels;
    std::vector<std::string> weaponLabelStorage;
    std::vector<const char*> weaponLabels;
    std::array<char, 385> titleBuffer{};
    std::array<char, 8193> descriptionBuffer{};
    std::array<char, 1024> modelPathBuffer{};
    engine::UIFloatInputState weightInput;
    engine::UIIntInputState maxStackSizeInput;
    engine::UIIntInputState healAmountInput;
    engine::UIFloatInputState healDurationInput;
    bool deleteConfirmationOpen = false;
    std::string deleteConfirmationId;
    std::string validationMessage;
};

} // namespace game
