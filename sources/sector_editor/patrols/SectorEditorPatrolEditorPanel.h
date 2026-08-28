#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/patrols/SectorEditorPatrolEditorService.h"

#include <algorithm>

namespace game {

struct SectorEditorPatrolEditorLayout {
    Rectangle modal = {};
    Rectangle listPane = {};
    Rectangle listBounds = {};
    Rectangle formBounds = {};
    Rectangle addButton = {};
    Rectangle deleteButton = {};
    Rectangle saveButton = {};
    Rectangle cancelButton = {};
};

struct SectorEditorPatrolWaypointRowLayout {
    Rectangle markerLabel = {};
    Rectangle markerInput = {};
    Rectangle delayLabel = {};
    Rectangle delayInput = {};
    Rectangle gaitInput = {};
    Rectangle lookCheckbox = {};
    Rectangle arcLabel = {};
    Rectangle arcInput = {};
};

inline Rectangle BuildSectorEditorPatrolWaypointCardDrawRect(
        Rectangle viewport,
        Vector2 scrollOffset,
        float localY,
        float contentWidth,
        float height)
{
    return Rectangle{
            viewport.x - scrollOffset.x,
            viewport.y + localY - scrollOffset.y,
            contentWidth,
            height};
}

inline SectorEditorPatrolWaypointRowLayout
BuildSectorEditorPatrolWaypointRowLayout(
        float contentWidth,
        float markerY,
        float settingsY)
{
    SectorEditorPatrolWaypointRowLayout layout;
    layout.markerLabel = {10.0f, markerY, 140.0f, 42.0f};
    layout.markerInput = {
            160.0f, markerY,
            std::max(100.0f, std::min(600.0f, contentWidth - 170.0f)),
            42.0f};
    layout.delayLabel = {10.0f, settingsY, 125.0f, 42.0f};
    layout.delayInput = {140.0f, settingsY, 110.0f, 42.0f};
    layout.gaitInput = {260.0f, settingsY, 110.0f, 42.0f};
    layout.lookCheckbox = {380.0f, settingsY, 180.0f, 42.0f};
    layout.arcLabel = {570.0f, settingsY, 90.0f, 42.0f};
    layout.arcInput = {
            665.0f, settingsY,
            std::max(90.0f, std::min(120.0f, contentWidth - 675.0f)),
            42.0f};
    return layout;
}

inline SectorEditorPatrolEditorLayout BuildSectorEditorPatrolEditorLayoutForViewport(
        float viewportWidth,
        float viewportHeight)
{
    SectorEditorPatrolEditorLayout layout;
    layout.modal = {(viewportWidth - 1400.0f) * 0.5f,
            (viewportHeight - 900.0f) * 0.5f, 1400.0f, 900.0f};
    layout.listPane = {layout.modal.x + 24.0f, layout.modal.y + 72.0f,
            320.0f, layout.modal.height - 156.0f};
    layout.listBounds = {layout.listPane.x, layout.listPane.y,
            layout.listPane.width, layout.listPane.height - 58.0f};
    constexpr float gap = 10.0f;
    const float buttonWidth = (layout.listPane.width - gap) * 0.5f;
    layout.addButton = {layout.listPane.x,
            layout.listBounds.y + layout.listBounds.height + 10.0f,
            buttonWidth, 44.0f};
    layout.deleteButton = {layout.addButton.x + buttonWidth + gap,
            layout.addButton.y, buttonWidth, 44.0f};
    layout.formBounds = {layout.listPane.x + layout.listPane.width + 22.0f,
            layout.listPane.y,
            layout.modal.x + layout.modal.width - 24.0f
                    - (layout.listPane.x + layout.listPane.width + 22.0f),
            layout.listPane.height};
    layout.saveButton = {layout.modal.x + layout.modal.width - 330.0f,
            layout.modal.y + layout.modal.height - 64.0f, 140.0f, 44.0f};
    layout.cancelButton = {layout.modal.x + layout.modal.width - 170.0f,
            layout.saveButton.y, 140.0f, 44.0f};
    return layout;
}

enum class SectorEditorPatrolEditorPanelResult { None, Saved, Cancelled };

SectorEditorPatrolEditorPanelResult DrawSectorEditorPatrolEditorPanel(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont,
        SectorEditorPatrolEditorService& editor);

} // namespace game
