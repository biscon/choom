#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/SectorEditorModalTypes.h"
#include "sector_demo/SectorAuthoringGraph.h"

namespace game {

enum class SectorEditorLiquidSettingsModalAction {
    None,
    Cancel,
    Apply
};

void OpenSectorEditorLiquidSettingsModal(
        SectorEditorLiquidSettingsModalState& state,
        const SectorAuthoringFaceAnchor& anchor);

SectorEditorLiquidSettingsModalAction DrawSectorEditorLiquidSettingsModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont,
        SectorEditorLiquidSettingsModalState& state);

} // namespace game
