#include "sector_editor/patrols/SectorEditorPatrolEditorPanel.h"

#include "engine/input/InputEvents.h"
#include "sector_editor/SectorEditorHelpers.h"

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

namespace game {
namespace {

constexpr float RowHeight = 42.0f;
constexpr float Gap = 10.0f;

float ScrollContentWidth(float width, const engine::UIConfig& config)
{
    return std::max(0.0f, width - config.borderThickness * 2.0f
            - config.scrollbarSize - engine::DefaultScrollAreaPaddingPx * 2.0f);
}

void ConsumeRemainingInput(engine::Input& input)
{
    input.ForEachEvent(engine::InputEventType::Any, true,
            [](engine::InputEvent& event) { engine::ConsumeEvent(event); });
}

void DrawDeleteConfirmation(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        SectorEditorPatrolEditorService& editor,
        Rectangle modal)
{
    const Rectangle popup{modal.x + 400.0f, modal.y + 300.0f, 600.0f, 240.0f};
    DrawRectangleRec(popup, Color{27, 32, 42, 255});
    DrawRectangleLinesEx(popup, config.borderThickness, config.borderColor);
    engine::Text(config, assets,
            {popup.x + 24.0f, popup.y + 20.0f, popup.width - 48.0f, 38.0f},
            font, "Remove patrol?");
    engine::Text(config, assets,
            {popup.x + 24.0f, popup.y + 70.0f, popup.width - 48.0f, 72.0f},
            font, "Remove this patrol from the level? Level Markers are kept.",
            engine::UITextJustify::Left, config.textColor, true);
    if (engine::Button(ui, config, input, assets,
                "sector_editor_patrol_delete_confirm",
                {popup.x + popup.width - 310.0f, popup.y + 168.0f, 130.0f, 44.0f},
                font, "Remove")) {
        editor.ConfirmDeleteSelected();
    }
    if (engine::Button(ui, config, input, assets,
                "sector_editor_patrol_delete_cancel",
                {popup.x + popup.width - 160.0f, popup.y + 168.0f, 130.0f, 44.0f},
                font, "Cancel")) {
        editor.CancelDelete();
    }
}

} // namespace

SectorEditorPatrolEditorPanelResult DrawSectorEditorPatrolEditorPanel(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont,
        SectorEditorPatrolEditorService& editor)
{
    SectorEditorPatrolEditorState& state = editor.State();
    if (!state.open) return SectorEditorPatrolEditorPanelResult::None;
    const float viewportWidth = static_cast<float>(GetScreenWidth());
    const float viewportHeight = static_cast<float>(GetScreenHeight());
    const SectorEditorPatrolEditorLayout layout =
            BuildSectorEditorPatrolEditorLayoutForViewport(
                    viewportWidth, viewportHeight);

    bool cancelRequested = false;
    input.ForEachEvent(engine::InputEventType::KeyPressed, true,
            [&cancelRequested, &state](engine::InputEvent& event) {
                if (event.key.key != KEY_ESCAPE) return;
                if (state.deleteConfirmationOpen) {
                    state.deleteConfirmationOpen = false;
                    state.deleteConfirmationEditorId = -1;
                } else {
                    cancelRequested = true;
                }
                engine::ConsumeEvent(event);
            });
    if (cancelRequested) {
        editor.Cancel();
        ConsumeRemainingInput(input);
        return SectorEditorPatrolEditorPanelResult::Cancelled;
    }

    DrawRectangle(0, 0, static_cast<int>(viewportWidth),
            static_cast<int>(viewportHeight), Color{0, 0, 0, 150});
    DrawRectangleRec(layout.modal, Color{20, 24, 32, 250});
    DrawRectangleLinesEx(layout.modal, config.borderThickness, config.borderColor);
    engine::Text(config, assets,
            {layout.modal.x + 24.0f, layout.modal.y + 16.0f,
                    layout.modal.width - 48.0f, 42.0f},
            font, "Patrol Editor");

    if (state.deleteConfirmationOpen) {
        DrawDeleteConfirmation(ui, config, input, assets, font, editor, layout.modal);
        ConsumeRemainingInput(input);
        return SectorEditorPatrolEditorPanelResult::None;
    }

    const Vector2 listContentSize{
            ScrollContentWidth(layout.listBounds.width, config),
            std::max(layout.listBounds.height,
                    config.listItemHeight * static_cast<float>(state.listLabels.size()))};
    engine::UIScrollAreaResult listScroll = engine::BeginScrollArea(
            ui, config, input, "sector_editor_patrol_list_scroll",
            layout.listBounds, listContentSize, state.listScroll);
    if (!state.listLabels.empty()) {
        int selected = state.selectedIndex;
        engine::List(ui, config, input, assets, "sector_editor_patrol_list",
                {0.0f, 0.0f, listScroll.viewport.width, listContentSize.y},
                smallFont, state.listLabels.data(), state.listLabels.size(), selected);
        if (selected != state.selectedIndex && editor.SelectIndex(selected)) {
            ui.focusedId = 0;
            ui.openOptionId = 0;
        }
    }
    engine::EndScrollArea(ui, config, input, listScroll, state.listScroll);

    if (engine::Button(ui, config, input, assets, "sector_editor_patrol_add",
                layout.addButton, font, "Add")) {
        editor.AddPatrol();
        ui.focusedId = 0;
    }
    if (engine::Button(ui, config, input, assets, "sector_editor_patrol_remove",
                layout.deleteButton, font, "Remove")) {
        editor.RequestDeleteSelected();
    }

    SectorAuthoringPatrol* patrol = editor.Selected();
    if (patrol == nullptr) {
        engine::Text(config, assets,
                {layout.formBounds.x + 20.0f, layout.formBounds.y + 20.0f,
                        layout.formBounds.width - 40.0f, 80.0f},
                font, "No patrols. Press Add to create one.",
                engine::UITextJustify::Left, config.mutedTextColor, true);
    } else {
        const float contentWidth = ScrollContentWidth(layout.formBounds.width, config);
        const std::string usage = editor.SelectedUsageText();
        const float contentHeight = 316.0f
                + (usage.empty() ? 0.0f : 40.0f)
                + static_cast<float>(patrol->waypoints.size()) * 188.0f;
        engine::UIScrollAreaResult formScroll = engine::BeginScrollArea(
                ui, config, input, "sector_editor_patrol_form_scroll",
                layout.formBounds, {contentWidth, contentHeight}, state.formScroll);
        float y = 0.0f;
        constexpr float labelWidth = 170.0f;
        engine::Text(ui, config, assets, {0.0f, y, labelWidth, RowHeight},
                font, "Patrol ID", engine::UITextJustify::Left,
                config.mutedTextColor);
        const engine::UITextInputResult idResult = engine::TextInput(
                ui, config, input, assets, "sector_editor_patrol_id",
                {labelWidth, y, formScroll.viewport.width - labelWidth, RowHeight},
                font, state.idBuffer, sizeof(state.idBuffer), 0,
                sizeof(state.idBuffer) - 1);
        if (idResult.submitted) editor.ApplyIdBuffer();
        y += RowHeight + Gap;

        static const std::vector<std::string> orderedModes = {
                "Once", "Loop", "Ping-pong"};
        static const std::vector<std::string> shuffledModes = {"Once", "Loop"};
        const std::vector<std::string>& modes = patrol->shuffleWaypoints
                ? shuffledModes : orderedModes;
        int mode = patrol->mode == SectorPatrolMode::Once ? 0
                : patrol->mode == SectorPatrolMode::Loop ? 1 : 2;
        const int previousMode = mode;
        engine::Text(ui, config, assets, {0.0f, y, labelWidth, RowHeight},
                font, "Playback", engine::UITextJustify::Left,
                config.mutedTextColor);
        if (engine::Option(ui, config, input, assets,
                    "sector_editor_patrol_mode",
                    {labelWidth, y, formScroll.viewport.width - labelWidth, RowHeight},
                    font, modes, mode) && mode != previousMode) {
            editor.SetMode(mode == 0 ? SectorPatrolMode::Once
                    : mode == 1 ? SectorPatrolMode::Loop
                                : SectorPatrolMode::PingPong);
        }
        y += RowHeight + Gap;

        bool shuffleWaypoints = patrol->shuffleWaypoints;
        if (engine::Checkbox(ui, config, input, assets,
                    "sector_editor_patrol_shuffle_waypoints",
                    {0.0f, y, formScroll.viewport.width, RowHeight},
                    font, "Shuffle Waypoints", shuffleWaypoints)) {
            editor.SetShuffleWaypoints(shuffleWaypoints);
        }
        y += RowHeight + Gap;

        bool faceWaypointOrientation = patrol->faceWaypointOrientation;
        if (engine::Checkbox(ui, config, input, assets,
                    "sector_editor_patrol_face_waypoint_orientation",
                    {0.0f, y, formScroll.viewport.width, RowHeight},
                    font, "Face Waypoint Orientation", faceWaypointOrientation)) {
            editor.SetFaceWaypointOrientation(faceWaypointOrientation);
        }
        y += RowHeight + Gap;

        if (!usage.empty()) {
            engine::Text(ui, config, assets,
                    {0.0f, y, formScroll.viewport.width, 34.0f},
                    smallFont, ("Assigned to: " + usage).c_str(),
                    engine::UITextJustify::Left, config.mutedTextColor, true);
            y += 40.0f;
        }

        for (size_t index = 0; index < patrol->waypoints.size(); ++index) {
            SectorAuthoringPatrolWaypoint& waypoint = patrol->waypoints[index];
            DrawRectangleRec(BuildSectorEditorPatrolWaypointCardDrawRect(
                    formScroll.viewport,
                    state.formScroll.offset,
                    y,
                    formScroll.viewport.width,
                    174.0f),
                    Color{27, 32, 42, 230});
            engine::Text(ui, config, assets, {10.0f, y + 4.0f, 120.0f, 32.0f},
                    smallFont, ("Waypoint " + std::to_string(index + 1)).c_str(),
                    engine::UITextJustify::Left, config.accentColor);
            const float buttonY = y + 2.0f;
            if (engine::Button(ui, config, input, assets,
                        ("sector_editor_patrol_wp_up_" + std::to_string(index)).c_str(),
                        {formScroll.viewport.width - 206.0f, buttonY, 58.0f, 34.0f},
                        smallFont, "Up")) {
                editor.MoveWaypoint(index, -1);
                break;
            }
            if (engine::Button(ui, config, input, assets,
                        ("sector_editor_patrol_wp_down_" + std::to_string(index)).c_str(),
                        {formScroll.viewport.width - 142.0f, buttonY, 66.0f, 34.0f},
                        smallFont, "Down")) {
                editor.MoveWaypoint(index, 1);
                break;
            }
            if (engine::Button(ui, config, input, assets,
                        ("sector_editor_patrol_wp_remove_" + std::to_string(index)).c_str(),
                        {formScroll.viewport.width - 70.0f, buttonY, 66.0f, 34.0f},
                        smallFont, "X")) {
                editor.RemoveWaypoint(index);
                break;
            }

            float rowY = y + 42.0f;
            const SectorEditorPatrolWaypointRowLayout waypointLayout =
                    BuildSectorEditorPatrolWaypointRowLayout(
                            formScroll.viewport.width,
                            rowY,
                            rowY + RowHeight + 6.0f);
            engine::Text(ui, config, assets, waypointLayout.markerLabel,
                    smallFont, "Level Marker", engine::UITextJustify::Left,
                    config.mutedTextColor);
            int markerOption = -1;
            for (size_t marker = 0; marker < state.markerIds.size(); ++marker) {
                if (state.markerIds[marker] == waypoint.levelMarkerId) {
                    markerOption = static_cast<int>(marker);
                    break;
                }
            }
            const int previousMarker = markerOption;
            if (!state.markerLabels.empty()
                    && engine::Option(ui, config, input, assets,
                            ("sector_editor_patrol_wp_marker_"
                                    + std::to_string(index)).c_str(),
                            waypointLayout.markerInput,
                            smallFont, state.markerLabels.data(),
                            state.markerLabels.size(), markerOption)
                    && markerOption != previousMarker && markerOption >= 0) {
                editor.SetWaypointMarker(index,
                        state.markerIds[static_cast<size_t>(markerOption)]);
            }
            rowY += RowHeight + 6.0f;

            engine::Text(ui, config, assets, waypointLayout.delayLabel,
                    smallFont, "Delay (sec)", engine::UITextJustify::Left,
                    config.mutedTextColor);
            float delaySeconds = static_cast<float>(waypoint.delayMilliseconds) / 1000.0f;
            const engine::UINumericInputResult delayResult = engine::FloatInput(
                    ui, config, input, assets,
                    ("sector_editor_patrol_wp_delay_" + std::to_string(index)).c_str(),
                    waypointLayout.delayInput, smallFont,
                    delaySeconds, state.delayInputs[index], 0.0f,
                    static_cast<float>(std::numeric_limits<int>::max()) / 1000.0f, 3);
            if (delayResult.changed) editor.SetWaypointDelaySeconds(index, delaySeconds);

            static const std::vector<std::string> gaits = {"Walk", "Run"};
            int gait = waypoint.gait == SectorPatrolGait::Run ? 1 : 0;
            const int previousGait = gait;
            if (engine::Option(ui, config, input, assets,
                        ("sector_editor_patrol_wp_gait_" + std::to_string(index)).c_str(),
                        waypointLayout.gaitInput, smallFont, gaits, gait)
                    && gait != previousGait) {
                editor.SetWaypointGait(index,
                        gait == 1 ? SectorPatrolGait::Run : SectorPatrolGait::Walk);
            }

            bool look = waypoint.lookAround;
            if (engine::Checkbox(ui, config, input, assets,
                        ("sector_editor_patrol_wp_look_" + std::to_string(index)).c_str(),
                        waypointLayout.lookCheckbox, smallFont,
                        "Look around", look)) {
                editor.SetWaypointLookAround(index, look);
            }
            engine::Text(ui, config, assets,
                    waypointLayout.arcLabel, smallFont, "Arc (deg)",
                    engine::UITextJustify::Left, config.mutedTextColor);
            float arc = waypoint.lookArcDegrees;
            const engine::UINumericInputResult arcResult = engine::FloatInput(
                    ui, config, input, assets,
                    ("sector_editor_patrol_wp_arc_" + std::to_string(index)).c_str(),
                    waypointLayout.arcInput,
                    smallFont, arc, state.arcInputs[index], 0.0f, 360.0f, 1);
            if (arcResult.changed) editor.SetWaypointLookArc(index, arc);
            y += 188.0f;
        }

        if (engine::Button(ui, config, input, assets,
                    "sector_editor_patrol_add_waypoint",
                    {0.0f, y, std::min(220.0f, formScroll.viewport.width), RowHeight},
                    font, "Add Waypoint")) {
            editor.AddWaypoint();
        }
        engine::EndScrollArea(ui, config, input, formScroll, state.formScroll);
    }

    engine::Text(config, assets,
            {layout.listPane.x, layout.modal.y + layout.modal.height - 66.0f,
                    layout.saveButton.x - layout.listPane.x - 18.0f, 44.0f},
            smallFont, state.validationMessage.c_str(),
            engine::UITextJustify::Left,
            state.validationMessage.empty() ? config.mutedTextColor
                                            : config.invalidColor,
            true);
    if (engine::Button(ui, config, input, assets, "sector_editor_patrol_save",
                layout.saveButton, font, "Save") && editor.SaveAndClose()) {
        ConsumeRemainingInput(input);
        return SectorEditorPatrolEditorPanelResult::Saved;
    }
    if (engine::Button(ui, config, input, assets, "sector_editor_patrol_cancel",
                layout.cancelButton, font, "Cancel")) {
        editor.Cancel();
        ConsumeRemainingInput(input);
        return SectorEditorPatrolEditorPanelResult::Cancelled;
    }
    ConsumeRemainingInput(input);
    return SectorEditorPatrolEditorPanelResult::None;
}

} // namespace game
