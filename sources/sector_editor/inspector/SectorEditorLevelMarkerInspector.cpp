#include "sector_editor/inspector/SectorEditorLevelMarkerInspector.h"

#include "sector_editor/SectorEditorUiHelpers.h"
#include "sector_demo/SectorTopologyUnits.h"

#include <cstdio>

namespace game {

float MeasureSectorEditorLevelMarkerInspectorContentHeight(
        const LevelMarkerEditingUiState& uiState,
        float rowHeight,
        float gap)
{
    return 38.0f + (rowHeight + gap) * 6.0f
            + (uiState.referenceIdError.empty() ? 0.0f : 36.0f);
}

bool DrawSectorEditorLevelMarkerInspector(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        float contentWidth,
        float rowHeight,
        float gap,
        const SectorAuthoringLevelMarker& marker,
        LevelMarkerEditingUiState& uiState,
        SectorEditorLevelMarkerEditingService& editing)
{
    if (uiState.bufferedMarkerId != marker.id) {
        std::snprintf(
                uiState.referenceIdBuffer,
                sizeof(uiState.referenceIdBuffer),
                "%s",
                marker.referenceId.c_str());
        uiState.bufferedMarkerId = marker.id;
        uiState.referenceIdError.clear();
        uiState.xInput = engine::UIFloatInputState{};
        uiState.yInput = engine::UIFloatInputState{};
        uiState.zInput = engine::UIFloatInputState{};
        uiState.orientationInput = engine::UIFloatInputState{};
    }

    float y = 0.0f;
    engine::Text(
            ui, config, assets, Rectangle{0.0f, y, contentWidth, 34.0f}, font,
            TextFormat("Level Marker: %d", marker.id),
            engine::UITextJustify::Left, config.textColor);
    y += 38.0f;

    constexpr float LabelWidth = 92.0f;
    engine::Text(
            ui, config, assets, Rectangle{0.0f, y, LabelWidth, rowHeight}, font,
            "ID", engine::UITextJustify::Left, config.mutedTextColor);
    const engine::UITextInputResult idResult = engine::TextInput(
            ui, config, input, assets,
            "sector_editor_level_marker_id",
            Rectangle{LabelWidth, y, contentWidth - LabelWidth, rowHeight},
            font,
            uiState.referenceIdBuffer,
            sizeof(uiState.referenceIdBuffer),
            1,
            sizeof(uiState.referenceIdBuffer) - 1,
            engine::UITextJustify::Left);
    if (idResult.submitted) {
        const std::string requestedId{uiState.referenceIdBuffer};
        if (editing.ValidateSelectedReferenceId(requestedId, uiState.referenceIdError)
                && (requestedId == marker.referenceId || editing.RenameSelected(requestedId))) {
            uiState.referenceIdError.clear();
        }
    }
    y += rowHeight + gap;
    if (!uiState.referenceIdError.empty()) {
        engine::Text(
                ui, config, assets, Rectangle{0.0f, y, contentWidth, 34.0f}, font,
                uiState.referenceIdError.c_str(), engine::UITextJustify::Left,
                config.invalidColor, true);
        y += 36.0f;
    }

    const auto drawPosition = [&](const char* controlId,
                                  const char* label,
                                  float current,
                                  engine::UIFloatInputState& inputState,
                                  int axis) {
        const SectorEditorInspectorNumericRowLayout layout =
                BuildSectorEditorInspectorRightFloatRowLayout(y, contentWidth, rowHeight, gap);
        const SectorEditorFloatInputResult result = DrawLabeledFloatInput(
                ui, config, input, assets, font, controlId, label,
                layout.labelRect, layout.inputRect, engine::UITextJustify::Right,
                current, inputState, -8192.0f, 8192.0f, 3);
        if (result.changed && result.finite && result.value != current) {
            Vector3 position{
                    SectorCoordToVisibleAuthoring(marker.x),
                    marker.y,
                    SectorCoordToVisibleAuthoring(marker.z)};
            if (axis == 0) position.x = result.value;
            if (axis == 1) position.y = result.value;
            if (axis == 2) position.z = result.value;
            editing.SetSelectedPosition(position);
        }
        y += rowHeight + gap;
    };
    drawPosition("sector_editor_level_marker_x", "X", SectorCoordToVisibleAuthoring(marker.x), uiState.xInput, 0);
    drawPosition("sector_editor_level_marker_y", "Y", marker.y, uiState.yInput, 1);
    drawPosition("sector_editor_level_marker_z", "Z", SectorCoordToVisibleAuthoring(marker.z), uiState.zInput, 2);

    const SectorEditorInspectorNumericRowLayout orientationLayout =
            BuildSectorEditorInspectorRightFloatRowLayout(y, contentWidth, rowHeight, gap);
    const SectorEditorFloatInputResult orientationResult = DrawLabeledFloatInput(
            ui, config, input, assets, font,
            "sector_editor_level_marker_orientation", "Orientation",
            orientationLayout.labelRect, orientationLayout.inputRect,
            engine::UITextJustify::Right,
            marker.orientationDegrees, uiState.orientationInput,
            -36000.0f, 36000.0f, 2);
    if (orientationResult.changed && orientationResult.finite
            && orientationResult.value != marker.orientationDegrees) {
        editing.SetSelectedOrientation(orientationResult.value);
    }
    y += rowHeight + gap;

    return engine::Button(
            ui, config, input, assets,
            "sector_editor_level_marker_delete",
            Rectangle{0.0f, y, contentWidth, rowHeight},
            font, "Delete Level Marker");
}

} // namespace game
