#pragma once

#include "engine/ui/UI.h"
#include "game/FpsViewmodel.h"
#include "game/FpsWeaponRegistry.h"
#include "sector_editor/services/sounds/SectorEditorAudioAssetPicker.h"

#include <array>
#include <string>
#include <vector>

namespace game {

struct SectorEditorWeaponEditorSessionState {
    std::string selectedWeaponId;
    engine::UIScrollState listScroll;
    engine::UIScrollState formScroll;
};

struct SectorEditorWeaponEditorState {
    bool open = false;
    bool openedFromPreview3D = false;
    FpsWeaponRegistry draftRegistry;
    FpsApplicationSettings previewApplicationSettings;
    std::string originalActiveWeaponId;
    FpsViewmodelEquipState originalEquipState =
            FpsViewmodelEquipState::Holstered;
    float originalEquipProgress = 0.0f;
    int selectedIndex = -1;
    std::vector<std::string> listLabelStorage;
    std::vector<const char*> listLabels;

    char idBuffer[96] = {};
    char armsModelPathBuffer[512] = {};
    char idleAnimationBuffer[128] = {};
    char attachmentModelPathBuffer[512] = {};
    char attachmentBoneBuffer[64] = {};
    char shootSoundBuffer[512] = {};

    std::array<engine::UIFloatInputState, 128> floatInputs;
    std::array<engine::UIIntInputState, 64> intInputs;
    engine::UIScrollState animationScroll;
    engine::ModelHandle animationOptionsModel = engine::NullModelHandle();
    std::vector<std::string> animationOptionStorage;
    std::vector<const char*> animationOptions;

    bool deleteConfirmationOpen = false;
    std::string deleteConfirmationId;
    bool previewReloadRequested = false;
    std::string validationMessage;
    std::string warningMessage;
    SectorEditorAudioAssetPickerState audioPicker;
};

} // namespace game
