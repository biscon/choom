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
        const auto pathResult = engine::TextInput(ui, config, input, assets,
                "sector_editor_material_registry_path",
                Rectangle{fieldX, y, fieldWidth, RowHeight}, smallFont,
                state.pathBuffer, sizeof(state.pathBuffer), 1, sizeof(state.pathBuffer) - 1);
        if (pathResult.submitted) editor.ApplyPathBuffer();
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
