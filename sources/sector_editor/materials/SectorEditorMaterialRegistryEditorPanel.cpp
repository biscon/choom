#include "sector_editor/materials/SectorEditorMaterialRegistryEditorPanel.h"

#include "engine/input/InputEvents.h"
#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/SectorEditorUiHelpers.h"
#include "sector_demo/SectorTextureTypes.h"

#include <algorithm>

namespace game {
namespace {

constexpr float RowHeight = 40.0f;
constexpr float Gap = 9.0f;

float ScrollContentWidth(float width, const engine::UIConfig& config)
{
    return std::max(0.0f, width - config.borderThickness * 2.0f
            - config.scrollbarSize - engine::DefaultScrollAreaPaddingPx * 2.0f);
}

void DrawAlbedoPickerModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont,
        SectorEditorMaterialRegistryEditorService& editor)
{
    SectorEditorMaterialAlbedoPickerState& picker = editor.State().albedoPicker;
    bool cancelRequested = false;
    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [&](engine::InputEvent& event) {
                if (event.key.key != KEY_ESCAPE) return;
                cancelRequested = true;
                engine::ConsumeEvent(event);
            });
    if (cancelRequested) {
        editor.CancelAlbedoPicker(&assets);
        return;
    }

    DrawRectangle(0, 0, static_cast<int>(EditorWidth),
            static_cast<int>(EditorHeight), Color{0, 0, 0, 150});
    const Rectangle modal{
            (EditorWidth - 1120.0f) * 0.5f,
            (EditorHeight - 700.0f) * 0.5f,
            1120.0f,
            700.0f};
    DrawRectangleRec(modal, Color{20, 24, 32, 250});
    DrawRectangleLinesEx(modal, config.borderThickness, config.borderColor);
    engine::Text(config, assets,
            Rectangle{modal.x + 22.0f, modal.y + 16.0f, modal.width - 44.0f, 38.0f},
            font, "Choose Albedo PNG");

    const float leftX = modal.x + 22.0f;
    const float leftWidth = 555.0f;
    const Rectangle filterBounds{leftX, modal.y + 68.0f, leftWidth, 42.0f};
    const float filterLabelWidth = 82.0f;
    engine::Text(config, assets,
            Rectangle{filterBounds.x, filterBounds.y,
                    filterLabelWidth, filterBounds.height},
            smallFont, "Filter", engine::UITextJustify::Left,
            config.mutedTextColor);
    const engine::UITextInputResult filterResult = engine::TextInput(
            ui, config, input, assets,
            "sector_editor_material_albedo_picker_filter",
            Rectangle{filterBounds.x + filterLabelWidth, filterBounds.y,
                    filterBounds.width - filterLabelWidth, filterBounds.height},
            smallFont,
            picker.filterBuffer, sizeof(picker.filterBuffer),
            0, sizeof(picker.filterBuffer) - 1);
    if (filterResult.changed) editor.ApplyAlbedoPickerFilter();

    const Rectangle listBounds{leftX, modal.y + 122.0f, leftWidth, 484.0f};
    if (picker.scrollSelectionIntoView && picker.selectedFilteredIndex >= 0) {
        picker.scroll.offset.y = config.listItemHeight
                * static_cast<float>(picker.selectedFilteredIndex);
        picker.scrollSelectionIntoView = false;
    }
    const Vector2 listContentSize{
            ScrollContentWidth(listBounds.width, config),
            std::max(listBounds.height,
                    config.listItemHeight
                            * static_cast<float>(picker.listLabels.size()))};
    engine::UIScrollAreaResult listScroll = engine::BeginScrollArea(
            ui, config, input,
            "sector_editor_material_albedo_picker_scroll",
            listBounds, listContentSize, picker.scroll);
    if (!picker.listLabels.empty()) {
        int selected = picker.selectedFilteredIndex;
        engine::List(
                ui, config, input, assets,
                "sector_editor_material_albedo_picker_list",
                Rectangle{0.0f, 0.0f, listScroll.viewport.width, listContentSize.y},
                smallFont,
                picker.listLabels.data(), picker.listLabels.size(), selected);
        if (selected != picker.selectedFilteredIndex) {
            editor.SelectAlbedoPickerIndex(selected);
        }
    }
    engine::EndScrollArea(ui, config, input, listScroll, picker.scroll);

