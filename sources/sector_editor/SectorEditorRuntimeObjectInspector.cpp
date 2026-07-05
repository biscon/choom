#include "sector_editor/SectorEditorRuntimeObjectInspector.h"

#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/SectorEditorTextureModals.h"
#include "sector_editor/SectorEditorUiHelpers.h"
#include "sector_demo/SectorBillboardRuntime.h"
#include "sector_demo/SectorDoorRuntime.h"
#include "sector_demo/SectorRuntimeObjects.h"
#include "sector_demo/SectorTopologyMap.h"

#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

namespace game {
namespace {

constexpr float BillboardSizeMin = 0.001f;
constexpr float BillboardSizeMax = 100000.0f;

std::string RuntimeObjectSpriteLabel(const SectorPlacedRuntimeObject& object)
{
    return object.billboard.spriteAnimationPath.empty()
            ? std::string("Sprite: None selected")
            : std::string(TextFormat("Sprite: %s", object.billboard.spriteAnimationPath.c_str()));
}

bool ResolveBillboardAspectFromAnimation(
        const engine::AssetManager& assets,
        const engine::SpriteAnimationHandle animation,
        uint32_t clipIndex,
        float& outAspect)
{
    const engine::SpriteAnimationAsset* asset = assets.GetSpriteAnimation(animation);
    if (asset == nullptr || asset->frames.empty()) {
        return false;
    }

    uint32_t frameIndex = 0;
    if (clipIndex != engine::InvalidSpriteClipIndex && clipIndex < asset->clips.size()) {
        const engine::SpriteClip& clip = asset->clips[clipIndex];
        if (clip.frameCount > 0 && clip.firstFrame < asset->frames.size()) {
            frameIndex = clip.firstFrame;
        }
    }

    const engine::SpriteFrame& frame = asset->frames[frameIndex];
    Vector2 frameSize = frame.sourceSize;
    if (frameSize.x <= 0.0f || frameSize.y <= 0.0f) {
        frameSize = Vector2{std::abs(frame.source.width), std::abs(frame.source.height)};
    }
    if (frameSize.x <= 0.0f || frameSize.y <= 0.0f) {
        return false;
    }

    outAspect = frameSize.x / frameSize.y;
    return std::isfinite(outAspect) && outAspect > 0.0f;
}

float ResolveBillboardAspect(
        const SectorEditorState& state,
        engine::EngineContext* engineContext,
        const engine::AssetManager& assets,
        const SectorPlacedRuntimeObject& object)
{
    if (engineContext == nullptr || object.kind != "billboard") {
        return 0.0f;
    }

    engine::SpriteAnimationHandle animation = engine::NullSpriteAnimationHandle();
    uint32_t clipIndex = engine::InvalidSpriteClipIndex;
    for (const SectorPlacedRuntimeObjectEntity& entry : state.runtimeObjects.placedObjectEntities) {
        if (entry.placedObjectId != object.id
                || !engineContext->world.IsAlive(entry.entity)
                || !engineContext->world.Has<SectorBillboardSprite>(entry.entity)) {
            continue;
        }

        const SectorBillboardSprite& sprite = engineContext->world.Get<SectorBillboardSprite>(entry.entity);
        animation = sprite.animation;
        clipIndex = sprite.clipIndex;
        break;
    }

    if (engine::IsNull(animation)) {
        return 0.0f;
    }

    if (object.billboard.directional) {
        const uint32_t frontClipIndex = assets.FindSpriteClipIndex(
                animation,
                object.billboard.frontClip.c_str());
        if (frontClipIndex != engine::InvalidSpriteClipIndex) {
            clipIndex = frontClipIndex;
        }
    } else if (!object.billboard.clip.empty()) {
        const uint32_t selectedClipIndex = assets.FindSpriteClipIndex(
                animation,
                object.billboard.clip.c_str());
        if (selectedClipIndex != engine::InvalidSpriteClipIndex) {
            clipIndex = selectedClipIndex;
        }
    }

    float aspect = 0.0f;
    return ResolveBillboardAspectFromAnimation(assets, animation, clipIndex, aspect)
            ? aspect
            : 0.0f;
}

int DoorMotionOptionIndex(SectorDoorMotionType motion)
{
    switch (motion) {
        case SectorDoorMotionType::SlideVertical:
            return 0;
        case SectorDoorMotionType::SlideLeft:
            return 1;
        case SectorDoorMotionType::SlideRight:
            return 2;
    }
    return 0;
}

SectorDoorMotionType DoorMotionFromOptionIndex(int index)
{
    switch (index) {
        case 1:
            return SectorDoorMotionType::SlideLeft;
        case 2:
            return SectorDoorMotionType::SlideRight;
        case 0:
        default:
            return SectorDoorMotionType::SlideVertical;
    }
}

} // namespace

float MeasureSectorEditorRuntimeObjectInspectorContentHeight(
        const SectorEditorRuntimeObjectInspectorMeasureContext& context)
{
    const SectorPlacedRuntimeObject* object = context.callbacks.selectedRuntimeObject();
    if (object == nullptr) {
        return 42.0f;
    }

    const bool isBillboard = object->kind == "billboard";
    const bool isDoor = object->kind == "door";
    if (isDoor) {
        const SectorResolvedDoorAnchor resolved = ResolveSectorDoorAnchor(context.state.topologyMap, object->door);
        const std::string anchorStatus = resolved.valid
                ? TextFormat(
                        "Anchor valid: line %d, sectors %d -> %d",
                        object->door.anchor.lineDefId,
                        object->door.anchor.frontSectorId,
                        object->door.anchor.backSectorId)
                : TextFormat(
                        "Anchor invalid: %s",
                        resolved.diagnostic.empty()
                                ? "unable to resolve portal"
                                : resolved.diagnostic.c_str());
        const float anchorStatusHeight = MeasureSectorEditorWrappedTextHeight(
                context.smallConfig,
                context.assets,
                context.smallFont,
                anchorStatus.c_str(),
                context.contentW,
                2);
        const bool textureMissing = !object->door.textureId.empty()
                && FindSectorTopologyTexture(context.state.topologyMap, object->door.textureId) == nullptr;
        const std::string textureStatus = object->door.textureId.empty()
                ? "Texture: default material"
                : textureMissing
                        ? TextFormat("Texture missing: %s", object->door.textureId.c_str())
                        : TextFormat("Texture: %s", object->door.textureId.c_str());
        const float textureStatusHeight = MeasureSectorEditorWrappedTextHeight(
                context.smallConfig,
                context.assets,
                context.smallFont,
                textureStatus.c_str(),
                context.contentW,
                1);
        return SectorEditorDoorInspectorContentHeight(
                context.rowH,
                context.gap,
                anchorStatusHeight,
                textureStatusHeight);
    }

    const std::string spriteLabel = RuntimeObjectSpriteLabel(*object);
    const float spriteLabelHeight = MeasureSectorEditorWrappedTextHeight(
            context.smallConfig,
            context.assets,
            context.smallFont,
            spriteLabel.c_str(),
            context.contentW,
            1);
    const bool hasBillboardAspect = isBillboard
            && ResolveBillboardAspect(context.state, context.engineContext, context.assets, *object) > 0.0f;
    const bool keepAspectWarningVisible =
            isBillboard && object->billboard.keepAspectRatio && !hasBillboardAspect;
    return SectorEditorRuntimeObjectInspectorContentHeight(
            context.rowH,
            context.gap,
            isBillboard,
            keepAspectWarningVisible,
            isBillboard && object->billboard.directional,
            spriteLabelHeight,
            28.0f);
}

void DrawSectorEditorRuntimeObjectInspector(
        SectorEditorRuntimeObjectInspectorContext& context)
{
    engine::UIContext& ui = context.ui;
    const engine::UIConfig& config = context.config;
    engine::Input& input = context.input;
    engine::AssetManager& assets = context.assets;
    const engine::FontHandle font = context.font;
    const engine::FontHandle smallFont = context.smallFont;
    SectorEditorState& state = context.state;
    SectorEditorUiState& uiState = context.uiState;
    const SectorEditorRuntimeObjectInspectorCallbacks& callbacks = context.callbacks;
    const float contentW = context.contentW;
    const float rowH = context.rowH;
    const float gap = context.gap;
    const engine::UIConfig smallConfig = SectorEditorSmallFontConfig(config, assets, smallFont);

    float y = 0.0f;
    const SectorPlacedRuntimeObject* selectedObject = callbacks.selectedRuntimeObject();
    if (selectedObject == nullptr) {
        return;
    }

    engine::Text(
            ui,
            config,
            assets,
            Rectangle{0.0f, y, contentW, 34.0f},
            font,
            TextFormat("Object ID: %d", selectedObject->id),
            engine::UITextJustify::Left,
            config.textColor);
    y += 38.0f;

    const bool isBillboard = selectedObject->kind == "billboard";
    const bool isDoor = selectedObject->kind == "door";
    engine::Text(
            ui,
            config,
            assets,
            Rectangle{0.0f, y, contentW, 30.0f},
            font,
            isBillboard ? "Type: Billboard" : isDoor ? "Type: Door" : "Type: Unsupported object",
            engine::UITextJustify::Left,
            isBillboard || isDoor ? config.mutedTextColor : config.invalidColor);
    y += 34.0f;

    if (isDoor) {
        const SectorResolvedDoorAnchor resolved = ResolveSectorDoorAnchor(state.topologyMap, selectedObject->door);
        const std::string anchorStatus = resolved.valid
                ? TextFormat(
                        "Anchor valid: line %d, sectors %d -> %d",
                        selectedObject->door.anchor.lineDefId,
                        selectedObject->door.anchor.frontSectorId,
                        selectedObject->door.anchor.backSectorId)
                : TextFormat(
                        "Anchor invalid: %s",
                        resolved.diagnostic.empty()
                                ? "unable to resolve portal"
                                : resolved.diagnostic.c_str());
        const float anchorStatusHeight = MeasureSectorEditorWrappedTextHeight(
                smallConfig,
                assets,
                smallFont,
                anchorStatus.c_str(),
                contentW,
                2);
        engine::Text(
                ui,
                smallConfig,
                assets,
                Rectangle{0.0f, y, contentW, anchorStatusHeight},
                smallFont,
                anchorStatus.c_str(),
                engine::UITextJustify::Left,
                resolved.valid ? config.mutedTextColor : config.invalidColor,
                true);
        y += anchorStatusHeight + gap;

        auto drawDoorFloat =
                [&](const char* id,
                    const char* label,
                    float value,
                    engine::UIFloatInputState& inputState,
                    float minValue,
                    float maxValue,
                    int decimals,
                    const std::function<bool(SectorPlacedDoor&, float)>& applyValue) {
                    const float labelW = 104.0f;
                    const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
                            ui,
                            config,
                            input,
                            assets,
                            font,
                            id,
                            label,
                            Rectangle{0.0f, y, labelW, rowH},
                            Rectangle{labelW + gap, y, std::max(0.0f, contentW - labelW - gap), rowH},
                            engine::UITextJustify::Left,
                            value,
                            inputState,
                            minValue,
                            maxValue,
                            decimals);
                    if (result.changed && result.finite && result.value != value) {
                        callbacks.mutateSelectedRuntimeObject(
                                "Updated door properties",
                                [applyValue, value = result.value](SectorPlacedRuntimeObject& object) {
                                    if (object.kind != "door") {
                                        return false;
                                    }
                                    return applyValue(object.door, value);
                                });
                    }
                    y += rowH + gap;
                };

        drawDoorFloat(
                "sector_editor_door_width",
                "Width",
                selectedObject->door.width,
                uiState.runtimeObjectWidthInput,
                0.0f,
                100000.0f,
                3,
                [](SectorPlacedDoor& door, float value) {
                    const float width = std::max(0.0f, value);
                    if (door.width == width) {
                        return false;
                    }
                    door.width = width;
                    return true;
                });
        selectedObject = callbacks.selectedRuntimeObject();
        if (selectedObject == nullptr) {
            return;
        }

        drawDoorFloat(
                "sector_editor_door_height",
                "Height",
                selectedObject->door.height,
                uiState.runtimeObjectHeightInput,
                0.0f,
                100000.0f,
                3,
                [](SectorPlacedDoor& door, float value) {
                    const float height = std::max(0.0f, value);
                    if (door.height == height) {
                        return false;
                    }
                    door.height = height;
                    return true;
                });
        selectedObject = callbacks.selectedRuntimeObject();
        if (selectedObject == nullptr) {
            return;
        }

        drawDoorFloat(
                "sector_editor_door_thickness",
                "Thickness",
                selectedObject->door.thickness,
                uiState.runtimeObjectThicknessInput,
                0.001f,
                100000.0f,
                3,
                [](SectorPlacedDoor& door, float value) {
                    const float thickness = std::max(0.001f, value);
                    if (door.thickness == thickness) {
                        return false;
                    }
                    door.thickness = thickness;
                    return true;
                });
        selectedObject = callbacks.selectedRuntimeObject();
        if (selectedObject == nullptr) {
            return;
        }

        drawDoorFloat(
                "sector_editor_door_normal_offset",
                "Offset",
                selectedObject->door.normalOffset,
                uiState.runtimeObjectNormalOffsetInput,
                -100000.0f,
                100000.0f,
                3,
                [](SectorPlacedDoor& door, float value) {
                    if (door.normalOffset == value) {
                        return false;
                    }
                    door.normalOffset = value;
                    return true;
                });
        selectedObject = callbacks.selectedRuntimeObject();
        if (selectedObject == nullptr) {
            return;
        }

        const char* motionOptions[] = {
                "Slide Vertical",
                "Slide Left",
                "Slide Right"};
        int motionIndex = DoorMotionOptionIndex(selectedObject->door.motion);
        const int previousMotionIndex = motionIndex;
        const SectorEditorInspectorStackedOptionRowLayout motionLayout =
                BuildSectorEditorInspectorStackedOptionRowLayout(y, contentW, rowH, gap);
        engine::Text(
                ui,
                config,
                assets,
                motionLayout.labelRect,
                font,
                "Motion",
                engine::UITextJustify::Left,
                config.mutedTextColor);
        if (engine::Option(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_door_motion",
                    motionLayout.fieldRect,
                    font,
                    motionOptions,
                    3,
                    motionIndex)
                && motionIndex != previousMotionIndex) {
            const SectorDoorMotionType motion = DoorMotionFromOptionIndex(motionIndex);
            callbacks.mutateSelectedRuntimeObject(
                    "Updated door motion",
                    [motion](SectorPlacedRuntimeObject& object) {
                        if (object.kind != "door" || object.door.motion == motion) {
                            return false;
                        }
                        object.door.motion = motion;
                        return true;
                    });
        }
        y += motionLayout.height + gap;
        selectedObject = callbacks.selectedRuntimeObject();
        if (selectedObject == nullptr) {
            return;
        }

        drawDoorFloat(
                "sector_editor_door_open_distance",
                "Open Dist",
                selectedObject->door.openDistance,
                uiState.runtimeObjectOpenDistanceInput,
                0.0f,
                100000.0f,
                3,
                [](SectorPlacedDoor& door, float value) {
                    const float distance = std::max(0.0f, value);
                    if (door.openDistance == distance) {
                        return false;
                    }
                    door.openDistance = distance;
                    return true;
                });
        selectedObject = callbacks.selectedRuntimeObject();
        if (selectedObject == nullptr) {
            return;
        }

        drawDoorFloat(
                "sector_editor_door_speed",
                "Speed",
                selectedObject->door.speed,
                uiState.runtimeObjectSpeedInput,
                0.0f,
                100000.0f,
                3,
                [](SectorPlacedDoor& door, float value) {
                    const float speed = std::max(0.0f, value);
                    if (door.speed == speed) {
                        return false;
                    }
                    door.speed = speed;
                    return true;
                });
        selectedObject = callbacks.selectedRuntimeObject();
        if (selectedObject == nullptr) {
            return;
        }

        drawDoorFloat(
                "sector_editor_door_initial_open_fraction",
                "Initial",
                selectedObject->door.initialOpenFraction,
                uiState.runtimeObjectInitialOpenFractionInput,
                0.0f,
                1.0f,
                3,
                [](SectorPlacedDoor& door, float value) {
                    const float fraction = Clamp(value, 0.0f, 1.0f);
                    if (door.initialOpenFraction == fraction) {
                        return false;
                    }
                    door.initialOpenFraction = fraction;
                    return true;
                });
        selectedObject = callbacks.selectedRuntimeObject();
        if (selectedObject == nullptr) {
            return;
        }

        bool autoOpen = selectedObject->door.autoOpen;
        if (engine::Checkbox(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_door_auto_open",
                    Rectangle{0.0f, y, contentW, rowH},
                    font,
                    "Auto Open",
                    autoOpen)) {
            callbacks.mutateSelectedRuntimeObject(
                    "Updated door auto-open",
                    [autoOpen](SectorPlacedRuntimeObject& object) {
                        if (object.kind != "door" || object.door.autoOpen == autoOpen) {
                            return false;
                        }
                        object.door.autoOpen = autoOpen;
                        return true;
                    });
        }
        y += rowH + gap;
        selectedObject = callbacks.selectedRuntimeObject();
        if (selectedObject == nullptr) {
            return;
        }

        drawDoorFloat(
                "sector_editor_door_auto_open_distance",
                "Auto Dist",
                selectedObject->door.autoOpenDistance,
                uiState.runtimeObjectAutoOpenDistanceInput,
                0.001f,
                100000.0f,
                3,
                [](SectorPlacedDoor& door, float value) {
                    const float distance = std::max(0.001f, value);
                    if (door.autoOpenDistance == distance) {
                        return false;
                    }
                    door.autoOpenDistance = distance;
                    return true;
                });
        selectedObject = callbacks.selectedRuntimeObject();
        if (selectedObject == nullptr) {
            return;
        }

        drawDoorFloat(
                "sector_editor_door_interaction_distance",
                "Use Dist",
                selectedObject->door.interactionDistance,
                uiState.runtimeObjectInteractionDistanceInput,
                0.001f,
                100000.0f,
                3,
                [](SectorPlacedDoor& door, float value) {
                    const float distance = std::max(0.001f, value);
                    if (door.interactionDistance == distance) {
                        return false;
                    }
                    door.interactionDistance = distance;
                    return true;
                });
        selectedObject = callbacks.selectedRuntimeObject();
        if (selectedObject == nullptr) {
            return;
        }

        bool runtimeTargetOpen = false;
        const bool runtimeDoorAvailable = callbacks.selectedDoorRuntimeTargetOpen(runtimeTargetOpen);
        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_door_debug_target",
                    Rectangle{0.0f, y, contentW, rowH},
                    font,
                    !runtimeDoorAvailable
                            ? "Runtime Target Unavailable"
                            : (runtimeTargetOpen ? "Debug Target Close" : "Debug Target Open"))) {
            if (runtimeDoorAvailable) {
                callbacks.setSelectedDoorRuntimeTargetOpen(!runtimeTargetOpen);
            }
        }
        y += rowH + gap;

        const bool textureMissing = !selectedObject->door.textureId.empty()
                && FindSectorTopologyTexture(state.topologyMap, selectedObject->door.textureId) == nullptr;
        const std::string textureStatus = selectedObject->door.textureId.empty()
                ? "Texture: default material"
                : textureMissing
                        ? TextFormat("Texture missing: %s", selectedObject->door.textureId.c_str())
                        : TextFormat("Texture: %s", selectedObject->door.textureId.c_str());
        const float textureStatusHeight = MeasureSectorEditorWrappedTextHeight(
                smallConfig,
                assets,
                smallFont,
                textureStatus.c_str(),
                contentW,
                1);
        engine::Text(
                ui,
                smallConfig,
                assets,
                Rectangle{0.0f, y, contentW, textureStatusHeight},
                smallFont,
                textureStatus.c_str(),
                engine::UITextJustify::Left,
                textureMissing ? config.invalidColor : config.mutedTextColor,
                true);
        y += textureStatusHeight + gap;

        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_door_pick_texture",
                    Rectangle{0.0f, y, contentW, rowH},
                    font,
                    "Pick Texture")) {
            callbacks.openDoorTexturePicker();
        }
        y += rowH + gap;

        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_door_uv_settings",
                    Rectangle{0.0f, y, contentW, rowH},
                    font,
                    "UV Settings...")) {
            callbacks.openDoorTextureSettingsModal();
        }
        y += rowH + gap;

        if (engine::Button(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_delete_runtime_object",
                    Rectangle{0.0f, y, contentW, rowH},
                    font,
                    "Delete")) {
            callbacks.deleteSelectedRuntimeObject();
            return;
        }
        return;
    }

    const std::string spriteLabel = RuntimeObjectSpriteLabel(*selectedObject);
    const float spriteLabelHeight = MeasureSectorEditorWrappedTextHeight(
            smallConfig,
            assets,
            smallFont,
            spriteLabel.c_str(),
            contentW,
            1);
    engine::Text(
            ui,
            smallConfig,
            assets,
            Rectangle{0.0f, y, contentW, spriteLabelHeight},
            smallFont,
            spriteLabel.c_str(),
            engine::UITextJustify::Left,
            selectedObject->billboard.spriteAnimationPath.empty() ? config.invalidColor : config.textColor,
            true);
    y += spriteLabelHeight + gap;

    const float selectedBillboardAspect =
            ResolveBillboardAspect(state, context.engineContext, assets, *selectedObject);
    const bool hasBillboardAspect = selectedBillboardAspect > 0.0f;

    auto drawObjectFloat =
            [&](const char* id,
                const char* label,
                float value,
                engine::UIFloatInputState& inputState,
                float minValue,
                float maxValue,
                int decimals,
                const std::function<bool(SectorPlacedRuntimeObject&, float)>& applyValue) {
                const float labelW = 92.0f;
                const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
                        ui,
                        config,
                        input,
                        assets,
                        font,
                        id,
                        label,
                        Rectangle{0.0f, y, labelW, rowH},
                        Rectangle{labelW + gap, y, std::max(0.0f, contentW - labelW - gap), rowH},
                        engine::UITextJustify::Left,
                        value,
                        inputState,
                        minValue,
                        maxValue,
                        decimals);
                if (result.changed && result.finite && result.value != value) {
                    callbacks.mutateSelectedRuntimeObject(
                            "Updated object transform",
                            [applyValue, value = result.value](SectorPlacedRuntimeObject& object) {
                                return applyValue(object, value);
                            });
                }
                y += rowH + gap;
            };

    auto drawBillboardFloat =
            [&](const char* id,
                const char* label,
                float value,
                engine::UIFloatInputState& inputState,
                float minValue,
                float maxValue,
                int decimals,
                const std::function<bool(SectorPlacedRuntimeObject&, float)>& applyValue) {
                const float labelW = 92.0f;
                const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
                        ui,
                        config,
                        input,
                        assets,
                        font,
                        id,
                        label,
                        Rectangle{0.0f, y, labelW, rowH},
                        Rectangle{labelW + gap, y, std::max(0.0f, contentW - labelW - gap), rowH},
                        engine::UITextJustify::Left,
                        value,
                        inputState,
                        minValue,
                        maxValue,
                        decimals);
                if (result.changed && result.finite && result.value != value) {
                    callbacks.mutateSelectedRuntimeObject(
                            "Updated billboard properties",
                            [applyValue, value = result.value](SectorPlacedRuntimeObject& object) {
                                return applyValue(object, value);
                            });
                }
                y += rowH + gap;
            };

    auto openBillboardPickerButton =
            [&](const char* id,
                const char* label) {
                if (engine::Button(
                            ui,
                            config,
                            input,
                            assets,
                            id,
                            Rectangle{0.0f, y, contentW, rowH},
                            font,
                            label)) {
                    callbacks.openBillboardSpritePicker();
                }
                y += rowH + gap;
            };

    const SectorSpriteMetadata* selectedBillboardMetadata = nullptr;
    std::string selectedBillboardMetadataStatus;
    auto resolveSelectedBillboardMetadata = [&]() -> const SectorSpriteMetadata* {
        if (!isBillboard || selectedObject == nullptr) {
            return nullptr;
        }
        const std::string spritePath = selectedObject->billboard.spriteAnimationPath;
        if (spritePath.empty()) {
            selectedBillboardMetadataStatus = "Pick a sprite first";
            return nullptr;
        }

        const bool targetChanged = state.billboardMetadataObjectId != selectedObject->id
                || state.billboardMetadataSpriteAnimationPath != spritePath;
        if (targetChanged) {
            state.billboardMetadataObjectId = selectedObject->id;
            state.billboardMetadataSpriteAnimationPath = spritePath;
            state.billboardMetadataInitialRepairAttempted = false;
            if (!state.spriteMetadataCatalog.scanned
                    || FindSpriteMetadata(state.spriteMetadataCatalog, spritePath) == nullptr) {
                RefreshSpriteMetadataCatalog(state.spriteMetadataCatalog);
            }
        }

        const SectorSpriteMetadata* metadata = FindSpriteMetadata(state.spriteMetadataCatalog, spritePath);
        if (metadata == nullptr) {
            selectedBillboardMetadataStatus = state.spriteMetadataCatalog.scanMessage.empty()
                    ? "Sprite metadata unavailable"
                    : state.spriteMetadataCatalog.scanMessage;
            return nullptr;
        }
        if (metadata->clipNames.empty()) {
            selectedBillboardMetadataStatus = "Sprite has no clips";
            return nullptr;
        }

        if (!state.billboardMetadataInitialRepairAttempted) {
            state.billboardMetadataInitialRepairAttempted = true;
            const int objectId = selectedObject->id;
            callbacks.mutateSelectedRuntimeObject(
                    "Updated billboard clip defaults",
                    [objectId, spritePath, metadata](SectorPlacedRuntimeObject& object) {
                        if (object.id != objectId
                                || object.kind != "billboard"
                                || object.billboard.spriteAnimationPath != spritePath) {
                            return false;
                        }
                        return RepairBillboardClipsForSpriteMetadata(object.billboard, *metadata, false);
                    });
            selectedObject = callbacks.selectedRuntimeObject();
        }
        return metadata;
    };

    auto drawBillboardClipOptionRow =
            [&](const char* optionId,
                const char* label,
                const std::string& clipName,
                const std::function<bool(SectorPlacedRuntimeObject&, const std::string&)>& applyClip) {
                const SectorEditorInspectorStackedOptionRowLayout layout =
                        BuildSectorEditorInspectorStackedOptionRowLayout(y, contentW, rowH, gap);
                engine::Text(
                        ui,
                        config,
                        assets,
                        layout.labelRect,
                        font,
                        label,
                        engine::UITextJustify::Left,
                        config.mutedTextColor);
                if (selectedBillboardMetadata == nullptr || selectedBillboardMetadata->clipNames.empty()) {
                    engine::Text(
                            ui,
                            config,
                            assets,
                            layout.fieldRect,
                            font,
                            selectedBillboardMetadataStatus.empty()
                                    ? "Sprite metadata unavailable"
                                    : selectedBillboardMetadataStatus.c_str(),
                            engine::UITextJustify::Left,
                            config.mutedTextColor);
                    y += layout.height + gap;
                    return;
                }

                int selectedIndex = 0;
                const auto it = std::find(
                        selectedBillboardMetadata->clipNames.begin(),
                        selectedBillboardMetadata->clipNames.end(),
                        clipName);
                if (it != selectedBillboardMetadata->clipNames.end()) {
                    selectedIndex = static_cast<int>(
                            std::distance(selectedBillboardMetadata->clipNames.begin(), it));
                }
                const int previousIndex = selectedIndex;
                if (engine::Option(
                            ui,
                            config,
                            input,
                            assets,
                            optionId,
                            layout.fieldRect,
                            font,
                            selectedBillboardMetadata->clipNames,
                            selectedIndex)
                        && selectedIndex >= 0
                        && selectedIndex < static_cast<int>(selectedBillboardMetadata->clipNames.size())
                        && selectedIndex != previousIndex) {
                    const std::string selectedClip =
                            selectedBillboardMetadata->clipNames[static_cast<size_t>(selectedIndex)];
                    callbacks.mutateSelectedRuntimeObject(
                            "Updated billboard clip",
                            [applyClip, selectedClip](SectorPlacedRuntimeObject& object) {
                                if (object.kind != "billboard") {
                                    return false;
                                }
                                return applyClip(object, selectedClip);
                            });
                }
                y += layout.height + gap;
            };

    if (isBillboard) {
        openBillboardPickerButton(
                "sector_editor_runtime_object_pick_sprite",
                "Pick Sprite");
        selectedObject = callbacks.selectedRuntimeObject();
        if (selectedObject == nullptr) {
            return;
        }
    }

    drawObjectFloat(
            "sector_editor_runtime_object_x",
            "Position X",
            selectedObject->position.x,
            uiState.runtimeObjectXInput,
            -100000.0f,
            100000.0f,
            3,
            [](SectorPlacedRuntimeObject& object, float value) {
                if (object.position.x == value) {
                    return false;
                }
                object.position.x = value;
                return true;
            });
    selectedObject = callbacks.selectedRuntimeObject();
    if (selectedObject == nullptr) {
        return;
    }
    drawObjectFloat(
            "sector_editor_runtime_object_y",
            "Position Y",
            selectedObject->position.y,
            uiState.runtimeObjectYInput,
            -100000.0f,
            100000.0f,
            3,
            [](SectorPlacedRuntimeObject& object, float value) {
                if (object.position.y == value) {
                    return false;
                }
                object.position.y = value;
                return true;
            });
    selectedObject = callbacks.selectedRuntimeObject();
    if (selectedObject == nullptr) {
        return;
    }
    drawObjectFloat(
            "sector_editor_runtime_object_z",
            "Position Z",
            selectedObject->position.z,
            uiState.runtimeObjectZInput,
            -100000.0f,
            100000.0f,
            3,
            [](SectorPlacedRuntimeObject& object, float value) {
                if (object.position.z == value) {
                    return false;
                }
                object.position.z = value;
                return true;
            });
    selectedObject = callbacks.selectedRuntimeObject();
    if (selectedObject == nullptr) {
        return;
    }
    constexpr float radiansToDegrees = 180.0f / PI;
    constexpr float degreesToRadians = PI / 180.0f;
    drawObjectFloat(
            "sector_editor_runtime_object_yaw",
            "Yaw",
            selectedObject->yawRadians * radiansToDegrees,
            uiState.runtimeObjectYawInput,
            -3600.0f,
            3600.0f,
            2,
            [degreesToRadians](SectorPlacedRuntimeObject& object, float value) {
                const float radians = value * degreesToRadians;
                if (object.yawRadians == radians) {
                    return false;
                }
                object.yawRadians = radians;
                return true;
            });
    selectedObject = callbacks.selectedRuntimeObject();
    if (selectedObject == nullptr) {
        return;
    }

    if (isBillboard) {
        drawBillboardFloat(
                "sector_editor_runtime_object_width",
                "Width",
                selectedObject->billboard.sizeWorld.x,
                uiState.runtimeObjectWidthInput,
                BillboardSizeMin,
                BillboardSizeMax,
                3,
                [hasBillboardAspect, selectedBillboardAspect](SectorPlacedRuntimeObject& object, float value) {
                    const float width = std::max(BillboardSizeMin, value);
                    bool changed = object.billboard.sizeWorld.x != width;
                    object.billboard.sizeWorld.x = width;
                    if (object.billboard.keepAspectRatio && hasBillboardAspect) {
                        const float height = std::max(BillboardSizeMin, width / selectedBillboardAspect);
                        changed = changed || object.billboard.sizeWorld.y != height;
                        object.billboard.sizeWorld.y = height;
                    }
                    return changed;
                });
        selectedObject = callbacks.selectedRuntimeObject();
        if (selectedObject == nullptr) {
            return;
        }

        drawBillboardFloat(
                "sector_editor_runtime_object_height",
                "Height",
                selectedObject->billboard.sizeWorld.y,
                uiState.runtimeObjectHeightInput,
                BillboardSizeMin,
                BillboardSizeMax,
                3,
                [hasBillboardAspect, selectedBillboardAspect](SectorPlacedRuntimeObject& object, float value) {
                    const float height = std::max(BillboardSizeMin, value);
                    bool changed = object.billboard.sizeWorld.y != height;
                    object.billboard.sizeWorld.y = height;
                    if (object.billboard.keepAspectRatio && hasBillboardAspect) {
                        const float width = std::max(BillboardSizeMin, height * selectedBillboardAspect);
                        changed = changed || object.billboard.sizeWorld.x != width;
                        object.billboard.sizeWorld.x = width;
                    }
                    return changed;
                });
        selectedObject = callbacks.selectedRuntimeObject();
        if (selectedObject == nullptr) {
            return;
        }

        bool keepAspect = selectedObject->billboard.keepAspectRatio;
        if (engine::Checkbox(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_runtime_object_keep_aspect",
                    Rectangle{0.0f, y, contentW, rowH},
                    font,
                    "Keep aspect ratio",
                    keepAspect)) {
            callbacks.mutateSelectedRuntimeObject(
                    keepAspect && !hasBillboardAspect
                            ? "Billboard aspect unavailable until sprite metadata loads"
                            : "Updated billboard aspect mode",
                    [keepAspect](SectorPlacedRuntimeObject& object) {
                        if (object.billboard.keepAspectRatio == keepAspect) {
                            return false;
                        }
                        object.billboard.keepAspectRatio = keepAspect;
                        return true;
                    });
        }
        y += rowH + gap;
        selectedObject = callbacks.selectedRuntimeObject();
        if (selectedObject == nullptr) {
            return;
        }
        if (selectedObject->billboard.keepAspectRatio && !hasBillboardAspect) {
            engine::Text(
                    ui,
                    smallConfig,
                    assets,
                    Rectangle{0.0f, y, contentW, 28.0f},
                    smallFont,
                    "Aspect unavailable until sprite metadata loads",
                    engine::UITextJustify::Left,
                    config.mutedTextColor);
            y += 28.0f + gap;
        }

        drawBillboardFloat(
                "sector_editor_runtime_object_origin_x",
                "Origin X",
                selectedObject->billboard.originNormalized.x,
                uiState.runtimeObjectOriginXInput,
                0.0f,
                1.0f,
                3,
                [](SectorPlacedRuntimeObject& object, float value) {
                    const float origin = Clamp(value, 0.0f, 1.0f);
                    if (object.billboard.originNormalized.x == origin) {
                        return false;
                    }
                    object.billboard.originNormalized.x = origin;
                    return true;
                });
        selectedObject = callbacks.selectedRuntimeObject();
        if (selectedObject == nullptr) {
            return;
        }

        drawBillboardFloat(
                "sector_editor_runtime_object_origin_y",
                "Origin Y",
                selectedObject->billboard.originNormalized.y,
                uiState.runtimeObjectOriginYInput,
                0.0f,
                1.0f,
                3,
                [](SectorPlacedRuntimeObject& object, float value) {
                    const float origin = Clamp(value, 0.0f, 1.0f);
                    if (object.billboard.originNormalized.y == origin) {
                        return false;
                    }
                    object.billboard.originNormalized.y = origin;
                    return true;
                });
        selectedObject = callbacks.selectedRuntimeObject();
        if (selectedObject == nullptr) {
            return;
        }

        bool directional = selectedObject->billboard.directional;
        if (engine::Checkbox(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_runtime_object_directional",
                    Rectangle{0.0f, y, contentW, rowH},
                    font,
                    "Directional",
                    directional)) {
            callbacks.mutateSelectedRuntimeObject(
                    "Updated billboard directional mode",
                    [directional](SectorPlacedRuntimeObject& object) {
                        if (object.billboard.directional == directional) {
                            return false;
                        }
                        object.billboard.directional = directional;
                        return true;
                    });
        }
        y += rowH + gap;
        selectedObject = callbacks.selectedRuntimeObject();
        if (selectedObject == nullptr) {
            return;
        }

        selectedBillboardMetadata = resolveSelectedBillboardMetadata();
        selectedObject = callbacks.selectedRuntimeObject();
        if (selectedObject == nullptr) {
            return;
        }

        if (selectedObject->billboard.directional) {
            drawBillboardClipOptionRow(
                    "sector_editor_billboard_clip_front_option",
                    "Front Clip",
                    selectedObject->billboard.frontClip,
                    [](SectorPlacedRuntimeObject& object, const std::string& clip) {
                        if (object.billboard.frontClip == clip) {
                            return false;
                        }
                        object.billboard.frontClip = clip;
                        return true;
                    });
            selectedObject = callbacks.selectedRuntimeObject();
            if (selectedObject == nullptr) {
                return;
            }
            drawBillboardClipOptionRow(
                    "sector_editor_billboard_clip_back_option",
                    "Back Clip",
                    selectedObject->billboard.backClip,
                    [](SectorPlacedRuntimeObject& object, const std::string& clip) {
                        if (object.billboard.backClip == clip) {
                            return false;
                        }
                        object.billboard.backClip = clip;
                        return true;
                    });
            selectedObject = callbacks.selectedRuntimeObject();
            if (selectedObject == nullptr) {
                return;
            }
            drawBillboardClipOptionRow(
                    "sector_editor_billboard_clip_left_option",
                    "Left Clip",
                    selectedObject->billboard.leftClip,
                    [](SectorPlacedRuntimeObject& object, const std::string& clip) {
                        if (object.billboard.leftClip == clip) {
                            return false;
                        }
                        object.billboard.leftClip = clip;
                        return true;
                    });
            selectedObject = callbacks.selectedRuntimeObject();
            if (selectedObject == nullptr) {
                return;
            }
            drawBillboardClipOptionRow(
                    "sector_editor_billboard_clip_right_option",
                    "Right Clip",
                    selectedObject->billboard.rightClip,
                    [](SectorPlacedRuntimeObject& object, const std::string& clip) {
                        if (object.billboard.rightClip == clip) {
                            return false;
                        }
                        object.billboard.rightClip = clip;
                        return true;
                    });
            selectedObject = callbacks.selectedRuntimeObject();
            if (selectedObject == nullptr) {
                return;
            }
        } else {
            drawBillboardClipOptionRow(
                    "sector_editor_billboard_clip_single_option",
                    "Clip",
                    selectedObject->billboard.clip,
                    [](SectorPlacedRuntimeObject& object, const std::string& clip) {
                        if (object.billboard.clip == clip) {
                            return false;
                        }
                        object.billboard.clip = clip;
                        return true;
                    });
            selectedObject = callbacks.selectedRuntimeObject();
            if (selectedObject == nullptr) {
                return;
            }
        }

        bool playing = selectedObject->billboard.playing;
        if (engine::Checkbox(
                    ui,
                    config,
                    input,
                    assets,
                    "sector_editor_runtime_object_playing",
                    Rectangle{0.0f, y, contentW, rowH},
                    font,
                    "Playing",
                    playing)) {
            callbacks.mutateSelectedRuntimeObject(
                    "Updated billboard playback",
                    [playing](SectorPlacedRuntimeObject& object) {
                        if (object.billboard.playing == playing) {
                            return false;
                        }
                        object.billboard.playing = playing;
                        return true;
                    });
        }
        y += rowH + gap;
    }

    if (engine::Button(
                ui,
                config,
                input,
                assets,
                "sector_editor_delete_runtime_object",
                Rectangle{0.0f, y, contentW, rowH},
                font,
                "Delete")) {
        callbacks.deleteSelectedRuntimeObject();
    }
}

} // namespace game
