#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/items/SectorEditorItemEditorService.h"
#include "sector_editor/services/static_model_picker/SectorEditorStaticModelPickerService.h"

#include <algorithm>

namespace game {

struct SectorEditorItemEditorLayout {
    Rectangle panel = {};
    Rectangle listBounds = {};
    Rectangle formBounds = {};
    Rectangle addButton = {};
    Rectangle deleteButton = {};
    Rectangle validationMessage = {};
    Rectangle saveButton = {};
    Rectangle cancelButton = {};
};

inline SectorEditorItemEditorLayout BuildSectorEditorItemEditorLayoutForViewport(
        float viewportWidth,
        float viewportHeight)
{
    SectorEditorItemEditorLayout layout;
    layout.panel = Rectangle{
            std::max(16.0f, (viewportWidth - 1320.0f) * 0.5f),
            std::max(16.0f, (viewportHeight - 900.0f) * 0.5f),
            std::min(1320.0f, viewportWidth - 32.0f),
            std::min(900.0f, viewportHeight - 32.0f)};
    const float listWidth = std::min(310.0f, layout.panel.width * 0.3f);
    layout.listBounds = Rectangle{
            layout.panel.x + 20.0f,
            layout.panel.y + 66.0f,
            listWidth,
            layout.panel.height - 190.0f};
    layout.formBounds = Rectangle{
            layout.listBounds.x + layout.listBounds.width + 18.0f,
            layout.listBounds.y,
            layout.panel.x + layout.panel.width - 20.0f
                    - (layout.listBounds.x + layout.listBounds.width + 18.0f),
            layout.listBounds.height + 96.0f};
    layout.addButton = Rectangle{
            layout.listBounds.x,
            layout.listBounds.y + layout.listBounds.height + 8.0f,
            (layout.listBounds.width - 6.0f) * 0.5f,
            42.0f};
    layout.deleteButton = Rectangle{
            layout.addButton.x + layout.addButton.width + 6.0f,
            layout.addButton.y,
            layout.addButton.width,
            42.0f};
    layout.validationMessage = Rectangle{
            layout.panel.x + 20.0f,
            layout.panel.y + layout.panel.height - 62.0f,
            layout.panel.width - 360.0f,
            44.0f};
    layout.saveButton = Rectangle{
            layout.panel.x + layout.panel.width - 314.0f,
            layout.panel.y + layout.panel.height - 62.0f,
            136.0f,
            44.0f};
    layout.cancelButton = Rectangle{
            layout.panel.x + layout.panel.width - 162.0f,
            layout.saveButton.y,
            136.0f,
            44.0f};
    return layout;
}

struct SectorEditorItemEditorPanelResult {
    bool saved = false;
    bool cancelled = false;
};

SectorEditorItemEditorPanelResult DrawSectorEditorItemEditorPanel(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont,
        SectorEditorItemEditorService& editor,
        SectorEditorStaticModelPickerService& modelPicker);

} // namespace game
