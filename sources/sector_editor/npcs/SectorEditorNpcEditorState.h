#pragma once

#include "engine/assets/AssetHandles.h"
#include "engine/ui/UI.h"
#include "game/npc/NpcDefinitions.h"
#include "sector_editor/services/sounds/SectorEditorAudioAssetPicker.h"

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

enum class SectorEditorNpcAudioPickerTarget {
    None,
    PlayerDetected,
    Action,
    Attack,
    AmbientAdd,
    AmbientReplace
};

struct SectorEditorNpcAudioPickerState {
    SectorEditorAudioAssetPickerState assetPicker;
    SectorEditorNpcAudioPickerTarget target =
            SectorEditorNpcAudioPickerTarget::None;
    NpcAction action = NpcAction::Hurt;
    size_t ambientIndex = 0;
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
    engine::UIIntInputState baseHealthInput;
    engine::UIIntInputState corpseDespawnDelayMillisecondsInput;
    engine::UIIntInputState corpseFadeDurationMillisecondsInput;
    engine::UIFloatInputState animationBlendSecondsInput;
    engine::UIFloatInputState ambientMinimumDelaySecondsInput;
    engine::UIFloatInputState ambientMaximumDelaySecondsInput;
    engine::UIFloatInputState visionRangeWorldInput;
    engine::UIFloatInputState visionAngleDegreesInput;
    engine::UIFloatInputState hearingRangeWorldInput;
    engine::UIIntInputState investigationDurationMillisecondsInput;
    engine::UIFloatInputState attackHitPhaseInput;
    engine::UIFloatInputState attackRangeWorldInput;
    engine::UIFloatInputState attackAdvanceSpeedMultiplierInput;
    engine::UIFloatInputState attackAimTrackingEndPhaseInput;
    engine::UIFloatInputState attackHitArcDegreesInput;
    engine::UIIntInputState attackDamageInput;
    engine::UIFloatInputState attackKnockbackInput;
    engine::UIIntInputState attackStunMillisecondsInput;
    engine::UIFloatInputState attackCameraPitchKickInput;
    engine::UIFloatInputState attackCameraRollKickInput;
    engine::UIFloatInputState attackCameraSpringFrequencyInput;
    engine::UIFloatInputState attackCameraSpringDampingInput;
    engine::UIFloatInputState attackCameraMaxPitchInput;
    engine::UIFloatInputState attackCameraMaxRollInput;
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
    SectorEditorNpcAudioPickerState audioPicker;
};

} // namespace game
