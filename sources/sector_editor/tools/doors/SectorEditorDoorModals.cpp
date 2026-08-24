#include "sector_editor/tools/doors/SectorEditorDoorModals.h"

#include "engine/input/InputEvents.h"
#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/SectorEditorUiHelpers.h"
#include "sector_demo/SectorDoorRuntime.h"

#include <cmath>

namespace game {

namespace {

void ResetDoorTextureSettingsInputs(DoorTextureSettingsModalState& modalState)
{
    modalState.scaleUInput = engine::UIFloatInputState{};
    modalState.scaleVInput = engine::UIFloatInputState{};
    modalState.offsetUInput = engine::UIFloatInputState{};
    modalState.offsetVInput = engine::UIFloatInputState{};
}

bool ApplySelectedDoorFaceUvValue(
        SectorEditorDoorTextureSettingsModalContext& context,
        int component,
        float value)
{
    DoorTextureSettingsModalState& modal = context.modalState;
    if (!modal.open || context.selectedRuntimeObjectId != modal.runtimeObjectId) {
        context.statusText = "Door UV target unavailable.";
        return false;
    }
    if (!std::isfinite(value)) {
        context.statusText = "Door UV values must be finite.";
        return false;
    }
    if ((component == 0 || component == 1) && !IsValidSectorDoorUvScale(value)) {
        context.statusText = "Door UV scale must be between 0.001 and 64.";
        return false;
    }

    const SectorDoorFace face = modal.selectedFace;
    if (!context.callbacks.mutateSelectedRuntimeObject) {
        return false;
    }
    return context.callbacks.mutateSelectedRuntimeObject(
            TextFormat("Updated door %s UV", SectorDoorFaceName(face)),
            [face, component, value](SectorPlacedRuntimeObject& object) {
                if (object.kind != "door") {
                    return false;
                }
                SectorDoorFaceUv& uv = DoorFaceUv(object.door.faceUvs, face);
                float* target = nullptr;
                switch (component) {
                    case 0: target = &uv.scale.x; break;
                    case 1: target = &uv.scale.y; break;
                    case 2: target = &uv.offset.x; break;
                    case 3: target = &uv.offset.y; break;
                    default: break;
                }
                if (target == nullptr || *target == value) {
                    return false;
                }
                *target = value;
                return true;
            });
}

bool ApplySelectedDoorFaceUvFit(
        SectorEditorDoorTextureSettingsModalContext& context,
        SectorDoorUvFitMode mode)
{
    DoorTextureSettingsModalState& modal = context.modalState;
    const SectorPlacedRuntimeObject* object = context.callbacks.selectedRuntimeObject
            ? context.callbacks.selectedRuntimeObject()
            : nullptr;
    if (!modal.open
            || object == nullptr
            || object->kind != "door"
            || object->id != modal.runtimeObjectId) {
        modal.statusMessage = "Door UV target unavailable.";
        context.statusText = modal.statusMessage;
        return false;
    }

    const SectorResolvedDoorAnchor resolved = ResolveSectorDoorAnchor(context.topologyMap, object->door);
    if (!resolved.valid) {
        modal.statusMessage = "Fit needs a valid door anchor.";
        context.statusText = modal.statusMessage;
        return false;
    }

    const SectorDoorRender render{
            resolved.width,
            resolved.height,
            object->door.thickness,
            object->door.normalOffset,
            object->door.heightOffsetWorld,
            object->door.materialId,
            object->door.faceUvs,
            WHITE,
            true};
    const SectorDoorFace face = modal.selectedFace;
    if (!context.callbacks.mutateSelectedRuntimeObject) {
        return false;
    }
    const bool changed = context.callbacks.mutateSelectedRuntimeObject(
            TextFormat("Fit door %s UV", SectorDoorFaceName(face)),
            [face, mode, render, &modal](SectorPlacedRuntimeObject& target) {
                if (target.kind != "door") {
                    return false;
                }
                std::string error;
                const SectorDoorFaceUvSet before = target.door.faceUvs;
                if (!FitSectorDoorFaceUv(target.door.faceUvs, face, mode, render, &error)) {
                    modal.statusMessage = error;
                    return false;
                }
                return !SameSectorDoorFaceUvSet(before, target.door.faceUvs);
            });
    if (changed) {
        modal.statusMessage = TextFormat("Fit %s UV to one texture repeat.", SectorDoorFaceName(face));
    } else if (modal.statusMessage.empty()) {
        modal.statusMessage = "Door UV fit unchanged.";
    }
    context.statusText = modal.statusMessage;
    return changed;
}

bool ResetSelectedDoorFaceUv(SectorEditorDoorTextureSettingsModalContext& context)
{
    DoorTextureSettingsModalState& modal = context.modalState;
    if (!modal.open || context.selectedRuntimeObjectId != modal.runtimeObjectId) {
        modal.statusMessage = "Door UV target unavailable.";
        context.statusText = modal.statusMessage;
        return false;
    }
    const SectorDoorFace face = modal.selectedFace;
    if (!context.callbacks.mutateSelectedRuntimeObject) {
        return false;
    }
    const bool changed = context.callbacks.mutateSelectedRuntimeObject(
            TextFormat("Reset door %s UV", SectorDoorFaceName(face)),
            [face](SectorPlacedRuntimeObject& object) {
                return object.kind == "door" && ResetSectorDoorFaceUv(object.door.faceUvs, face);
            });
    modal.statusMessage = changed
            ? TextFormat("Reset %s UV.", SectorDoorFaceName(face))
            : "Door face UV already default.";
    context.statusText = modal.statusMessage;
    return changed;
}

bool CopySelectedDoorFaceUvFromFront(SectorEditorDoorTextureSettingsModalContext& context)
{
    DoorTextureSettingsModalState& modal = context.modalState;
    if (!modal.open || context.selectedRuntimeObjectId != modal.runtimeObjectId) {
        modal.statusMessage = "Door UV target unavailable.";
        context.statusText = modal.statusMessage;
        return false;
    }
    const SectorDoorFace face = modal.selectedFace;
    if (!context.callbacks.mutateSelectedRuntimeObject) {
        return false;
    }
    const bool changed = context.callbacks.mutateSelectedRuntimeObject(
            TextFormat("Copied front UV to door %s", SectorDoorFaceName(face)),
            [face](SectorPlacedRuntimeObject& object) {
                return object.kind == "door"
                        && CopySectorDoorFaceUv(object.door.faceUvs, SectorDoorFace::Front, face);
            });
    modal.statusMessage = changed
            ? TextFormat("Copied Front UV to %s.", SectorDoorFaceName(face))
            : "Copy From Front unchanged.";
    context.statusText = modal.statusMessage;
    return changed;
}

bool ApplySelectedDoorFaceUvToAll(SectorEditorDoorTextureSettingsModalContext& context)
{
    DoorTextureSettingsModalState& modal = context.modalState;
    if (!modal.open || context.selectedRuntimeObjectId != modal.runtimeObjectId) {
        modal.statusMessage = "Door UV target unavailable.";
        context.statusText = modal.statusMessage;
        return false;
    }
    const SectorDoorFace face = modal.selectedFace;
    if (!context.callbacks.mutateSelectedRuntimeObject) {
        return false;
    }
    const bool changed = context.callbacks.mutateSelectedRuntimeObject(
            TextFormat("Applied door %s UV to all faces", SectorDoorFaceName(face)),
            [face](SectorPlacedRuntimeObject& object) {
                return object.kind == "door" && ApplySectorDoorFaceUvToAll(object.door.faceUvs, face);
            });
    modal.statusMessage = changed
            ? TextFormat("Applied %s UV to all faces.", SectorDoorFaceName(face))
            : "Apply To All unchanged.";
    context.statusText = modal.statusMessage;
    return changed;
}

} // namespace

bool OpenSectorEditorDoorTextureSettingsModal(
        DoorTextureSettingsModalState& modalState,
        const SectorPlacedRuntimeObject* selectedObject,
        std::string& statusText)
{
    if (selectedObject == nullptr || selectedObject->kind != "door") {
        statusText = "Select a door first.";
        return false;
    }

    modalState = DoorTextureSettingsModalState{};
    modalState.open = true;
    modalState.runtimeObjectId = selectedObject->id;
    modalState.selectedFace = SectorDoorFace::Front;
    return true;
}

void DrawSectorEditorDoorTextureSettingsModal(
        SectorEditorDoorTextureSettingsModalContext& context)
{
    DoorTextureSettingsModalState& modalState = context.modalState;
    if (!modalState.open) {
        return;
    }

    bool closeRequested = false;
    context.input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [&closeRequested](engine::InputEvent& event) {
                if (event.key.key == KEY_ESCAPE) {
                    closeRequested = true;
                    engine::ConsumeEvent(event);
                }
            });

