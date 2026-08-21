#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "game/FpsViewmodel.h"
#include "sector_editor/services/sounds/SectorEditorAudioAssetPicker.h"
#include "sector_editor/services/static_model_picker/SectorEditorStaticModelPickerService.h"
#include "sector_editor/weapons/SectorEditorWeaponEditorService.h"

namespace game {

struct SectorEditorWeaponEditorLayout {
    Rectangle panel = {};
    Rectangle listPane = {};
    Rectangle listBounds = {};
    Rectangle formBounds = {};
    Rectangle addButton = {};
    Rectangle duplicateButton = {};
    Rectangle deleteButton = {};
    Rectangle saveButton = {};
    Rectangle cancelButton = {};
};

inline SectorEditorWeaponEditorLayout BuildSectorEditorWeaponEditorLayoutForViewport(
        float viewportWidth,
        float viewportHeight,
        bool preview3D)
{
    SectorEditorWeaponEditorLayout layout;
    if (preview3D) {
        layout.panel = Rectangle{16.0f, 16.0f, 820.0f, viewportHeight - 32.0f};
    } else {
        layout.panel = Rectangle{
                (viewportWidth - 1400.0f) * 0.5f,
                (viewportHeight - 900.0f) * 0.5f,
                1400.0f,
                900.0f};
    }
    const float listWidth = preview3D ? 210.0f : 300.0f;
    layout.listPane = Rectangle{
            layout.panel.x + 20.0f,
            layout.panel.y + 66.0f,
            listWidth,
            layout.panel.height - 142.0f};
    layout.listBounds = Rectangle{
            layout.listPane.x,
            layout.listPane.y,
            layout.listPane.width,
            layout.listPane.height - 104.0f};
    layout.formBounds = Rectangle{
            layout.listPane.x + layout.listPane.width + 18.0f,
            layout.listPane.y,
            layout.panel.x + layout.panel.width - 20.0f
                    - (layout.listPane.x + layout.listPane.width + 18.0f),
            layout.listPane.height};
    constexpr float gap = 6.0f;
    const float actionWidth = (layout.listPane.width - gap) * 0.5f;
    layout.addButton = Rectangle{
            layout.listPane.x,
            layout.listBounds.y + layout.listBounds.height + 8.0f,
            actionWidth,
            42.0f};
    layout.duplicateButton = Rectangle{
            layout.addButton.x + actionWidth + gap,
            layout.addButton.y,
            actionWidth,
            42.0f};
    layout.deleteButton = Rectangle{
            layout.listPane.x,
            layout.addButton.y + 48.0f,
            layout.listPane.width,
            42.0f};
    layout.saveButton = Rectangle{
            layout.panel.x + layout.panel.width - 314.0f,
            layout.panel.y + layout.panel.height - 58.0f,
            136.0f,
            42.0f};
    layout.cancelButton = Rectangle{
            layout.panel.x + layout.panel.width - 162.0f,
            layout.saveButton.y,
            136.0f,
            42.0f};
    return layout;
}

struct SectorEditorWeaponEditorPanelResult {
    bool saved = false;
    bool cancelled = false;
    bool previewFireRequested = false;
    bool holsterToggleRequested = false;
};

SectorEditorWeaponEditorPanelResult DrawSectorEditorWeaponEditorPanel(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont,
        const FpsViewmodelRuntimeState* viewmodel,
        SectorEditorWeaponEditorService& editor,
        SectorEditorStaticModelPickerService& modelPicker,
        SectorEditorAudioAssetPickerService& audioPicker);

} // namespace game
