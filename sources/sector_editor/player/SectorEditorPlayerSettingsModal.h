#pragma once

#include "sector_editor/player/SectorEditorPlayerSettingsService.h"

namespace game {

SectorEditorPlayerSettingsSaveResult DrawSectorEditorPlayerSettingsModal(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont,
        engine::EngineContext& engineContext,
        SectorEditorPlayerSettingsService& service);

} // namespace game
