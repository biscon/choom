#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/services/material_edit/SectorEditorMaterialEditingService.h"
#include "sector_editor/SectorEditorTypes.h"

#include <functional>
#include <string>

namespace game {

struct SectorEditorMaterialInspectorCallbacks {
    std::function<void(int, TopologyWallPart)> selectTopologySideDef;
    std::function<bool(int, bool)> setAuthoringLineDefBlocksPlayer;
};

struct SectorEditorMaterialInspectorContext {
    engine::UIContext& ui;
    const engine::UIConfig& config;
    engine::Input& input;
    engine::AssetManager& assets;
    engine::FontHandle font;
    engine::FontHandle smallFont;
    engine::UIScrollAreaResult scroll;
    float contentW = 0.0f;
    float rowH = 0.0f;
    float gap = 0.0f;
    SectorEditorState& state;
    SectorEditorUiState& uiState;
    std::string& statusText;
    const SectorEditorMaterialInspectorCallbacks& callbacks;
    SectorEditorMaterialEditingService& materialEditing;
};

bool DrawTopologySideDefMaterialInspector(SectorEditorMaterialInspectorContext& context);

} // namespace game
