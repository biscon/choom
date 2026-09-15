#include "sector_editor/inspector/SectorEditorCameraInspector.h"

#include "sector_editor/SectorEditorUiHelpers.h"
#include "sector_demo/SectorTopologyUnits.h"

#include <cstdio>

namespace game {

bool DrawSectorEditorCameraInspector(
        engine::UIContext& ui, const engine::UIConfig& config, engine::Input& input,
        engine::AssetManager& assets, engine::FontHandle font,
        float contentWidth, float rowHeight, float gap,
        const SectorAuthoringCamera& camera, CameraEditingUiState& uiState,
        SectorEditorCameraEditingService& editing)
{
    if (uiState.bufferedCameraId != camera.id) {
        uiState = {};
        std::snprintf(uiState.referenceIdBuffer, sizeof(uiState.referenceIdBuffer), "%s", camera.referenceId.c_str());
        uiState.bufferedCameraId = camera.id;
    }
    float y = 0;
    engine::Text(ui, config, assets, {0, y, contentWidth, 34}, font,
            TextFormat("Camera: %d", camera.id), engine::UITextJustify::Left, config.textColor);
    y += 38;
    const auto idLayout = BuildSectorEditorCameraFieldLayout(y, contentWidth, rowHeight, gap);
    engine::Text(ui, config, assets, idLayout.labelRect, font,
            "Script ID", engine::UITextJustify::Left, config.mutedTextColor);
    const auto idResult = engine::TextInput(ui, config, input, assets, "camera_id",
            idLayout.inputRect, font, uiState.referenceIdBuffer, sizeof(uiState.referenceIdBuffer),
            1, sizeof(uiState.referenceIdBuffer) - 1, engine::UITextJustify::Left);
    if (idResult.submitted) {
        const std::string requested{uiState.referenceIdBuffer};
        if (editing.ValidateSelectedReferenceId(requested, uiState.referenceIdError)
                && (requested == camera.referenceId || editing.RenameSelected(requested)))
            uiState.referenceIdError.clear();
    }
    y += SectorEditorCameraFieldHeight(rowHeight, gap);
    if (!uiState.referenceIdError.empty()) {
        engine::Text(ui, config, assets, {0, y, contentWidth, rowHeight * 2}, font,
                uiState.referenceIdError.c_str(), engine::UITextJustify::Left, config.invalidColor, true);
        y += rowHeight * 2 + gap;
    }
    const float values[] = {SectorCoordToVisibleAuthoring(camera.x), camera.y,
            SectorCoordToVisibleAuthoring(camera.z), camera.yawDegrees, camera.pitchDegrees,
            camera.rollDegrees, camera.verticalFovDegrees};
    const float minimum[] = {-8192, -8192, -8192, -36000, -89.9f, -36000, 1};
    const float maximum[] = {8192, 8192, 8192, 36000, 89.9f, 36000, 179};
    engine::UIFloatInputState* states[] = {&uiState.xInput, &uiState.yInput, &uiState.zInput,
            &uiState.orientationInput, &uiState.pitchInput, &uiState.rollInput, &uiState.fovInput};
    static_assert(std::size(values) == std::size(SectorEditorCameraFields));
    for (size_t i = 0; i < std::size(SectorEditorCameraFields); ++i) {
        const auto layout = BuildSectorEditorCameraFieldLayout(y, contentWidth, rowHeight, gap);
        const auto result = DrawLabeledFloatInput(ui, config, input, assets, font,
                SectorEditorCameraFields[i].id, SectorEditorCameraFields[i].label,
                layout.labelRect, layout.inputRect, engine::UITextJustify::Right,
                values[i], *states[i], minimum[i], maximum[i], 3);
        if (result.changed && result.finite && result.value != values[i]) {
            if (i < 3) {
                Vector3 position{values[0], values[1], values[2]};
                if (i == 0) position.x = result.value;
                if (i == 1) position.y = result.value;
                if (i == 2) position.z = result.value;
                editing.SetSelectedPosition(position);
            } else if (i == 3) editing.SetSelectedOrientation(result.value);
            else editing.SetSelectedLens(i == 4 ? result.value : values[4],
                    i == 5 ? result.value : values[5], i == 6 ? result.value : values[6]);
        }
        y += SectorEditorCameraFieldHeight(rowHeight, gap);
    }
    return engine::Button(ui, config, input, assets, "camera_delete",
            {0, y, contentWidth, rowHeight}, font, "Delete Camera");
}

} // namespace game
