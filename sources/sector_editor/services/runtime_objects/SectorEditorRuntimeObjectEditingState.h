#pragma once

#include "engine/ui/UI.h"
#include "sector_editor/SectorEditorModalTypes.h"
#include "sector_editor/SectorEditorSelectionTypes.h"
#include "sector_editor/npcs/SectorEditorNpcPlacementState.h"

#include <string>
#include <cstdint>
#include <vector>

namespace game {

enum class PreviewObjectNudgePreset {
    Fine,
    Normal,
    Coarse
};

struct PreviewObjectAdjustmentState {
    bool active = false;
    bool changed = false;
    bool bakedStatusRefreshed = false;
    int objectId = -1;
    std::string objectKind;
    Vector3 originalPosition = {};
    float originalYawRadians = 0.0f;
    float originalHeightOffsetWorld = 0.0f;
    PreviewObjectNudgePreset preset = PreviewObjectNudgePreset::Normal;
};

enum class ModelPickerTarget {
    StaticModel,
    DynamicModel,
    NpcDefinition,
    ItemDefinition,
    WeaponArms,
    WeaponAttachment
};

struct StaticModelPickerState {
    bool open = false;
    bool scanned = false;
    std::string scanMessage;
    engine::UIScrollState scroll;
    std::vector<std::string> modelPaths;
    std::vector<std::string> optionLabelStorage;
    std::vector<const char*> optionLabels;
    int selectedModelIndex = -1;
    std::string requestedModelPath;
    ModelPickerTarget target = ModelPickerTarget::StaticModel;
};

struct RuntimeObjectEditingState {
    struct ItemPlacementState {
        std::string lastDefinitionId;
        std::uint64_t registryRevision = 0;
        std::vector<std::string> definitionIds;
        std::vector<std::string> labelStorage;
        std::vector<const char*> labels;
    } itemPlacement;
    PreviewObjectAdjustmentState previewAdjustment;
    RuntimeObjectDragState drag;
    SectorEditorNpcPlacementState npcPlacement;
    SectorSpriteMetadataCatalog spriteMetadataCatalog;
    int billboardMetadataObjectId = -1;
    std::string billboardMetadataSpriteAnimationPath;
    bool billboardMetadataInitialRepairAttempted = false;
    SpritePickerState spritePicker;
    StaticModelPickerState staticModelPicker;
    uint64_t swingDoorStyleCatalogRevision = 0;
    std::vector<std::string> swingDoorStyleIds;
    std::vector<std::string> swingDoorStyleLabels;
};

struct RuntimeObjectEditingUiState {
    char staticModelInstanceIdBuffer[64] = {};
    int staticModelInstanceIdObjectId = -1;
    std::string staticModelInstanceIdError;
    char dynamicModelInstanceIdBuffer[64] = {};
    int dynamicModelInstanceIdObjectId = -1;
    std::string dynamicModelInstanceIdError;
    char dynamicModelUseTitleBuffer[128] = {};
    char dynamicModelOnUseScriptBuffer[128] = {};
    std::string dynamicModelUseError;
    int dynamicModelUseObjectId = -1;
    char itemInstanceIdBuffer[64] = {};
    char itemOnTakeScriptBuffer[128] = {};
    char itemOnUseScriptBuffer[128] = {};
    int itemBufferedObjectId = -1;
    std::string itemError;
    engine::UIIntInputState itemQuantityInput;
    engine::UIFloatInputState itemTakeDistanceInput;
    char doorInstanceIdBuffer[64] = {};
    char doorUseTitleBuffer[128] = {};
    char doorCanOpenScriptBuffer[128] = {};
    char doorCanCloseScriptBuffer[128] = {};
    int doorScriptFieldsObjectId = -1;
    std::string doorInstanceIdError;
    engine::UIFloatInputState xInput;
    engine::UIFloatInputState yInput;
    engine::UIFloatInputState zInput;
    engine::UIFloatInputState rotationXInput;
    engine::UIFloatInputState yawInput;
    engine::UIFloatInputState rotationZInput;
    engine::UIFloatInputState heightOffsetInput;
    engine::UIFloatInputState scaleInput;
    engine::UIFloatInputState animationSpeedInput;
    engine::UIFloatInputState widthInput;
    engine::UIFloatInputState heightInput;
    engine::UIFloatInputState thicknessInput;
    engine::UIFloatInputState normalOffsetInput;
    engine::UIFloatInputState horizontalOffsetInput;
    engine::UIFloatInputState verticalOffsetInput;
    engine::UIFloatInputState opacityInput;
    engine::UIFloatInputState roughnessInput;
    engine::UIFloatInputState indexOfRefractionInput;
    engine::UIIntInputState windowTintRedInput;
    engine::UIIntInputState windowTintGreenInput;
    engine::UIIntInputState windowTintBlueInput;
    engine::UIFloatInputState openDistanceInput;
    engine::UIFloatInputState speedInput;
    engine::UIFloatInputState modelScaleInput;
    engine::UIFloatInputState openAngleDegreesInput;
    engine::UIFloatInputState angularSpeedDegreesInput;
    engine::UIFloatInputState initialOpenFractionInput;
    engine::UIFloatInputState autoOpenDistanceInput;
    engine::UIFloatInputState interactionDistanceInput;
    engine::UIFloatInputState dynamicModelUseDistanceInput;
    engine::UIFloatInputState originXInput;
    engine::UIFloatInputState originYInput;
};

} // namespace game
