#include "sector_editor/sounds/SectorEditorSoundEditorPanel.h"

#include "engine/input/InputEvents.h"
#include "sector_editor/SectorEditorHelpers.h"

#include <algorithm>

namespace game {
namespace {

constexpr float RowHeight = 42.0f;
constexpr float RowGap = 10.0f;

float ScrollContentWidth(float width, const engine::UIConfig& config)
{
    const float client = std::max(0.0f, width - config.borderThickness * 2.0f);
    return std::max(0.0f, client - config.scrollbarSize
            - engine::DefaultScrollAreaPaddingPx * 2.0f);
}

Rectangle ScrollBounds(const engine::UIContext& ui, Rectangle bounds)
{
    if (!ui.inScrollArea) return bounds;
    bounds.x += ui.scrollViewport.x - ui.scrollOffset.x;
    bounds.y += ui.scrollViewport.y - ui.scrollOffset.y;
    return bounds;
}

void ConsumeRemainingInput(engine::Input& input)
{
    input.ForEachEvent(engine::InputEventType::Any, true,
            [](engine::InputEvent& event) { engine::ConsumeEvent(event); });
}

void DrawDeleteConfirmation(
        engine::UIContext& ui, const engine::UIConfig& config,
        engine::Input& input, engine::AssetManager& assets,
        engine::FontHandle font, SectorEditorSoundEditorService& editor,
        Rectangle modal)
{
    const Rectangle popup{modal.x + 300.0f, modal.y + 245.0f, 600.0f, 240.0f};
    DrawRectangleRec(popup, Color{27, 32, 42, 255});
    DrawRectangleLinesEx(popup, config.borderThickness, config.borderColor);
    engine::Text(config, assets,
            {popup.x + 24.0f, popup.y + 20.0f, popup.width - 48.0f, 38.0f},
            font, "Remove map sound?");
    const std::string message = "Remove '" + editor.State().deleteConfirmationId
            + "' from this level? The audio file will not be deleted.";
    engine::Text(config, assets,
            {popup.x + 24.0f, popup.y + 70.0f, popup.width - 48.0f, 72.0f},
            font, message.c_str(), engine::UITextJustify::Left,
            config.textColor, true);
    if (engine::Button(ui, config, input, assets, "sector_editor_sound_delete_confirm",
                {popup.x + popup.width - 310.0f, popup.y + 168.0f, 130.0f, 44.0f},
                font, "Remove")) {
        editor.ConfirmDeleteSelected();
    }
    if (engine::Button(ui, config, input, assets, "sector_editor_sound_delete_cancel",
                {popup.x + popup.width - 160.0f, popup.y + 168.0f, 130.0f, 44.0f},
                font, "Cancel")) {
        editor.CancelDelete();
    }
}

} // namespace

SectorEditorSoundEditorPanelResult DrawSectorEditorSoundEditorPanel(
        engine::UIContext& ui, const engine::UIConfig& config,
        engine::Input& input, engine::AssetManager& assets,
        engine::FontHandle font, engine::FontHandle smallFont,
        SectorEditorSoundEditorService& editor,
        SectorEditorAudioAssetPickerService& audioPicker)
{
    SectorEditorSoundEditorState& state = editor.State();
    if (!state.open) return SectorEditorSoundEditorPanelResult::None;
    const SectorEditorSoundEditorLayout layout =
            BuildSectorEditorSoundEditorLayoutForViewport(EditorWidth, EditorHeight);

    if (state.assetPicker.open) {
        const SectorEditorAudioAssetPickerResult result = audioPicker.DrawModal(
                ui, config, input, font, state.assetPicker);
        if (result == SectorEditorAudioAssetPickerResult::Selected) {
            editor.SetSelectedPath(audioPicker.SelectedPath(state.assetPicker));
            audioPicker.Close(state.assetPicker);
        } else if (result == SectorEditorAudioAssetPickerResult::Cancelled) {
            audioPicker.Close(state.assetPicker);
        }
        return SectorEditorSoundEditorPanelResult::None;
    }

    bool cancelRequested = false;
    input.ForEachEvent(engine::InputEventType::KeyPressed, true,
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
        editor.Cancel();
        ConsumeRemainingInput(input);
        return SectorEditorSoundEditorPanelResult::Cancelled;
    }

    DrawRectangle(0, 0, static_cast<int>(EditorWidth),
            static_cast<int>(EditorHeight), Color{0, 0, 0, 150});
    DrawRectangleRec(layout.modal, Color{20, 24, 32, 250});
    DrawRectangleLinesEx(layout.modal, config.borderThickness, config.borderColor);
    engine::Text(config, assets,
            {layout.modal.x + 22.0f, layout.modal.y + 14.0f,
                    layout.modal.width - 44.0f, 42.0f},
            font, "Sound Editor");

    if (state.deleteConfirmationOpen) {
        DrawDeleteConfirmation(ui, config, input, assets, font, editor, layout.modal);
        ConsumeRemainingInput(input);
        return SectorEditorSoundEditorPanelResult::None;
    }

    const Vector2 listContentSize{
            ScrollContentWidth(layout.listBounds.width, config),
            std::max(layout.listBounds.height,
                    config.listItemHeight * static_cast<float>(state.listLabels.size()))};
    engine::UIScrollAreaResult listScroll = engine::BeginScrollArea(
            ui, config, input, "sector_editor_sound_editor_list_scroll",
            layout.listBounds, listContentSize, state.listScroll);
    if (!state.listLabels.empty()) {
        int selectedIndex = state.selectedIndex;
        engine::List(ui, config, input, assets, "sector_editor_sound_editor_list",
                {0.0f, 0.0f, listScroll.viewport.width, listContentSize.y},
                smallFont, state.listLabels.data(), state.listLabels.size(), selectedIndex);
        if (selectedIndex != state.selectedIndex && editor.SelectIndex(selectedIndex)) {
            ui.focusedId = 0;
            ui.openOptionId = 0;
        }
    }
    engine::EndScrollArea(ui, config, input, listScroll, state.listScroll);

    if (engine::Button(ui, config, input, assets, "sector_editor_sound_editor_add",
                layout.addButton, font, "Add")) {
        editor.AddSound();
        ui.focusedId = 0;
    }
    if (engine::Button(ui, config, input, assets, "sector_editor_sound_editor_remove",
                layout.deleteButton, font, "Remove")) {
        editor.RequestDeleteSelected();
    }

    SectorEditorSoundDraft* draft = editor.SelectedDraft();
    if (draft == nullptr) {
        engine::Text(config, assets,
                {layout.formBounds.x + 20.0f, layout.formBounds.y + 20.0f,
                        layout.formBounds.width - 40.0f, 80.0f},
                font, "No map sounds. Press Add to create one.",
                engine::UITextJustify::Left, config.mutedTextColor, true);
    } else {
        const bool referenced = editor.SelectedIsReferenced();
        const float contentWidth = ScrollContentWidth(layout.formBounds.width, config);
        const size_t usageLineCount = state.usageText.empty()
                ? 0 : 1 + static_cast<size_t>(std::count(
                        state.usageText.begin(), state.usageText.end(), '\n'));
        const float usageHeight = std::max(
                120.0f, static_cast<float>(usageLineCount) * 42.0f);
        const float contentHeight = referenced ? 400.0f + usageHeight : 410.0f;
        engine::UIScrollAreaResult formScroll = engine::BeginScrollArea(
                ui, config, input, "sector_editor_sound_editor_form_scroll",
                layout.formBounds, {contentWidth, contentHeight}, state.formScroll);
        float y = 4.0f;
        constexpr float LabelWidth = 130.0f;

        engine::Text(ui, config, assets, {0.0f, y, LabelWidth, RowHeight},
                smallFont, "Sound ID", engine::UITextJustify::Left,
                config.mutedTextColor);
        if (referenced) {
            DrawRectangleRec(ScrollBounds(ui,
                    {LabelWidth, y, formScroll.viewport.width - LabelWidth, RowHeight}),
                    config.disabledColor);
            engine::Text(ui, config, assets,
                    {LabelWidth + 8.0f, y, formScroll.viewport.width - LabelWidth - 16.0f, RowHeight},
                    smallFont, draft->definition.id.c_str(), engine::UITextJustify::Left,
                    config.mutedTextColor);
        } else {
            const engine::UITextInputResult idResult = engine::TextInput(
                    ui, config, input, assets, "sector_editor_sound_editor_id",
                    {LabelWidth, y, formScroll.viewport.width - LabelWidth, RowHeight},
                    smallFont, state.idBuffer, sizeof(state.idBuffer), 0,
                    sizeof(state.idBuffer) - 1);
            if (idResult.submitted) editor.ApplyIdBuffer();
        }
        y += RowHeight + RowGap;

        engine::Text(ui, config, assets, {0.0f, y, LabelWidth, RowHeight},
                smallFont, "Audio file", engine::UITextJustify::Left,
                config.mutedTextColor);
        const float pickerWidth = 120.0f;
        engine::Text(ui, config, assets,
                {LabelWidth, y, formScroll.viewport.width - LabelWidth - pickerWidth - 8.0f,
                        RowHeight * 1.6f},
                smallFont, draft->definition.path.empty() ? "<none>"
                        : draft->definition.path.c_str(),
                engine::UITextJustify::Left, config.textColor, true);
        if (engine::Button(ui, config, input, assets,
                    "sector_editor_sound_editor_pick_file",
                    {formScroll.viewport.width - pickerWidth, y, pickerWidth, RowHeight},
                    smallFont, "Pick File")) {
            audioPicker.Open(state.assetPicker, "Choose Audio File",
                    draft->definition.path, draft->definition.type);
        }
        y += RowHeight * 1.7f + RowGap;

        engine::Text(ui, config, assets, {0.0f, y, LabelWidth, RowHeight},
                smallFont, "Type", engine::UITextJustify::Left,
                config.mutedTextColor);
        const float typeWidth = (formScroll.viewport.width - LabelWidth - 8.0f) * 0.5f;
        if (engine::ToolButton(ui, config, input, assets,
                    "sector_editor_sound_editor_type_sound",
                    {LabelWidth, y, typeWidth, RowHeight}, smallFont, "Sound",
                    draft->definition.type == SectorSoundType::Sound)) {
            editor.SetSelectedType(SectorSoundType::Sound);
        }
        if (engine::ToolButton(ui, config, input, assets,
                    "sector_editor_sound_editor_type_music",
                    {LabelWidth + typeWidth + 8.0f, y, typeWidth, RowHeight},
                    smallFont, "Music", draft->definition.type == SectorSoundType::Music)) {
            editor.SetSelectedType(SectorSoundType::Music);
        }
        y += RowHeight + RowGap;

        engine::Text(ui, config, assets, {0.0f, y, formScroll.viewport.width, 34.0f},
                smallFont,
                draft->definition.type == SectorSoundType::Music
                        ? "Music is streamed and is used for roomtones."
                        : "Sound is buffered and is used for emitters, doors, and scripts.",
                engine::UITextJustify::Left, config.mutedTextColor, true);
        y += 44.0f;

        if (referenced) {
            engine::Text(ui, config, assets, {0.0f, y, formScroll.viewport.width, 30.0f},
                    smallFont, "Referenced by", engine::UITextJustify::Left,
                    config.mutedTextColor);
            y += 30.0f;
            const Rectangle usageBounds = ScrollBounds(
                    ui, {0.0f, y, formScroll.viewport.width, usageHeight});
            DrawRectangleRec(usageBounds, Color{15, 18, 24, 255});
            DrawRectangleLinesEx(
                    usageBounds, config.borderThickness, config.borderColor);
            engine::Text(ui, config, assets,
                    {8.0f, y + 6.0f, formScroll.viewport.width - 16.0f, usageHeight - 12.0f},
                    smallFont, state.usageText.c_str(), engine::UITextJustify::Left,
                    config.textColor, true);
            y += usageHeight + RowGap;
            engine::Text(ui, config, assets, {0.0f, y, formScroll.viewport.width, 42.0f},
                    smallFont, "ID, type, and removal are locked while referenced. The file may be replaced.",
                    engine::UITextJustify::Left, config.mutedTextColor, true);
            y += 50.0f;
        }
        if (!state.validationMessage.empty()) {
            engine::Text(ui, config, assets, {0.0f, y, formScroll.viewport.width, 54.0f},
                    smallFont, state.validationMessage.c_str(),
                    engine::UITextJustify::Left, config.invalidColor, true);
        }
        engine::EndScrollArea(ui, config, input, formScroll, state.formScroll);
    }

    if (engine::Button(ui, config, input, assets, "sector_editor_sound_editor_save",
                layout.saveButton, font, "Save")) {
        if (editor.SaveAndClose()) {
            ConsumeRemainingInput(input);
            return SectorEditorSoundEditorPanelResult::Saved;
        }
    }
    if (engine::Button(ui, config, input, assets, "sector_editor_sound_editor_cancel",
                layout.cancelButton, font, "Cancel")) {
        editor.Cancel();
        ConsumeRemainingInput(input);
        return SectorEditorSoundEditorPanelResult::Cancelled;
    }
    ConsumeRemainingInput(input);
    return SectorEditorSoundEditorPanelResult::None;
}

} // namespace game