    const SectorPlacedRuntimeObject* selectedObject = context.callbacks.selectedRuntimeObject
            ? context.callbacks.selectedRuntimeObject()
            : nullptr;
    if (selectedObject == nullptr
            || selectedObject->kind != "door"
            || selectedObject->id != modalState.runtimeObjectId) {
        modalState.statusMessage = "Door texture settings target is no longer selected.";
    }

    DrawRectangle(0, 0, static_cast<int>(EditorWidth), static_cast<int>(EditorHeight), Color{0, 0, 0, 145});
    const Rectangle modal{
            (EditorWidth - 680.0f) * 0.5f,
            (EditorHeight - 600.0f) * 0.5f,
            680.0f,
            600.0f
    };
    constexpr float padding = 26.0f;
    constexpr float gap = 10.0f;
    const SectorEditorDoorTextureSettingsModalLayout layout =
            BuildSectorEditorDoorTextureSettingsModalLayout(modal, padding, gap);
    DrawRectangleRec(modal, Color{20, 24, 32, 248});
    DrawRectangleLinesEx(modal, context.config.borderThickness, context.config.borderColor);

    engine::Text(context.config, context.assets, layout.titleRect, context.font, "Door Texture Settings");

    for (int i = 0; i < SectorDoorFaceCount; ++i) {
        const SectorDoorFace face = SectorDoorFaceFromIndex(i);
        const bool selected = SectorDoorFaceIndex(modalState.selectedFace) == i;
        if (engine::ToolButton(
                    context.ui,
                    context.config,
                    context.input,
                    context.assets,
                    TextFormat("sector_editor_door_uv_face_%d", i),
                    layout.faceButtonRects[i],
                    context.font,
                    SectorDoorFaceName(face),
                    selected)) {
            if (!selected) {
                modalState.selectedFace = face;
                ResetDoorTextureSettingsInputs(modalState);
                modalState.statusMessage.clear();
            }
        }
    }

