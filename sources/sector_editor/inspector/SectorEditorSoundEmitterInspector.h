#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/services/sound_emitters/SectorEditorSoundEmitterEditingService.h"

namespace game {

class SectorEditorSoundService;

float MeasureSectorEditorSoundEmitterInspectorContentHeight(
        const SoundEmitterEditingUiState& uiState, float rowHeight, float gap);

bool DrawSectorEditorSoundEmitterInspector(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        float contentWidth,
        float rowHeight,
        float gap,
        const SectorAuthoringSoundEmitter& emitter,
        SoundEmitterEditingUiState& uiState,
        SectorEditorSoundEmitterEditingService& editing,
        SectorEditorSoundService& sounds);

} // namespace game
