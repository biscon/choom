#pragma once

#include "engine/EngineContext.h"
#include "engine/ui/UI.h"
#include "sector_editor/services/level_markers/SectorEditorLevelMarkerEditingService.h"
#include "sector_editor/services/level_markers/SectorEditorLevelMarkerEditingState.h"

namespace game {

float MeasureSectorEditorLevelMarkerInspectorContentHeight(
        const LevelMarkerEditingUiState& uiState,
        float rowHeight,
        float gap);

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
        SectorEditorLevelMarkerEditingService& editing);

} // namespace game
