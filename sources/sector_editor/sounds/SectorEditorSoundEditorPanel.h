#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/services/sounds/SectorEditorAudioAssetPicker.h"
#include "sector_editor/sounds/SectorEditorSoundEditorService.h"

namespace game {

struct SectorEditorSoundEditorLayout {
    Rectangle modal = {};
    Rectangle listPane = {};
    Rectangle listBounds = {};
    Rectangle formBounds = {};
    Rectangle addButton = {};
    Rectangle deleteButton = {};
    Rectangle saveButton = {};
    Rectangle cancelButton = {};
};

inline SectorEditorSoundEditorLayout BuildSectorEditorSoundEditorLayoutForViewport(
        float viewportWidth, float viewportHeight)
{
    SectorEditorSoundEditorLayout layout;
    layout.modal = Rectangle{
            (viewportWidth - 1200.0f) * 0.5f,
            (viewportHeight - 760.0f) * 0.5f,
            1200.0f, 760.0f};
    layout.listPane = Rectangle{
            layout.modal.x + 22.0f, layout.modal.y + 66.0f,
            330.0f, layout.modal.height - 142.0f};
    layout.listBounds = Rectangle{
            layout.listPane.x, layout.listPane.y,
            layout.listPane.width, layout.listPane.height - 52.0f};
    constexpr float gap = 8.0f;
    const float buttonWidth = (layout.listPane.width - gap) * 0.5f;
    layout.addButton = Rectangle{
            layout.listPane.x, layout.listBounds.y + layout.listBounds.height + 8.0f,
            buttonWidth, 42.0f};
    layout.deleteButton = Rectangle{
            layout.addButton.x + buttonWidth + gap, layout.addButton.y,
            buttonWidth, 42.0f};
    layout.formBounds = Rectangle{
            layout.listPane.x + layout.listPane.width + 18.0f,
            layout.listPane.y,
            layout.modal.x + layout.modal.width - 22.0f
                    - (layout.listPane.x + layout.listPane.width + 18.0f),
            layout.listPane.height};
    layout.saveButton = Rectangle{
            layout.modal.x + layout.modal.width - 316.0f,
            layout.modal.y + layout.modal.height - 58.0f, 136.0f, 42.0f};
    layout.cancelButton = Rectangle{
            layout.modal.x + layout.modal.width - 164.0f,
            layout.saveButton.y, 136.0f, 42.0f};
    return layout;
}

enum class SectorEditorSoundEditorPanelResult { None, Saved, Cancelled };

SectorEditorSoundEditorPanelResult DrawSectorEditorSoundEditorPanel(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont,
        SectorEditorSoundEditorService& editor,
        SectorEditorAudioAssetPickerService& audioPicker);

} // namespace game