    selectedObject = context.callbacks.selectedRuntimeObject
            ? context.callbacks.selectedRuntimeObject()
            : nullptr;
    const SectorDoorFaceUv selectedUv = selectedObject != nullptr
                    && selectedObject->kind == "door"
                    && selectedObject->id == modalState.runtimeObjectId
            ? DoorFaceUv(selectedObject->door.faceUvs, modalState.selectedFace)
            : SectorDoorFaceUv{};

    auto drawUvFloat =
            [&](const char* id,
                const char* label,
                float value,
                engine::UIFloatInputState& inputState,
                float minValue,
                float maxValue,
                int component,
                int row) {
                const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
                        context.ui,
                        context.config,
                        context.input,
                        context.assets,
                        context.font,
                        id,
                        label,
                        layout.uvLabelRects[row],
                        layout.uvInputRects[row],
                        engine::UITextJustify::Left,
                        value,
                        inputState,
                        minValue,
                        maxValue,
                        3);
                if (result.changed && result.value != value) {
                    if (!result.finite) {
                        modalState.statusMessage = "Door UV values must be finite.";
                    } else if (!ApplySelectedDoorFaceUvValue(context, component, result.value)) {
                        modalState.statusMessage = context.statusText.empty()
                                ? "Door UV value unchanged."
                                : context.statusText;
                    } else {
                        modalState.statusMessage = TextFormat(
                                "Updated %s UV.",
                                SectorDoorFaceName(modalState.selectedFace));
                    }
                }
            };

    drawUvFloat(
            "sector_editor_door_uv_scale_u",
            "UV Scale U",
            selectedUv.scale.x,
            modalState.scaleUInput,
            TopologyUvScaleMin,
            TopologyUvScaleMax,
            0,
            0);
    drawUvFloat(
            "sector_editor_door_uv_scale_v",
            "UV Scale V",
            selectedUv.scale.y,
            modalState.scaleVInput,
            TopologyUvScaleMin,
            TopologyUvScaleMax,
            1,
            1);
    drawUvFloat(
            "sector_editor_door_uv_offset_u",
            "UV Offset U",
            selectedUv.offset.x,
            modalState.offsetUInput,
            -100000.0f,
            100000.0f,
            2,
            2);
    drawUvFloat(
            "sector_editor_door_uv_offset_v",
            "UV Offset V",
            selectedUv.offset.y,
            modalState.offsetVInput,
            -100000.0f,
            100000.0f,
            3,
            3);

    auto actionButton = [&](const char* id, const char* label, Rectangle bounds, const std::function<bool()>& action) {
        if (engine::Button(context.ui, context.config, context.input, context.assets, id, bounds, context.font, label)) {
            if (action && action()) {
                ResetDoorTextureSettingsInputs(modalState);
            } else if (modalState.statusMessage.empty() && !context.statusText.empty()) {
                modalState.statusMessage = context.statusText;
            }
        }
    };

    actionButton(
            "sector_editor_door_uv_fit_width",
            "Fit Width",
            layout.actionButtonRects[0],
            [&context]() { return ApplySelectedDoorFaceUvFit(context, SectorDoorUvFitMode::Width); });
    actionButton(
            "sector_editor_door_uv_fit_height",
            "Fit Height",
            layout.actionButtonRects[1],
            [&context]() { return ApplySelectedDoorFaceUvFit(context, SectorDoorUvFitMode::Height); });
    actionButton(
            "sector_editor_door_uv_fit_both",
            "Fit Both",
            layout.actionButtonRects[2],
            [&context]() { return ApplySelectedDoorFaceUvFit(context, SectorDoorUvFitMode::Both); });
    actionButton(
            "sector_editor_door_uv_reset_face",
            "Reset Face",
            layout.actionButtonRects[3],
            [&context]() { return ResetSelectedDoorFaceUv(context); });
    actionButton(
            "sector_editor_door_uv_copy_front",
            "Copy From Front",
            layout.actionButtonRects[4],
            [&context]() { return CopySelectedDoorFaceUvFromFront(context); });
    actionButton(
            "sector_editor_door_uv_apply_all",
            "Apply To All",
            layout.actionButtonRects[5],
            [&context]() { return ApplySelectedDoorFaceUvToAll(context); });

    if (!modalState.statusMessage.empty()) {
        const engine::UIConfig smallConfig =
                SectorEditorSmallFontConfig(context.config, context.assets, context.smallFont);
        engine::Text(
                context.ui,
                smallConfig,
                context.assets,
                layout.statusRect,
                context.smallFont,
                modalState.statusMessage.c_str(),
                engine::UITextJustify::Left,
                context.config.mutedTextColor,
                true);
    }

    closeRequested = closeRequested || engine::Button(
            context.ui,
            context.config,
            context.input,
            context.assets,
            "sector_editor_door_uv_done",
            layout.doneButtonRect,
            context.font,
            "Done");

    context.input.ForEachEvent(engine::InputEventType::Any, true, [](engine::InputEvent& event) {
        engine::ConsumeEvent(event);
    });

    if (closeRequested) {
        modalState = DoorTextureSettingsModalState{};
    }
}

} // namespace game
