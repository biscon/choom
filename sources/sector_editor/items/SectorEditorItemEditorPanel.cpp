#include "sector_editor/items/SectorEditorItemEditorPanel.h"

#include "engine/input/InputEvents.h"
#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/services/static_model_picker/SectorEditorModelPickerModal.h"

#include <algorithm>
#include <iterator>

namespace game {
namespace {

constexpr float RowHeight = 40.0f;
constexpr float RowGap = 10.0f;

float ScrollContentWidth(float width, const engine::UIConfig& config)
{
    return std::max(0.0f,
            width - config.borderThickness * 2.0f - config.scrollbarSize
                    - engine::DefaultScrollAreaPaddingPx * 2.0f);
}

} // namespace

SectorEditorItemEditorPanelResult DrawSectorEditorItemEditorPanel(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont,
        SectorEditorItemEditorService& editor,
        SectorEditorStaticModelPickerService& modelPicker)
{
    SectorEditorItemEditorPanelResult result;
    SectorEditorItemEditorState& state = editor.State();
    if (!state.open) return result;

    if (modelPicker.State().open
            && modelPicker.State().target == ModelPickerTarget::ItemDefinition) {
        const SectorEditorModelPickerModalResult pickerResult =
                DrawSectorEditorModelPickerModal(
                        ui, config, input, assets, font, modelPicker);
        if (pickerResult == SectorEditorModelPickerModalResult::Selected) {
            editor.SetModelPath(modelPicker.SelectedModelPath());
            modelPicker.State().open = false;
        }
        return result;
    }

    bool cancelRequested = false;
    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [&cancelRequested, &editor, &state](engine::InputEvent& event) {
                if (event.key.key != KEY_ESCAPE) return;
                if (state.deleteConfirmationOpen) editor.CancelDelete();
                else cancelRequested = true;
                engine::ConsumeEvent(event);
            });

    const SectorEditorItemEditorLayout layout =
            BuildSectorEditorItemEditorLayoutForViewport(
                    EditorWidth, EditorHeight);
    DrawRectangle(0, 0, static_cast<int>(EditorWidth),
            static_cast<int>(EditorHeight), Color{0, 0, 0, 150});
    DrawRectangleRec(layout.panel, Color{20, 24, 32, 250});
    DrawRectangleLinesEx(
            layout.panel, config.borderThickness, config.borderColor);
    engine::Text(
            config, assets,
            Rectangle{layout.panel.x + 20.0f, layout.panel.y + 14.0f,
                    layout.panel.width - 40.0f, 40.0f},
            font, "Item Editor");

    const float listWidth = ScrollContentWidth(layout.listBounds.width, config);
    const Vector2 listSize{
            listWidth,
            std::max(layout.listBounds.height,
                    config.listItemHeight
                            * static_cast<float>(state.listLabels.size()))};
    engine::UIScrollAreaResult listScroll = engine::BeginScrollArea(
            ui, config, input, "sector_editor_item_list_scroll",
            layout.listBounds, listSize, editor.Session().listScroll);
    if (!state.listLabels.empty()) {
        int selected = state.selectedIndex;
        engine::List(
                ui, config, input, assets, "sector_editor_item_list",
                Rectangle{0.0f, 0.0f, listScroll.viewport.width, listSize.y},
                smallFont, state.listLabels.data(), state.listLabels.size(),
                selected);
        if (selected != state.selectedIndex) editor.SelectIndex(selected);
    }
    engine::EndScrollArea(
            ui, config, input, listScroll, editor.Session().listScroll);

    if (engine::Button(
                ui, config, input, assets, "sector_editor_item_add",
                layout.addButton, smallFont, "Add")) {
        editor.AddItem();
    }
    if (engine::Button(
                ui, config, input, assets, "sector_editor_item_delete",
                layout.deleteButton, smallFont, "Delete")) {
        editor.RequestDeleteSelected();
    }

