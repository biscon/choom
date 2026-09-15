#pragma once

#include "engine/EngineContext.h"
#include "engine/ui/UI.h"
#include "sector_editor/SectorEditorUiHelpers.h"
#include <iterator>
#include "sector_editor/services/cameras/SectorEditorCameraEditingService.h"
#include "sector_editor/services/cameras/SectorEditorCameraEditingState.h"

namespace game {

struct SectorEditorCameraField { const char* id; const char* label; };
inline constexpr SectorEditorCameraField SectorEditorCameraFields[] = {
        {"camera_x", "X (map units)"}, {"camera_y", "Height (map units)"},
        {"camera_z", "Z (map units)"}, {"camera_yaw", "Yaw (degrees)"},
        {"camera_pitch", "Pitch (degrees)"}, {"camera_roll", "Roll (degrees)"},
        {"camera_fov", "Vertical FOV (degrees)"}};

inline float SectorEditorCameraFieldHeight(float rowHeight, float gap)
{
    return rowHeight * 2 + gap * 2;
}
inline SectorEditorInspectorNumericRowLayout BuildSectorEditorCameraFieldLayout(
        float y, float width, float rowHeight, float gap)
{
    return {{0, y, width, rowHeight}, {0, y + rowHeight + gap, width, rowHeight}};
}
inline float MeasureSectorEditorCameraInspectorContentHeight(
        const CameraEditingUiState& uiState, float rowHeight, float gap)
{
    return 38 + (std::size(SectorEditorCameraFields) + 1) * SectorEditorCameraFieldHeight(rowHeight, gap)
            + (uiState.referenceIdError.empty() ? 0 : rowHeight * 2 + gap)
            + rowHeight + gap; // Delete and bottom padding.
}

bool DrawSectorEditorCameraInspector(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        float contentWidth,
        float rowHeight,
        float gap,
        const SectorAuthoringCamera& camera,
        CameraEditingUiState& uiState,
        SectorEditorCameraEditingService& editing);

} // namespace game