    const std::string listMessage = !picker.selectionMessage.empty()
            ? picker.selectionMessage
            : (!picker.scanMessage.empty()
                    ? picker.scanMessage
                    : "Found " + std::to_string(picker.paths.size())
                            + " albedo PNG files");
    engine::Text(config, assets,
            Rectangle{leftX, listBounds.y + listBounds.height + 8.0f,
                    leftWidth, 34.0f},
            smallFont, listMessage.c_str(), engine::UITextJustify::Left,
            picker.listLabels.empty()
                    ? config.invalidColor : config.mutedTextColor, true);

    editor.EnsureAlbedoPickerPreview(assets);
    const float rightX = modal.x + 607.0f;
    const float rightWidth = modal.x + modal.width - 22.0f - rightX;
    engine::Text(config, assets,
            Rectangle{rightX, modal.y + 68.0f, rightWidth, 38.0f},
            font, "Preview", engine::UITextJustify::Left, config.textColor);
    engine::Image(config, assets,
            Rectangle{rightX, modal.y + 114.0f, rightWidth, 360.0f},
            picker.previewTexture);
    const std::string selectedLabel = EditorAssetPathDisplayLabel(
            editor.SelectedAlbedoPickerPath(), "assets/images/");
    engine::Text(config, assets,
            Rectangle{rightX, modal.y + 488.0f, rightWidth, 92.0f},
            smallFont,
            selectedLabel.empty() ? "<no selection>" : selectedLabel.c_str(),
            engine::UITextJustify::Left,
            selectedLabel.empty() ? config.mutedTextColor : config.textColor,
            true);

    const float buttonY = modal.y + modal.height - 64.0f;
    if (engine::Button(
                ui, config, input, assets,
                "sector_editor_material_albedo_picker_okay",
                Rectangle{modal.x + modal.width - 334.0f, buttonY, 150.0f, 44.0f},
                smallFont, "Okay")) {
        editor.ConfirmAlbedoPicker(assets);
    }
    if (engine::Button(
                ui, config, input, assets,
                "sector_editor_material_albedo_picker_cancel",
                Rectangle{modal.x + modal.width - 172.0f, buttonY, 150.0f, 44.0f},
                smallFont, "Cancel")) {
        editor.CancelAlbedoPicker(&assets);
    }

    input.ForEachEvent(
            engine::InputEventType::Any,
            true,
            [](engine::InputEvent& event) { engine::ConsumeEvent(event); });
}

} // namespace

