#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/services/material_edit/SectorEditorMaterialEditingService.h"
#include "sector_editor/SectorEditorTypes.h"

#include <raylib.h>

#include <functional>
#include <string>

namespace game {

struct SectorEditorPreviewUvPanelContext {
    engine::UIContext& ui;
    const engine::UIConfig& config;
    engine::Input& input;
    engine::AssetManager& assets;
    engine::FontHandle font;
    engine::FontHandle smallFont;

    Rectangle panelRect;

    SectorEditorState& state;
    SectorEditorUiState& uiState;
    std::string& statusText;

    SectorEditorMaterialEditingService& materialEditing;
    std::function<bool(int, bool)> setLineDefBlocksPlayer;
};

Rectangle BuildSectorEditorPreviewUvPanelRect();

bool DrawSectorEditorPreviewUvPanel(SectorEditorPreviewUvPanelContext& context);

} // namespace game
