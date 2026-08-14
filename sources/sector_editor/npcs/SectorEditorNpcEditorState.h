#pragma once

#include "engine/assets/AssetHandles.h"
#include "engine/ui/UI.h"
#include "game/npc/NpcDefinitions.h"

#include <array>
#include <string>
#include <vector>

namespace game {

struct SectorEditorNpcEditorSessionState {
    std::string selectedNpcId;
    engine::UIScrollState listScroll;
    engine::UIScrollState formScroll;
};

struct SectorEditorNpcDefinitionDraft {
    NpcDefinition definition;
    NpcDefinition originalDefinition;
    std::string originalId;
    bool isNew = false;
};

struct SectorEditorNpcEditorState {
    bool open = false;
    std::vector<SectorEditorNpcDefinitionDraft> drafts;
    std::vector<std::string> stagedDeleteIds;
    std::vector<NpcDefinitionCatalogError> catalogErrors;
    std::vector<std::string> listLabelStorage;
    std::vector<const char*> listLabels;
    int selectedIndex = -1;

    char idBuffer[64] = {};
    char nameBuffer[256] = {};
    engine::UIFloatInputState animationBlendSecondsInput;
    std::array<engine::UIFloatInputState, kNpcActionCount>
            animationSpeedInputs;
    std::array<engine::UIFloatInputState, kNpcActionCount>
            movementSpeedInputs;

    bool deleteConfirmationOpen = false;
    std::string deleteConfirmationId;
    std::string validationMessage;
    std::string warningMessage;

    engine::AssetScopeHandle modelScope = engine::NullAssetScopeHandle();
    engine::ModelHandle selectedModel = engine::NullModelHandle();
    std::string selectedModelPath;
    std::vector<std::string> animationOptionStorage;
    std::vector<const char*> animationOptions;
};

} // namespace game
