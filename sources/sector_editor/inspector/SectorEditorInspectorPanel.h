#pragma once

#include "engine/EngineContext.h"
#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/selection/SectorEditorSelectionService.h"
#include "sector_editor/services/material_edit/SectorEditorMaterialEditingService.h"
#include "sector_editor/tools/placed_objects/SectorEditorPlacedObjectActions.h"

#include <array>
#include <string>

#include <raylib.h>

namespace game {

enum class SectorEditorInspectorPanelRequestKind {
    MarkTopologyDocumentEdited,
    RebuildSectorCollisionWorld,
    BeginAuthoringInsertVertex,
    DeleteSelectedRuntimeObject,
    DeleteSelectedLight,
    BakeLightmaps
};

struct SectorEditorInspectorPanelRequest {
    SectorEditorInspectorPanelRequestKind kind = SectorEditorInspectorPanelRequestKind::BakeLightmaps;
    std::string status;
    int lineId = -1;
};

struct SectorEditorInspectorPanelResult {
    std::array<SectorEditorInspectorPanelRequest, 8> requests{};
    int requestCount = 0;
};

struct SectorEditorInspectorPanelContext {
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

    SectorEditorSelectionServiceContext& selection;
    SectorEditorPlacedObjectActionContext& placedObjectActions;
    SectorEditorMaterialEditingService& materialEditing;
    engine::EngineContext* engineContext = nullptr;
};

SectorEditorInspectorPanelResult DrawSectorEditorInspectorPanel(
        SectorEditorInspectorPanelContext& context);

} // namespace game
