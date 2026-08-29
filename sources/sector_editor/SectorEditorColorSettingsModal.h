#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/SectorEditorModalTypes.h"

namespace game {

enum class SectorEditorColorSettingsModalAction {
    None,
    Cancel,
    Apply
};

SectorEditorColorSettingsModalAction DrawSectorEditorColorSettingsModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont,
        SectorEditorColorSettingsModalState& state);

} // namespace game
