#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/services/triggers/SectorEditorTriggerEditingService.h"

namespace game {

float MeasureSectorEditorTriggerInspectorContentHeight(
        const TriggerEditingUiState& uiState,
        float rowHeight,
        float gap);

bool DrawSectorEditorTriggerInspector(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        float contentWidth,
        float rowHeight,
        float gap,
        const SectorAuthoringTrigger& trigger,
        TriggerEditingUiState& uiState,
        SectorEditorTriggerEditingService& editing);

} // namespace game
