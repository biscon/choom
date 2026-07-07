#pragma once

#include "engine/assets/AssetManager.h"
#include "engine/input/Input.h"
#include "engine/ui/UI.h"
#include "sector_editor/inspector/SectorEditorInspectorUiState.h"
#include "sector_editor/services/material_edit/SectorEditorMaterialEditingService.h"
#include "sector_editor/SectorEditorTypes.h"
#include "sector_editor/selection/SectorEditorSelectionState.h"

#include <raylib.h>

#include <functional>

namespace game {

class SectorEditorTextureCatalogService;

struct SectorEditorSectorInspectorCallbacks {
    std::function<bool()> tryRenameSelectedTopologySector;
    std::function<void(const char*)> setStatusText;
    std::function<void(const char*)> markTopologyDocumentEdited;
    std::function<bool(float, float)> applySectorHeights;
    std::function<bool(bool)> applySectorCeilingSky;
    std::function<bool(float)> applySectorAmbientIntensity;
    std::function<bool(Color)> applySectorAmbientColor;
    std::function<bool(TopologySectorTextureField, const SectorTopologyUvSettings&)> applySectorUv;
};

float SectorInspectorContentHeight(float rowH, float gap, bool hasIdError);

bool DrawTopologySectorInspector(
        engine::UIContext& ui,
        const engine::UIConfig& config,
        engine::Input& input,
        engine::AssetManager& assets,
        engine::FontHandle font,
        engine::FontHandle smallFont,
        engine::UIScrollAreaResult scroll,
        float contentW,
        float rowH,
        float gap,
        SectorTopologySector& sector,
        SectorEditorState& state,
        SelectionState& selectionState,
        SectorEditorUiState& uiState,
        InspectorIdUiState& inspectorIdUiState,
        MaterialEditingUiState& materialUiState,
        SectorEditorMaterialEditingService& materialEditing,
        SectorEditorTextureCatalogService& textureCatalog,
        const SectorEditorSectorInspectorCallbacks& callbacks);

} // namespace game