    ItemDefinition* definition = editor.SelectedItem();
    if (definition == nullptr) {
        engine::Text(
                config, assets,
                Rectangle{layout.formBounds.x + 18.0f,
                        layout.formBounds.y + 18.0f,
                        layout.formBounds.width - 36.0f, 60.0f},
                font, "No item definitions are available.",
                engine::UITextJustify::Left, config.mutedTextColor, true);
    } else {
        const float contentWidth = ScrollContentWidth(
                layout.formBounds.width, config);
        const float contentHeight = definition->type == ItemType::Health
                        && definition->healOverTime
                ? 810.0f : 760.0f;
        engine::UIScrollAreaResult formScroll = engine::BeginScrollArea(
                ui, config, input, "sector_editor_item_form_scroll",
                layout.formBounds,
                Vector2{contentWidth, contentHeight},
                editor.Session().formScroll);
        float y = 0.0f;
        const float labelWidth = std::min(
                180.0f, formScroll.viewport.width * 0.32f);
        const float fieldX = labelWidth + 12.0f;
        const float fieldWidth = std::max(
                100.0f, formScroll.viewport.width - fieldX);
        const auto label = [&](const char* text, float height = RowHeight) {
            engine::Text(
                    ui, config, assets,
                    Rectangle{0.0f, y, labelWidth, height},
                    smallFont, text, engine::UITextJustify::Left,
                    config.mutedTextColor);
        };

        label("ID");
        engine::Text(
                ui, config, assets,
                Rectangle{fieldX, y, fieldWidth, RowHeight},
                smallFont, definition->id.c_str(), engine::UITextJustify::Left,
                config.mutedTextColor);
        y += RowHeight + RowGap;

        label("Title");
        const engine::UITextInputResult titleResult = engine::TextInput(
                ui, config, input, assets, "sector_editor_item_title",
                Rectangle{fieldX, y, fieldWidth, RowHeight}, smallFont,
                state.titleBuffer.data(), state.titleBuffer.size(), 1,
                kMaximumItemTitleCodepoints);
        if (titleResult.changed || titleResult.focusLost
                || titleResult.submitted) {
            editor.ApplyTitleBuffer();
        }
        y += RowHeight + RowGap;

        constexpr float DescriptionHeight = 160.0f;
        label("Description", DescriptionHeight);
        const engine::UITextInputResult descriptionResult = engine::TextArea(
                ui, config, input, assets, "sector_editor_item_description",
                Rectangle{fieldX, y, fieldWidth, DescriptionHeight}, smallFont,
                state.descriptionBuffer.data(), state.descriptionBuffer.size(),
                1, kMaximumItemDescriptionCodepoints);
        if (descriptionResult.changed || descriptionResult.focusLost) {
            editor.ApplyDescriptionBuffer();
        }
        y += DescriptionHeight + RowGap;

        label("Model");
        constexpr float PickWidth = 90.0f;
        const float modelWidth = std::max(
                80.0f, fieldWidth - PickWidth - 6.0f);
        const engine::UITextInputResult modelResult = engine::TextInput(
                ui, config, input, assets, "sector_editor_item_model",
                Rectangle{fieldX, y, modelWidth, RowHeight}, smallFont,
                state.modelPathBuffer.data(), state.modelPathBuffer.size(),
                1, state.modelPathBuffer.size() - 1);
        if (modelResult.changed || modelResult.focusLost
                || modelResult.submitted) {
            editor.ApplyModelPathBuffer();
        }
        if (engine::Button(
                    ui, config, input, assets,
                    "sector_editor_item_model_pick",
                    Rectangle{fieldX + modelWidth + 6.0f, y,
                            PickWidth, RowHeight},
                    smallFont, "Pick")) {
            modelPicker.Open(
                    definition->modelPath, ModelPickerTarget::ItemDefinition);
        }
        y += RowHeight + RowGap;

        label("Type");
        const char* const typeOptions[] = {
                "Object", "Weapon", "Ammo", "Health"};
        int typeIndex = static_cast<int>(definition->type);
        if (engine::Option(
                    ui, config, input, assets, "sector_editor_item_type",
                    Rectangle{fieldX, y, fieldWidth, RowHeight}, smallFont,
                    typeOptions, std::size(typeOptions), typeIndex)) {
            editor.SetType(static_cast<ItemType>(typeIndex));
            definition = editor.SelectedItem();
        }
        y += RowHeight + RowGap;

        label("Weight (kg)");
        if (engine::FloatInput(
                    ui, config, input, assets, "sector_editor_item_weight",
                    Rectangle{fieldX, y, fieldWidth, RowHeight}, smallFont,
                    definition->weightKg, state.weightInput,
                    0.0f, 1000000.0f, 3).changed) {
            state.validationMessage.clear();
        }
        y += RowHeight + RowGap;

        label("Max stack size");
        if (engine::IntInput(
                    ui, config, input, assets,
                    "sector_editor_item_max_stack_size",
                    Rectangle{fieldX, y, fieldWidth, RowHeight}, smallFont,
                    definition->maxStackSize, state.maxStackSizeInput,
                    1, kMaximumItemStackSize, 1).changed) {
            state.validationMessage.clear();
        }
        y += RowHeight + RowGap;

        if (definition->type == ItemType::Weapon
                || definition->type == ItemType::Ammo) {
            label("Weapon");
            int selectedWeapon = editor.SelectedWeaponIndex();
            if (!state.weaponLabels.empty()
                    && engine::Option(
                            ui, config, input, assets,
                            "sector_editor_item_weapon",
                            Rectangle{fieldX, y, fieldWidth, RowHeight},
                            smallFont, state.weaponLabels.data(),
                            state.weaponLabels.size(), selectedWeapon)) {
                editor.SetWeaponIndex(selectedWeapon);
            }
            y += RowHeight + RowGap;
        } else if (definition->type == ItemType::Health) {
            label("Heal amount");
            if (engine::IntInput(
                        ui, config, input, assets,
                        "sector_editor_item_heal_amount",
                        Rectangle{fieldX, y, fieldWidth, RowHeight}, smallFont,
                        definition->healAmount, state.healAmountInput,
                        1, 1000000, 1).changed) {
                state.validationMessage.clear();
            }
            y += RowHeight + RowGap;
            label("Heal over time");
            if (engine::Checkbox(
                        ui, config, input, assets,
                        "sector_editor_item_heal_over_time",
                        Rectangle{fieldX, y, fieldWidth, RowHeight}, smallFont,
                        "Enabled", definition->healOverTime)) {
                if (definition->healOverTime
                        && definition->healDurationSeconds <= 0.0f) {
                    definition->healDurationSeconds = 1.0f;
                }
                state.validationMessage.clear();
            }
            y += RowHeight + RowGap;
            if (definition->healOverTime) {
                label("Duration (seconds)");
                if (engine::FloatInput(
                            ui, config, input, assets,
                            "sector_editor_item_heal_duration",
                            Rectangle{fieldX, y, fieldWidth, RowHeight},
                            smallFont, definition->healDurationSeconds,
                            state.healDurationInput,
                            0.001f, 1000000.0f, 3).changed) {
                    state.validationMessage.clear();
                }
                y += RowHeight + RowGap;
            }
        }
        engine::EndScrollArea(
                ui, config, input, formScroll, editor.Session().formScroll);
    }

