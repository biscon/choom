#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/npcs/SectorEditorNpcEditorService.h"
#include "sector_editor/services/static_model_picker/SectorEditorStaticModelPickerService.h"

namespace game {

struct SectorEditorNpcEditorModalLayout {
    Rectangle modal = {};
    Rectangle listPane = {};
    Rectangle listBounds = {};
    Rectangle formBounds = {};
    Rectangle addButton = {};
    Rectangle deleteButton = {};
    Rectangle saveButton = {};
    Rectangle cancelButton = {};
};

inline SectorEditorNpcEditorModalLayout BuildSectorEditorNpcEditorModalLayoutForViewport(
        float viewportWidth,
        float viewportHeight)
{
    constexpr float rowGap = 10.0f;
    SectorEditorNpcEditorModalLayout layout;
    layout.modal = Rectangle{
            (viewportWidth - 1400.0f) * 0.5f,
            (viewportHeight - 900.0f) * 0.5f,
            1400.0f,
            900.0f};
    layout.listPane = Rectangle{
            layout.modal.x + 24.0f,
            layout.modal.y + 72.0f,
            300.0f,
            layout.modal.height - 156.0f};
    layout.listBounds = Rectangle{
            layout.listPane.x,
            layout.listPane.y,
            layout.listPane.width,
            layout.listPane.height - 58.0f};
    layout.formBounds = Rectangle{
            layout.listPane.x + layout.listPane.width + 22.0f,
            layout.listPane.y,
            layout.modal.width - layout.listPane.width - 70.0f,
            layout.listPane.height};
    const float halfButton = (layout.listPane.width - rowGap) * 0.5f;
    layout.addButton = Rectangle{
            layout.listPane.x,
            layout.listPane.y + layout.listPane.height - 46.0f,
            halfButton,
            46.0f};
    layout.deleteButton = Rectangle{
            layout.addButton.x + halfButton + rowGap,
            layout.addButton.y,
            halfButton,
            46.0f};
    layout.saveButton = Rectangle{
            layout.modal.x + layout.modal.width - 330.0f,
            layout.modal.y + layout.modal.height - 64.0f,
            140.0f,
            44.0f};
    layout.cancelButton = Rectangle{
            layout.modal.x + layout.modal.width - 170.0f,
            layout.saveButton.y,
            140.0f,
            44.0f};
    return layout;
}

SectorEditorNpcEditorModalLayout BuildSectorEditorNpcEditorModalLayout();

void DrawSectorEditorNpcEditorModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont,
        SectorEditorNpcEditorService& editor,
        SectorEditorStaticModelPickerService& modelPicker);

} // namespace game
