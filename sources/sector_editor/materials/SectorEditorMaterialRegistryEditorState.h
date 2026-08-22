#pragma once

#include "engine/assets/AssetHandles.h"
#include "engine/ui/UI.h"
#include "sector_demo/SectorTextureTypes.h"

#include <string>
#include <vector>

namespace game {

struct SectorEditorMaterialRegistryDraft {
    SectorMaterialDefinition definition;
    std::string originalId;
    bool idWasEdited = false;
};

struct SectorEditorMaterialRegistryEditorState {
    bool open = false;
    std::vector<SectorEditorMaterialRegistryDraft> drafts;
    std::vector<std::string> listLabelStorage;
    std::vector<const char*> listLabels;
    int selectedIndex = -1;
    engine::UIScrollState listScroll;
    engine::UIScrollState formScroll;
    char idBuffer[96] = {};
    char pathBuffer[512] = {};
    engine::UIFloatInputState metallicInput;
    engine::UIFloatInputState roughnessInput;
    engine::UIFloatInputState normalStrengthInput;
    bool deleteConfirmationOpen = false;
    std::string deleteConfirmationId;
    std::string validationMessage;
    engine::AssetScopeHandle previewScope = engine::NullAssetScopeHandle();
    engine::TextureHandle previewTexture = engine::NullTextureHandle();
    std::string previewPath;
    SectorMaterialFilter previewFilter = SectorMaterialFilter::Anisotropic8x;
};

} // namespace game