SectorEditorMaterialRegistryEditorResult DrawSectorEditorMaterialRegistryEditor(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont,
        SectorEditorMaterialRegistryEditorService& editor)
{
    SectorEditorMaterialRegistryEditorState& state = editor.State();
    if (!state.open) return SectorEditorMaterialRegistryEditorResult::None;
    if (state.albedoPicker.open) {
        DrawAlbedoPickerModal(
                ui, config, input, assets, font, smallFont, editor);
        return SectorEditorMaterialRegistryEditorResult::None;
    }

    bool cancelRequested = false;
    input.ForEachEvent(
            engine::InputEventType::KeyPressed,
            true,
            [&](engine::InputEvent& event) {
                if (event.key.key != KEY_ESCAPE) return;
                if (state.deleteConfirmationOpen) editor.CancelDelete();
                else cancelRequested = true;
                engine::ConsumeEvent(event);
            });

    DrawRectangle(0, 0, static_cast<int>(EditorWidth),
            static_cast<int>(EditorHeight), Color{0, 0, 0, 150});
    const Rectangle panel{
            (EditorWidth - 1320.0f) * 0.5f,
            (EditorHeight - 860.0f) * 0.5f,
            1320.0f,
            860.0f};
    DrawRectangleRec(panel, Color{20, 24, 32, 250});
    DrawRectangleLinesEx(panel, config.borderThickness, config.borderColor);
    engine::Text(config, assets,
            Rectangle{panel.x + 20.0f, panel.y + 14.0f, panel.width - 40.0f, 40.0f},
            font, "Material Editor");

    const Rectangle listBounds{panel.x + 20.0f, panel.y + 66.0f, 300.0f, 660.0f};
    const float listWidth = ScrollContentWidth(listBounds.width, config);
    const Vector2 listContentSize{
            listWidth,
            std::max(listBounds.height,
                    config.listItemHeight * static_cast<float>(state.listLabels.size()))};
    engine::UIScrollAreaResult listScroll = engine::BeginScrollArea(
            ui, config, input, "sector_editor_material_registry_list_scroll",
            listBounds, listContentSize, state.listScroll);
    if (!state.listLabels.empty()) {
        int selected = state.selectedIndex;
        engine::List(ui, config, input, assets,
                "sector_editor_material_registry_list",
                Rectangle{0.0f, 0.0f, listScroll.viewport.width, listContentSize.y},
                smallFont, state.listLabels.data(), state.listLabels.size(), selected);
        if (selected != state.selectedIndex) editor.SelectIndex(selected);
    }
    engine::EndScrollArea(ui, config, input, listScroll, state.listScroll);

    if (engine::Button(ui, config, input, assets,
                "sector_editor_material_registry_add",
                Rectangle{listBounds.x, listBounds.y + listBounds.height + 10.0f,
                        145.0f, RowHeight}, smallFont, "Add")) {
        editor.AddMaterial();
    }
    if (engine::Button(ui, config, input, assets,
                "sector_editor_material_registry_delete",
                Rectangle{listBounds.x + 155.0f, listBounds.y + listBounds.height + 10.0f,
                        145.0f, RowHeight}, smallFont, "Delete")) {
        editor.RequestDeleteSelected();
    }

    const Rectangle formBounds{
            listBounds.x + listBounds.width + 18.0f,
            listBounds.y,
            panel.x + panel.width - 20.0f - (listBounds.x + listBounds.width + 18.0f),
            listBounds.height + RowHeight + 10.0f};
    SectorEditorMaterialRegistryDraft* draft = editor.SelectedDraft();
    if (draft == nullptr) {
        engine::Text(config, assets, formBounds, font, "No materials are available.",
                engine::UITextJustify::Left, config.mutedTextColor, true);
    } else {
        editor.EnsurePreview(assets);
        const float contentWidth = ScrollContentWidth(formBounds.width, config);
        engine::UIScrollAreaResult formScroll = engine::BeginScrollArea(
                ui, config, input, "sector_editor_material_registry_form_scroll",
                formBounds, Vector2{contentWidth, 1050.0f}, state.formScroll);
        float y = 0.0f;
        const float labelWidth = 180.0f;
        const float fieldX = labelWidth + 12.0f;
        const float fieldWidth = std::max(100.0f, formScroll.viewport.width - fieldX);
        const auto label = [&](const char* text) {
            engine::Text(ui, config, assets,
                    Rectangle{0.0f, y, labelWidth, RowHeight}, smallFont, text,
                    engine::UITextJustify::Left, config.mutedTextColor);
        };

        label("Material ID");
        const auto idResult = engine::TextInput(ui, config, input, assets,
                "sector_editor_material_registry_id",
                Rectangle{fieldX, y, fieldWidth, RowHeight}, smallFont,
                state.idBuffer, sizeof(state.idBuffer), 1, sizeof(state.idBuffer) - 1);
        if (idResult.changed || idResult.submitted) editor.ApplyIdBuffer();
        y += RowHeight + Gap;

        label("Albedo PNG");
        const std::string albedoLabel = EditorAssetPathDisplayLabel(
                draft->definition.path, "assets/images/");
        if (engine::Button(ui, config, input, assets,
                    "sector_editor_material_registry_path_picker",
                    Rectangle{fieldX, y, fieldWidth, RowHeight}, smallFont,
                    albedoLabel.empty() ? "Choose PNG..." : albedoLabel.c_str(),
                    engine::UITextJustify::Left)) {
            editor.OpenAlbedoPicker();
        }
        y += RowHeight + Gap;

        label("Filtering");
        const char* filterLabels[] = {"Point", "Bilinear", "Trilinear", "Aniso 8x"};
        int filter = static_cast<int>(draft->definition.filter);
        if (engine::Option(ui, config, input, assets,
                    "sector_editor_material_registry_filter",
                    Rectangle{fieldX, y, fieldWidth, RowHeight}, smallFont,
                    filterLabels, std::size(filterLabels), filter)) {
            draft->definition.filter = static_cast<SectorMaterialFilter>(filter);
        }
        y += RowHeight + Gap;

        label("Metalness");
        engine::FloatInput(ui, config, input, assets,
                "sector_editor_material_registry_metallic",
                Rectangle{fieldX, y, fieldWidth, RowHeight}, smallFont,
                draft->definition.metallicFactor, state.metallicInput, 0.0f, 1.0f, 3);
        y += RowHeight + Gap;
        label("Roughness");
        engine::FloatInput(ui, config, input, assets,
                "sector_editor_material_registry_roughness",
                Rectangle{fieldX, y, fieldWidth, RowHeight}, smallFont,
                draft->definition.roughnessFactor, state.roughnessInput, 0.0f, 1.0f, 3);
        y += RowHeight + Gap;
        label("Normal strength");
        engine::FloatInput(ui, config, input, assets,
                "sector_editor_material_registry_normal_strength",
                Rectangle{fieldX, y, fieldWidth, RowHeight}, smallFont,
                draft->definition.normalStrength, state.normalStrengthInput, 0.0f, 1.0f, 3);
        y += RowHeight + Gap;

        const std::string normalPath = SectorMaterialNormalMapPath(draft->definition.path);
        const bool normalPresent = FileExists(ResolveEditorAssetPath(normalPath).c_str());
        const std::string normalStatus = normalPresent
                ? "Normal: " + normalPath + " (OpenGL Y+, loaded automatically)"
                : "Normal: missing (" + normalPath + ")";
        const float normalStatusHeight = MeasureSectorEditorWrappedTextHeight(
                config,
                assets,
                smallFont,
                normalStatus.c_str(),
                formScroll.viewport.width,
                2);
        engine::Text(ui, config, assets,
                Rectangle{0.0f, y, formScroll.viewport.width, normalStatusHeight}, smallFont,
                normalStatus.c_str(),
                engine::UITextJustify::Left,
                normalPresent ? config.mutedTextColor : config.invalidColor, true);
        y += normalStatusHeight + Gap;

        engine::Text(ui, config, assets,
                Rectangle{0.0f, y, formScroll.viewport.width, RowHeight},
                font, "Albedo preview", engine::UITextJustify::Left, config.textColor);
        y += RowHeight + Gap;
        engine::Image(ui, config, assets,
                Rectangle{0.0f, y, std::min(500.0f, formScroll.viewport.width), 320.0f},
                state.previewTexture);
        engine::EndScrollArea(ui, config, input, formScroll, state.formScroll);
    }

    if (!state.validationMessage.empty()) {
        engine::Text(config, assets,
                Rectangle{panel.x + 340.0f, panel.y + panel.height - 58.0f,
                        panel.width - 690.0f, 42.0f},
                smallFont, state.validationMessage.c_str(),
                engine::UITextJustify::Left, config.invalidColor, true);
    }
    if (engine::Button(ui, config, input, assets,
                "sector_editor_material_registry_save",
                Rectangle{panel.x + panel.width - 314.0f, panel.y + panel.height - 58.0f,
                        136.0f, 42.0f}, smallFont, "Save")) {
        if (editor.SaveAndClose(assets)) return SectorEditorMaterialRegistryEditorResult::Saved;
    }
    if (engine::Button(ui, config, input, assets,
                "sector_editor_material_registry_cancel",
                Rectangle{panel.x + panel.width - 162.0f, panel.y + panel.height - 58.0f,
                        136.0f, 42.0f}, smallFont, "Cancel") || cancelRequested) {
        editor.Cancel(&assets);
        return SectorEditorMaterialRegistryEditorResult::Cancelled;
    }

    if (state.deleteConfirmationOpen) {
        const Rectangle modal{(EditorWidth - 540.0f) * 0.5f,
                (EditorHeight - 220.0f) * 0.5f, 540.0f, 220.0f};
        DrawRectangle(0, 0, static_cast<int>(EditorWidth),
                static_cast<int>(EditorHeight), Color{0, 0, 0, 130});
        DrawRectangleRec(modal, Color{25, 29, 38, 255});
        DrawRectangleLinesEx(modal, config.borderThickness, config.borderColor);
        engine::Text(config, assets,
                Rectangle{modal.x + 20.0f, modal.y + 18.0f, modal.width - 40.0f, 90.0f},
                font, TextFormat("Delete material '%s'?", state.deleteConfirmationId.c_str()),
                engine::UITextJustify::Left, config.textColor, true);
        if (engine::Button(ui, config, input, assets,
                    "sector_editor_material_registry_delete_confirm",
                    Rectangle{modal.x + modal.width - 310.0f, modal.y + modal.height - 62.0f,
                            135.0f, 42.0f}, smallFont, "Delete")) {
            editor.ConfirmDeleteSelected();
        }
        if (engine::Button(ui, config, input, assets,
                    "sector_editor_material_registry_delete_cancel",
                    Rectangle{modal.x + modal.width - 160.0f, modal.y + modal.height - 62.0f,
                            135.0f, 42.0f}, smallFont, "Cancel")) {
            editor.CancelDelete();
        }
    }

    input.ForEachEvent(engine::InputEventType::Any, true,
            [](engine::InputEvent& event) { engine::ConsumeEvent(event); });
    return SectorEditorMaterialRegistryEditorResult::None;
}

} // namespace game