    if (!state.validationMessage.empty()) {
        engine::Text(
                config, assets, layout.validationMessage, smallFont,
                state.validationMessage.c_str(), engine::UITextJustify::Left,
                config.invalidColor, true);
    }
    bool saveRequested = engine::Button(
            ui, config, input, assets, "sector_editor_item_save",
            layout.saveButton, font, "Save");
    cancelRequested = cancelRequested || engine::Button(
            ui, config, input, assets, "sector_editor_item_cancel",
            layout.cancelButton, font, "Cancel");

    if (state.deleteConfirmationOpen) {
        const Rectangle confirmation{
                layout.panel.x + (layout.panel.width - 520.0f) * 0.5f,
                layout.panel.y + (layout.panel.height - 230.0f) * 0.5f,
                520.0f,
                230.0f};
        DrawRectangleRec(confirmation, Color{24, 28, 38, 255});
        DrawRectangleLinesEx(
                confirmation, config.borderThickness, config.borderColor);
        const std::string message = "Delete item '"
                + state.deleteConfirmationId + "'?";
        engine::Text(
                config, assets,
                Rectangle{confirmation.x + 22.0f, confirmation.y + 28.0f,
                        confirmation.width - 44.0f, 80.0f},
                font, message.c_str(), engine::UITextJustify::Left,
                config.textColor, true);
        if (engine::Button(
                    ui, config, input, assets,
                    "sector_editor_item_confirm_delete",
                    Rectangle{confirmation.x + confirmation.width - 300.0f,
                            confirmation.y + confirmation.height - 62.0f,
                            130.0f, 42.0f},
                    smallFont, "Delete")) {
            editor.ConfirmDeleteSelected();
        }
        if (engine::Button(
                    ui, config, input, assets,
                    "sector_editor_item_cancel_delete",
                    Rectangle{confirmation.x + confirmation.width - 152.0f,
                            confirmation.y + confirmation.height - 62.0f,
                            130.0f, 42.0f},
                    smallFont, "Cancel")) {
            editor.CancelDelete();
        }
        saveRequested = false;
        cancelRequested = false;
    }

    if (saveRequested) result.saved = editor.SaveAndClose();
    if (cancelRequested) {
        editor.Cancel();
        result.cancelled = true;
    }
    input.ForEachEvent(
            engine::InputEventType::Any,
            true,
            [](engine::InputEvent& event) { engine::ConsumeEvent(event); });
    return result;
}

} // namespace game
