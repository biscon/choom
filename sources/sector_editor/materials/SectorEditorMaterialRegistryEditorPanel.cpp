#include "sector_editor/materials/SectorEditorMaterialRegistryEditorPanel.h"
#include "sector_editor/materials/SectorEditorMaterialFormLayout.h"

#include "engine/input/InputEvents.h"
#include "sector_editor/SectorEditorHelpers.h"
#include "sector_editor/SectorEditorUiHelpers.h"
#include "sector_editor/services/SectorEditorAssetPickerUi.h"
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
            font, picker.macroMask ? "Choose Macro Mask PNG" : "Choose Albedo PNG");

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
                            + (picker.macroMask ? " macro mask PNG files" : " albedo PNG files"));
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

    const Rectangle browserBounds{panel.x + 20.0f, panel.y + 66.0f, 300.0f, 710.0f};
    const auto browserLayout = MeasureSectorMaterialBrowser(browserBounds);
    auto& session = editor.Session();
    if (DrawSectorEditorAssetPickerFilter(ui, config, input, assets, smallFont,
                "sector_editor_material_registry_filter_text", browserLayout.filter,
                session.filterBuffer, sizeof(session.filterBuffer))) {
        editor.ApplyFilter();
    }
    const Rectangle listBounds = browserLayout.list;
    if (state.scrollSelectionIntoView) {
        const float viewportHeight = std::max(0.0f, listBounds.height
                - config.borderThickness * 2.0f - engine::DefaultScrollAreaPaddingPx * 2.0f);
        state.listScroll.offset.y = SectorMaterialSelectionScrollOffset(
                state.listScroll.offset.y, state.selectedFilteredIndex,
                config.listItemHeight, viewportHeight);
        state.scrollSelectionIntoView = false;
    }
    const float listWidth = ScrollContentWidth(listBounds.width, config);
    const Vector2 listContentSize{
            listWidth,
            std::max(listBounds.height,
                    config.listItemHeight * static_cast<float>(state.listLabels.size()))};
    engine::UIScrollAreaResult listScroll = engine::BeginScrollArea(
            ui, config, input, "sector_editor_material_registry_list_scroll",
            listBounds, listContentSize, state.listScroll);
    if (!state.listLabels.empty()) {
        int selected = state.selectedFilteredIndex;
        engine::List(ui, config, input, assets,
                "sector_editor_material_registry_list",
                Rectangle{0.0f, 0.0f, listScroll.viewport.width, listContentSize.y},
                smallFont, state.listLabels.data(), state.listLabels.size(), selected);
        if (selected != state.selectedFilteredIndex) editor.SelectFilteredIndex(selected);
    }
    engine::EndScrollArea(ui, config, input, listScroll, state.listScroll);

    if (engine::Button(ui, config, input, assets,
                "sector_editor_material_registry_add",
                browserLayout.add, smallFont, "Add")) {
        editor.AddMaterial();
    }
    if (engine::Button(ui, config, input, assets,
                "sector_editor_material_registry_delete",
                browserLayout.remove, smallFont, "Delete",
                engine::UITextJustify::Center, editor.SelectedDraft() != nullptr)) {
        editor.RequestDeleteSelected();
    }

    const Rectangle formBounds{
            browserBounds.x + browserBounds.width + 18.0f,
            browserBounds.y,
            panel.x + panel.width - 20.0f - (browserBounds.x + browserBounds.width + 18.0f),
            browserBounds.height};
    SectorEditorMaterialRegistryDraft* draft = editor.SelectedDraft();
    if (draft == nullptr) {
        engine::Text(config, assets, formBounds, font,
                state.drafts.empty() ? "No materials are available." : "No materials match the filter",
                engine::UITextJustify::Left, config.mutedTextColor, true);
    } else {
        editor.EnsurePreview(assets);
        const float contentWidth = ScrollContentWidth(formBounds.width, config);
        const auto& definition = draft->definition;
        const std::string normalPath = SectorMaterialNormalMapPath(definition.path);
        const bool normalPresent = FileExists(ResolveEditorAssetPath(normalPath).c_str());
        const std::string normalStatus = normalPresent
                ? "Normal: " + normalPath + " (OpenGL Y+, loaded automatically)"
                : "Normal: missing (" + normalPath + ")";
        const std::string ormPath = SectorMaterialOrmMapPath(definition.path);
        const std::string roughnessPath = SectorMaterialRoughnessMapPath(definition.path);
        const bool ormPresent = FileExists(ResolveEditorAssetPath(ormPath).c_str());
        const bool roughnessPresent = FileExists(ResolveEditorAssetPath(roughnessPath).c_str());
        const std::string propertyStatus = ormPresent
                ? "ORM: " + ormPath + " (active: R=AO, G=roughness, B=metallic)"
                        + (roughnessPresent ? " | Roughness map ignored while ORM is present" : "")
                : (roughnessPresent ? "Roughness: " + roughnessPath + " (active: R channel)"
                        : "Properties: missing (using metallic/roughness factors)");
        const auto& macro = definition.macro;
        const bool macroExpanded = macro.enabled;
        const std::string macroHelp = "Black: unchanged. White: full effect. Positive roughness makes patches duller; negative makes them more polished. Static architecture only.\nMask: "
                + (macro.maskPath.empty() ? std::string("<none selected>") : macro.maskPath)
                + (!macro.maskPath.empty() && !FileExists(ResolveEditorAssetPath(macro.maskPath).c_str())
                        ? " (missing; no macro effect)" : "");
        const auto textHeight = [&](const std::string& text) {
            return MeasureSectorEditorWrappedTextHeight(config, assets, smallFont,
                    text.c_str(), contentWidth, 2);
        };
        // Measure the labels actually used below; stack them when fields cannot fit.
        const char* fieldLabels[] = {"Material ID", "Albedo PNG", "Filtering", "Metalness",
                "Roughness", "Normal strength", "Macro mask", "Repeat (metres)",
                "Darkening", "Roughness change"};
        float labelWidth = 0.0f;
        const engine::FontAsset* labelFont = assets.GetFont(smallFont);
        for (const char* text : fieldLabels) {
            const float measured = labelFont != nullptr
                    ? MeasureTextEx(labelFont->font, text, config.fontSize, config.textSpacing).x
                    : static_cast<float>(std::char_traits<char>::length(text)) * config.fontSize * 0.6f;
            labelWidth = std::max(labelWidth, measured + config.paddingX * 2.0f);
        }
        const auto layout = MeasureSectorMaterialForm(contentWidth, labelWidth,
                config.fontSize, macroExpanded, textHeight(macroHelp),
                textHeight(normalStatus), textHeight(propertyStatus));
        engine::UIScrollAreaResult formScroll = engine::BeginScrollArea(
                ui, config, input, "sector_editor_material_registry_form_scroll",
                formBounds, Vector2{contentWidth, layout.contentHeight}, state.formScroll);
        using Row = SectorMaterialFormRow;
        const auto bounds = [&](Row row) { return layout.rows[static_cast<std::size_t>(row)]; };
        const auto field = [&](Row row, const char* text) {
            const Rectangle area = bounds(row);
            engine::Text(ui, config, assets,
                    Rectangle{0.0f, area.y, layout.fieldX == 0.0f ? contentWidth : labelWidth,
                            layout.fieldOffsetY == 0.0f ? RowHeight : config.fontSize},
                    smallFont, text, engine::UITextJustify::Left, config.mutedTextColor);
            return Rectangle{layout.fieldX, area.y + layout.fieldOffsetY,
                    contentWidth - layout.fieldX, RowHeight};
        };
        const auto number = [&](Row row, const char* text, const char* id,
                float& value, engine::UIFloatInputState& buffer, float low, float high) {
            engine::FloatInput(ui, config, input, assets, id, field(row, text),
                    smallFont, value, buffer, low, high, 3);
        };
        const auto idResult = engine::TextInput(ui, config, input, assets,
                "sector_editor_material_registry_id", field(Row::Id, fieldLabels[0]), smallFont,
                state.idBuffer, sizeof(state.idBuffer), 1, sizeof(state.idBuffer) - 1);
        if (idResult.changed || idResult.submitted) editor.ApplyIdBuffer();
        const std::string albedoLabel = EditorAssetPathDisplayLabel(definition.path, "assets/images/");
        if (engine::Button(ui, config, input, assets, "sector_editor_material_registry_path_picker",
                    field(Row::Albedo, fieldLabels[1]), smallFont,
                    albedoLabel.empty() ? "Choose PNG..." : albedoLabel.c_str(), engine::UITextJustify::Left)) {
            editor.OpenAlbedoPicker();
        }
        const char* filterLabels[] = {"Point", "Bilinear", "Trilinear", "Aniso 8x"};
        int filter = static_cast<int>(definition.filter);
        if (engine::Option(ui, config, input, assets, "sector_editor_material_registry_filter",
                    field(Row::Filter, fieldLabels[2]), smallFont, filterLabels, std::size(filterLabels), filter)) {
            draft->definition.filter = static_cast<SectorMaterialFilter>(filter);
        }
        number(Row::Metallic, fieldLabels[3], "sector_editor_material_registry_metallic",
                draft->definition.metallicFactor, state.metallicInput, 0.0f, 1.0f);
        number(Row::Roughness, fieldLabels[4], "sector_editor_material_registry_roughness",
                draft->definition.roughnessFactor, state.roughnessInput, 0.0f, 1.0f);
        number(Row::NormalStrength, fieldLabels[5], "sector_editor_material_registry_normal_strength",
                draft->definition.normalStrength, state.normalStrengthInput, 0.0f, 1.0f);
        engine::Checkbox(ui, config, input, assets, "sector_editor_material_macro_enabled",
                bounds(Row::MacroEnabled), smallFont, "Enable macro variation", draft->definition.macro.enabled);
        if (macroExpanded) {
            const std::string maskLabel = EditorAssetPathDisplayLabel(macro.maskPath, "assets/images/macros/");
            if (engine::Button(ui, config, input, assets, "sector_editor_material_macro_picker",
                        field(Row::MacroMask, fieldLabels[6]), smallFont,
                        maskLabel.empty() ? "Choose mask PNG..." : maskLabel.c_str(), engine::UITextJustify::Left)) {
                editor.OpenMacroPicker();
            }
            if (engine::Button(ui, config, input, assets, "sector_editor_material_macro_clear",
                        bounds(Row::MacroClear), smallFont, "Clear macro mask")) editor.ClearMacroMask();
            number(Row::MacroRepeat, fieldLabels[7], "sector_editor_material_macro_repeat",
                    draft->definition.macro.repeatMeters, state.macroRepeatInput, 0.1f, 1024.0f);
            number(Row::MacroDarkening, fieldLabels[8], "sector_editor_material_macro_darkening",
                    draft->definition.macro.darkening, state.macroDarkeningInput, 0.0f, 1.0f);
            number(Row::MacroRoughness, fieldLabels[9], "sector_editor_material_macro_roughness",
                    draft->definition.macro.roughnessChange, state.macroRoughnessInput, -1.0f, 1.0f);
            engine::Text(ui, config, assets, bounds(Row::MacroHelp), smallFont,
                    macroHelp.c_str(), engine::UITextJustify::Left, config.mutedTextColor, true);
        }
        engine::Text(ui, config, assets, bounds(Row::NormalStatus), smallFont,
                normalStatus.c_str(), engine::UITextJustify::Left,
                normalPresent ? config.mutedTextColor : config.invalidColor, true);
        engine::Text(ui, config, assets, bounds(Row::PropertyStatus), smallFont,
                propertyStatus.c_str(), engine::UITextJustify::Left,
                ormPresent || roughnessPresent ? config.mutedTextColor : config.invalidColor, true);
        engine::Text(ui, config, assets, bounds(Row::PreviewTitle), font, "Albedo preview");
        Rectangle previewBounds = bounds(Row::PreviewImage);
        previewBounds.width = std::min(500.0f, previewBounds.width);
        engine::Image(ui, config, assets, previewBounds, state.previewTexture);
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
