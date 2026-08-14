#include "sector_editor/npcs/SectorEditorNpcEditorModal.h"

#include "engine/input/InputEvents.h"
#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/SectorEditorUiHelpers.h"
#include "sector_editor/services/static_model_picker/SectorEditorModelPickerModal.h"

#include <algorithm>
#include <string>
#include <vector>

namespace game {
namespace {

constexpr float RowHeight = 42.0f;
constexpr float RowGap = 10.0f;
constexpr float LabelWidth = 220.0f;

float ScrollContentWidth(float boundsWidth, const engine::UIConfig& config)
{
    const float clientWidth = std::max(
            0.0f,
            boundsWidth - config.borderThickness * 2.0f);
    return std::max(
            0.0f,
            clientWidth - config.scrollbarSize
                    - engine::DefaultScrollAreaPaddingPx * 2.0f);
}

void DrawModalFrame(
        const engine::UIConfig& config,
        engine::AssetManager& assets,
        engine::FontHandle font,
        const SectorEditorNpcEditorModalLayout& layout)
{
    DrawRectangle(
            0,
            0,
            static_cast<int>(EditorWidth),
            static_cast<int>(EditorHeight),
            Color{0, 0, 0, 150});
    DrawRectangleRec(layout.modal, Color{20, 24, 32, 250});
    DrawRectangleLinesEx(layout.modal, config.borderThickness, config.borderColor);
    engine::Text(
            config,
            assets,
            Rectangle{
                    layout.modal.x + 24.0f,
                    layout.modal.y + 16.0f,
                    layout.modal.width - 48.0f,
                    42.0f},
            font,
            "NPC Editor");
}

void ConsumeRemainingInput(engine::Input& input)
{
    input.ForEachEvent(
            engine::InputEventType::Any,
            true,
            [](engine::InputEvent& event) { engine::ConsumeEvent(event); });
}

void DrawDeleteConfirmation(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        SectorEditorNpcEditorService& editor,
        const SectorEditorNpcEditorModalLayout& layout)
{
    const Rectangle popup{
            layout.modal.x + (layout.modal.width - 600.0f) * 0.5f,
            layout.modal.y + (layout.modal.height - 250.0f) * 0.5f,
            600.0f,
            250.0f};
    DrawRectangleRec(popup, Color{27, 32, 42, 255});
    DrawRectangleLinesEx(popup, config.borderThickness, config.borderColor);
    engine::Text(
            config, assets,
            Rectangle{popup.x + 24.0f, popup.y + 20.0f, popup.width - 48.0f, 38.0f},
            font, "Delete NPC definition?");
    const std::string message = "Delete '"
            + editor.State().deleteConfirmationId
            + "' when the catalog is saved? The character model will not be deleted.";
    engine::Text(
            config, assets,
            Rectangle{popup.x + 24.0f, popup.y + 70.0f, popup.width - 48.0f, 72.0f},
            font, message.c_str(), engine::UITextJustify::Left,
            config.textColor, true);
    if (engine::Button(
                ui, config, input, assets,
                "sector_editor_npc_delete_confirm",
                Rectangle{popup.x + popup.width - 310.0f, popup.y + 178.0f, 130.0f, 44.0f},
                font, "Delete")) {
        editor.ConfirmDeleteSelected();
    }
    if (engine::Button(
                ui, config, input, assets,
                "sector_editor_npc_delete_cancel",
                Rectangle{popup.x + popup.width - 160.0f, popup.y + 178.0f, 130.0f, 44.0f},
                font, "Cancel")) {
        editor.CancelDelete();
    }
}

} // namespace

SectorEditorNpcEditorModalLayout BuildSectorEditorNpcEditorModalLayout()
{
    return BuildSectorEditorNpcEditorModalLayoutForViewport(
            EditorWidth,
            EditorHeight);
}

SectorEditorNpcEditorModalResult DrawSectorEditorNpcEditorModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont,
        SectorEditorNpcEditorService& editor,
        SectorEditorStaticModelPickerService& modelPicker)
{
    SectorEditorNpcEditorState& state = editor.State();
    if (!state.open) return SectorEditorNpcEditorModalResult::None;
    const SectorEditorNpcEditorModalLayout layout =
            BuildSectorEditorNpcEditorModalLayout();
    DrawModalFrame(config, assets, font, layout);

    if (modelPicker.State().open
            && modelPicker.State().target == ModelPickerTarget::NpcDefinition) {
        const SectorEditorModelPickerModalResult result =
                DrawSectorEditorModelPickerModal(
                        ui, config, input, assets, font, modelPicker);
        if (result == SectorEditorModelPickerModalResult::Selected) {
            editor.SetSelectedModelPath(modelPicker.SelectedModelPath(), assets);
            modelPicker.State().open = false;
        }
        return SectorEditorNpcEditorModalResult::None;
    }

    bool cancelRequested = false;
    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [&cancelRequested, &state](engine::InputEvent& event) {
                if (event.key.key != KEY_ESCAPE) return;
                if (state.deleteConfirmationOpen) {
                    state.deleteConfirmationOpen = false;
                    state.deleteConfirmationId.clear();
                } else {
                    cancelRequested = true;
                }
                engine::ConsumeEvent(event);
            });
    if (cancelRequested) {
        editor.Cancel(&assets);
        ConsumeRemainingInput(input);
        return SectorEditorNpcEditorModalResult::Cancelled;
    }

    if (state.deleteConfirmationOpen) {
        DrawDeleteConfirmation(ui, config, input, assets, font, editor, layout);
        ConsumeRemainingInput(input);
        return SectorEditorNpcEditorModalResult::None;
    }

    const float listContentW = ScrollContentWidth(layout.listBounds.width, config);
    const Vector2 listContentSize{
            listContentW,
            std::max(
                    layout.listBounds.height,
                    config.listItemHeight
                            * static_cast<float>(state.listLabels.size()))};
    engine::UIScrollAreaResult listScroll = engine::BeginScrollArea(
            ui, config, input,
            "sector_editor_npc_list_scroll",
            layout.listBounds,
            listContentSize,
            editor.Session().listScroll);
    if (!state.listLabels.empty()) {
        int selectedIndex = state.selectedIndex;
        engine::List(
                ui, config, input, assets,
                "sector_editor_npc_list",
                Rectangle{0.0f, 0.0f, listScroll.viewport.width, listContentSize.y},
                smallFont,
                state.listLabels.data(),
                state.listLabels.size(),
                selectedIndex);
        if (selectedIndex != state.selectedIndex && editor.SelectIndex(selectedIndex)) {
            ui.focusedId = 0;
            ui.openOptionId = 0;
            editor.EnsureSelectedModelRequested(assets);
        }
    }
    engine::EndScrollArea(
            ui, config, input, listScroll, editor.Session().listScroll);

    if (engine::Button(
                ui, config, input, assets,
                "sector_editor_npc_add",
                layout.addButton,
                font, "Add")) {
        editor.AddDefinition();
        ui.focusedId = 0;
        ui.openOptionId = 0;
    }
    if (engine::Button(
                ui, config, input, assets,
                "sector_editor_npc_delete",
                layout.deleteButton,
                font, "Delete")
            && editor.SelectedDraft() != nullptr) {
        editor.RequestDeleteSelected();
    }

    SectorEditorNpcDefinitionDraft* selected = editor.SelectedDraft();
    if (selected == nullptr) {
        engine::Text(
                config, assets,
                Rectangle{
                        layout.formBounds.x + 24.0f,
                        layout.formBounds.y + 24.0f,
                        layout.formBounds.width - 48.0f,
                        80.0f},
                font,
                "No NPC definitions. Press Add to create one.",
                engine::UITextJustify::Left,
                config.mutedTextColor,
                true);
    } else {
        editor.RefreshAnimationOptions(assets);
        const float contentW = ScrollContentWidth(layout.formBounds.width, config);
        const float actionSectionHeight = 4.0f * (RowHeight + RowGap) + 54.0f;
        const float contentHeight = 6.0f * (RowHeight + RowGap)
                + actionSectionHeight * static_cast<float>(kNpcActionCount)
                + 80.0f;
        engine::UIScrollAreaResult formScroll = engine::BeginScrollArea(
                ui, config, input,
                "sector_editor_npc_form_scroll",
                layout.formBounds,
                Vector2{contentW, contentHeight},
                editor.Session().formScroll);
        float y = 0.0f;
        const float fieldX = LabelWidth + 16.0f;
        const float fieldW = std::max(160.0f, formScroll.viewport.width - fieldX);
        const auto drawLabel = [&](const char* label) {
            engine::Text(
                    ui, config, assets,
                    Rectangle{0.0f, y, LabelWidth, RowHeight},
                    font, label, engine::UITextJustify::Left,
                    config.mutedTextColor);
        };

        drawLabel("ID");
        const engine::UITextInputResult idResult = engine::TextInput(
                ui, config, input, assets,
                "sector_editor_npc_id",
                Rectangle{fieldX, y, fieldW, RowHeight},
                font,
                state.idBuffer,
                sizeof(state.idBuffer),
                0,
                sizeof(state.idBuffer) - 1);
        if (idResult.changed) editor.ApplyIdBuffer();
        y += RowHeight + RowGap;

        drawLabel("Name");
        const engine::UITextInputResult nameResult = engine::TextInput(
                ui, config, input, assets,
                "sector_editor_npc_name",
                Rectangle{fieldX, y, fieldW, RowHeight},
                font,
                state.nameBuffer,
                sizeof(state.nameBuffer),
                0,
                sizeof(state.nameBuffer) - 1);
        if (nameResult.changed) editor.ApplyNameBuffer();
        y += RowHeight + RowGap;

        bool hostile = selected->definition.hostile;
        if (engine::Checkbox(
                    ui, config, input, assets,
                    "sector_editor_npc_hostile",
                    Rectangle{fieldX, y, 260.0f, RowHeight},
                    font, "Hostile", hostile)) {
            editor.SetSelectedHostile(hostile);
        }
        y += RowHeight + RowGap;

        drawLabel("Model");
        engine::Text(
                ui, config, assets,
                Rectangle{fieldX, y, std::max(0.0f, fieldW - 190.0f), RowHeight},
                smallFont,
                selected->definition.modelPath.empty()
                        ? "<none>"
                        : selected->definition.modelPath.c_str(),
                engine::UITextJustify::Left,
                selected->definition.modelPath.empty()
                        ? config.invalidColor
                        : config.textColor);
        if (engine::Button(
                    ui, config, input, assets,
                    "sector_editor_npc_choose_model",
                    Rectangle{fieldX + fieldW - 176.0f, y, 176.0f, RowHeight},
                    font, "Choose Model")) {
            modelPicker.Open(
                    selected->definition.modelPath,
                    ModelPickerTarget::NpcDefinition);
        }
        y += RowHeight + RowGap;

        drawLabel("Animation blend");
        float animationBlendSeconds = selected->definition.animationBlendSeconds;
        const engine::UINumericInputResult blendResult = engine::FloatInput(
                ui, config, input, assets,
                "sector_editor_npc_animation_blend_seconds",
                Rectangle{fieldX, y, 190.0f, RowHeight},
                font,
                animationBlendSeconds,
                state.animationBlendSecondsInput,
                kMinimumNpcAnimationBlendSeconds,
                kMaximumNpcAnimationBlendSeconds,
                2);
        if (blendResult.changed) {
            editor.SetSelectedAnimationBlendSeconds(animationBlendSeconds);
        }
        y += RowHeight + RowGap;

        const char* modelStatus = !state.warningMessage.empty()
                ? state.warningMessage.c_str()
                : (selected->definition.modelPath.empty()
                ? "Select a character model before saving"
                : (editor.SelectedModelReady(assets)
                        ? (state.animationOptionStorage.size() <= 1
                                ? "Model contains no named animations"
                                : "Model animations loaded")
                        : (editor.SelectedModelFailed(assets)
                                ? "Model failed to load; saved action names are preserved"
                                : "Model animations loading...")));
        engine::Text(
                ui, config, assets,
                Rectangle{fieldX, y, fieldW, RowHeight},
                smallFont, modelStatus, engine::UITextJustify::Left,
                !state.warningMessage.empty()
                        || editor.SelectedModelFailed(assets)
                        || selected->definition.modelPath.empty()
                        ? config.invalidColor
                        : config.mutedTextColor);
        y += RowHeight + RowGap;

        for (const NpcActionMetadata& metadata : NpcActionMetadataTable()) {
            engine::Separator(
                    config,
                    Rectangle{
                            formScroll.viewport.x,
                            formScroll.viewport.y
                                    - editor.Session().formScroll.offset.y + y,
                            formScroll.viewport.width,
                            12.0f});
            y += 18.0f;
            engine::Text(
                    ui, config, assets,
                    Rectangle{0.0f, y, formScroll.viewport.width, 34.0f},
                    font, metadata.displayName,
                    engine::UITextJustify::Left,
                    config.accentColor);
            y += 38.0f;

            NpcActionDefinition& action =
                    GetNpcAction(selected->definition, metadata.action);
            drawLabel("Animation");
            if (!state.animationOptions.empty()) {
                int selectedAnimation = 0;
                bool authoredAnimationFound = action.animation.empty();
                for (size_t index = 1; index < state.animationOptionStorage.size(); ++index) {
                    if (state.animationOptionStorage[index] == action.animation) {
                        selectedAnimation = static_cast<int>(index);
                        authoredAnimationFound = true;
                        break;
                    }
                }
                std::string missingOptionLabel;
                std::vector<const char*> actionOptions = state.animationOptions;
                if (!authoredAnimationFound) {
                    missingOptionLabel = "<Missing: " + action.animation + ">";
                    actionOptions.push_back(missingOptionLabel.c_str());
                    selectedAnimation = static_cast<int>(actionOptions.size()) - 1;
                }
                const std::string optionId = std::string{"sector_editor_npc_animation_"}
                        + metadata.jsonKey;
                if (engine::Option(
                            ui, config, input, assets,
                            optionId.c_str(),
                            Rectangle{fieldX, y, fieldW, RowHeight},
                            font,
                            actionOptions.data(),
                            actionOptions.size(),
                            selectedAnimation)) {
                    editor.SetSelectedAnimation(
                            metadata.action,
                            selectedAnimation <= 0
                                    || selectedAnimation
                                            >= static_cast<int>(
                                                    state.animationOptionStorage.size())
                                    ? std::string{}
                                    : state.animationOptionStorage[
                                            static_cast<size_t>(selectedAnimation)]);
                }
            } else {
                engine::Text(
                        ui, config, assets,
                        Rectangle{fieldX, y, fieldW, RowHeight},
                        smallFont,
                        action.animation.empty()
                                ? "<Unassigned>"
                                : action.animation.c_str(),
                        engine::UITextJustify::Left,
                        config.mutedTextColor);
            }
            y += RowHeight + 2.0f;

            const bool missingAnimation = !action.animation.empty()
                    && editor.SelectedModelReady(assets)
                    && !editor.SelectedAnimationExists(assets, action.animation);
            const char* mappingMessage = action.animation.empty()
                    ? "Unassigned (allowed)"
                    : (missingAnimation
                            ? "Saved animation is missing from this model"
                            : "");
            engine::Text(
                    ui, config, assets,
                    Rectangle{fieldX, y, fieldW, 28.0f},
                    smallFont, mappingMessage,
                    engine::UITextJustify::Left,
                    missingAnimation ? config.invalidColor : config.mutedTextColor);
            y += 30.0f;

            drawLabel("Animation speed");
            float animationSpeed = action.animationSpeed;
            const std::string speedId = std::string{"sector_editor_npc_animation_speed_"}
                    + metadata.jsonKey;
            const engine::UINumericInputResult speedResult = engine::FloatInput(
                    ui, config, input, assets,
                    speedId.c_str(),
                    Rectangle{fieldX, y, 190.0f, RowHeight},
                    font,
                    animationSpeed,
                    state.animationSpeedInputs[static_cast<size_t>(metadata.action)],
                    0.01f,
                    10.0f,
                    3);
            if (speedResult.changed) {
                editor.SetSelectedAnimationSpeed(metadata.action, animationSpeed);
            }
            y += RowHeight + RowGap;

            if (metadata.hasMovementSpeed) {
                drawLabel("Movement speed");
                float movementSpeed = action.movementSpeed;
                const std::string movementId =
                        std::string{"sector_editor_npc_movement_speed_"}
                        + metadata.jsonKey;
                const engine::UINumericInputResult movementResult = engine::FloatInput(
                        ui, config, input, assets,
                        movementId.c_str(),
                        Rectangle{fieldX, y, 190.0f, RowHeight},
                        font,
                        movementSpeed,
                        state.movementSpeedInputs[static_cast<size_t>(metadata.action)],
                        0.1f,
                        200.0f,
                        2);
                if (movementResult.changed) {
                    editor.SetSelectedMovementSpeed(metadata.action, movementSpeed);
                }
                y += RowHeight + RowGap;
            }
        }
        engine::EndScrollArea(
                ui, config, input, formScroll, editor.Session().formScroll);
    }

    std::string message = state.validationMessage;
    if (message.empty()) message = state.warningMessage;
    if (message.empty() && !state.catalogErrors.empty()) {
        message = state.catalogErrors.front().path + ": "
                + state.catalogErrors.front().message;
    }
    engine::Text(
            config, assets,
            Rectangle{
                    layout.listPane.x,
                    layout.modal.y + layout.modal.height - 66.0f,
                    layout.saveButton.x - layout.listPane.x - 18.0f,
                    44.0f},
            smallFont,
            message.c_str(),
            engine::UITextJustify::Left,
            message.empty() ? config.mutedTextColor : config.invalidColor,
            true);

    if (engine::Button(
                ui, config, input, assets,
                "sector_editor_npc_save",
                layout.saveButton,
                font, "Save")) {
        if (editor.SaveAndClose(&assets)) {
            ConsumeRemainingInput(input);
            return SectorEditorNpcEditorModalResult::Saved;
        }
    }
    if (engine::Button(
                ui, config, input, assets,
                "sector_editor_npc_cancel",
                layout.cancelButton,
                font, "Cancel")) {
        editor.Cancel(&assets);
        ConsumeRemainingInput(input);
        return SectorEditorNpcEditorModalResult::Cancelled;
    }
    ConsumeRemainingInput(input);
    return SectorEditorNpcEditorModalResult::None;
}

} // namespace game
